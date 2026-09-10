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

#include "Spell.h"
#include "Cast/Triggers/TriggerBook.h"
#include "Item.h"
#include "Player.h"
#include "Unit.h"
#include "Utilities/Util.h"

bool Spell::ThrowWhatTheTableNames(const cast::Operation& )
{
    const std::vector<cast::Trigger>* rows = cast::Triggers().Of(m_spellInfo->ID);
    if (rows == nullptr)
    {
        return false;
    }

    cast::Circumstance how;
    how.casterIsPlayer =IsPlayer(m_caster);
    how.hasTarget = unitTarget != nullptr;
    how.fromItem = m_CastItem != nullptr;
    how.casterGender = m_caster->getGender() == GENDER_MALE ? 1 : 2;

    if (unitTarget != nullptr)
    {
        how.targetIsPlayer =IsPlayer(unitTarget);
        how.targetIsCreature =IsCreature(unitTarget);
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

        return true;
    }

    const cast::Trigger* drawn = cast::DrawAt(eligible, urand(0, total - 1));

    Unit* thrower = drawn->castBy == 1 ? unitTarget : m_caster;
    Unit* caught = drawn->castOn == 1 ? unitTarget : m_caster;

    thrower->CastSpell(caught, drawn->triggerSpell, true, drawn->carriesItem ? m_CastItem : nullptr);
    return true;
}
