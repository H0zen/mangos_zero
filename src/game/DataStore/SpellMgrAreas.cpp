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

#include "SpellMgr.h"
#include "SpellAuraDefines.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "Chat.h"
#include "Spell.h"
#include "Unit.h"
#include "World.h"

void SpellMgr::LoadSpellAreas()
{
    mSpellAreaMap.clear();
    mSpellAreaForAuraMap.clear();

    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `spell`, `area`, `quest_start`, `quest_start_active`, `quest_end`, `condition_id`, `aura_spell`, `racemask`, `gender`, `autocast` FROM `spell_area`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell area requirements", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 spell = fields[0].GetUInt32();
        SpellArea spellArea;
        spellArea.spellId             = spell;
        spellArea.areaId              = fields[1].GetUInt32();
        spellArea.questStart          = fields[2].GetUInt32();
        spellArea.questStartCanActive = fields[3].GetBool();
        spellArea.questEnd            = fields[4].GetUInt32();
        spellArea.conditionId         = fields[5].GetUInt16();
        spellArea.auraSpell           = fields[6].GetInt32();
        spellArea.raceMask            = fields[7].GetUInt32();
        spellArea.gender              = Gender(fields[8].GetUInt8());
        spellArea.autocast            = fields[9].GetBool();

        if (!sSpellStore.LookupEntry(spell))
        {
            sLog.outErrorDb("Spell %u listed in `spell_area` does not exist", spell);
            continue;
        }

        {
            bool ok = true;
            SpellAreaMapBounds sa_bounds = GetSpellAreaMapBounds(spellArea.spellId);
            for (SpellAreaMap::const_iterator itr = sa_bounds.first; itr != sa_bounds.second; ++itr)
            {
                if (spellArea.spellId != itr->second.spellId)
                {
                    continue;
                }
                if (spellArea.areaId != itr->second.areaId)
                {
                    continue;
                }
                if (spellArea.questStart != itr->second.questStart)
                {
                    continue;
                }
                if (spellArea.auraSpell != itr->second.auraSpell)
                {
                    continue;
                }
                if ((spellArea.raceMask & itr->second.raceMask) == 0)
                {
                    continue;
                }
                if (spellArea.gender != itr->second.gender)
                {
                    continue;
                }

                ok = false;
                break;
            }

            if (!ok)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` already listed with similar requirements.", spell);
                continue;
            }
        }

        if (spellArea.areaId && !GetAreaEntryByAreaID(spellArea.areaId))
        {
            sLog.outErrorDb("Spell %u listed in `spell_area` have wrong area (%u) requirement", spell, spellArea.areaId);
            continue;
        }

        if (spellArea.conditionId && !sConditionStorage.LookupEntry<PlayerCondition>(spellArea.conditionId))
        {
            sLog.outErrorDb("Spell %u listed in `spell_area` have wrong conditionId (%u) requirement", spell, spellArea.conditionId);
            continue;
        }
        else if (!spellArea.conditionId)
        {
            if (spellArea.questStart && !sObjectMgr.GetQuestTemplate(spellArea.questStart))
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong start quest (%u) requirement", spell, spellArea.questStart);
                continue;
            }

            if (spellArea.questEnd)
            {
                if (!sObjectMgr.GetQuestTemplate(spellArea.questEnd))
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have wrong end quest (%u) requirement", spell, spellArea.questEnd);
                    continue;
                }

                if (spellArea.questEnd == spellArea.questStart && !spellArea.questStartCanActive)
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have quest (%u) requirement for start and end in same time", spell, spellArea.questEnd);
                    continue;
                }
            }

            if (spellArea.raceMask && (spellArea.raceMask & RACEMASK_ALL_PLAYABLE) == 0)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong race mask (%u) requirement", spell, spellArea.raceMask);
                continue;
            }

            if (spellArea.gender != GENDER_NONE && spellArea.gender != GENDER_FEMALE && spellArea.gender != GENDER_MALE)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong gender (%u) requirement", spell, spellArea.gender);
                continue;
            }
        }

        if (spellArea.auraSpell)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(abs(spellArea.auraSpell));
            if (!spellInfo)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong aura spell (%u) requirement", spell, abs(spellArea.auraSpell));
                continue;
            }

            switch (spellInfo->EffectAura[EFFECT_INDEX_0])
            {
                case SPELL_AURA_DUMMY:
                case SPELL_AURA_GHOST:
                    break;
                default:
                    sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell requirement (%u) without dummy/phase/ghost aura in effect 0", spell, abs(spellArea.auraSpell));
                    continue;
            }

            if (uint32(abs(spellArea.auraSpell)) == spellArea.spellId)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell (%u) requirement for itself", spell, abs(spellArea.auraSpell));
                continue;
            }

            if (spellArea.autocast && spellArea.auraSpell > 0)
            {
                bool chain = false;
                SpellAreaForAuraMapBounds saBound = GetSpellAreaForAuraMapBounds(spellArea.spellId);
                for (SpellAreaForAuraMap::const_iterator itr = saBound.first; itr != saBound.second; ++itr)
                {
                    if (itr->second->autocast && itr->second->auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell (%u) requirement that itself autocast from aura", spell, spellArea.auraSpell);
                    continue;
                }

                SpellAreaMapBounds saBound2 = GetSpellAreaMapBounds(spellArea.auraSpell);
                for (SpellAreaMap::const_iterator itr2 = saBound2.first; itr2 != saBound2.second; ++itr2)
                {
                    if (itr2->second.autocast && itr2->second.auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell (%u) requirement that itself autocast from aura", spell, spellArea.auraSpell);
                    continue;
                }
            }
        }

        SpellArea const* sa = &mSpellAreaMap.insert(SpellAreaMap::value_type(spell, spellArea))->second;

        if (spellArea.areaId)
        {
            mSpellAreaForAreaMap.insert(SpellAreaForAreaMap::value_type(spellArea.areaId, sa));
        }

        if (spellArea.auraSpell)
        {
            mSpellAreaForAuraMap.insert(SpellAreaForAuraMap::value_type(abs(spellArea.auraSpell), sa));
        }

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u spell area requirements", count);
    sLog.outString();
}

SpellCastResult SpellMgr::GetSpellAllowedInLocationError(SpellEntry const* spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id, Player const* player)
{

    SpellAreaMapBounds saBounds = GetSpellAreaMapBounds(spellInfo->ID);
    if (saBounds.first != saBounds.second)
    {
        for (SpellAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        {
            if (itr->second.IsFitToRequirements(player, zone_id, area_id))
            {
                return SPELL_CAST_OK;
            }
        }
        return SPELL_FAILED_REQUIRES_AREA;
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_EX3_BATTLEGROUND))
    {
        if (!player || !player->Battle().InOne())
        {
            return SPELL_FAILED_ONLY_BATTLEGROUNDS;
        }
    }

    switch (spellInfo->ID)
    {

        case 22564:
        case 22563:
        {
            if (!player)
            {
                return SPELL_FAILED_REQUIRES_AREA;
            }
            BattleGround* bg = player->Battle().Ground();
            return map_id == 30 && bg &&
                bg->GetStatus() != STATUS_WAIT_JOIN ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
        }
        case 23333:
        case 23335:
            return map_id == 489 && player && player->Battle().InOne() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
        case 2584:
        {
            return player && player->Battle().InOne() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS;
        }
        case 22011:
        case 22012:
        case 24171:
        {
            MapEntry const* mapEntry = sMapStore.LookupEntry(map_id);
            if (!mapEntry)
            {
                return SPELL_FAILED_REQUIRES_AREA;
            }
            return mapEntry->IsBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS;
        }
    }

    return SPELL_CAST_OK;
}

void SpellMgr::LoadSkillLineAbilityMap()
{
    mSkillLineAbilityMap.clear();

    BarGoLink bar(sSkillLineAbilityStore.GetNumRows());
    uint32 count = 0;

    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
    {
        bar.step();
        SkillLineAbilityEntry const* SkillInfo = sSkillLineAbilityStore.LookupEntry(i);
        if (!SkillInfo)
        {
            continue;
        }

        mSkillLineAbilityMap.insert(SkillLineAbilityMap::value_type(SkillInfo->Spell, SkillInfo));
        ++count;
    }

    sLog.outString(">> Loaded %u SkillLineAbility MultiMap Data", count);
    sLog.outString();
}

void SpellMgr::LoadSkillRaceClassInfoMap()
{
    mSkillRaceClassInfoMap.clear();

    BarGoLink bar(sSkillRaceClassInfoStore.GetNumRows());
    uint32 count = 0;

    for (uint32 i = 0; i < sSkillRaceClassInfoStore.GetNumRows(); ++i)
    {
        bar.step();
        SkillRaceClassInfoEntry const* skillRCInfo = sSkillRaceClassInfoStore.LookupEntry(i);
        if (!skillRCInfo)
        {
            continue;
        }

        if (!sSkillLineStore.LookupEntry(skillRCInfo->SkillID))
        {
            continue;
        }

        mSkillRaceClassInfoMap.insert(SkillRaceClassInfoMap::value_type(skillRCInfo->SkillID, skillRCInfo));

        ++count;
    }

    sLog.outString(">> Loaded %u SkillRaceClassInfo MultiMap Data", count);
    sLog.outString();
}
