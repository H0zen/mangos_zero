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

#define DEFAULT_MAX_LEVEL 60

#define MAX_LEVEL    100

#define STRONG_MAX_LEVEL 255

enum AreaTeams
{
    AREATEAM_NONE  = 0,
    AREATEAM_ALLY  = 2,
    AREATEAM_HORDE = 4
};

enum AreaFlags
{
    AREA_FLAG_SNOW                  = 0x00000001,
    AREA_FLAG_UNK1                  = 0x00000002,
    AREA_FLAG_UNK2                  = 0x00000004,
    AREA_FLAG_SLAVE_CAPITAL         = 0x00000008,
    AREA_FLAG_UNK3                  = 0x00000010,
    AREA_FLAG_SLAVE_CAPITAL2        = 0x00000020,
    AREA_FLAG_DUEL                  = 0x00000040,
    AREA_FLAG_ARENA                 = 0x00000080,
    AREA_FLAG_CAPITAL               = 0x00000100,
    AREA_FLAG_CITY                  = 0x00000200,
};

enum FactionTemplateFlags
{
    FACTION_TEMPLATE_FLAG_PVP               = 0x00000800,
    FACTION_TEMPLATE_FLAG_CONTESTED_GUARD   = 0x00001000,
};

enum FactionMasks
{
    FACTION_MASK_PLAYER   = 1,
    FACTION_MASK_ALLIANCE = 2,
    FACTION_MASK_HORDE    = 4,
    FACTION_MASK_MONSTER  = 8

};

enum MapTypes
{
    MAP_COMMON          = 0,
    MAP_INSTANCE        = 1,
    MAP_RAID            = 2,
    MAP_BATTLEGROUND    = 3,
};

enum AbilytyLearnType
{
    ABILITY_LEARNED_ON_GET_PROFESSION_SKILL     = 1,
    ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL  = 2
};

enum AbilitySkillFlags
{
    ABILITY_SKILL_NONTRAINABLE = 0x100
};

enum ItemEnchantmentType
{
    ITEM_ENCHANTMENT_TYPE_NONE             = 0,
    ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL     = 1,
    ITEM_ENCHANTMENT_TYPE_DAMAGE           = 2,
    ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL      = 3,
    ITEM_ENCHANTMENT_TYPE_RESISTANCE       = 4,
    ITEM_ENCHANTMENT_TYPE_STAT             = 5,
    ITEM_ENCHANTMENT_TYPE_TOTEM            = 6
};

enum SpellCastTargetFlags
{
    TARGET_FLAG_SELF            = 0x00000000,
    TARGET_FLAG_UNUSED1         = 0x00000001,
    TARGET_FLAG_UNIT            = 0x00000002,
    TARGET_FLAG_UNUSED2         = 0x00000004,
    TARGET_FLAG_UNUSED3         = 0x00000008,
    TARGET_FLAG_ITEM            = 0x00000010,
    TARGET_FLAG_SOURCE_LOCATION = 0x00000020,
    TARGET_FLAG_DEST_LOCATION   = 0x00000040,
    TARGET_FLAG_OBJECT_UNK      = 0x00000080,
    TARGET_FLAG_UNIT_UNK        = 0x00000100,
    TARGET_FLAG_PVP_CORPSE      = 0x00000200,
    TARGET_FLAG_UNIT_CORPSE     = 0x00000400,
    TARGET_FLAG_OBJECT          = 0x00000800,
    TARGET_FLAG_TRADE_ITEM      = 0x00001000,
    TARGET_FLAG_STRING          = 0x00002000,
    TARGET_FLAG_GAMEOBJECT_ITEM = 0x00004000,
    TARGET_FLAG_CORPSE          = 0x00008000,
    TARGET_FLAG_UNK2            = 0x00010000,
};

enum SpellEffectIndex
{
    EFFECT_INDEX_0     = 0,
    EFFECT_INDEX_1     = 1,
    EFFECT_INDEX_2     = 2
};

#define MAX_EFFECT_INDEX 3

enum SpellFamily
{
    SPELLFAMILY_GENERIC     = 0,
    SPELLFAMILY_ENVIRONMENT = 1,

    SPELLFAMILY_MAGE        = 3,
    SPELLFAMILY_WARRIOR     = 4,
    SPELLFAMILY_WARLOCK     = 5,
    SPELLFAMILY_PRIEST      = 6,
    SPELLFAMILY_DRUID       = 7,
    SPELLFAMILY_ROGUE       = 8,
    SPELLFAMILY_HUNTER      = 9,
    SPELLFAMILY_PALADIN     = 10,
    SPELLFAMILY_SHAMAN      = 11,

    SPELLFAMILY_POTION      = 13,
};
