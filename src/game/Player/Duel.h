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

enum DuelCompleteType
{
    DUEL_INTERRUPTED            = 0,
    DUEL_WON                    = 1,
    DUEL_FLED                   = 2
};

class Duel
{
    public:

        explicit Duel(Player& who) : m_owner(who) {}

        bool Stands() const { return m_against != nullptr; }

        bool Begun() const { return m_startedAt != 0; }

        bool With(Player const* other) const { return m_against == other && m_startedAt != 0; }

        Player* Initiator() const { return m_initiator; }
        Player* Against() const { return m_against; }

        time_t AcceptedAt() const { return m_acceptedAt; }
        time_t StartedAt() const { return m_startedAt; }

        void Offered(Player* initiator, Player* against)
        {
            m_initiator = initiator;
            m_against = against;
            m_acceptedAt = 0;
            m_startedAt = 0;
            m_outOfBoundsSince = 0;
        }

        void Accepted(time_t when) { m_acceptedAt = when; }

        void CountdownRunsOut(time_t now);

        void WatchTheFlag(time_t now);

        void Complete(DuelCompleteType type);

        void TellCountdown(uint32 milliseconds);

    private:

        void Strip(Player& from, ObjectGuid castBy);

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
