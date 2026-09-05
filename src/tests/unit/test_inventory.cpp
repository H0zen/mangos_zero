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

// Where a place is.
//
// These answers decide what a move is allowed to do, and they are questions
// about a number rather than about a character, so they are fixed here.

#include "doctest.h"

#include "Inventory/Inventory.h"
#include "Unit.h"
#include "UpdateFields.h"
#include "Item.h"
#include "Bag.h"

TEST_CASE("place: a number belongs to the character himself only under bag 255")
{
    CHECK(Inventory::IsHisOwn(INVENTORY_SLOT_BAG_0));
    CHECK_FALSE(Inventory::IsHisOwn(INVENTORY_SLOT_BAG_START));
    CHECK_FALSE(Inventory::IsHisOwn(0));
}

TEST_CASE("place: the two halves survive being packed into one number")
{
    uint16 const place = Inventory::Packed(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

    CHECK(Inventory::Container(place) == INVENTORY_SLOT_BAG_0);
    CHECK(Inventory::Slot(place) == EQUIPMENT_SLOT_MAINHAND);

    // And a place inside a worn bag, where the container is not the character.
    uint16 const inBag = Inventory::Packed(INVENTORY_SLOT_BAG_START, 7);
    CHECK(Inventory::Container(inBag) == INVENTORY_SLOT_BAG_START);
    CHECK(Inventory::Slot(inBag) == 7);
}

TEST_CASE("place: worn means the gear places and the slots a bag hangs in")
{
    CHECK(Inventory::IsWorn(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_HEAD));
    CHECK(Inventory::IsWorn(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_TABARD));
    CHECK(Inventory::IsWorn(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_BAG_START));

    // The backpack is carried, not worn.
    CHECK_FALSE(Inventory::IsWorn(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START));

    // And nothing inside a bag is worn, whatever the slot number is.
    CHECK_FALSE(Inventory::IsWorn(INVENTORY_SLOT_BAG_START, EQUIPMENT_SLOT_HEAD));
}

TEST_CASE("place: carried means the backpack, the keyring, and inside a worn bag")
{
    CHECK(Inventory::IsCarried(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START));
    CHECK(Inventory::IsCarried(INVENTORY_SLOT_BAG_0, KEYRING_SLOT_START));
    CHECK(Inventory::IsCarried(INVENTORY_SLOT_BAG_START, 0));

    // Worn gear is not carried, and neither is the bank.
    CHECK_FALSE(Inventory::IsCarried(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_HEAD));
    CHECK_FALSE(Inventory::IsCarried(INVENTORY_SLOT_BAG_0, BANK_SLOT_ITEM_START));
}

TEST_CASE("place: asking for no slot in particular counts as carried")
{
    // A store that has not chosen a place yet still names the backpack.
    CHECK(Inventory::IsCarried(INVENTORY_SLOT_BAG_0, NULL_SLOT));
}

TEST_CASE("place: banked means the bank's own places, its bag slots, and inside them")
{
    CHECK(Inventory::IsBanked(INVENTORY_SLOT_BAG_0, BANK_SLOT_ITEM_START));
    CHECK(Inventory::IsBanked(INVENTORY_SLOT_BAG_0, BANK_SLOT_BAG_START));
    CHECK(Inventory::IsBanked(BANK_SLOT_BAG_START, 0));

    CHECK_FALSE(Inventory::IsBanked(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START));
    CHECK_FALSE(Inventory::IsBanked(INVENTORY_SLOT_BAG_START, 0));
}

TEST_CASE("place: the three regions never overlap")
{
    for (uint8 slot = 0; slot < KEYRING_SLOT_END; ++slot)
    {
        int const claims = int(Inventory::IsWorn(INVENTORY_SLOT_BAG_0, slot))
                         + int(Inventory::IsCarried(INVENTORY_SLOT_BAG_0, slot))
                         + int(Inventory::IsBanked(INVENTORY_SLOT_BAG_0, slot));

        CHECK_MESSAGE(claims <= 1, "slot ", int(slot), " is claimed by two regions");
    }
}

TEST_CASE("place: only the buyback row is claimed by no region at all")
{
    for (uint8 slot = 0; slot < KEYRING_SLOT_END; ++slot)
    {
        bool const claimed = Inventory::IsWorn(INVENTORY_SLOT_BAG_0, slot)
                          || Inventory::IsCarried(INVENTORY_SLOT_BAG_0, slot)
                          || Inventory::IsBanked(INVENTORY_SLOT_BAG_0, slot);

        bool const isBuyback = slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END;
        CHECK_MESSAGE(claimed != isBuyback, "slot ", int(slot), " is on the wrong side");
    }
}

TEST_CASE("place: a bag hangs in four places on him and six in the bank")
{
    int worn = 0;
    int banked = 0;
    for (uint8 slot = 0; slot < KEYRING_SLOT_END; ++slot)
    {
        if (!Inventory::HoldsBag(Inventory::Packed(INVENTORY_SLOT_BAG_0, slot)))
        {
            continue;
        }

        if (slot < BANK_SLOT_BAG_START)
        {
            ++worn;
        }
        else
        {
            ++banked;
        }
    }

    CHECK(worn == INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START);
    CHECK(banked == BANK_SLOT_BAG_END - BANK_SLOT_BAG_START);

    // A place inside a bag never holds a bag of its own.
    CHECK_FALSE(Inventory::HoldsBag(Inventory::Packed(INVENTORY_SLOT_BAG_START, 0)));
}

TEST_CASE("place: three worn places feed a swing and the rest feed none")
{
    CHECK(Inventory::AttackFrom(EQUIPMENT_SLOT_MAINHAND) == BASE_ATTACK);
    CHECK(Inventory::AttackFrom(EQUIPMENT_SLOT_OFFHAND) == OFF_ATTACK);
    CHECK(Inventory::AttackFrom(EQUIPMENT_SLOT_RANGED) == RANGED_ATTACK);

    CHECK(Inventory::AttackFrom(EQUIPMENT_SLOT_HEAD) == MAX_ATTACK);
    CHECK(Inventory::AttackFrom(EQUIPMENT_SLOT_TABARD) == MAX_ATTACK);
    CHECK(Inventory::AttackFrom(INVENTORY_SLOT_ITEM_START) == MAX_ATTACK);
}

TEST_CASE("place: the regions run end to end with no gap between them")
{
    // Each region is an enum of its own, so a place number is compared as the
    // number it is rather than as a member of either.
    CHECK(uint32(EQUIPMENT_SLOT_END) == uint32(INVENTORY_SLOT_BAG_START));
    CHECK(uint32(INVENTORY_SLOT_BAG_END) == uint32(INVENTORY_SLOT_ITEM_START));
    CHECK(uint32(INVENTORY_SLOT_ITEM_END) == uint32(BANK_SLOT_ITEM_START));
    CHECK(uint32(BANK_SLOT_ITEM_END) == uint32(BANK_SLOT_BAG_START));
    CHECK(uint32(BANK_SLOT_BAG_END) == uint32(BUYBACK_SLOT_START));
    CHECK(uint32(BUYBACK_SLOT_END) == uint32(KEYRING_SLOT_START));
}

TEST_CASE("place: the places stop where the last region does")
{
    CHECK(uint32(PLAYER_SLOT_END) == uint32(KEYRING_SLOT_END));
}

// The client keeps one guid per place, in the same order and with no gap, so a
// place number doubles as an index into the character's fields. That is what
// lets a single base plus twice the slot address any of them, and it holds only
// while each region's first place lands on the field the client names for it.

TEST_CASE("field: each region's first place lands on the field the client names")
{
    auto const mirror = [](uint8 slot) { return PLAYER_FIELD_INV_SLOT_HEAD + slot * 2; };

    CHECK(mirror(EQUIPMENT_SLOT_START) == PLAYER_FIELD_INV_SLOT_HEAD);
    CHECK(mirror(INVENTORY_SLOT_ITEM_START) == PLAYER_FIELD_PACK_SLOT_1);
    CHECK(mirror(BANK_SLOT_ITEM_START) == PLAYER_FIELD_BANK_SLOT_1);
    CHECK(mirror(BANK_SLOT_BAG_START) == PLAYER_FIELD_BANKBAG_SLOT_1);
    CHECK(mirror(BUYBACK_SLOT_START) == PLAYER_FIELD_VENDORBUYBACK_SLOT_1);
    CHECK(mirror(KEYRING_SLOT_START) == PLAYER_FIELD_KEYRING_SLOT_1);
}

TEST_CASE("field: no place reaches past the guids into what follows them")
{
    uint16 const last = uint16(PLAYER_FIELD_INV_SLOT_HEAD + (PLAYER_SLOT_END - 1) * 2);

    CHECK(last <= PLAYER_FIELD_KEYRING_SLOT_LAST);
    CHECK(last + 2 <= PLAYER_FARSIGHT);
}

TEST_CASE("field: the client keeps more keyring room than the game gives out")
{
    // Thirty-two places of keyring in the layout, sixteen of them in use.
    uint32 const room = (PLAYER_FARSIGHT - PLAYER_FIELD_KEYRING_SLOT_1) / 2;

    CHECK(room == 32);
    CHECK(KEYRING_SLOT_END - KEYRING_SLOT_START == 16);
}

// The nineteen worn places have a second, public face: what onlookers are shown
// of the piece in each of them. It sits in its own block, ahead of the private
// slot guids, so a face computed for a place that is not worn walks straight out
// of the block and into them.

TEST_CASE("face: exactly the worn places have one, and it fills its block")
{
    uint32 const perFace = MAX_VISIBLE_ITEM_OFFSET;
    uint32 const block = PLAYER_FIELD_INV_SLOT_HEAD - PLAYER_VISIBLE_ITEM_1_CREATOR;

    CHECK(block == EQUIPMENT_SLOT_END * perFace);
}

TEST_CASE("face: the last worn place's face ends inside the block")
{
    uint32 const last = PLAYER_VISIBLE_ITEM_1_CREATOR
                      + (EQUIPMENT_SLOT_END - 1) * MAX_VISIBLE_ITEM_OFFSET;

    CHECK(last == PLAYER_VISIBLE_ITEM_LAST_CREATOR);
    CHECK(last + MAX_VISIBLE_ITEM_OFFSET - 1 == PLAYER_VISIBLE_ITEM_LAST_PAD);
}

TEST_CASE("face: a place past the worn ones would land among the private guids")
{
    // The first backpack place, were a face computed for it, writes over the
    // slot guids. Nothing may ask for the face of a place that is not worn.
    uint32 const strayed = PLAYER_VISIBLE_ITEM_1_0
                         + INVENTORY_SLOT_ITEM_START * MAX_VISIBLE_ITEM_OFFSET;

    CHECK(strayed > PLAYER_FIELD_INV_SLOT_HEAD);
}

TEST_CASE("face: two enchantments are shown of the seven an item can carry")
{
    CHECK(MAX_INSPECTED_ENCHANTMENT_SLOT == 2);
    CHECK(PERM_ENCHANTMENT_SLOT < MAX_INSPECTED_ENCHANTMENT_SLOT);
    CHECK(TEMP_ENCHANTMENT_SLOT < MAX_INSPECTED_ENCHANTMENT_SLOT);

    // The face keeps room for one entry and seven ids; five are never filled.
    uint32 const room = PLAYER_VISIBLE_ITEM_1_PROPERTIES - PLAYER_VISIBLE_ITEM_1_0;
    CHECK(room == 8);
}

TEST_CASE("bag: a container holds at most what its slot guids allow")
{
    CHECK(MAX_BAG_SIZE == 36);
}
