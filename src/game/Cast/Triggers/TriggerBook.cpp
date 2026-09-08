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

#include "Cast/Triggers/TriggerBook.h"

#include "Database/DatabaseEnv.h"
#include "DataStore/DBCStores.h"
#include "DataStore/DBCStructure.h"
#include "Log.h"
#include "ProgressBar.h"

namespace cast
{
    bool Suits(const Trigger& row, const Circumstance& how)
    {
        // a row that names the unit the spell landed on cannot be drawn without one
        if ((row.castBy == 1 || row.castOn == 1 || (row.needs & NEED_A_TARGET) != 0) && !how.hasTarget)
        {
            return false;
        }

        if ((row.needs & NEED_CASTER_IS_PLAYER) != 0 && !how.casterIsPlayer)
        {
            return false;
        }

        if ((row.needs & NEED_TARGET_IS_PLAYER) != 0 && !how.targetIsPlayer)
        {
            return false;
        }

        if ((row.needs & NEED_TARGET_IS_CREATURE) != 0 && !how.targetIsCreature)
        {
            return false;
        }

        if ((row.needs & NEED_TARGET_USES_MANA) != 0 && !how.targetUsesMana)
        {
            return false;
        }

        if ((row.needs & NEED_CAST_FROM_ITEM) != 0 && !how.fromItem)
        {
            return false;
        }

        if (row.gender != 0)
        {
            const uint8 whose = row.castOn == 1 ? how.targetGender : how.casterGender;
            if (whose != row.gender)
            {
                return false;
            }
        }

        return true;
    }

    uint32 TotalWeight(const std::vector<const Trigger*>& eligible)
    {
        uint32 total = 0;
        for (const auto* row : eligible)
        {
            total += row->weight;
        }
        return total;
    }

    const Trigger* DrawAt(const std::vector<const Trigger*>& eligible, uint32 lot)
    {
        if (eligible.empty())
        {
            return nullptr;
        }

        for (const auto* row : eligible)
        {
            if (lot < row->weight)
            {
                return row;
            }
            lot -= row->weight;
        }

        return eligible.back();
    }

    size_t TriggerBook::Load()
    {
        m_bySpell.clear();
        m_count = 0;

        QueryResult* result = WorldDatabase.Query(
            "SELECT `entry`, `trigger_spell`, `weight`, `caster`, `target`, "
            "`needs`, `gender`, `no_aura`, `carries_item` FROM `spell_dummy`");

        if (result == nullptr)
        {
            BarGoLink bar(1);
            bar.step();
            sLog.outString(">> Loaded %u rows of spell_dummy", 0);
            sLog.outString();
            return 0;
        }

        BarGoLink bar(result->GetRowCount());
        do
        {
            Field* fields = result->Fetch();
            bar.step();

            Trigger row;
            row.spell = fields[0].GetUInt32();
            row.triggerSpell = fields[1].GetUInt32();
            row.weight = fields[2].GetUInt32();
            row.castBy = fields[3].GetUInt8();
            row.castOn = fields[4].GetUInt8();
            row.needs = fields[5].GetUInt32();
            row.gender = fields[6].GetUInt8();
            row.noAura = fields[7].GetUInt32();
            row.carriesItem = fields[8].GetUInt8() != 0;

            if (sSpellStore.LookupEntry(row.spell) == nullptr)
            {
                sLog.outErrorDb("Spell %u listed in `spell_dummy` does not exist", row.spell);
                continue;
            }

            if (sSpellStore.LookupEntry(row.triggerSpell) == nullptr)
            {
                sLog.outErrorDb("Spell %u of `spell_dummy` throws spell %u, which does not exist",
                                row.spell, row.triggerSpell);
                continue;
            }

            if (row.noAura != 0 && sSpellStore.LookupEntry(row.noAura) == nullptr)
            {
                sLog.outErrorDb("Spell %u of `spell_dummy` names aura %u, which does not exist",
                                row.spell, row.noAura);
                continue;
            }

            if (row.weight == 0)
            {
                sLog.outErrorDb("Spell %u of `spell_dummy` throws %u with no weight, so it could never be drawn",
                                row.spell, row.triggerSpell);
                continue;
            }

            if (row.castBy > 1 || row.castOn > 1)
            {
                sLog.outErrorDb("Spell %u of `spell_dummy` names a caster or target that is neither the caster (0) nor the unit it landed on (1)",
                                row.spell);
                continue;
            }

            if (row.gender > 2)
            {
                sLog.outErrorDb("Spell %u of `spell_dummy` names gender %u, which is neither anyone (0), male (1) nor female (2)",
                                row.spell, row.gender);
                continue;
            }

            m_bySpell[row.spell].push_back(row);
            ++m_count;
        }
        while (result->NextRow());

        delete result;

        sLog.outString(">> Loaded %zu rows of spell_dummy, for %zu spells", m_count, m_bySpell.size());
        sLog.outString();

        return m_count;
    }

    const std::vector<Trigger>* TriggerBook::Of(uint32 spell) const
    {
        const auto found = m_bySpell.find(spell);
        return found != m_bySpell.end() ? &found->second : nullptr;
    }

    TriggerBook& Triggers()
    {
        static TriggerBook book;
        return book;
    }
}
