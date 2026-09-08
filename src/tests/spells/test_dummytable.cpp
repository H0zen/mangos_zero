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

// The cases below are the spells the table carries: the Deviate Fish rolling one
// of five, the Curse of the Eye that lands differently on a man and on a woman,
// the dragonlings only a trinket may summon.

#include "doctest.h"

#include "Cast/Triggers/TriggerBook.h"

namespace
{
    cast::Trigger Row(uint32 trigger, uint32 weight = 1)
    {
        cast::Trigger row;
        row.spell = 8063;
        row.triggerSpell = trigger;
        row.weight = weight;
        return row;
    }

    std::vector<const cast::Trigger*> Hat(const std::vector<cast::Trigger>& rows,
                                          const cast::Circumstance& how)
    {
        std::vector<const cast::Trigger*> eligible;
        for (const auto& row : rows)
        {
            if (cast::Suits(row, how))
            {
                eligible.push_back(&row);
            }
        }
        return eligible;
    }
}

TEST_CASE("a row that asks nothing suits any cast")
{
    const cast::Trigger row = Row(8064);
    CHECK(cast::Suits(row, cast::Circumstance()));
}

TEST_CASE("a row that names the unit the spell landed on needs one")
{
    cast::Trigger onTarget = Row(24085);
    onTarget.castOn = 1;

    cast::Trigger byTarget = Row(24085);
    byTarget.castBy = 1;

    cast::Trigger onCasterButNeedsOne = Row(13493);
    onCasterButNeedsOne.needs = cast::NEED_A_TARGET;

    cast::Circumstance alone;
    CHECK_FALSE(cast::Suits(onTarget, alone));
    CHECK_FALSE(cast::Suits(byTarget, alone));
    CHECK_FALSE(cast::Suits(onCasterButNeedsOne, alone));

    cast::Circumstance withOne;
    withOne.hasTarget = true;
    CHECK(cast::Suits(onTarget, withOne));
    CHECK(cast::Suits(byTarget, withOne));
    CHECK(cast::Suits(onCasterButNeedsOne, withOne));
}

TEST_CASE("what a row demands of the caster and of the one it lands on")
{
    cast::Trigger row = Row(8064);
    row.needs = cast::NEED_CASTER_IS_PLAYER;

    cast::Circumstance creatureCasting;
    CHECK_FALSE(cast::Suits(row, creatureCasting));

    cast::Circumstance playerCasting;
    playerCasting.casterIsPlayer = true;
    CHECK(cast::Suits(row, playerCasting));

    row.needs = cast::NEED_TARGET_IS_CREATURE | cast::NEED_TARGET_USES_MANA;
    row.castOn = 1;

    cast::Circumstance manaless;
    manaless.hasTarget = true;
    manaless.targetIsCreature = true;
    CHECK_FALSE(cast::Suits(row, manaless));

    cast::Circumstance drinker;
    drinker.hasTarget = true;
    drinker.targetIsCreature = true;
    drinker.targetUsesMana = true;
    CHECK(cast::Suits(row, drinker));
}

TEST_CASE("a row cast from an item is not drawn when the spell came from no item")
{
    cast::Trigger row = Row(19804);
    row.needs = cast::NEED_CAST_FROM_ITEM;

    CHECK_FALSE(cast::Suits(row, cast::Circumstance()));

    cast::Circumstance trinket;
    trinket.fromItem = true;
    CHECK(cast::Suits(row, trinket));
}

TEST_CASE("gender is asked of whoever the row lands on")
{
    // Curse of the Eye lands on the target, so it is the target's gender
    cast::Trigger onAMan = Row(10651);
    onAMan.castOn = 1;
    onAMan.gender = 1;

    cast::Trigger onAWoman = Row(10653);
    onAWoman.castOn = 1;
    onAWoman.gender = 2;

    cast::Circumstance manTargeted;
    manTargeted.hasTarget = true;
    manTargeted.targetGender = 1;
    manTargeted.casterGender = 2;

    CHECK(cast::Suits(onAMan, manTargeted));
    CHECK_FALSE(cast::Suits(onAWoman, manTargeted));

    // Savory Deviate Delight lands on the caster, so it is his own gender
    cast::Trigger onHimself = Row(8219);
    onHimself.gender = 2;

    cast::Circumstance woman;
    woman.casterGender = 2;
    CHECK(cast::Suits(onHimself, woman));
}

TEST_CASE("the Deviate Fish gives each of its five one chance in five")
{
    const std::vector<cast::Trigger> rows = {
        Row(8064), Row(8065), Row(8066), Row(8067), Row(8068)
    };

    cast::Circumstance player;
    player.casterIsPlayer = true;

    const auto hat = Hat(rows, player);
    REQUIRE(hat.size() == 5u);
    CHECK(cast::TotalWeight(hat) == 5u);

    CHECK(cast::DrawAt(hat, 0)->triggerSpell == 8064u);
    CHECK(cast::DrawAt(hat, 1)->triggerSpell == 8065u);
    CHECK(cast::DrawAt(hat, 2)->triggerSpell == 8066u);
    CHECK(cast::DrawAt(hat, 3)->triggerSpell == 8067u);
    CHECK(cast::DrawAt(hat, 4)->triggerSpell == 8068u);
}

TEST_CASE("a heavier row takes a wider band of the draw")
{
    const std::vector<cast::Trigger> rows = { Row(1, 3), Row(2, 1) };
    const auto hat = Hat(rows, cast::Circumstance());

    CHECK(cast::TotalWeight(hat) == 4u);
    CHECK(cast::DrawAt(hat, 0)->triggerSpell == 1u);
    CHECK(cast::DrawAt(hat, 2)->triggerSpell == 1u);
    CHECK(cast::DrawAt(hat, 3)->triggerSpell == 2u);
}

TEST_CASE("the only row that suits the cast is always the one drawn")
{
    std::vector<cast::Trigger> rows = { Row(10651), Row(10653) };
    rows[0].castOn = 1;
    rows[0].gender = 1;
    rows[1].castOn = 1;
    rows[1].gender = 2;

    cast::Circumstance woman;
    woman.hasTarget = true;
    woman.targetGender = 2;

    const auto hat = Hat(rows, woman);
    REQUIRE(hat.size() == 1u);
    CHECK(cast::DrawAt(hat, 0)->triggerSpell == 10653u);
}

TEST_CASE("a cast that suits no row draws nothing")
{
    std::vector<cast::Trigger> rows = { Row(19804) };
    rows[0].needs = cast::NEED_CAST_FROM_ITEM;

    const auto hat = Hat(rows, cast::Circumstance());
    CHECK(hat.empty());
    CHECK(cast::TotalWeight(hat) == 0u);
    CHECK(cast::DrawAt(hat, 0) == nullptr);
}

TEST_CASE("a lot past the end of the hat falls on the last row")
{
    const std::vector<cast::Trigger> rows = { Row(1), Row(2) };
    const auto hat = Hat(rows, cast::Circumstance());

    CHECK(cast::DrawAt(hat, 99)->triggerSpell == 2u);
}
