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

class Pickings
{
    public:

        bool PocketsPicked() const { return m_pocketsPicked; }
        void PocketsPicked(bool picked) { m_pocketsPicked = picked; }

        bool BodyTaken() const { return m_bodyTaken; }
        void BodyTaken(bool taken) { m_bodyTaken = taken; }

        bool Skinned() const { return m_skinned; }
        void Skinned(bool skinned) { m_skinned = skinned; }

        bool Opened() const { return m_opened; }
        void Opened(bool opened) { m_opened = opened; }

        uint32 AssignedTo() const { return m_assignedTo; }
        void AssignedTo(uint32 lowGuid) { m_assignedTo = lowGuid; }

        uint32 Money() const { return m_money; }
        void Money(uint32 amount) { m_money = amount; }

        uint32 DamageOwed() const { return m_damageOwed; }
        void DamageOwed(uint32 amount) { m_damageOwed = amount; }

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
