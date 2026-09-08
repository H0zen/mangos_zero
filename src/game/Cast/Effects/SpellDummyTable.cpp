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

/**
 * @file SpellDummyTable.cpp
 * @brief Throwing the spell that `spell_dummy` names.
 *
 * A spell whose whole doing is to throw another spell is a row in a table, not
 * a case in a switch. This gathers what the table may ask about the cast, keeps
 * the rows that suit it, draws one by weight and throws it.
 */

#include "Spell.h"
#include "Cast/Triggers/TriggerBook.h"
#include "Item.h"
#include "Player.h"
#include "Unit.h"
#include "Utilities/Util.h"

/**
 * @brief Throws whatever `spell_dummy` says this spell throws.
 *
 * @param operation The slot being run.
 * @return True when the table knows this spell, whether or not this particular
 *         cast met any row; false when it says nothing about it.
 */
bool Spell::ThrowWhatTheTableNames(const cast::Operation& /*operation*/)
{
    const std::vector<cast::Trigger>* rows = cast::Triggers().Of(m_spellInfo->ID);
    if (rows == nullptr)
    {
        return false;
    }

    cast::Circumstance how;
    how.casterIsPlayer = m_caster->IsPlayer();
    how.hasTarget = unitTarget != nullptr;
    how.fromItem = m_CastItem != nullptr;
    how.casterGender = m_caster->getGender() == GENDER_MALE ? 1 : 2;

    if (unitTarget != nullptr)
    {
        how.targetIsPlayer = unitTarget->IsPlayer();
        how.targetIsCreature = unitTarget->IsCreature();
        how.targetUsesMana = unitTarget->GetPowerType() == POWER_MANA;
        how.targetGender = unitTarget->getGender() == GENDER_MALE ? 1 : 2;
    }

    std::vector<const cast::Trigger*> eligible;
    for (const auto& row : *rows)
    {
        if (!cast::Suits(row, how))
        {
            continue;
        }

        // whether the one it would land on already carries a forbidden aura is
        // asked of the world, so it is asked here rather than in the arithmetic
        const Unit* whoCatches = row.castOn == 1 ? unitTarget : m_caster;
        if (row.noAura != 0 && whoCatches != nullptr && whoCatches->HasAura(row.noAura))
        {
            continue;
        }

        eligible.push_back(&row);
    }

    const uint32 total = cast::TotalWeight(eligible);
    if (total == 0)
    {
        // the table knows this spell and this cast suits none of its rows; that
        // is an answer, not a gap for anything else to fill
        return true;
    }

    const cast::Trigger* drawn = cast::DrawAt(eligible, urand(0, total - 1));

    Unit* thrower = drawn->castBy == 1 ? unitTarget : m_caster;
    Unit* caught = drawn->castOn == 1 ? unitTarget : m_caster;

    thrower->CastSpell(caught, drawn->triggerSpell, true, drawn->carriesItem ? m_CastItem : nullptr);
    return true;
}
