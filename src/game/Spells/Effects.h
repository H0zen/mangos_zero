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

#pragma once

#include "Combat/Attempt.h"
#include "Spells/AuraBook.h"
#include "Spells/Caster.h"
#include "Spells/Ids.h"

#include <cstdint>

namespace spells
{
    /**
     * What one effect of one spell is given when it runs.
     *
     * The context is handed in whole rather than assembled by each effect from
     * a caster pointer, which is how the old handlers each grew their own
     * slightly different idea of who was casting and at whom.
     */
    struct EffectCtx
    {
        CastId cast = CastId::None;
        SpellId spell = SpellId::None;
        EffectIndex index = EffectIndex::First;

        const Caster* caster = nullptr;
        Target target;

        uint32_t schoolMask = 0;
        uint64_t castItem = 0;

        /// The effect's base value, already through the talent modifiers.
        int32_t value = 0;
        int32_t misc = 0;
        float radius = 0.f;

        /**
         * The resolved attempt, for effects that deal damage or healing.
         *
         * Mitigation has already run. A damage effect reads this and applies it;
         * it does not roll, mitigate or absorb again. Effects that do not deal
         * damage -- an aura, a summon, a teleport, a dispel -- never produce an
         * attempt at all, so this stays empty for them.
         */
        combat::Outcome outcome;
    };

    /// What an effect did, for the log packets and for procs.
    struct EffectResult
    {
        bool ran = false;
        int32_t amount = 0;         ///< damage dealt, healing done, power given
        AuraRef aura = AuraRef::None;
        uint64_t spawned = 0;       ///< a summon or an object, when one was made
    };

    /// The signature of every spell effect. A free function, never a method.
    using EffectFn = EffectResult (*)(EffectCtx&);

    /**
     * The signature of an aura effect coming or going.
     *
     * Applying and removing are two entries, not one function told which way to
     * go by a boolean. The old engine's `Handle*(bool apply, bool real)` was
     * three behaviours in one body -- apply, remove, and a "not really" mode --
     * and every handler had to re-derive which one it was in before doing
     * anything. Splitting them is what makes each one readable on its own.
     */
    using AuraFn = void (*)(const Caster& caster, Target& bearer, const AuraEffect& effect);

    /**
     * The dispatch tables.
     *
     * The semantics of the effect indices are baked into what the client
     * expects, so each one has its own behaviour and this is, unavoidably, a
     * table of hand-written functions. What is data-driven are the *parameters*
     * that come out of Spell.dbc, not the behaviours. It is not sold as
     * anything more.
     *
     * The tables are filled at start-up. Binding an index twice stops the boot
     * rather than letting the last registration silently win. An index nothing
     * registered is a logged no-op, never a crash.
     */
    class EffectTable
    {
        public:
            static constexpr uint16_t MAX_EFFECTS = 192;
            static constexpr uint16_t MAX_AURA_TYPES = 192;

            /// Registers a spell effect. Aborts the boot on a double bind.
            static void Bind(uint16_t effectId, EffectFn fn, const char* name);

            /// Registers the two halves of an aura type. Either may be null when
            /// that direction genuinely does nothing.
            static void BindAura(uint16_t auraType, AuraFn onApply, AuraFn onRemove, const char* name);

            /// Runs an effect, or logs and does nothing if none is bound.
            static EffectResult Run(uint16_t effectId, EffectCtx& ctx);

            static void RunAuraApply(uint16_t auraType, const Caster& caster, Target& bearer, const AuraEffect& effect);
            static void RunAuraRemove(uint16_t auraType, const Caster& caster, Target& bearer, const AuraEffect& effect);

            /// Names, for logs and for the boot-time report of what is covered.
            static const char* NameOf(uint16_t effectId);
            static uint16_t BoundCount();
    };

    /**
     * Registers an effect at start-up by existing.
     *
     * A translation unit implementing an effect declares one of these at file
     * scope, so adding an effect is adding a file rather than also remembering
     * to edit a table somewhere else.
     */
    struct BindEffect
    {
        BindEffect(uint16_t effectId, EffectFn fn, const char* name) { EffectTable::Bind(effectId, fn, name); }
    };

    struct BindAuraType
    {
        BindAuraType(uint16_t auraType, AuraFn onApply, AuraFn onRemove, const char* name)
        {
            EffectTable::BindAura(auraType, onApply, onRemove, name);
        }
    };
}
