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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "Platform/Define.h"

#include <utility>

class Player;

class Weaponry
{
    public:

        explicit Weaponry(Player& who) : m_owner(who) {}

        uint32 WeaponTraining() const { return m_weapons; }
        uint32 ArmourTraining() const { return m_armour; }

        void LearnWeapon(uint32 kinds) { m_weapons |= kinds; }
        void LearnArmour(uint32 kinds) { m_armour |= kinds; }

        bool CanParry() const { return m_canParry; }
        bool CanBlock() const { return m_canBlock; }
        bool CanDualWield() const { return m_canDualWield; }

        void CanParry(bool can);
        void CanBlock(bool can);

        void CanDualWield(bool can) { m_canDualWield = can; }

        std::pair<float, float> Ammo() const { return { m_ammoLeast, m_ammoMost }; }
        void Ammo(float least, float most) { m_ammoLeast = least; m_ammoMost = most; }

        uint8 SwingError() const { return m_swingError; }
        void SwingError(uint8 why) { m_swingError = why; }

        uint32 ChangeTimer() const { return m_changeTimer; }
        void ChangeTimer(uint32 left) { m_changeTimer = left; }

    private:

        Player& m_owner;

        uint32 m_weapons = 0;
        uint32 m_armour = 0;

        bool m_canParry = false;
        bool m_canBlock = false;
        bool m_canDualWield = false;

        uint8 m_swingError = 0;

        float m_ammoLeast = 0.0f;
        float m_ammoMost = 0.0f;

        uint32 m_changeTimer = 0;
};
