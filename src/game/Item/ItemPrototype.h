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
#include <string>
#include <vector>

enum ItemModType
{
    ITEM_MOD_MANA                     = 0,
    ITEM_MOD_HEALTH                   = 1,
    ITEM_MOD_AGILITY                  = 3,
    ITEM_MOD_STRENGTH                 = 4,
    ITEM_MOD_INTELLECT                = 5,
    ITEM_MOD_SPIRIT                   = 6,
    ITEM_MOD_STAMINA                  = 7,
};

#define MAX_ITEM_MOD                    8

enum ItemSpelltriggerType
{
    ITEM_SPELLTRIGGER_ON_USE          = 0,
    ITEM_SPELLTRIGGER_ON_EQUIP        = 1,
    ITEM_SPELLTRIGGER_CHANCE_ON_HIT   = 2,
    ITEM_SPELLTRIGGER_SOULSTONE       = 4,

    ITEM_SPELLTRIGGER_ON_NO_DELAY_USE = 5,
};

#define MAX_ITEM_SPELLTRIGGER           6

enum ItemBondingType
{
    NO_BIND                                     = 0,
    BIND_WHEN_PICKED_UP                         = 1,
    BIND_WHEN_EQUIPPED                          = 2,
    BIND_WHEN_USE                               = 3,
    BIND_QUEST_ITEM                             = 4,
    BIND_QUEST_ITEM1                            = 5
};

#define MAX_BIND_TYPE                             6

enum ItemPrototypeFlags
{
    ITEM_FLAG_RESERVED_0                      = 0x00000001,
    ITEM_FLAG_CONJURED                        = 0x00000002,
    ITEM_FLAG_LOOTABLE                        = 0x00000004,
    ITEM_FLAG_WRAPPED                         = 0x00000008,
    ITEM_FLAG_DEPRECATED                      = 0x00000010,
    ITEM_FLAG_INDESTRUCTIBLE                  = 0x00000020,
    ITEM_FLAG_USABLE                          = 0x00000040,
    ITEM_FLAG_NO_EQUIP_COOLDOWN               = 0x00000080,
    ITEM_FLAG_RESERVED_1                      = 0x00000100,
    ITEM_FLAG_WRAPPER                         = 0x00000200,
    ITEM_FLAG_STACKABLE                       = 0x00000400,
    ITEM_FLAG_PARTY_LOOT                      = 0x00000800,
    ITEM_FLAG_RESEVERD_2                      = 0x00001000,
    ITEM_FLAG_CHARTER                         = 0x00002000,
    ITEM_FLAG_LETTER                          = 0x00004000,
    ITEM_FLAG_PVP_REWARD                      = 0x00008000,
    ITEM_FLAG_UNK16                           = 0x00010000,
    ITEM_FLAG_UNK17                           = 0x00020000,

    ITEM_FLAG_UNIQUE_EQUIPPED                 = 0x00080000,
};

enum BagFamily
{
    BAG_FAMILY_NONE                             = 0,
    BAG_FAMILY_ARROWS                           = 1,
    BAG_FAMILY_BULLETS                          = 2,
    BAG_FAMILY_SOUL_SHARDS                      = 3,
    BAG_FAMILY_UNKNOWN1                         = 4,
    BAG_FAMILY_UNKNOWN2                         = 5,
    BAG_FAMILY_HERBS                            = 6,
    BAG_FAMILY_ENCHANTING_SUPP                  = 7,
    BAG_FAMILY_ENGINEERING_SUPP                 = 8,
    BAG_FAMILY_KEYS                             = 9,
};

enum InventoryType
{
    INVTYPE_NON_EQUIP                           = 0,
    INVTYPE_HEAD                                = 1,
    INVTYPE_NECK                                = 2,
    INVTYPE_SHOULDERS                           = 3,
    INVTYPE_BODY                                = 4,
    INVTYPE_CHEST                               = 5,
    INVTYPE_WAIST                               = 6,
    INVTYPE_LEGS                                = 7,
    INVTYPE_FEET                                = 8,
    INVTYPE_WRISTS                              = 9,
    INVTYPE_HANDS                               = 10,
    INVTYPE_FINGER                              = 11,
    INVTYPE_TRINKET                             = 12,
    INVTYPE_WEAPON                              = 13,
    INVTYPE_SHIELD                              = 14,
    INVTYPE_RANGED                              = 15,
    INVTYPE_CLOAK                               = 16,
    INVTYPE_2HWEAPON                            = 17,
    INVTYPE_BAG                                 = 18,
    INVTYPE_TABARD                              = 19,
    INVTYPE_ROBE                                = 20,
    INVTYPE_WEAPONMAINHAND                      = 21,
    INVTYPE_WEAPONOFFHAND                       = 22,
    INVTYPE_HOLDABLE                            = 23,
    INVTYPE_AMMO                                = 24,
    INVTYPE_THROWN                              = 25,
    INVTYPE_RANGEDRIGHT                         = 26,
    INVTYPE_QUIVER                              = 27,
    INVTYPE_RELIC                               = 28
};

#define MAX_INVTYPE                               29

enum ItemClass
{
    ITEM_CLASS_CONSUMABLE                       = 0,
    ITEM_CLASS_CONTAINER                        = 1,
    ITEM_CLASS_WEAPON                           = 2,
    ITEM_CLASS_RESERVED_1                       = 3,
    ITEM_CLASS_ARMOR                            = 4,
    ITEM_CLASS_REAGENT                          = 5,
    ITEM_CLASS_PROJECTILE                       = 6,
    ITEM_CLASS_TRADE_GOODS                      = 7,
    ITEM_CLASS_RESERVED_2                       = 8,
    ITEM_CLASS_RECIPE                           = 9,
    ITEM_CLASS_RESERVED_3                       = 10,
    ITEM_CLASS_QUIVER                           = 11,
    ITEM_CLASS_QUEST                            = 12,
    ITEM_CLASS_KEY                              = 13,
    ITEM_CLASS_RESERVED_4                       = 14,
    ITEM_CLASS_MISC                             = 15
};

#define MAX_ITEM_CLASS                            16

enum ItemSubclassConsumable
{
    ITEM_SUBCLASS_CONSUMABLE                    = 0,
    ITEM_SUBCLASS_POTION                        = 1,
    ITEM_SUBCLASS_ELIXIR                        = 2,
    ITEM_SUBCLASS_FLASK                         = 3,
    ITEM_SUBCLASS_SCROLL                        = 4,
    ITEM_SUBCLASS_FOOD                          = 5,
    ITEM_SUBCLASS_ITEM_ENHANCEMENT              = 6,
    ITEM_SUBCLASS_BANDAGE                       = 7,
    ITEM_SUBCLASS_CONSUMABLE_OTHER              = 8
};

#define MAX_ITEM_SUBCLASS_CONSUMABLE              9

enum ItemSubclassContainer
{
    ITEM_SUBCLASS_CONTAINER                     = 0,
    ITEM_SUBCLASS_SOUL_CONTAINER                = 1,
    ITEM_SUBCLASS_HERB_CONTAINER                = 2,
    ITEM_SUBCLASS_ENCHANTING_CONTAINER          = 3,
    ITEM_SUBCLASS_ENGINEERING_CONTAINER         = 4
};

#define MAX_ITEM_SUBCLASS_CONTAINER               5

enum ItemSubclassWeapon
{
    ITEM_SUBCLASS_WEAPON_AXE                    = 0,
    ITEM_SUBCLASS_WEAPON_AXE2                   = 1,
    ITEM_SUBCLASS_WEAPON_BOW                    = 2,
    ITEM_SUBCLASS_WEAPON_GUN                    = 3,
    ITEM_SUBCLASS_WEAPON_MACE                   = 4,
    ITEM_SUBCLASS_WEAPON_MACE2                  = 5,
    ITEM_SUBCLASS_WEAPON_POLEARM                = 6,
    ITEM_SUBCLASS_WEAPON_SWORD                  = 7,
    ITEM_SUBCLASS_WEAPON_SWORD2                 = 8,
    ITEM_SUBCLASS_WEAPON_RESERVED_1             = 9,
    ITEM_SUBCLASS_WEAPON_STAFF                  = 10,
    ITEM_SUBCLASS_WEAPON_RESERVED_2             = 11,
    ITEM_SUBCLASS_WEAPON_RESERVED_3             = 12,
    ITEM_SUBCLASS_WEAPON_FIST                   = 13,
    ITEM_SUBCLASS_WEAPON_MISC                   = 14,
    ITEM_SUBCLASS_WEAPON_DAGGER                 = 15,
    ITEM_SUBCLASS_WEAPON_THROWN                 = 16,
    ITEM_SUBCLASS_WEAPON_SPEAR                  = 17,
    ITEM_SUBCLASS_WEAPON_CROSSBOW               = 18,
    ITEM_SUBCLASS_WEAPON_WAND                   = 19,
    ITEM_SUBCLASS_WEAPON_FISHING_POLE           = 20
};

#define MAX_ITEM_SUBCLASS_WEAPON                  21

#define MAX_ITEM_SUBCLASS_RESERVED_1              1

enum ItemSubclassArmor
{
    ITEM_SUBCLASS_ARMOR_MISC                    = 0,
    ITEM_SUBCLASS_ARMOR_CLOTH                   = 1,
    ITEM_SUBCLASS_ARMOR_LEATHER                 = 2,
    ITEM_SUBCLASS_ARMOR_MAIL                    = 3,
    ITEM_SUBCLASS_ARMOR_PLATE                   = 4,
    ITEM_SUBCLASS_ARMOR_BUCKLER                 = 5,
    ITEM_SUBCLASS_ARMOR_SHIELD                  = 6,
    ITEM_SUBCLASS_ARMOR_LIBRAM                  = 7,
    ITEM_SUBCLASS_ARMOR_IDOL                    = 8,
    ITEM_SUBCLASS_ARMOR_TOTEM                   = 9
};

#define MAX_ITEM_SUBCLASS_ARMOR                   10

enum ItemSubclassReagent
{
    ITEM_SUBCLASS_REAGENT                       = 0
};

#define MAX_ITEM_SUBCLASS_REAGENT                 1

enum ItemSubclassProjectile
{
    ITEM_SUBCLASS_PROJ_RESERVED_1               = 0,
    ITEM_SUBCLASS_PROJ_RESERVED_2               = 1,
    ITEM_SUBCLASS_ARROW                         = 2,
    ITEM_SUBCLASS_BULLET                        = 3,
    ITEM_SUBCLASS_PROJ_RESERVED_3               = 4
};

#define MAX_ITEM_SUBCLASS_PROJECTILE              5

enum ItemSubclassTradeGoods
{
    ITEM_SUBCLASS_TRADE_GOODS                   = 0,
    ITEM_SUBCLASS_PARTS                         = 1,
    ITEM_SUBCLASS_EXPLOSIVES                    = 2,
    ITEM_SUBCLASS_DEVICES                       = 3
};

#define MAX_ITEM_SUBCLASS_TRADE_GOODS             4

#define MAX_ITEM_CLASS_RESERVED_2                  1

enum ItemSubclassRecipe
{
    ITEM_SUBCLASS_BOOK                          = 0,
    ITEM_SUBCLASS_LEATHERWORKING_PATTERN        = 1,
    ITEM_SUBCLASS_TAILORING_PATTERN             = 2,
    ITEM_SUBCLASS_ENGINEERING_SCHEMATIC         = 3,
    ITEM_SUBCLASS_BLACKSMITHING                 = 4,
    ITEM_SUBCLASS_COOKING_RECIPE                = 5,
    ITEM_SUBCLASS_ALCHEMY_RECIPE                = 6,
    ITEM_SUBCLASS_FIRST_AID_MANUAL              = 7,
    ITEM_SUBCLASS_ENCHANTING_FORMULA            = 8,
    ITEM_SUBCLASS_FISHING_MANUAL                = 9
};

#define MAX_ITEM_SUBCLASS_RECIPE                  11

#define MAX_ITEM_CLASS_RESERVED_3                 1

enum ItemSubclassQuiver
{
    ITEM_SUBCLASS_QUIV_RESERVED_1               = 0,
    ITEM_SUBCLASS_QUIV_RESERVED_2               = 1,
    ITEM_SUBCLASS_QUIVER                        = 2,
    ITEM_SUBCLASS_AMMO_POUCH                    = 3
};

#define MAX_ITEM_SUBCLASS_QUIVER                  4

enum ItemSubclassQuest
{
    ITEM_SUBCLASS_QUEST                         = 0
};

#define MAX_ITEM_SUBCLASS_QUEST                   1

enum ItemSubclassKey
{
    ITEM_SUBCLASS_KEY                           = 0,
    ITEM_SUBCLASS_LOCKPICK                      = 1
};

#define MAX_ITEM_SUBCLASS_KEY                     2

enum ItemSubclassPermanent
{
    ITEM_SUBCLASS_PERMANENT                     = 0
};

#define MAX_ITEM_SUBCLASS_PERMANENT               1

enum ItemSubclassJunk
{
    ITEM_SUBCLASS_JUNK                          = 0
};

#define MAX_ITEM_SUBCLASS_JUNK                    1

const uint32 MaxItemSubclassValues[MAX_ITEM_CLASS] =
{
    MAX_ITEM_SUBCLASS_CONSUMABLE,
    MAX_ITEM_SUBCLASS_CONTAINER,
    MAX_ITEM_SUBCLASS_WEAPON,
    MAX_ITEM_SUBCLASS_RESERVED_1,
    MAX_ITEM_SUBCLASS_ARMOR,
    MAX_ITEM_SUBCLASS_REAGENT,
    MAX_ITEM_SUBCLASS_PROJECTILE,
    MAX_ITEM_SUBCLASS_TRADE_GOODS,
    MAX_ITEM_CLASS_RESERVED_2,
    MAX_ITEM_SUBCLASS_RECIPE,
    MAX_ITEM_CLASS_RESERVED_3,
    MAX_ITEM_SUBCLASS_QUIVER,
    MAX_ITEM_SUBCLASS_QUEST,
    MAX_ITEM_SUBCLASS_KEY,
    MAX_ITEM_SUBCLASS_PERMANENT,
    MAX_ITEM_SUBCLASS_JUNK
};

inline uint8 ItemSubClassToDurabilityMultiplierId(uint32 ItemClass, uint32 ItemSubClass)
{
    switch (ItemClass)
    {
        case ITEM_CLASS_WEAPON: return ItemSubClass;
        case ITEM_CLASS_ARMOR:  return ItemSubClass + 21;
    }
    return 0;
}

enum ItemExtraFlags
{
    ITEM_EXTRA_NON_CONSUMABLE     = 0x01,
    ITEM_EXTRA_REAL_TIME_DURATION = 0x02,

    ITEM_EXTRA_ALL                = 0x03
};

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct _Damage
{
    float   DamageMin;
    float   DamageMax;
    uint32  DamageType;
};

struct _ItemStat
{
    uint32  ItemStatType;
    int32   ItemStatValue;
};
struct _Spell
{
    uint32 SpellId;
    uint32 SpellTrigger;
    int32  SpellCharges;
    float  SpellPPMRate;
    int32  SpellCooldown;
    uint32 SpellCategory;
    int32  SpellCategoryCooldown;
};

struct _Socket
{
    uint32 Color;
    uint32 Content;
};

#define MAX_ITEM_PROTO_DAMAGES 5
#define MAX_ITEM_PROTO_SPELLS  5
#define MAX_ITEM_PROTO_STATS  10

struct ItemPrototype
{
    uint32 ItemId;
    uint32 Class;
    uint32 SubClass;
    char*  Name1;
    uint32 DisplayInfoID;
    uint32 Quality;
    uint32 Flags;
    uint32 BuyCount;
    uint32 BuyPrice;
    uint32 SellPrice;
    uint32 InventoryType;
    uint32 AllowableClass;
    uint32 AllowableRace;
    uint32 ItemLevel;
    uint32 RequiredLevel;
    uint32 RequiredSkill;
    uint32 RequiredSkillRank;
    uint32 RequiredSpell;
    uint32 RequiredHonorRank;
    uint32 RequiredCityRank;
    uint32 RequiredReputationFaction;
    uint32 RequiredReputationRank;
    uint32 MaxCount;
    uint32 Stackable;
    uint32 ContainerSlots;
    _ItemStat ItemStat[MAX_ITEM_PROTO_STATS];
    _Damage Damage[MAX_ITEM_PROTO_DAMAGES];
    uint32 Armor;
    uint32 HolyRes;
    uint32 FireRes;
    uint32 NatureRes;
    uint32 FrostRes;
    uint32 ShadowRes;
    uint32 ArcaneRes;
    uint32 Delay;
    uint32 AmmoType;
    float  RangedModRange;
    _Spell Spells[MAX_ITEM_PROTO_SPELLS];
    uint32 Bonding;
    char*  Description;
    uint32 PageText;
    uint32 LanguageID;
    uint32 PageMaterial;
    uint32 StartQuest;
    uint32 LockID;
    uint32 Material;
    uint32 Sheath;
    uint32 RandomProperty;
    uint32 Block;
    uint32 ItemSet;
    uint32 MaxDurability;
    uint32 Area;
    uint32 Map;
    uint32 BagFamily;
    uint32 DisenchantID;
    uint32 FoodType;
    uint32 MinMoneyLoot;
    uint32 MaxMoneyLoot;
    uint32 Duration;
    uint32 ExtraFlags;

    bool CanChangeEquipStateInCombat() const
    {
        switch (InventoryType)
        {
            case INVTYPE_RELIC:
            case INVTYPE_SHIELD:
            case INVTYPE_HOLDABLE:
                return true;
        }

        switch (Class)
        {
            case ITEM_CLASS_WEAPON:
            case ITEM_CLASS_PROJECTILE:
                return true;
        }

        return false;
    }

    uint32 GetMaxStackSize() const
    {
        return Stackable;
    }

    bool IsPotion() const
    {
        return Class == ITEM_CLASS_CONSUMABLE && SubClass == ITEM_SUBCLASS_POTION;
    }

    bool IsConjuredConsumable() const
    {
        return Class == ITEM_CLASS_CONSUMABLE && (Flags & ITEM_FLAG_CONJURED);
    }

    bool IsWeaponVellum() const
    {
        return Class == ITEM_CLASS_TRADE_GOODS;
    }
    bool IsArmorVellum() const
    {
        return Class == ITEM_CLASS_TRADE_GOODS;
    }
};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

struct ItemLocale
{
    std::vector<std::string> Name;
    std::vector<std::string> Description;
};
