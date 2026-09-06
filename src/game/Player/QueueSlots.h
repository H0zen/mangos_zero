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

/**
 * The places a character holds in battleground queues.
 *
 * He may stand in three at once, and the slot he stands in is his handle on that
 * queue: the queue itself calls him back by slot, so a slot keeps its number for
 * as long as he holds it and is not shuffled up when a neighbour is given up.
 *
 * A slot holds two things: which queue it is, and which battleground instance
 * has called him from it. Called and not yet gone in is the state the client
 * draws as an invitation with a clock on it; giving the slot up clears both,
 * because an invitation cannot outlive the queue it came from.
 *
 * Nothing here knows what a battleground is. Whether he may join one, which
 * bracket he falls in, where he came from -- none of that is a slot's business.
 */
class QueueSlots
{
    public:

        /// What SlotOf answers when he holds no slot for that queue.
        static constexpr uint32 NOWHERE = PLAYER_MAX_BATTLEGROUND_QUEUES;

        /// He is waiting in at least one queue.
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

        /// Which queue the given slot is for.
        BattleGroundQueueTypeId Kind(uint32 slot) const { return m_slots[slot].kind; }

        /// Which slot holds the given queue, or NOWHERE.
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

        /// There is room to stand in one more.
        bool AnyFree() const { return SlotOf(BATTLEGROUND_QUEUE_NONE) != NOWHERE; }

        /// Takes a slot for the given queue, or hands back NOWHERE when full.
        /// Standing in the same queue twice gives the slot he already holds.
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

        /// Gives up the slot held for the given queue, with any call in it.
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

        /// A battleground has called him from the slot he holds for that queue.
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

        /// He has been called from the slot he holds for that queue.
        bool Called(BattleGroundQueueTypeId kind) const
        {
            uint32 const slot = SlotOf(kind);
            return slot != NOWHERE && m_slots[slot].calledTo != 0;
        }

        /// He has been called to that particular battleground. Nothing counts as
        /// a call to instance nothing, which is what an uncalled slot holds.
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
