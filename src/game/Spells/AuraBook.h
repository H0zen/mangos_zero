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

#include "Spells/Ids.h"
#include "Spells/Wire.h"

#include <cstdint>
#include <vector>

namespace spells
{
    /// A live aura, addressed by handle rather than by pointer.
    enum class AuraRef : uint32_t { None = 0 };

    /// What one effect of one aura is doing to its bearer.
    struct AuraEffect
    {
        EffectIndex index = EffectIndex::First;
        uint16_t type = 0;          ///< the SPELL_AURA_* kind
        int32_t amount = 0;
        int32_t misc = 0;
        uint32_t periodMs = 0;      ///< 0 for a non-ticking effect
        uint32_t sincePulseMs = 0;
    };

    /// Everything needed to place an aura; built by the effect that applies it.
    struct AuraSpec
    {
        SpellId spell = SpellId::None;
        uint64_t caster = 0;
        uint64_t castItem = 0;
        CastId cast = CastId::None;

        int32_t durationMs = 0;     ///< negative means it does not expire
        uint8_t stacks = 1;
        uint8_t maxStacks = 1;
        uint8_t casterLevel = 1;
        bool positive = true;
        bool passive = false;       ///< never occupies a visible slot

        std::vector<AuraEffect> effects;
    };

    /**
     * The auras carried by one unit.
     *
     * ## An aura is a property, not an object
     *
     * Auras belong to a unit the way health and power do. They are values held
     * inside the unit's own state, not entities with lives of their own that a
     * unit happens to point at. Nothing here allocates an aura, and there is no
     * aura class to inherit from: an aura is a record in this book, addressed by
     * a handle in the same spirit as a field index.
     *
     * That is what removes the deferred-deletion lists the old engine kept --
     * one for auras and one for holders -- which existed because a removal in
     * the middle of applying a modifier could destroy an object somebody still
     * held a pointer to. Those lists were the symptom of an aura being a loose
     * object nobody owned. When the aura is part of the unit, there is no
     * pointer to dangle: a removal retires a record, a walk in progress keeps
     * its footing, and the storage is reclaimed when the book is next quiet.
     *
     * ## Modifiers are a derived stat
     *
     * The simulation reads "how much does this unit's auras modify X" constantly
     * and changes it rarely -- exactly the shape of maximum health, which is
     * maintained when stamina changes rather than recomputed on every read. So
     * the totals are kept up to date as auras come and go, and reading one is a
     * lookup. The old engine summed a linked list on every query, from ninety-odd
     * call sites, in every damage calculation.
     *
     * ## Two jobs, kept apart
     *
     * A unit's auras answer two unrelated questions, and the old engine fused
     * them into one container:
     *
     *  - the simulation asks "what modifies me, and by how much" -- answered by
     *    an index keyed on aura type;
     *  - the client asks "what do I draw on this unit" -- answered by a fixed
     *    set of visible slots, of which there are exactly
     *    `VISIBLE_AURA_SLOTS` and no more.
     *
     * They are separate here. An aura that finds no free slot is still fully
     * simulated; it is simply not drawn. The old engine let slot exhaustion
     * decide whether an aura existed at all.
     */
    class AuraBook
    {
        public:
            /// A read-only look at one live aura.
            struct View
            {
                AuraRef ref = AuraRef::None;
                SpellId spell = SpellId::None;
                uint64_t caster = 0;
                uint8_t stacks = 1;
                int32_t durationMs = 0;
                bool positive = true;

                bool Valid() const { return ref != AuraRef::None; }
            };

            // --- placing and removing -----------------------------------------

            /**
             * Adds the aura, or folds it into a matching one already present.
             *
             * Stacking is decided here and nowhere else, so "refresh duration",
             * "add a stack" and "replace a weaker one" are one rule rather than
             * a habit repeated across effect handlers.
             */
            AuraRef Apply(const AuraSpec& spec);

            /// Drops one aura. Silent if the ref no longer names anything.
            void Remove(AuraRef ref);

            /// Drops every aura of a spell, whoever cast it.
            void RemoveSpell(SpellId spell);

            /// Drops every aura a given caster placed here.
            void RemoveFrom(uint64_t caster);

            /// Drops one stack, removing the aura when the last one goes.
            void RemoveStack(AuraRef ref);

            void Clear();

            // --- asking --------------------------------------------------------

            View Get(AuraRef ref) const;

            bool Has(SpellId spell) const;

            /// Live auras carrying an effect of this type.
            std::vector<AuraRef> OfType(uint16_t auraType) const;

            /**
             * Summed amount across every effect of this type.
             *
             * Maintained as auras change, not computed here, because this is
             * read on every damage calculation and every stat query.
             */
            int32_t Total(uint16_t auraType) const;

            /// Largest positive and most negative single amount of this type.
            int32_t Strongest(uint16_t auraType) const;
            int32_t Weakest(uint16_t auraType) const;

            /// Whether any aura of this type is present, without building a list.
            bool HasType(uint16_t auraType) const;

            /**
             * The aura of this type applied most recently.
             *
             * Recency is recorded, never read out of iteration order. Taunt
             * priority is the reason this exists: the old engine walked a list
             * backwards and so made the layout of a container into a game rule.
             */
            AuraRef Newest(uint16_t auraType) const;

            /// Auras of this type, newest first, for callers that need the order.
            std::vector<AuraRef> ByRecency(uint16_t auraType) const;

            // --- time ----------------------------------------------------------

            /**
             * Advances durations and periodic effects.
             *
             * Ticks due in this step are collected rather than fired, so the
             * book never calls out into effect code while its own state is half
             * advanced. The caller fires them.
             */
            struct Pulse
            {
                AuraRef aura = AuraRef::None;
                EffectIndex effect = EffectIndex::First;
            };

            void Advance(uint32_t elapsedMs, std::vector<Pulse>& due, std::vector<AuraRef>& expired);

            // --- what the client sees ------------------------------------------

            /// The visible slots, in slot order; unused slots are absent.
            const std::vector<wire::VisibleAura>& Visible() const { return m_visible; }

            /// True when a slot changed since the last time this was cleared.
            bool VisibleDirty() const { return m_visibleDirty; }
            void ClearVisibleDirty() { m_visibleDirty = false; }

        private:
            struct Held
            {
                AuraRef ref = AuraRef::None;
                AuraSpec spec;
                uint64_t rank = 0;          ///< order of application, never reused
                VisibleSlot slot = VisibleSlot::None;
                bool retired = false;       ///< removed; storage not yet reclaimed
            };

            /// A maintained sum, the way maximum health is maintained.
            struct Running
            {
                uint16_t type = 0;
                int32_t total = 0;
                int32_t strongest = 0;
                int32_t weakest = 0;
                uint16_t count = 0;
            };

            /// Finds a drawable slot, or None when all are taken.
            VisibleSlot ClaimSlot(bool positive);
            void ReleaseSlot(VisibleSlot slot);
            void Redraw();

            /// Folds one effect into the running totals, or takes it back out.
            void Account(const AuraEffect& effect, int8_t sign);

            const Running* Find(uint16_t auraType) const;

            /// Drops retired entries. Only safe when no walk is in progress.
            void Reclaim();

            std::vector<Held> m_held;
            std::vector<Running> m_totals;      ///< sorted by type
            std::vector<wire::VisibleAura> m_visible;
            uint64_t m_clock = 0;
            uint32_t m_nextRef = 1;
            int m_walks = 0;
            bool m_visibleDirty = false;
    };
}
