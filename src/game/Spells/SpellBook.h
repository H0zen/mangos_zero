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

#include <cstdint>
#include <vector>

namespace spells
{
    /// The knobs a talent turns on a spell.
    enum class Mod : uint8_t
    {
        Damage, Duration, Cost, CastTime, Range, Radius, CritChance,
        Stacks, Charges, Cooldown, GlobalCooldown, JumpTargets, ValueMultiplier,
    };

    /// One talent's alteration of a family of spells.
    struct SpellMod
    {
        Mod knob = Mod::Damage;
        bool percent = false;       ///< otherwise a flat addition
        int32_t value = 0;
        uint64_t family = 0;        ///< which spells it reaches
        SpellId source = SpellId::None;
        int16_t charges = -1;       ///< -1 for unlimited; consumed on use
    };

    /**
     * What one unit knows and when it may use it.
     *
     * Aggregated by a caster, absent entirely on things that do not learn
     * spells. Three concerns live here because they answer the same question --
     * may this cast start -- and separating them only forced the cast pipeline
     * to consult three objects in a fixed order.
     */
    class SpellBook
    {
        public:
            // --- knowing -------------------------------------------------------

            void Learn(SpellId spell);
            void Forget(SpellId spell);
            bool Knows(SpellId spell) const;

            /// Every spell known, for the initial-spells packet.
            const std::vector<SpellId>& Known() const { return m_known; }

            // --- readiness -----------------------------------------------------

            /**
             * Cooldowns run on two independent axes and a spell waits on both:
             * its own timer, and the timer shared by every spell of its
             * category. Potions share a category; healthstones share another.
             */
            void StartCooldown(SpellId spell, uint32_t categoryId, uint32_t spellMs, uint32_t categoryMs, uint64_t nowMs);

            bool Ready(SpellId spell, uint32_t categoryId, uint64_t nowMs) const;

            /// Milliseconds left on whichever axis is longer; zero when ready.
            uint32_t Remaining(SpellId spell, uint32_t categoryId, uint64_t nowMs) const;

            void ClearCooldown(SpellId spell);
            void ClearAllCooldowns();

            /// Cooldowns that have run out, so the client can be told once.
            void Expire(uint64_t nowMs, std::vector<SpellId>& finished);

            /**
             * The global cooldown is one timer for the whole book, not a
             * cooldown on any particular spell.
             */
            void StartGlobal(uint32_t durationMs, uint64_t nowMs);
            bool GlobalReady(uint64_t nowMs) const;

            // --- talents -------------------------------------------------------

            void AddMod(const SpellMod& mod);
            void RemoveMod(SpellId source);

            /**
             * Applies every mod that reaches this spell to one value.
             *
             * Modifiers are a first-class step of the cast, applied from the
             * first packet rather than bolted on later: the cost check, the cast
             * timer, the aura duration and the damage all pass through here. A
             * mod added after the fact would have to be threaded back through
             * every stage that had already been written without it.
             */
            int32_t Apply(Mod knob, SpellId spell, uint64_t family, int32_t value) const;

            /// Same, for values the client expects as a float.
            float Apply(Mod knob, SpellId spell, uint64_t family, float value) const;

            /// Spends a charge on every limited mod that reached this spell.
            void ConsumeCharges(Mod knob, SpellId spell, uint64_t family);

        private:
            struct Cooling
            {
                SpellId spell = SpellId::None;
                uint32_t category = 0;
                uint64_t spellUntilMs = 0;
                uint64_t categoryUntilMs = 0;
            };

            std::vector<SpellId> m_known;       ///< sorted
            std::vector<Cooling> m_cooling;     ///< sorted by spell
            std::vector<SpellMod> m_mods;
            uint64_t m_globalUntilMs = 0;
    };
}
