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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "Platform/Define.h"
#include "ObjectGuid.h"

#include <ctime>

class Player;

/// How a duel ended.
enum DuelCompleteType
{
    DUEL_INTERRUPTED            = 0,
    DUEL_WON                    = 1,
    DUEL_FLED                   = 2
};

/**
 * One duellist's side of a duel.
 *
 * Both men keep their own record and the two are kept in step, because almost
 * every question -- am I fighting, whom, since when -- is asked of one man about
 * himself. Each record names who threw down the challenge and who the other man
 * is; for the one who was challenged, both are the same person.
 *
 * A duel has three ages. Offered, when the flag is planted and the box is on the
 * other man's screen and nothing else is true yet. Accepted, when a three-second
 * countdown runs. Begun, from the moment that countdown runs out, which is when
 * blows start to count and when the auras cast from here on can be swept away at
 * the end.
 *
 * The flag is the boundary. Eighty yards from it he is warned; if he has not
 * come back within seventy after ten seconds he has fled, and fleeing is a loss.
 *
 * Ending is always both sides at once, so it is done from the one that is
 * ending, never left to the other man to notice.
 */
class Duel
{
    public:

        explicit Duel(Player& who) : m_owner(who) {}

        /// A challenge stands, whether or not it has begun.
        bool Stands() const { return m_against != nullptr; }

        /// Blows are being struck.
        bool Begun() const { return m_startedAt != 0; }

        /// He is fighting this man, and it has begun.
        bool With(Player const* other) const { return m_against == other && m_startedAt != 0; }

        Player* Initiator() const { return m_initiator; }
        Player* Against() const { return m_against; }

        time_t AcceptedAt() const { return m_acceptedAt; }
        time_t StartedAt() const { return m_startedAt; }

        /// A challenge is thrown down. Nothing is under way yet.
        void Offered(Player* initiator, Player* against)
        {
            m_initiator = initiator;
            m_against = against;
            m_acceptedAt = 0;
            m_startedAt = 0;
            m_outOfBoundsSince = 0;
        }

        /// The challenge was taken up; the countdown starts here.
        void Accepted(time_t when) { m_acceptedAt = when; }

        /// Three seconds after it was accepted, both men are set at each other.
        void CountdownRunsOut(time_t now);

        /// Watches the flag: warned at eighty yards, lost ten seconds past seventy.
        void WatchTheFlag(time_t now);

        /// Ends it for both men.
        void Complete(DuelCompleteType type);

        void TellCountdown(uint32 milliseconds);

    private:

        /// Takes off everything harmful `castBy` laid on `from` since it began.
        void Strip(Player& from, ObjectGuid castBy);

        /// Clears this side alone. Only Complete uses it, and it uses it twice.
        void Forget()
        {
            m_initiator = nullptr;
            m_against = nullptr;
            m_acceptedAt = 0;
            m_startedAt = 0;
            m_outOfBoundsSince = 0;
        }

        Player& m_owner;

        Player* m_initiator = nullptr;
        Player* m_against = nullptr;

        time_t m_acceptedAt = 0;
        time_t m_startedAt = 0;
        time_t m_outOfBoundsSince = 0;
};
