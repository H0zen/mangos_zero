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

#include <vector>

/**
 * The hundred and eighteen places a character keeps an item, and the way one is
 * named.
 *
 * A place is named by a pair: which container, and which slot inside it. The
 * container 255 means the character himself, and then the slot number walks the
 * regions below in one run -- worn gear, the bags he wears, the backpack, the
 * bank, the bank's bag slots, the vendor's buyback row, the keyring. Any other
 * container number is one of the bags he wears, and the slot is a place inside
 * that bag.
 *
 * The two halves are also carried as one sixteen-bit number, container in the
 * high byte and slot in the low one, wherever a single value is wanted.
 *
 * The regions run end to end with no gaps, which is why each one starts at the
 * previous one's end: a number tells you which region it is in only because the
 * bounds are kept in this order.
 */

/**
 * What a caller says when it has no particular place in mind: any container, or
 * any slot inside the one it named. A store asked this way picks the place
 * itself; a store that must land exactly where it was told refuses it.
 */
enum InventorySlot
{
    NULL_BAG                    = 0,
    NULL_SLOT                   = 255
};

// Player slots for items
enum PlayerSlots
{
    /// The lowest place number.
    PLAYER_SLOT_START           = 0,
    /// One past the highest.
    PLAYER_SLOT_END             = 118,
    PLAYER_SLOTS_COUNT          = (PLAYER_SLOT_END - PLAYER_SLOT_START)
};

#define INVENTORY_SLOT_BAG_0    255

// Equipment slots (19 slots)
enum EquipmentSlots
{
    EQUIPMENT_SLOT_START        = 0,
    EQUIPMENT_SLOT_HEAD         = 0,  // Head slot
    EQUIPMENT_SLOT_NECK         = 1,  // Neck slot
    EQUIPMENT_SLOT_SHOULDERS    = 2,  // Shoulders slot
    EQUIPMENT_SLOT_BODY         = 3,  // Body slot
    EQUIPMENT_SLOT_CHEST        = 4,  // Chest slot
    EQUIPMENT_SLOT_WAIST        = 5,  // Waist slot
    EQUIPMENT_SLOT_LEGS         = 6,  // Legs slot
    EQUIPMENT_SLOT_FEET         = 7,  // Feet slot
    EQUIPMENT_SLOT_WRISTS       = 8,  // Wrists slot
    EQUIPMENT_SLOT_HANDS        = 9,  // Hands slot
    EQUIPMENT_SLOT_FINGER1      = 10, // First finger slot
    EQUIPMENT_SLOT_FINGER2      = 11, // Second finger slot
    EQUIPMENT_SLOT_TRINKET1     = 12, // First trinket slot
    EQUIPMENT_SLOT_TRINKET2     = 13, // Second trinket slot
    EQUIPMENT_SLOT_BACK         = 14, // Back slot
    EQUIPMENT_SLOT_MAINHAND     = 15, // Main hand slot
    EQUIPMENT_SLOT_OFFHAND      = 16, // Off hand slot
    EQUIPMENT_SLOT_RANGED       = 17, // Ranged slot
    EQUIPMENT_SLOT_TABARD       = 18, // Tabard slot
    EQUIPMENT_SLOT_END          = 19  // End of equipment slots
};

// Inventory slots (4 slots)
enum InventorySlots
{
    INVENTORY_SLOT_BAG_START    = 19, // Start of bag slots
    INVENTORY_SLOT_BAG_END      = 23  // End of bag slots
};

// Inventory pack slots (16 slots)
enum InventoryPackSlots
{
    INVENTORY_SLOT_ITEM_START   = 23, // Start of item slots
    INVENTORY_SLOT_ITEM_END     = 39  // End of item slots
};

// Bank item slots (28 slots)
enum BankItemSlots
{
    BANK_SLOT_ITEM_START        = 39,
    BANK_SLOT_ITEM_END          = 63
};

// Bank bag slots (7 slots)
enum BankBagSlots
{
    BANK_SLOT_BAG_START         = 63,
    BANK_SLOT_BAG_END           = 69
};

// Buy back slots (12 slots)
enum BuyBackSlots
{
    // stored in m_buybackitems
    BUYBACK_SLOT_START          = 69,
    BUYBACK_SLOT_END            = 81
};

// Key ring slots (32 slots)
enum KeyRingSlots
{
    KEYRING_SLOT_START          = 81,
    KEYRING_SLOT_END            = 97
};

// Structure to hold item position and count
struct ItemPosCount
{
    ItemPosCount(uint16 _pos, uint8 _count) : pos(_pos), count(_count) {}
    bool isContainedIn(std::vector<ItemPosCount> const& vec) const;

    uint16 pos;  // Position of the item
    uint8 count; // Count of the item
};

typedef std::vector<ItemPosCount> ItemPosCountVec;
