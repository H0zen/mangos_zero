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

namespace spells
{
    /// Where a cast came from. Not a class hierarchy -- a fact about one cast.
    enum class Origin : uint8_t
    {
        Player,     ///< CMSG_CAST_SPELL
        Item,       ///< CMSG_USE_ITEM
        Creature,   ///< AI
        Proc,       ///< triggered by another spell landing
        Script,     ///< the world firing something
    };

    /// One request to cast, parsed once and thereafter read-only.
    struct Intent
    {
        uint64_t caster = 0;
        SpellId spell = SpellId::None;
        wire::Targets targets;

        Origin origin = Origin::Player;
        uint64_t castItem = 0;
        SpellId triggeredBy = SpellId::None;

        /// A triggered cast pays no cost, waits on no cooldown and cannot fail
        /// the checks that exist to police player input.
        bool Triggered() const { return origin == Origin::Proc || origin == Origin::Script; }
    };

    /// How far along a cast is.
    enum class Phase : uint8_t
    {
        Idle,
        Casting,        ///< cast bar running; SPELL_START has been sent
        Travelling,     ///< left the caster, has not arrived
        Channelling,
        Done,
    };

    /**
     * The one cast a unit has in flight, plus the one channel.
     *
     * A unit may have at most one ordinary cast and, separately, one channel.
     * Everything the old engine expressed by subclassing a spell -- delayed
     * missiles, auto-repeat, next-melee-swing, channelled -- is a flag on the
     * spell's own data and a phase here. There is nothing to derive from.
     */
    class CastSlot
    {
        public:
            bool Busy() const { return m_phase != Phase::Idle && m_phase != Phase::Done; }
            Phase Where() const { return m_phase; }
            CastId Id() const { return m_cast; }
            SpellId Spell() const { return m_intent.spell; }
            const Intent& Request() const { return m_intent; }

            /// Takes the slot. Fails if something is already in it.
            bool Begin(CastId id, const Intent& intent, uint32_t castTimeMs);

            /**
             * Advances the timer.
             *
             * Returns true when the cast becomes due to fire this step, so the
             * caller launches it; the slot never reaches into the world itself.
             */
            bool Advance(uint32_t elapsedMs);

            /// Time still to run, for SMSG_SPELL_START and the client's bar.
            uint32_t Remaining() const { return m_remainingMs; }

            /**
             * Pushes the cast back after taking damage.
             *
             * Whether a spell can be pushed back at all is a property of the
             * spell, checked by the caller; the slot only carries the timer.
             */
            void PushBack(uint32_t delayMs);

            void Cancel();

            /// Finishes cleanly, leaving the slot free.
            void Finish();

            // --- channels ------------------------------------------------------

            void BeginChannel(CastId id, SpellId spell, uint32_t durationMs);
            bool Channelling() const { return m_phase == Phase::Channelling; }

            /// Advances a channel; reports the ticks that came due.
            bool AdvanceChannel(uint32_t elapsedMs, uint32_t periodMs, uint32_t& ticks);

            uint32_t ChannelRemaining() const { return m_channelRemainingMs; }
            void EndChannel();

        private:
            Phase m_phase = Phase::Idle;
            CastId m_cast = CastId::None;
            Intent m_intent;

            uint32_t m_totalMs = 0;
            uint32_t m_remainingMs = 0;

            CastId m_channel = CastId::None;
            SpellId m_channelSpell = SpellId::None;
            uint32_t m_channelRemainingMs = 0;
            uint32_t m_sinceChannelTickMs = 0;
    };
}
