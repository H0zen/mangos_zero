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

// The field table and what it admits to whom.
//
// The table is generated from the client's own descriptor records, so these
// tests are not checking arithmetic -- they are checking that the generated
// data still says what the client says, and that the audience machinery folds
// it the way the wire needs. The sharpest of them is the allocation cut: a
// foreign player object is only 486 dwords wide inside the client, so a public
// field at or above that index would be a field with nowhere to land.

#include "doctest.h"

#include "FieldTable.h"
#include "UpdateFields.h"
#include "ObjectGuid.h"

#include <vector>

namespace
{
    bool MaskHas(uint32 const* mask, uint16 index)
    {
        return (mask[index >> 5] & (1u << (index & 31))) != 0;
    }

    std::vector<uint32> Fold(Fields::Table const& table, Fields::Audience audience)
    {
        std::vector<uint32> mask(table.blocks, 0u);
        Fields::MaskFor(table, audience, mask.data());
        return mask;
    }

    Fields::Audience const kStranger = Fields::VisPublic | Fields::VisDynamic;

    Fields::Audience const kSelf = Fields::VisPublic | Fields::VisPrivate | Fields::VisOwner
                                 | Fields::VisItemOwner | Fields::VisSpecial | Fields::VisParty
                                 | Fields::VisDynamic;
}

TEST_CASE("field table: every class is as wide as its *_END")
{
    CHECK(Fields::For(TYPEID_OBJECT).count == OBJECT_END);
    CHECK(Fields::For(TYPEID_ITEM).count == ITEM_END);
    CHECK(Fields::For(TYPEID_CONTAINER).count == CONTAINER_END);
    CHECK(Fields::For(TYPEID_UNIT).count == UNIT_END);
    CHECK(Fields::For(TYPEID_PLAYER).count == PLAYER_END);
    CHECK(Fields::For(TYPEID_GAMEOBJECT).count == GAMEOBJECT_END);
    CHECK(Fields::For(TYPEID_DYNAMICOBJECT).count == DYNAMICOBJECT_END);
    CHECK(Fields::For(TYPEID_CORPSE).count == CORPSE_END);
}

TEST_CASE("field table: every dword belongs to a field that covers it")
{
    for (uint8 typeId = 0; typeId < 8; ++typeId)
    {
        Fields::Table const& table = Fields::For(typeId);

        CHECK(table.blocks == (table.count + 31) / 32);
        CHECK(table.blocks <= Fields::MaxBlocks);

        for (uint16 index = 0; index < table.count; ++index)
        {
            Fields::Descriptor const& d = table.At(index);

            CHECK(d.index <= index);
            CHECK(index < d.index + d.words);
            CHECK(d.name != nullptr);

            // Every dword of one field carries that field's own description.
            CHECK(table.At(d.index).visibility == d.visibility);
            CHECK(table.At(d.index).words == d.words);
        }
    }
}

TEST_CASE("field table: the per-bit masks agree with the descriptors")
{
    for (uint8 typeId = 0; typeId < 8; ++typeId)
    {
        Fields::Table const& table = Fields::For(typeId);

        for (uint8 bit = 0; bit < 9; ++bit)
        {
            uint32 const* admits = table.MaskForBit(bit);

            for (uint16 index = 0; index < table.count; ++index)
            {
                bool const flagged = (table.At(index).visibility & (1u << bit)) != 0;
                CHECK(MaskHas(admits, index) == flagged);
            }
        }
    }
}

TEST_CASE("audience: padding is unreachable, self reaches everything else")
{
    for (uint8 typeId = 0; typeId < 8; ++typeId)
    {
        Fields::Table const& table = Fields::For(typeId);
        std::vector<uint32> const self = Fold(table, kSelf);

        for (uint16 index = 0; index < table.count; ++index)
        {
            bool const nameless = table.At(index).visibility == Fields::VisNone;
            CHECK(MaskHas(self.data(), index) == !nameless);
        }
    }
}

TEST_CASE("audience: a stranger is told less than the owner, who is told less than self")
{
    Fields::Table const& unit = Fields::For(TYPEID_UNIT);

    std::vector<uint32> const stranger = Fold(unit, kStranger);
    std::vector<uint32> const owner = Fold(unit, kStranger | Fields::VisOwner | Fields::VisItemOwner);
    std::vector<uint32> const self = Fold(unit, kSelf);

    for (uint16 block = 0; block < unit.blocks; ++block)
    {
        // Each audience is a superset of the narrower one.
        CHECK((stranger[block] & ~owner[block]) == 0u);
        CHECK((owner[block] & ~self[block]) == 0u);
    }

    // A pet's owner is shown the numbers the pet frame needs; a passer-by is not.
    CHECK_FALSE(MaskHas(stranger.data(), UNIT_FIELD_STAT0));
    CHECK(MaskHas(owner.data(), UNIT_FIELD_STAT0));

    CHECK_FALSE(MaskHas(stranger.data(), UNIT_FIELD_ATTACK_POWER));
    CHECK(MaskHas(owner.data(), UNIT_FIELD_ATTACK_POWER));

    // Damage and resistances are the inspectable detail, and Beast Lore is what
    // grants it -- not standing nearby.
    CHECK_FALSE(MaskHas(stranger.data(), UNIT_FIELD_MINDAMAGE));
    CHECK_FALSE(MaskHas(stranger.data(), UNIT_FIELD_RESISTANCES));
    CHECK(MaskHas(Fold(unit, kStranger | Fields::VisSpecial).data(), UNIT_FIELD_MINDAMAGE));
}

TEST_CASE("audience: a stranger's player block never crosses the client's allocation cut")
{
    // The client reserves 0x798 bytes -- 486 dwords -- for a player who is not
    // the one at the keyboard, and index 485 is the last public field there. A
    // field admitted past that point would have nowhere to be written.
    Fields::Table const& player = Fields::For(TYPEID_PLAYER);
    std::vector<uint32> const stranger = Fold(player, kStranger);

    REQUIRE(PLAYER_FIELD_INV_SLOT_HEAD == 486);

    for (uint16 index = PLAYER_FIELD_INV_SLOT_HEAD; index < player.count; ++index)
    {
        CHECK_FALSE(MaskHas(stranger.data(), index));
    }
}

TEST_CASE("audience: the quest log is for the party, health is for everyone")
{
    Fields::Table const& player = Fields::For(TYPEID_PLAYER);

    std::vector<uint32> const stranger = Fold(player, kStranger);
    std::vector<uint32> const party = Fold(player, kStranger | Fields::VisParty);

    // Twenty quest-log slots, three dwords apart, and only the first of each
    // triple is ever sent.
    for (uint16 slot = 0; slot < 20; ++slot)
    {
        uint16 const index = PLAYER_QUEST_LOG_1_1 + slot * 3;
        CHECK_FALSE(MaskHas(stranger.data(), index));
        CHECK(MaskHas(party.data(), index));
    }

    // Health is not withheld from anybody: it is rewritten for them instead.
    CHECK(MaskHas(stranger.data(), UNIT_FIELD_HEALTH));
    CHECK(MaskHas(stranger.data(), UNIT_FIELD_MAXHEALTH));
    CHECK(MaskHas(stranger.data(), UNIT_DYNAMIC_FLAGS));

    // Power is plain public, which is why it is never scaled.
    CHECK(MaskHas(stranger.data(), UNIT_FIELD_POWER1));
    CHECK(MaskHas(stranger.data(), UNIT_FIELD_MAXPOWER1));
}

TEST_CASE("audience: an item tells a stranger what it is, not what it holds")
{
    Fields::Table const& item = Fields::For(TYPEID_ITEM);

    std::vector<uint32> const stranger = Fold(item, kStranger);
    std::vector<uint32> const owner = Fold(item, kStranger | Fields::VisOwner | Fields::VisItemOwner);

    CHECK(MaskHas(stranger.data(), ITEM_FIELD_OWNER));
    CHECK(MaskHas(stranger.data(), ITEM_FIELD_ENCHANTMENT));

    CHECK_FALSE(MaskHas(stranger.data(), ITEM_FIELD_STACK_COUNT));
    CHECK_FALSE(MaskHas(stranger.data(), ITEM_FIELD_DURABILITY));
    CHECK_FALSE(MaskHas(stranger.data(), ITEM_FIELD_SPELL_CHARGES));

    CHECK(MaskHas(owner.data(), ITEM_FIELD_STACK_COUNT));
    CHECK(MaskHas(owner.data(), ITEM_FIELD_DURABILITY));
    CHECK(MaskHas(owner.data(), ITEM_FIELD_SPELL_CHARGES));
}

TEST_CASE("health: the unit and its owner read the real figure, nobody else does")
{
    ObjectGuid const hunter(HIGHGUID_PLAYER, uint32(1));
    ObjectGuid const passerby(HIGHGUID_PLAYER, uint32(2));
    ObjectGuid const pet(HIGHGUID_PET, uint32(300), uint32(3));
    ObjectGuid const wildBoar(HIGHGUID_UNIT, uint32(301), uint32(4));
    ObjectGuid const none;

    // A hunter reads their own pool, and their pet's -- the pet frame shows
    // exact figures and no other message carries them.
    CHECK(Fields::ReadsRealHitPoints(hunter, none, hunter));
    CHECK(Fields::ReadsRealHitPoints(pet, hunter, hunter));

    // Nobody else does, whoever they are.
    CHECK_FALSE(Fields::ReadsRealHitPoints(pet, hunter, passerby));
    CHECK_FALSE(Fields::ReadsRealHitPoints(hunter, none, passerby));
    CHECK_FALSE(Fields::ReadsRealHitPoints(wildBoar, none, hunter));

    // An ownerless unit is nobody's to read, and an empty owner never matches
    // an observer who simply has no guid of their own to compare.
    CHECK_FALSE(Fields::ReadsRealHitPoints(wildBoar, none, none));
}

TEST_CASE("health: a stranger is told a percentage, and a living unit is never zero")
{
    CHECK(Fields::HealthAsPercent(100, 100) == 100);
    CHECK(Fields::HealthAsPercent(50, 100) == 50);
    CHECK(Fields::HealthAsPercent(99, 100) == 99);

    // A guard with thousands of hit points still reports out of a hundred.
    CHECK(Fields::HealthAsPercent(4200, 8400) == 50);
    CHECK(Fields::HealthAsPercent(8400, 8400) == 100);

    // A sliver left on a large pool rounds to one, not to a corpse.
    CHECK(Fields::HealthAsPercent(1, 10000) == 1);
    CHECK(Fields::HealthAsPercent(49, 10000) == 1);

    // Dead is dead, and a unit with no pool at all cannot be a fraction of one.
    CHECK(Fields::HealthAsPercent(0, 10000) == 0);
    CHECK(Fields::HealthAsPercent(0, 0) == 0);
    CHECK(Fields::HealthAsPercent(10, 0) == 0);
}
