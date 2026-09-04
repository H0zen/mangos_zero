/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 1.12.x client.
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Spells/AuraBook.h"

#include <algorithm>

namespace spells
{
    namespace
    {
        /// A retired record is still in storage but is no longer an aura.
        bool Live(const AuraBook::View& view) { return view.Valid(); }
    }

    // -------------------------------------------------------------------------
    // running totals -- maintained the way maximum health is maintained
    // -------------------------------------------------------------------------

    const AuraBook::Running* AuraBook::Find(uint16_t auraType) const
    {
        const auto at = std::lower_bound(m_totals.begin(), m_totals.end(), auraType,
                                         [](const Running& running, uint16_t wanted) { return running.type < wanted; });
        return (at != m_totals.end() && at->type == auraType) ? &(*at) : nullptr;
    }

    void AuraBook::Account(const AuraEffect& effect, int8_t sign)
    {
        const auto at = std::lower_bound(m_totals.begin(), m_totals.end(), effect.type,
                                         [](const Running& running, uint16_t wanted) { return running.type < wanted; });

        if (at == m_totals.end() || at->type != effect.type)
        {
            if (sign < 0)
            {
                return;                     // nothing to take back out
            }

            Running fresh;
            fresh.type = effect.type;
            fresh.total = effect.amount;
            fresh.strongest = effect.amount;
            fresh.weakest = effect.amount;
            fresh.count = 1;
            m_totals.insert(at, fresh);
            return;
        }

        at->total += sign * effect.amount;
        at->count = static_cast<uint16_t>(at->count + sign);

        if (at->count == 0)
        {
            m_totals.erase(at);
            return;
        }

        if (sign > 0)
        {
            at->strongest = std::max(at->strongest, effect.amount);
            at->weakest = std::min(at->weakest, effect.amount);
            return;
        }

        // Only an extreme leaving costs a rescan; the common removal does not.
        if (effect.amount == at->strongest || effect.amount == at->weakest)
        {
            Rescan(effect.type);
        }
    }

    void AuraBook::Rescan(uint16_t auraType)
    {
        const auto at = std::lower_bound(m_totals.begin(), m_totals.end(), auraType,
                                         [](const Running& running, uint16_t wanted) { return running.type < wanted; });
        if (at == m_totals.end() || at->type != auraType)
        {
            return;
        }

        bool first = true;
        for (const auto& held : m_held)
        {
            if (held.retired)
            {
                continue;
            }
            for (const auto& effect : held.spec.effects)
            {
                if (effect.type != auraType)
                {
                    continue;
                }
                if (first)
                {
                    at->strongest = effect.amount;
                    at->weakest = effect.amount;
                    first = false;
                }
                else
                {
                    at->strongest = std::max(at->strongest, effect.amount);
                    at->weakest = std::min(at->weakest, effect.amount);
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // visible slots -- the only part of this the client can see
    // -------------------------------------------------------------------------

    VisibleSlot AuraBook::ClaimSlot(bool positive)
    {
        const uint8_t from = positive ? 0 : VISIBLE_POSITIVE_SLOTS;
        const uint8_t to = positive ? VISIBLE_POSITIVE_SLOTS : VISIBLE_AURA_SLOTS;

        for (uint8_t slot = from; slot < to; ++slot)
        {
            const bool taken = std::any_of(m_held.begin(), m_held.end(),
                                           [slot](const Held& held)
                                           {
                                               return !held.retired && Raw(held.slot) == slot;
                                           });
            if (!taken)
            {
                return static_cast<VisibleSlot>(slot);
            }
        }

        // Every slot is spoken for. The aura is still real and still simulated;
        // the client simply will not draw it.
        return VisibleSlot::None;
    }

    void AuraBook::ReleaseSlot(VisibleSlot slot)
    {
        if (Shown(slot))
        {
            m_visibleDirty = true;
        }
    }

    void AuraBook::Redraw()
    {
        m_visible.clear();
        for (const auto& held : m_held)
        {
            if (held.retired || !Shown(held.slot))
            {
                continue;
            }

            wire::VisibleAura shown;
            shown.spell = held.spec.spell;
            shown.level = held.spec.casterLevel;
            shown.stacks = held.spec.stacks;
            shown.durationMs = held.spec.durationMs;
            shown.maxDurationMs = held.spec.durationMs;
            shown.positive = held.spec.positive;
            m_visible.push_back(shown);
        }
        m_visibleDirty = false;
    }

    // -------------------------------------------------------------------------
    // placing and removing
    // -------------------------------------------------------------------------

    AuraRef AuraBook::Apply(const AuraSpec& spec)
    {
        // Stacking is decided here and nowhere else, so "refresh", "add a stack"
        // and "replace" are one rule rather than a habit repeated per effect.
        for (auto& held : m_held)
        {
            if (held.retired || held.spec.spell != spec.spell || held.spec.caster != spec.caster)
            {
                continue;
            }

            for (const auto& effect : held.spec.effects)
            {
                Account(effect, -1);
            }

            const uint8_t stacks = std::min<uint8_t>(static_cast<uint8_t>(held.spec.stacks + spec.stacks),
                                                     std::max<uint8_t>(spec.maxStacks, 1));
            const VisibleSlot slot = held.slot;

            held.spec = spec;
            held.spec.stacks = stacks;
            held.slot = slot;
            held.rank = ++m_clock;

            for (const auto& effect : held.spec.effects)
            {
                Account(effect, +1);
            }

            m_visibleDirty = true;
            return held.ref;
        }

        Held fresh;
        fresh.ref = static_cast<AuraRef>(m_nextRef++);
        fresh.spec = spec;
        fresh.spec.stacks = std::max<uint8_t>(spec.stacks, 1);
        fresh.rank = ++m_clock;
        fresh.slot = spec.passive ? VisibleSlot::None : ClaimSlot(spec.positive);

        for (const auto& effect : fresh.spec.effects)
        {
            Account(effect, +1);
        }

        m_held.push_back(fresh);
        m_visibleDirty = true;
        return fresh.ref;
    }

    void AuraBook::Remove(AuraRef ref)
    {
        for (auto& held : m_held)
        {
            if (held.retired || held.ref != ref)
            {
                continue;
            }

            for (const auto& effect : held.spec.effects)
            {
                Account(effect, -1);
            }

            ReleaseSlot(held.slot);
            held.retired = true;
            m_visibleDirty = true;
            break;
        }

        Reclaim();
    }

    void AuraBook::RemoveSpell(SpellId spell)
    {
        for (auto& held : m_held)
        {
            if (!held.retired && held.spec.spell == spell)
            {
                for (const auto& effect : held.spec.effects)
                {
                    Account(effect, -1);
                }
                ReleaseSlot(held.slot);
                held.retired = true;
                m_visibleDirty = true;
            }
        }
        Reclaim();
    }

    void AuraBook::RemoveFrom(uint64_t caster)
    {
        for (auto& held : m_held)
        {
            if (!held.retired && held.spec.caster == caster)
            {
                for (const auto& effect : held.spec.effects)
                {
                    Account(effect, -1);
                }
                ReleaseSlot(held.slot);
                held.retired = true;
                m_visibleDirty = true;
            }
        }
        Reclaim();
    }

    void AuraBook::RemoveStack(AuraRef ref)
    {
        for (auto& held : m_held)
        {
            if (held.retired || held.ref != ref)
            {
                continue;
            }

            if (held.spec.stacks <= 1)
            {
                Remove(ref);
                return;
            }

            for (const auto& effect : held.spec.effects)
            {
                Account(effect, -1);
            }

            --held.spec.stacks;

            for (const auto& effect : held.spec.effects)
            {
                Account(effect, +1);
            }

            m_visibleDirty = true;
            return;
        }
    }

    void AuraBook::Clear()
    {
        m_held.clear();
        m_totals.clear();
        m_visible.clear();
        m_visibleDirty = true;
    }

    void AuraBook::Reclaim()
    {
        // Storage is only compacted while nothing is walking it. Retired records
        // are harmless until then: they answer no query and modify nothing.
        if (m_walks != 0)
        {
            return;
        }

        m_held.erase(std::remove_if(m_held.begin(), m_held.end(),
                                    [](const Held& held) { return held.retired; }),
                     m_held.end());
    }

    // -------------------------------------------------------------------------
    // asking
    // -------------------------------------------------------------------------

    AuraBook::View AuraBook::Get(AuraRef ref) const
    {
        for (const auto& held : m_held)
        {
            if (held.retired || held.ref != ref)
            {
                continue;
            }

            View view;
            view.ref = held.ref;
            view.spell = held.spec.spell;
            view.caster = held.spec.caster;
            view.stacks = held.spec.stacks;
            view.durationMs = held.spec.durationMs;
            view.positive = held.spec.positive;
            return view;
        }

        // A handle that no longer names an aura resolves to nothing, which is
        // the point of handing out handles instead of pointers.
        return View();
    }

    bool AuraBook::Has(SpellId spell) const
    {
        return std::any_of(m_held.begin(), m_held.end(),
                           [spell](const Held& held) { return !held.retired && held.spec.spell == spell; });
    }

    bool AuraBook::HasType(uint16_t auraType) const
    {
        return Find(auraType) != nullptr;
    }

    int32_t AuraBook::Total(uint16_t auraType) const
    {
        const Running* running = Find(auraType);
        return running ? running->total : 0;
    }

    int32_t AuraBook::Strongest(uint16_t auraType) const
    {
        const Running* running = Find(auraType);
        return running ? running->strongest : 0;
    }

    int32_t AuraBook::Weakest(uint16_t auraType) const
    {
        const Running* running = Find(auraType);
        return running ? running->weakest : 0;
    }

    std::vector<AuraRef> AuraBook::OfType(uint16_t auraType) const
    {
        std::vector<AuraRef> found;
        for (const auto& held : m_held)
        {
            if (held.retired)
            {
                continue;
            }
            const bool carries = std::any_of(held.spec.effects.begin(), held.spec.effects.end(),
                                             [auraType](const AuraEffect& effect) { return effect.type == auraType; });
            if (carries)
            {
                found.push_back(held.ref);
            }
        }
        return found;
    }

    AuraRef AuraBook::Newest(uint16_t auraType) const
    {
        const Held* best = nullptr;
        for (const auto& held : m_held)
        {
            if (held.retired)
            {
                continue;
            }
            const bool carries = std::any_of(held.spec.effects.begin(), held.spec.effects.end(),
                                             [auraType](const AuraEffect& effect) { return effect.type == auraType; });
            if (carries && (!best || held.rank > best->rank))
            {
                best = &held;
            }
        }
        return best ? best->ref : AuraRef::None;
    }

    std::vector<AuraRef> AuraBook::ByRecency(uint16_t auraType) const
    {
        std::vector<const Held*> live;
        for (const auto& held : m_held)
        {
            if (held.retired)
            {
                continue;
            }
            const bool carries = std::any_of(held.spec.effects.begin(), held.spec.effects.end(),
                                             [auraType](const AuraEffect& effect) { return effect.type == auraType; });
            if (carries)
            {
                live.push_back(&held);
            }
        }

        std::sort(live.begin(), live.end(),
                  [](const Held* a, const Held* b) { return a->rank > b->rank; });

        std::vector<AuraRef> ordered;
        ordered.reserve(live.size());
        for (const auto* held : live)
        {
            ordered.push_back(held->ref);
        }
        return ordered;
    }

    // -------------------------------------------------------------------------
    // time
    // -------------------------------------------------------------------------

    void AuraBook::Advance(uint32_t elapsedMs, std::vector<Pulse>& due, std::vector<AuraRef>& expired)
    {
        ++m_walks;

        for (auto& held : m_held)
        {
            if (held.retired)
            {
                continue;
            }

            for (auto& effect : held.spec.effects)
            {
                if (effect.periodMs == 0)
                {
                    continue;
                }

                effect.sincePulseMs += elapsedMs;
                while (effect.sincePulseMs >= effect.periodMs)
                {
                    effect.sincePulseMs -= effect.periodMs;
                    due.push_back(Pulse{held.ref, effect.index});
                }
            }

            // A negative duration means the aura does not run out on its own.
            if (held.spec.durationMs < 0)
            {
                continue;
            }

            held.spec.durationMs -= static_cast<int32_t>(elapsedMs);
            if (held.spec.durationMs <= 0)
            {
                held.spec.durationMs = 0;
                expired.push_back(held.ref);
            }
        }

        --m_walks;

        // Ticks are reported, never fired from in here: the book must not call
        // out into effect code while its own state is half advanced. Expired
        // auras are reported too, and the caller removes them once it has run
        // whatever the removal is supposed to do.
        if (!due.empty() || !expired.empty())
        {
            m_visibleDirty = true;
        }
    }
}
