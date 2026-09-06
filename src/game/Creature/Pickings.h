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

/**
 * What can still be taken off a unit, and what has been already.
 *
 * There are three ways to take from one thing and they run in an order. Its
 * pockets can be picked while it lives. Its body can be emptied once it is
 * dead. What is left can then be skinned. Each is remembered separately,
 * because taking one does not stand for the others -- a thing whose pockets
 * were picked still has everything else on it.
 *
 * The body and the pockets share one pile between them, which is why picking
 * the pockets is unset again when the body is opened: the pile is being made
 * over for a different taking.
 *
 * Opened is not the same as emptied. It says only that the pile has been shown
 * to whoever was entitled, which is what settles a party's round-robin turn.
 *
 * The damage still owed is the gate on all of it and on the reward besides: a
 * thing killed with no help from a player leaves nothing worth having and gives
 * no experience. It counts down as players hurt it, and nothing is owed once it
 * reaches nothing.
 */
class Pickings
{
    public:

        /// Its pockets have been picked, and the pile now holds what was in them.
        bool PocketsPicked() const { return m_pocketsPicked; }
        void PocketsPicked(bool picked) { m_pocketsPicked = picked; }

        /// Its body has been opened, and the pile now holds what it carried.
        bool BodyTaken() const { return m_bodyTaken; }
        void BodyTaken(bool taken) { m_bodyTaken = taken; }

        /// Its hide has been taken.
        bool Skinned() const { return m_skinned; }
        void Skinned(bool skinned) { m_skinned = skinned; }

        /// The pile has been shown to whoever was entitled at least once.
        bool Opened() const { return m_opened; }
        void Opened(bool opened) { m_opened = opened; }

        /// Whose turn it is, by low guid, or nobody.
        uint32 AssignedTo() const { return m_assignedTo; }
        void AssignedTo(uint32 lowGuid) { m_assignedTo = lowGuid; }

        /// The coin on it.
        uint32 Money() const { return m_money; }
        void Money(uint32 amount) { m_money = amount; }

        /// How much more a player must do to it before any of this counts.
        uint32 DamageOwed() const { return m_damageOwed; }
        void DamageOwed(uint32 amount) { m_damageOwed = amount; }

        /// Players have done enough to it that the kill is theirs.
        bool EnoughPlayerDamage() const { return m_damageOwed == 0; }

    private:

        uint32 m_assignedTo = 0;
        uint32 m_money = 0;
        uint32 m_damageOwed = 0;

        bool m_pocketsPicked = false;
        bool m_bodyTaken = false;
        bool m_skinned = false;
        bool m_opened = false;
};
