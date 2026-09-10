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
#include "SharedDefines.h"

struct CreatureInfo;

enum CreatureSubtype
{
    CREATURE_SUBTYPE_GENERIC,
    CREATURE_SUBTYPE_PET,
    CREATURE_SUBTYPE_TOTEM,
    CREATURE_SUBTYPE_TEMPORARY_SUMMON,
};

class CreatureRecord
{
    public:

        CreatureRecord() = default;

        explicit CreatureRecord(CreatureInfo const& of) : m_of(&of) {}

        explicit operator bool() const { return m_of != nullptr; }

        char const* Name() const;

        char const* Title() const;

        uint32 Kind() const;

        uint32 Family() const;

        uint32 Rank() const;

        uint32 PetSpells() const;

        uint32 Flags() const;

        uint32 ExtraFlags() const;

        uint32 Inhabits() const;

        uint32 RegeneratesWhat() const;

        bool IsCivilian() const;

        bool IsRacialLeader() const;

        bool IsTameable() const;

        bool IsBoss() const;

        bool IsGuard() const;

        bool IsElite() const;

        bool IsWorldBoss() const;

        bool IsVisibleToGhosts() const;

        bool CanBeAssisted() const;

        bool IsMoreAudible() const;

        SkillType RequiredLootSkill() const;

    private:
        CreatureInfo const* m_of = nullptr;
};
