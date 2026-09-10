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
#include "BattleGround.h"
#include "SharedDefines.h"

class QueueSlots
{
    public:

        static constexpr uint32 NOWHERE = PLAYER_MAX_BATTLEGROUND_QUEUES;

        bool AnyHeld() const
        {
            for (auto const& slot : m_slots)
            {
                if (slot.kind != BATTLEGROUND_QUEUE_NONE)
                {
                    return true;
                }
            }

            return false;
        }

        BattleGroundQueueTypeId Kind(uint32 slot) const { return m_slots[slot].kind; }

        uint32 SlotOf(BattleGroundQueueTypeId kind) const
        {
            for (uint32 slot = 0; slot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++slot)
            {
                if (m_slots[slot].kind == kind)
                {
                    return slot;
                }
            }

            return NOWHERE;
        }

        bool Holds(BattleGroundQueueTypeId kind) const { return SlotOf(kind) != NOWHERE; }

        bool AnyFree() const { return SlotOf(BATTLEGROUND_QUEUE_NONE) != NOWHERE; }

        uint32 Take(BattleGroundQueueTypeId kind)
        {
            for (uint32 slot = 0; slot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++slot)
            {
                if (m_slots[slot].kind == BATTLEGROUND_QUEUE_NONE || m_slots[slot].kind == kind)
                {
                    m_slots[slot].kind = kind;
                    m_slots[slot].calledTo = 0;
                    return slot;
                }
            }

            return NOWHERE;
        }

        void Give(BattleGroundQueueTypeId kind)
        {
            uint32 const slot = SlotOf(kind);
            if (slot == NOWHERE)
            {
                return;
            }

            m_slots[slot].kind = BATTLEGROUND_QUEUE_NONE;
            m_slots[slot].calledTo = 0;
        }

        void CalledTo(BattleGroundQueueTypeId kind, uint32 instanceId)
        {
            for (auto& slot : m_slots)
            {
                if (slot.kind == kind)
                {
                    slot.calledTo = instanceId;
                }
            }
        }

        bool Called(BattleGroundQueueTypeId kind) const
        {
            uint32 const slot = SlotOf(kind);
            return slot != NOWHERE && m_slots[slot].calledTo != 0;
        }

        bool CalledToInstance(uint32 instanceId) const
        {
            if (!instanceId)
            {
                return false;
            }

            for (auto const& slot : m_slots)
            {
                if (slot.calledTo == instanceId)
                {
                    return true;
                }
            }

            return false;
        }

    private:

        struct Slot
        {
            BattleGroundQueueTypeId kind = BATTLEGROUND_QUEUE_NONE;
            uint32 calledTo = 0;
        };

        Slot m_slots[PLAYER_MAX_BATTLEGROUND_QUEUES];
};
