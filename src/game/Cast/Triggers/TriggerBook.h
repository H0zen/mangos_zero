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

#include "Platform/Define.h"

#include <map>
#include <vector>

namespace cast
{

    enum Need : uint32
    {
        NEED_CASTER_IS_PLAYER   = 0x01,
        NEED_TARGET_IS_PLAYER   = 0x02,
        NEED_TARGET_IS_CREATURE = 0x04,
        NEED_CAST_FROM_ITEM     = 0x08,
        NEED_TARGET_USES_MANA   = 0x10,
        NEED_A_TARGET           = 0x20,
    };

    struct Trigger
    {
        uint32 spell = 0;
        uint32 triggerSpell = 0;
        uint32 weight = 1;
        uint8 castBy = 0;
        uint8 castOn = 0;
        uint32 needs = 0;
        uint8 gender = 0;
        uint32 noAura = 0;
        bool carriesItem = false;
    };

    struct Circumstance
    {
        bool casterIsPlayer = false;
        bool hasTarget = false;
        bool targetIsPlayer = false;
        bool targetIsCreature = false;
        bool targetUsesMana = false;
        bool fromItem = false;
        uint8 casterGender = 0;
        uint8 targetGender = 0;
    };

    bool Suits(const Trigger& row, const Circumstance& how);

    uint32 TotalWeight(const std::vector<const Trigger*>& eligible);

    const Trigger* DrawAt(const std::vector<const Trigger*>& eligible, uint32 lot);

    class TriggerBook
    {
        public:

            size_t Load();

            const std::vector<Trigger>* Of(uint32 spell) const;

            size_t Count() const { return m_count; }

        private:

            std::map<uint32, std::vector<Trigger>> m_bySpell;
            size_t m_count = 0;
    };

    TriggerBook& Triggers();
}
