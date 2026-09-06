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

#include "CreatureRecord.h"

#include "Creature.h"

char const* CreatureRecord::Name() const
{
    return m_of ? m_of->Name : "";
}

char const* CreatureRecord::Title() const
{
    return m_of ? m_of->SubName : "";
}

uint32 CreatureRecord::Kind() const
{
    return m_of ? m_of->CreatureType : 0;
}

uint32 CreatureRecord::Family() const
{
    return m_of ? m_of->Family : 0;
}

uint32 CreatureRecord::Rank() const
{
    return m_of ? m_of->Rank : CREATURE_ELITE_NORMAL;
}

uint32 CreatureRecord::PetSpells() const
{
    return m_of ? m_of->PetSpellDataId : 0;
}

uint32 CreatureRecord::Flags() const
{
    return m_of ? m_of->CreatureTypeFlags : 0;
}

uint32 CreatureRecord::ExtraFlags() const
{
    return m_of ? m_of->ExtraFlags : 0;
}

uint32 CreatureRecord::Inhabits() const
{
    return m_of ? m_of->InhabitType : 0;
}

uint32 CreatureRecord::RegeneratesWhat() const
{
    return m_of ? m_of->RegenerateStats : 0;
}

bool CreatureRecord::IsGuard() const
{
    return (ExtraFlags() & CREATURE_FLAG_EXTRA_GUARD) != 0;
}

bool CreatureRecord::IsElite() const
{
    return Rank() != CREATURE_ELITE_NORMAL && Rank() != CREATURE_ELITE_RARE;
}

bool CreatureRecord::IsWorldBoss() const
{
    return Rank() == CREATURE_ELITE_WORLDBOSS;
}

bool CreatureRecord::IsCivilian() const
{
    return m_of && m_of->civilian != 0;
}

bool CreatureRecord::IsRacialLeader() const
{
    return m_of && m_of->RacialLeader;
}

bool CreatureRecord::IsTameable() const
{
    return Kind() == CREATURE_TYPE_BEAST && Family() != 0 &&
           (Flags() & CREATURE_TYPEFLAGS_TAMEABLE) != 0;
}

bool CreatureRecord::IsBoss() const
{
    return (Flags() & CREATURE_TYPEFLAGS_BOSS) != 0;
}

bool CreatureRecord::IsVisibleToGhosts() const
{
    return (Flags() & CREATURE_TYPEFLAGS_GHOST_VISIBLE) != 0;
}

bool CreatureRecord::CanBeAssisted() const
{
    return (Flags() & CREATURE_TYPEFLAGS_CAN_ASSIST) != 0;
}

bool CreatureRecord::IsMoreAudible() const
{
    return (Flags() & CREATURE_TYPEFLAGS_MORE_AUDIBLE) != 0;
}

SkillType CreatureRecord::RequiredLootSkill() const
{
    if (!m_of)
    {
        return SKILL_NONE;
    }

    // Skinning, on everything this core has: the herbalism and mining answers are
    // carried by flags no row in `creature_template` sets, because gathering from
    // a corpse arrives with the expansions.
    if (Flags() & CREATURE_TYPEFLAGS_HERBLOOT)
    {
        return SKILL_HERBALISM;
    }

    if (Flags() & CREATURE_TYPEFLAGS_MININGLOOT)
    {
        return SKILL_MINING;
    }

    return SKILL_SKINNING;
}
