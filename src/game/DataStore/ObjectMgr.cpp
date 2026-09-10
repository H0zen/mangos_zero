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

#include "Utterance.h"
#include "Common/Locales.h"
#include "Utilities/Errors.h"
#include <algorithm>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "ObjectMgr.h"
#include "LivingWorldAnchorPolicy.h"
#include "Movement/Generators/MotionMaster.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "Mint.h"

#include "SQLStorages.h"
#include "Log.h"
#include "MapCoords.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"

#include "ItemEnchantmentMgr.h"
#include <limits>
#include <cstdarg>
#include "PlayerRegistry.h"
#include "CorpseManager.h"
#include "Corpse.h"

bool normalizePlayerName(std::string& name)
{
    if (name.empty())
    {
        return false;
    }

    wchar_t wstr_buf[MAX_INTERNAL_PLAYER_NAME + 1];
    size_t wstr_len = MAX_INTERNAL_PLAYER_NAME;

    if (!Utf8toWStr(name, &wstr_buf[0], wstr_len))
    {
        return false;
    }

    wstr_buf[0] = wcharToUpper(wstr_buf[0]);
    for (size_t i = 1; i < wstr_len; ++i)
    {
        wstr_buf[i] = wcharToLower(wstr_buf[i]);
    }

    if (!WStrToUtf8(wstr_buf, wstr_len, name))
    {
        return false;
    }

    return true;
}

LanguageDesc lang_description[LANGUAGES_COUNT] =
{
    { LANG_ADDON,           0, 0                       },
    { LANG_UNIVERSAL,       0, 0                       },
    { LANG_ORCISH,        669, SKILL_LANG_ORCISH       },
    { LANG_DARNASSIAN,    671, SKILL_LANG_DARNASSIAN   },
    { LANG_TAURAHE,       670, SKILL_LANG_TAURAHE      },
    { LANG_DWARVISH,      672, SKILL_LANG_DWARVEN      },
    { LANG_COMMON,        668, SKILL_LANG_COMMON       },
    { LANG_DEMONIC,       815, SKILL_LANG_DEMON_TONGUE },
    { LANG_TITAN,         816, SKILL_LANG_TITAN        },
    { LANG_THALASSIAN,    813, SKILL_LANG_THALASSIAN   },
    { LANG_DRACONIC,      814, SKILL_LANG_DRACONIC     },
    { LANG_KALIMAG,       817, SKILL_LANG_OLD_TONGUE   },
    { LANG_GNOMISH,      7340, SKILL_LANG_GNOMISH      },
    { LANG_TROLL,        7341, SKILL_LANG_TROLL        },
    { LANG_GUTTERSPEAK, 17737, SKILL_LANG_GUTTERSPEAK  },
};

LanguageDesc const* GetLanguageDescByID(uint32 lang)
{
    for (int i = 0; i < LANGUAGES_COUNT; ++i)
    {
        if (uint32(lang_description[i].lang_id) == lang)
        {
            return &lang_description[i];
        }
    }

    return nullptr;
}

bool SpellClickInfo::IsFitToRequirements(Player const* player, Creature const* clickedCreature) const
{
    if (conditionId)
    {
        return sObjectMgr.IsPlayerMeetToCondition(conditionId, player, player->GetMap(), clickedCreature, CONDITION_FROM_SPELLCLICK);
    }

    if (questStart)
    {

        if (!player || ((!questStartCanActive || !player->IsActiveQuest(questStart)) && !player->GetQuestRewardStatus(questStart)))
        {
            return false;
        }
    }

    if (questEnd)
    {

        if (!player || player->GetQuestRewardStatus(questEnd))
        {
            return false;
        }
    }

    return true;
}

template<typename T>

T IdGenerator<T>::Generate()
{
    if (m_nextGuid >= std::numeric_limits<T>::max() - 1)
    {
        sLog.outError("%s guid overflow!! Can't continue, shutting down server. ", m_name);
        World::StopNow(ERROR_EXIT_CODE);
    }
    return m_nextGuid++;
}

template uint32 IdGenerator<uint32>::Generate();
template uint64 IdGenerator<uint64>::Generate();

bool operator < (const HonorStanding& lhs, const HonorStanding& rhs)
{
    return lhs.honorPoints > rhs.honorPoints;
}

bool AreaTrigger::IsLessOrEqualThan(AreaTrigger const* l) const
{
    MANGOS_ASSERT(target_mapId == l->target_mapId);
    if (!condition)
    {
        return true;
    }

    if (!l->condition)
    {
        return false;
    }

    if (condition == l->condition)
    {
        return true;
    }

    const PlayerCondition* pCond1 = sConditionStorage.LookupEntry<PlayerCondition>(condition);
    const PlayerCondition* pCond2 = sConditionStorage.LookupEntry<PlayerCondition>(l->condition);
    if (pCond1->m_condition == CONDITION_LEVEL && pCond2->m_condition == CONDITION_LEVEL)
    {
        return (pCond1->m_value1 <= pCond2->m_value1);
    }
    if (pCond1->m_condition == CONDITION_LEVEL)
    {
        return false;
    }
    if (pCond2->m_condition == CONDITION_LEVEL)
    {
        return true;
    }
    return false;
}

ObjectMgr::ObjectMgr()
    : DBCLocaleIndex(LOCALE_enUS)
{
}

ObjectMgr::~ObjectMgr()
{
    for (QuestMap::iterator i = mQuestTemplates.begin(); i != mQuestTemplates.end(); ++i)
    {
        delete i->second;
    }

    for (PetLevelInfoMap::iterator i = petInfo.begin(); i != petInfo.end(); ++i)
    {
        delete[] i->second;
    }

    for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
    {
        delete[] playerClassInfo[class_].levelInfo;
    }

    for (int race = 0; race < MAX_RACES; ++race)
    {
        for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
        {
            delete[] playerInfo[race][class_].levelInfo;
        }
    }

    for (GroupMap::iterator itr = mGroupMap.begin(); itr != mGroupMap.end(); ++itr)
    {
        delete itr->second;
    }

    for (CacheVendorItemMap::iterator itr = m_mCacheVendorTemplateItemMap.begin(); itr != m_mCacheVendorTemplateItemMap.end(); ++itr)
    {
        itr->second.Clear();
    }

    for (CacheVendorItemMap::iterator itr = m_mCacheVendorItemMap.begin(); itr != m_mCacheVendorItemMap.end(); ++itr)
    {
        itr->second.Clear();
    }

    for (CacheTrainerSpellMap::iterator itr = m_mCacheTrainerSpellMap.begin(); itr != m_mCacheTrainerSpellMap.end(); ++itr)
    {
        itr->second.Clear();
    }
}

Group* ObjectMgr::GetGroupById(uint32 id) const
{
    GroupMap::const_iterator itr = mGroupMap.find(id);
    if (itr != mGroupMap.end())
    {
        return itr->second;
    }

    return nullptr;
}

void ObjectMgr::AddLocaleString(std::string const& s, LocaleConstant locale, StringVector& data)
{
    if (!s.empty())
    {
        if (data.size() <= size_t(locale))
        {
            data.resize(locale + 1);
        }
        data[locale] = s;
    }
}

void ObjectMgr::LoadCreatureItemTemplates()
{
    sEquipmentStorageItem.Load(true);

    for (uint32 i = 0; i < sEquipmentStorageItem.GetMaxEntry(); ++i)
    {
        EquipmentInfoItem const* eqInfo = sEquipmentStorageItem.LookupEntry<EquipmentInfoItem>(i);

        if (!eqInfo)
        {
            continue;
        }

        EquipmentInfoItem const* itemProto = GetEquipmentInfoItem(eqInfo->entry);

        switch (itemProto->InventoryType)
        {
            case INVTYPE_2HWEAPON:
            case INVTYPE_HOLDABLE:
            case INVTYPE_QUIVER:
            case INVTYPE_RANGED:
            case INVTYPE_RANGEDRIGHT:
            case INVTYPE_RELIC:
            case INVTYPE_SHIELD:
            case INVTYPE_THROWN:
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
                break;
            default:
                sLog.outErrorDb("Item (entry=%u) in creature_item_template.entry for entry = %u is not equipable in a hand, forced to 0.", eqInfo->entry, i);
                const_cast<EquipmentInfoItem*>(eqInfo)->entry = 0;
                break;
        }
    }

    sLog.outString(">> Loaded %u creature item template", sEquipmentStorageItem.GetRecordCount());
    sLog.outString();
}

uint32 ObjectMgr::GetCreatureModelOtherTeamModel(uint32 modelId) const
{
    if (const CreatureModelInfo* modelInfo = GetCreatureModelInfo(modelId))
    {
        return modelInfo->modelid_other_team;
    }

    return 0;
}

void ObjectMgr::LoadCreatureSpells()
{

    std::set<uint32> spellScriptSet;

    std::unique_ptr<QueryResult> result (WorldDatabase.PQuery("SELECT `id` FROM `db_scripts` WHERE `script_type` = %d", DBS_ON_CREATURE_SPELL));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].GetUInt32();;
            spellScriptSet.insert(id);
        } while (result->NextRow());
    }

    std::set<uint32> spellScriptSetFull = spellScriptSet;

    m_CreatureSpellsMap.clear();

    result.reset(WorldDatabase.Query("SELECT `entry`, `spellId_1`, `probability_1`, `castTarget_1`, `targetParam1_1`, `targetParam2_1`, `castFlags_1`, `delayInitialMin_1`, `delayInitialMax_1`, `delayRepeatMin_1`, `delayRepeatMax_1`, `scriptId_1`, "

        "`spellId_2`, `probability_2`, `castTarget_2`, `targetParam1_2`, `targetParam2_2`, `castFlags_2`, `delayInitialMin_2`, `delayInitialMax_2`, `delayRepeatMin_2`, `delayRepeatMax_2`, `scriptId_2`, "

        "`spellId_3`, `probability_3`, `castTarget_3`, `targetParam1_3`, `targetParam2_3`, `castFlags_3`, `delayInitialMin_3`, `delayInitialMax_3`, `delayRepeatMin_3`, `delayRepeatMax_3`, `scriptId_3`, "

        "`spellId_4`, `probability_4`, `castTarget_4`, `targetParam1_4`, `targetParam2_4`, `castFlags_4`, `delayInitialMin_4`, `delayInitialMax_4`, `delayRepeatMin_4`, `delayRepeatMax_4`, `scriptId_4`, "

        "`spellId_5`, `probability_5`, `castTarget_5`, `targetParam1_5`, `targetParam2_5`, `castFlags_5`, `delayInitialMin_5`, `delayInitialMax_5`, `delayRepeatMin_5`, `delayRepeatMax_5`, `scriptId_5`, "

        "`spellId_6`, `probability_6`, `castTarget_6`, `targetParam1_6`, `targetParam2_6`, `castFlags_6`, `delayInitialMin_6`, `delayInitialMax_6`, `delayRepeatMin_6`, `delayRepeatMax_6`, `scriptId_6`, "

        "`spellId_7`, `probability_7`, `castTarget_7`, `targetParam1_7`, `targetParam2_7`, `castFlags_7`, `delayInitialMin_7`, `delayInitialMax_7`, `delayRepeatMin_7`, `delayRepeatMax_7`, `scriptId_7`, "

        "`spellId_8`, `probability_8`, `castTarget_8`, `targetParam1_8`, `targetParam2_8`, `castFlags_8`, `delayInitialMin_8`, `delayInitialMax_8`, `delayRepeatMin_8`, `delayRepeatMax_8`, `scriptId_8` FROM `creature_spells`"));
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 creature spell templates. DB table `creature_spells` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();;

        CreatureSpellsList spellsList;

        for (uint8 i = 0; i < CREATURE_SPELLS_MAX_SPELLS; i++)
        {
            uint16 spellId = fields[1 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt16();
            if (spellId)
            {
                if (!sSpellStore.LookupEntry(spellId))
                {
                    sLog.outErrorDb("Entry %u in table `creature_spells` has non-existent spell %u used as spellId_%u, skipping spell.", entry, spellId, i);
                    continue;
                }

                uint8 probability      = fields[2 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt8();

                if ((probability == 0) || (probability > 100))
                {
                    sLog.outErrorDb("Entry %u in table `creature_spells` has invalid probability_%u value %u, setting it to 100 instead.", entry, i, probability);
                    probability = 100;
                }

                uint8 castTarget       = fields[3 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt8();
                uint32 targetParam1    = fields[4 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt32();
                uint32 targetParam2    = fields[5 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt32();

                uint8 castFlags        = fields[6 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt8();

                uint32 delayInitialMin = fields[7 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt16() * IN_MILLISECONDS;
                uint32 delayInitialMax = fields[8 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt16() * IN_MILLISECONDS;

                if (delayInitialMin > delayInitialMax)
                {
                    sLog.outErrorDb("Entry %u in table `creature_spells` has invalid initial timers (Min_%u = %u, Max_%u = %u), skipping spell.", entry, i, delayInitialMin, i, delayInitialMax);
                    continue;
                }

                uint32 delayRepeatMin  = fields[9 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt16() * IN_MILLISECONDS;
                uint32 delayRepeatMax  = fields[10 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt16() * IN_MILLISECONDS;

                if (delayRepeatMin > delayRepeatMax)
                {
                    sLog.outErrorDb("Entry %u in table `creature_spells` has invalid repeat timers (Min_%u = %u, Max_%u = %u), skipping spell.", entry, i, delayRepeatMin, i, delayRepeatMax);
                    continue;
                }

                uint32 scriptId = fields[11 + i * CREATURE_SPELLS_MAX_COLUMNS].GetUInt32();

                if (scriptId)
                {
                    if (spellScriptSetFull.find(scriptId) == spellScriptSetFull.end())
                    {
                        sLog.outErrorDb("Entry %u in table `creature_spells` has non-existent scriptId_%u = %u, setting it to 0 instead.", entry, i, scriptId);
                        scriptId = 0;
                    }
                    else
                    {
                        spellScriptSet.erase(scriptId);
                    }
                }

                spellsList.emplace_back(spellId, probability, castTarget, targetParam1, targetParam2, castFlags, delayInitialMin, delayInitialMax, delayRepeatMin, delayRepeatMax, scriptId);
            }
        }

        if (!spellsList.empty())
        {
            m_CreatureSpellsMap.insert(CreatureSpellsMap::value_type(entry, spellsList));
        }

    } while (result->NextRow());

    for (const auto itr : spellScriptSet)
    {
        sLog.outErrorDb("Table `creature_spells_scripts` contains unused script, id %u.", itr);
    }

    sLog.outString();
    sLog.outString(">> Loaded %lu creature spell templates.", (unsigned long)m_CreatureSpellsMap.size());
}

ObjectGuid ObjectMgr::GetPlayerGuidByName(std::string name) const
{
    ObjectGuid guid = 0;

    CharacterDatabase.escape_string(name);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `name` = '%s'", name.c_str());
    if (result)
    {
        guid = MakeGuid(HIGHGUID_PLAYER, (*result)[0].GetUInt32());

        delete result;
    }

    return guid;
}

bool ObjectMgr::GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const
{

    if (Player* player = GetPlayer(guid))
    {
        name = player->GetName();
        return true;
    }

    uint32 lowguid = GuidCounter(guid);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `name` FROM `characters` WHERE `guid` = '%u'", lowguid);

    if (result)
    {
        name = (*result)[0].GetCppString();
        delete result;
        return true;
    }

    return false;
}

Team ObjectMgr::GetPlayerTeamByGUID(ObjectGuid guid) const
{

    if (Player* player = GetPlayer(guid))
    {
        return Player::TeamForRace(player->getRace());
    }

    uint32 lowguid = GuidCounter(guid);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `race` FROM `characters` WHERE `guid` = '%u'", lowguid);

    if (result)
    {
        uint8 race = (*result)[0].GetUInt8();
        delete result;
        return Player::TeamForRace(race);
    }

    return TEAM_NONE;
}

uint8 ObjectMgr::GetPlayerClassByGUID(ObjectGuid guid) const
{

    if (Player* player = GetPlayer(guid))
    {
        return player->getClass();
    }

    uint32 lowguid = GuidCounter(guid);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `class` FROM `characters` WHERE `guid` = '%u'", lowguid);

    if (result)
    {
        uint8 pClass = (*result)[0].GetUInt8();
        delete result;
        return pClass;
    }

    return 0;
}

uint32 ObjectMgr::GetPlayerAccountIdByGUID(ObjectGuid guid) const
{
    if (!(guid != 0 && GuidHigh(guid) == HIGHGUID_PLAYER))
    {
        return 0;
    }

    if (Player* player = GetPlayer(guid))
    {
        return player->GetSession()->GetAccountId();
    }

    uint32 lowguid = GuidCounter(guid);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `guid` = '%u'", lowguid);
    if (result)
    {
        uint32 acc = (*result)[0].GetUInt32();
        delete result;
        return acc;
    }

    return 0;
}

uint32 ObjectMgr::GetPlayerAccountIdByPlayerName(const std::string& name) const
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `name` = '%s'", name.c_str());
    if (result)
    {
        uint32 acc = (*result)[0].GetUInt32();
        delete result;
        return acc;
    }

    return 0;
}

void ObjectMgr::LoadStandingList(uint32 dateBegin)
{
    HonorStanding Standing;

    AllyHonorStandingList.clear();
    HordeHonorStandingList.clear();

    uint32 guid, kills, side;

    Field* fields = nullptr;
    QueryResult* result2 = nullptr;

    QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`,SUM(`honor`) as honor_sum FROM `character_honor_cp` WHERE `TYPE` = %u AND `date` BETWEEN %u AND %u AND `used`=0 GROUP BY `guid` ORDER BY `honor_sum` DESC", HONORABLE, dateBegin, dateBegin + 7);
    if (result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            fields = result->Fetch();
            guid  = fields[0].GetUInt32();
            side = GetPlayerTeamByGUID(MakeGuid(HIGHGUID_PLAYER, guid));

            kills = 0;

            result2 = CharacterDatabase.PQuery("SELECT COUNT(*) FROM `character_honor_cp` WHERE `guid` = %u AND `victim`>0 AND `TYPE` = %u AND `date` BETWEEN %u AND %u AND `used`=0", guid, HONORABLE, dateBegin, dateBegin + 7);
            if (result2)
            {
                kills = result2->Fetch()->GetUInt32();
            }

            if (kills < sWorld.getConfig(CONFIG_UINT32_MIN_HONOR_KILLS))
            {
                continue;
            }

            Standing.guid = guid;
            Standing.honorPoints = fields[1].GetUInt32();
            Standing.honorKills = kills;

            if (side == ALLIANCE)
            {
                AllyHonorStandingList.push_back(Standing);
            }
            else if (side == HORDE)
            {
                HordeHonorStandingList.push_back(Standing);
            }

            bar.step();
        }
        while (result->NextRow());

        delete result;
        delete result2;

        AllyHonorStandingList.sort();
        HordeHonorStandingList.sort();
    }
}

void ObjectMgr::LoadStandingList()
{

    uint32 LastWeekBegin = sWorld.GetDateLastMaintenanceDay() - 7;
    LoadStandingList(LastWeekBegin);

    DistributeRankPoints(ALLIANCE, LastWeekBegin);
    DistributeRankPoints(HORDE, LastWeekBegin);

    sLog.outString();
    sLog.outString(">> Loaded %lu Horde and %lu Ally honor standing definitions", HordeHonorStandingList.size(), AllyHonorStandingList.size());
}

void ObjectMgr::FlushRankPoints(uint32 dateTop)
{

    QueryResult* result = CharacterDatabase.PQuery("SELECT `date` FROM `character_honor_cp` WHERE `TYPE` = %u AND `date` <= %u AND `used`=0 GROUP BY `date` ORDER BY `date` DESC", HONORABLE, dateTop);
    if (result)
    {
        uint32 date;
        bool flush;
        uint32 WeekBegin = dateTop - 7;

        do
        {
            date = result->Fetch()->GetUInt32();
            while (WeekBegin && date < WeekBegin)
            {
                WeekBegin -= 7;
            }
        }
        while (result->NextRow());

        while (WeekBegin <= dateTop)
        {
            LoadStandingList(WeekBegin);

            flush = WeekBegin <= dateTop - 7;

            DistributeRankPoints(ALLIANCE, WeekBegin, flush);
            DistributeRankPoints(HORDE, WeekBegin, flush);

            WeekBegin += 7;
        }

        delete result;
    }

    static SqlStatementID updHonorable;
    static SqlStatementID updDishonorable;

    result = CharacterDatabase.PQuery("SELECT `guid`,`TYPE`,COUNT(*) AS kills FROM `character_honor_cp` WHERE `date` <= %u AND `victim_type`>0 AND `used`=0 GROUP BY `guid`,`type`", dateTop - 7);
    if (result)
    {
        CharacterDatabase.BeginTransaction();
        uint32 guid, kills;
        uint8 type;
        Field* fields = nullptr;
        do
        {
            fields = result->Fetch();
            guid   = fields[0].GetUInt32();
            type   = fields[1].GetUInt8();
            kills  = fields[2].GetUInt32();

            if (type == HONORABLE)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updHonorable, "UPDATE `characters` SET `stored_honorable_kills` = (`stored_honorable_kills` + ?) WHERE `guid` = ?");
                stmt.PExecute(kills, guid);
            }
            else if (type == DISHONORABLE)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updDishonorable, "UPDATE `characters` SET `stored_dishonorable_kills` = (`stored_dishonorable_kills` + ?) WHERE `guid` = ?");
                stmt.PExecute(kills, guid);
            }
        }
        while (result->NextRow());

        CharacterDatabase.PExecute("DELETE FROM `character_honor_cp` WHERE `date` <= %u", dateTop - 7);
        CharacterDatabase.CommitTransaction();
        delete result;
    }
    else
    {
        CharacterDatabase.PExecute("DELETE FROM `character_honor_cp` WHERE `date` <= %u AND `used`=1", dateTop - 7);
    }

    sLog.outString();
    sLog.outString(">> Flushed all ranking points");
}

void ObjectMgr::DistributeRankPoints(uint32 team, uint32 dateBegin , bool flush )
{
    float RP;
    uint32 HK;

    HonorStandingList list = GetStandingListBySide(team);

    if (list.empty())
    {
        return;
    }

    HonorScores scores = MaNGOS::Honor::GenerateScores(list, team);

    Field* fields = nullptr;
    QueryResult* result = nullptr;
    for (HonorStandingList::iterator itr = list.begin(); itr != list.end() ; ++itr)
    {
        RP = 0;
        result = CharacterDatabase.PQuery("SELECT `stored_honor_rating`,`stored_honorable_kills` FROM `characters` WHERE `guid` = %u ", itr->guid);
        if (!result)
        {
            continue;
        }

        fields = result->Fetch();
        RP = fields[0].GetFloat();
        HK = fields[1].GetUInt32();

        itr->rpEarning = MaNGOS::Honor::CalculateRpEarning(itr->GetInfo()->honorPoints, scores);
        RP             = MaNGOS::Honor::CalculateRpDecay(itr->rpEarning, RP);

        if (flush)
        {
            CharacterDatabase.BeginTransaction();
            CharacterDatabase.PExecute("UPDATE `character_honor_cp` SET `used`=1 WHERE `guid` = %u AND `TYPE` = %u AND `date` BETWEEN %u AND %u", itr->guid, HONORABLE, dateBegin, dateBegin + 7);
            CharacterDatabase.PExecute("UPDATE `characters` SET `stored_honor_rating` = %f , `stored_honorable_kills` = %u WHERE `guid` = %u", finiteAlways(RP + itr->rpEarning), HK + itr->honorKills, itr->guid);
            CharacterDatabase.CommitTransaction();
        }
    }

    delete result;
}

HonorStandingList ObjectMgr::GetStandingListBySide(uint32 side)
{
    switch (side)
    {
        case ALLIANCE: return AllyHonorStandingList;
        case HORDE:    return HordeHonorStandingList;
        default:       return AllyHonorStandingList;
    }
}

HonorStanding* ObjectMgr::GetHonorStandingByGUID(uint32 guid, uint32 side)
{
    HonorStandingList standingList = sObjectMgr.GetStandingListBySide(side);

    for (HonorStandingList::iterator itr = standingList.begin(); itr != standingList.end() ; ++itr)
    {
        if (itr->guid == guid)
        {
            return itr->GetInfo();
        }
    }
    return 0;
}

HonorStanding* ObjectMgr::GetHonorStandingByPosition(uint32 position, uint32 side)
{
    HonorStandingList standingList = sObjectMgr.GetStandingListBySide(side);
    uint32 pos = 1;

    for (HonorStandingList::iterator itr = standingList.begin(); itr != standingList.end() ; ++itr)
    {
        if (pos == position)
        {
            return itr->GetInfo();
        }
        pos++;
    }

    return 0;
}

uint32 ObjectMgr::GetHonorStandingPositionByGUID(uint32 guid, uint32 side)
{
    HonorStandingList standingList = sObjectMgr.GetStandingListBySide(side);
    uint32 pos = 1;

    for (HonorStandingList::iterator itr = standingList.begin(); itr != standingList.end() ; ++itr)
    {
        if (itr->guid == guid)
        {
            return pos;
        }
        pos++;
    }

    return 0;
}

GameObjectInfo const* ObjectMgr::GetGameObjectInfo(uint32 id) { return sGOStorage.LookupEntry<GameObjectInfo>(id); }

Player* ObjectMgr::GetPlayer(const char* name) { return sPlayerRegistry.FindByName(name); }

Player* ObjectMgr::GetPlayer(ObjectGuid guid, bool inWorld ) { return sPlayerRegistry.Find(guid, inWorld); }

CreatureInfo const* ObjectMgr::GetCreatureTemplate(uint32 id) { return sCreatureStorage.LookupEntry<CreatureInfo>(id); }

CreatureModelInfo const* ObjectMgr::GetCreatureModelInfo(uint32 modelid) { return sCreatureModelStorage.LookupEntry<CreatureModelInfo>(modelid); }

EquipmentInfo const* ObjectMgr::GetEquipmentInfo(uint32 entry) { return sEquipmentStorage.LookupEntry<EquipmentInfo>(entry); }

EquipmentInfoItem const* ObjectMgr::GetEquipmentInfoItem(uint32 entry) { return sEquipmentStorageItem.LookupEntry<EquipmentInfoItem>(entry); }

EquipmentInfoRaw const* ObjectMgr::GetEquipmentInfoRaw(uint32 entry) { return sEquipmentStorageRaw.LookupEntry<EquipmentInfoRaw>(entry); }

CreatureDataAddon const* ObjectMgr::GetCreatureAddon(uint32 lowguid) { return sCreatureDataAddonStorage.LookupEntry<CreatureDataAddon>(lowguid); }

CreatureDataAddon const* ObjectMgr::GetCreatureTemplateAddon(uint32 entry) { return sCreatureInfoAddonStorage.LookupEntry<CreatureDataAddon>(entry); }

ItemPrototype const* ObjectMgr::GetItemPrototype(uint32 id) { return sItemStorage.LookupEntry<ItemPrototype>(id); }

void ObjectMgr::LoadPetCreateSpells()
{
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `Spell1`, `Spell2`, `Spell3`, `Spell4` FROM `petcreateinfo_spell`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 pet create spells");

        return;
    }

    uint32 count = 0;

    BarGoLink bar(result->GetRowCount());

    mPetCreateSpell.clear();

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 creature_id = fields[0].GetUInt32();

        if (!creature_id)
        {
            sLog.outErrorDb("Creature id %u listed in `petcreateinfo_spell` not exist.", creature_id);
            continue;
        }

        CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(creature_id);
        if (!cInfo)
        {
            sLog.outErrorDb("Creature id %u listed in `petcreateinfo_spell` not exist.", creature_id);
            continue;
        }

        if (CreatureSpellDataEntry const* petSpellEntry = cInfo->PetSpellDataId ? sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId) : nullptr)
        {
            sLog.outErrorDb("Creature id %u listed in `petcreateinfo_spell` have set `PetSpellDataId` field and will use its instead, skip.", creature_id);
            continue;
        }

        PetCreateSpellEntry PetCreateSpell;

        bool have_spell = false;
        bool have_spell_db = false;
        for (int i = 0; i < 4; ++i)
        {
            PetCreateSpell.spellid[i] = fields[i + 1].GetUInt32();

            if (!PetCreateSpell.spellid[i])
            {
                continue;
            }

            have_spell_db = true;

            SpellEntry const* i_spell = sSpellStore.LookupEntry(PetCreateSpell.spellid[i]);
            if (!i_spell)
            {
                sLog.outErrorDb("Spell %u listed in `petcreateinfo_spell` does not exist", PetCreateSpell.spellid[i]);
                PetCreateSpell.spellid[i] = 0;
                continue;
            }

            have_spell = true;
        }

        if (!have_spell_db)
        {
            sLog.outErrorDb("Creature %u listed in `petcreateinfo_spell` have only 0 spell data, why it listed?", creature_id);
            continue;
        }

        if (!have_spell)
        {
            continue;
        }

        mPetCreateSpell[creature_id] = PetCreateSpell;
        ++count;
    }
    while (result->NextRow());

    delete result;

    std::map<uint32, uint32> learnCache;
    for (uint32 spell_id = 1; spell_id < sSpellStore.GetNumRows(); ++spell_id)
    {
        SpellEntry const* spellproto = sSpellStore.LookupEntry(spell_id);
        if (!spellproto)
        {
            continue;
        }

        if (spellproto->Effect[0] != SPELL_EFFECT_LEARN_SPELL && spellproto->Effect[0] != SPELL_EFFECT_LEARN_PET_SPELL)
        {
            continue;
        }

        if (!spellproto->EffectTriggerSpell[0])
        {
            continue;
        }

        learnCache[spellproto->EffectTriggerSpell[0]] = spellproto->ID;
    }

    uint32 dcount = 0;
    for (uint32 cr_id = 1; cr_id < sCreatureStorage.GetMaxEntry(); ++cr_id)
    {
        CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(cr_id);
        if (!cInfo)
        {
            continue;
        }

        CreatureSpellDataEntry const* petSpellEntry = cInfo->PetSpellDataId ? sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId) : nullptr;
        if (!petSpellEntry)
        {
            continue;
        }

        PetCreateSpellEntry PetCreateSpell;
        for (int i = 0; i < MAX_CREATURE_SPELL_DATA_SLOT; ++i)
        {
            uint32 petspell_id = petSpellEntry->SpellId[i];
            if (petspell_id)
            {

                std::map<uint32, uint32>::const_iterator cache_itr = learnCache.find(petspell_id);
                if (cache_itr != learnCache.end())
                {
                    petspell_id = cache_itr->second;
                }
            }

            PetCreateSpell.spellid[i] = petspell_id;
        }

        mPetCreateSpell[cr_id] = PetCreateSpell;
        ++dcount;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u pet create spells from table and %u from DBC", count, dcount);
}

struct SQLInstanceLoader : public SQLStorageLoaderBase<SQLInstanceLoader, SQLStorage>
{
    template<class D>
        void convert_from_str(uint32 , char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

void ObjectMgr::LoadInstanceTemplate()
{
    SQLInstanceLoader loader;
    loader.Load(sInstanceTemplate);

    for (uint32 i = 0; i < sInstanceTemplate.GetMaxEntry(); ++i)
    {
        InstanceTemplate const* temp = GetInstanceTemplate(i);
        if (!temp)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(temp->map);
        if (!mapEntry)
        {
            sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad mapid %d for template!", temp->map);
            sInstanceTemplate.EraseEntry(i);
            continue;
        }

        if (!mapEntry->Instanceable())
        {
            sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: non-instanceable mapid %d for template!", temp->map);
            sInstanceTemplate.EraseEntry(i);
            continue;
        }

        if (temp->parent > 0)
        {

            MapEntry const* parentEntry = sMapStore.LookupEntry(temp->parent);
            if (!parentEntry)
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad parent map id %u for instance template %d template!",
                    temp->parent, temp->map);
                const_cast<InstanceTemplate*>(temp)->parent = 0;
                continue;
            }

            if (parentEntry->IsContinent())
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: parent point to continent map id %u for instance template %d template, ignored, need be set only for non-continent parents!",
                    parentEntry->MapID, temp->map);
                const_cast<InstanceTemplate*>(temp)->parent = 0;
                continue;
            }
        }

        if (temp->ghostEntranceMap >= 0)
        {
            if (!MapCoords::Valid(temp->ghostEntranceMap, temp->ghostEntranceX, temp->ghostEntranceY))
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: ghost entrance coordinates invalid for instance template %d template, ignored, need be set only for non-continent parents!", temp->map);
                sInstanceTemplate.EraseEntry(i);
                continue;
            }

            MapEntry const* ghostEntry = sMapStore.LookupEntry(temp->ghostEntranceMap);
            if (!ghostEntry)
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad ghost entrance map id %u for instance template %d template!", ghostEntry->MapID, temp->map);
                sInstanceTemplate.EraseEntry(i);
                continue;
            }

            if (!ghostEntry->IsContinent())
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: ghost entrance not at continent map id %u for instance template %d template, ignored, need be set only for non-continent parents!", ghostEntry->MapID, temp->map);
                sInstanceTemplate.EraseEntry(i);
                continue;
            }
        }

        if (temp->reset_delay)
        {
            const_cast<InstanceTemplate*>(temp)->reset_delay = std::max((uint32)1, (uint32)(temp->reset_delay * sWorld.getConfig(CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME)));
        }
    }

    sLog.outString(">> Loaded %u Instance Template definitions", sInstanceTemplate.GetRecordCount());
    sLog.outString();
}

struct SQLWorldLoader : public SQLStorageLoaderBase<SQLWorldLoader, SQLStorage>
{
    template<class D>
        void convert_from_str(uint32 , char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

void ObjectMgr::LoadConditions()
{
    SQLWorldLoader loader;
    loader.Load(sConditionStorage);

    for (uint32 i = 0; i < sConditionStorage.GetMaxEntry(); ++i)
    {
        const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(i);
        if (!condition)
        {
            continue;
        }

        if (!condition->IsValid())
        {
            sLog.outErrorDb("ObjectMgr::LoadConditions: invalid condition_entry %u, skip", i);
            sConditionStorage.EraseEntry(i);
            continue;
        }
    }

    sLog.outString(">> Loaded %u Condition definitions", sConditionStorage.GetRecordCount());
    sLog.outString();
}

GossipText const* ObjectMgr::GetGossipText(uint32 Text_ID) const
{
    GossipTextMap::const_iterator itr = mGossipText.find(Text_ID);
    if (itr != mGossipText.end())
    {
        return &itr->second;
    }
    return nullptr;
}

void ObjectMgr::ReturnOrDeleteOldMails(bool serverUp)
{
    time_t curTime = time(nullptr);
    std::tm lt = safe_localtime(curTime);
    uint64 basetime(curTime);
    sLog.outString("Returning mails current time: hour: %d, minute: %d, second: %d ", lt.tm_hour, lt.tm_min, lt.tm_sec);

    if (!serverUp)
    {
        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `expire_time` < '" UI64FMTD "' AND `has_items` = '0' AND `body` = ''", (uint64)basetime);
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `id`,`messageType`,`sender`,`receiver`,`has_items`,`expire_time`,`cod`,`checked`,`mailTemplateId` FROM `mail` WHERE `expire_time` < '" UI64FMTD "'", (uint64)basetime);
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Only expired mails (need to be return or delete) or DB table `mail` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;
    Field* fields;

    do
    {
        bar.step();

        fields = result->Fetch();
        Mail* m = new Mail;
        m->messageID = fields[0].GetUInt32();
        m->messageType = fields[1].GetUInt8();
        m->sender = fields[2].GetUInt32();
        m->receiverGuid = MakeGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
        bool has_items = fields[4].GetBool();
        m->expire_time = (time_t)fields[5].GetUInt64();
        m->deliver_time = 0;
        m->COD = fields[6].GetUInt32();
        m->checked = fields[7].GetUInt32();
        m->mailTemplateId = fields[8].GetInt16();

        Player* pl = 0;
        if (serverUp)
        {
            pl = GetPlayer(m->receiverGuid);
        }
        if (pl)
        {

            delete m;
            continue;
        }

        if (has_items)
        {
            QueryResult* resultItems = CharacterDatabase.PQuery("SELECT `item_guid`,`item_template` FROM `mail_items` WHERE `mail_id`='%u'", m->messageID);
            if (resultItems)
            {
                do
                {
                    Field* fields2 = resultItems->Fetch();

                    uint32 item_guid_low = fields2[0].GetUInt32();
                    uint32 item_template = fields2[1].GetUInt32();

                    m->AddItem(item_guid_low, item_template);
                }
                while (resultItems->NextRow());

                delete resultItems;
            }

            if (m->messageType != MAIL_NORMAL || (m->checked & (MAIL_CHECK_MASK_COD_PAYMENT | MAIL_CHECK_MASK_RETURNED)))
            {

                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", itr2->item_guid);
                }
            }
            else
            {

                CharacterDatabase.PExecute("UPDATE `mail` SET `sender` = '%u', `receiver` = '%u', `expire_time` = '" UI64FMTD "', `deliver_time` = '" UI64FMTD "', `cod` = '0', `checked` = '%u' WHERE `id` = '%u'",
                    GuidCounter(m->receiverGuid), m->sender, (uint64)(basetime + 30 * DAY), (uint64)basetime, MAIL_CHECK_MASK_RETURNED, m->messageID);
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {

                    CharacterDatabase.PExecute("UPDATE `mail_items` SET `receiver` = %u WHERE `item_guid` = '%u'", m->sender, itr2->item_guid);
                    CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = %u WHERE `guid` = '%u'", m->sender, itr2->item_guid);
                }
                delete m;
                continue;
            }
        }

        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", m->messageID);
        delete m;
        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u mails", count);
    sLog.outString();
}

void ObjectMgr::LoadQuestAreaTriggers()
{
    mQuestAreaTriggerMap.clear();

    QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d", QA_AREATRIGGER);

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u quest trigger points", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 trigger_ID = fields[0].GetUInt32();
        uint32 quest_ID   = fields[1].GetUInt32();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `quest_relations` has area trigger (ID: %u) not listed in `AreaTrigger.dbc`.", trigger_ID);
            continue;
        }

        Quest const* quest = GetQuestTemplate(quest_ID);
        if (!quest)
        {
            sLog.outErrorDb("Table `quest_relations` has record (id: %u) for not existing quest %u", trigger_ID, quest_ID);
            continue;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
        {
            sLog.outErrorDb("Table `quest_relations` has record (id: %u) for not quest %u, but quest not have flag QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT. Trigger or quest flags must be fixed, quest modified to require objective.", trigger_ID, quest_ID);

            const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);

        }

        mQuestAreaTriggerMap[trigger_ID] = quest_ID;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u quest trigger points", count);
    sLog.outString();
}

void ObjectMgr::LoadTavernAreaTriggers()
{
    mTavernAreaTriggerSet.clear();

    QueryResult* result = WorldDatabase.Query("SELECT `id` FROM `areatrigger_tavern`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u tavern triggers", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 Trigger_ID      = fields[0].GetUInt32();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `areatrigger_tavern` has area trigger (ID:%u) not listed in `AreaTrigger.dbc`.", Trigger_ID);
            continue;
        }

        mTavernAreaTriggerSet.insert(Trigger_ID);
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u tavern triggers", count);
    sLog.outString();
}

void ObjectMgr::PackGroupIds()
{

    std::set<uint32> groupIds;

    QueryResult* result = CharacterDatabase.Query("SELECT `groupId` FROM `groups`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 id = fields[0].GetUInt32();

            if (id == 0)
            {
                CharacterDatabase.BeginTransaction();
                CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId` = '%u'", id);
                CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId` = '%u'", id);
                CharacterDatabase.CommitTransaction();
                continue;
            }

            groupIds.insert(id);
        }
        while (result->NextRow());
        delete result;
    }

    BarGoLink bar(groupIds.size() + 1);
    bar.step();

    uint32 groupId = 1;

    for (std::set<uint32>::iterator i = groupIds.begin(); i != groupIds.end(); ++i)
    {
        if (*i != groupId)
        {

            CharacterDatabase.BeginTransaction();
            CharacterDatabase.PExecute("UPDATE `groups` SET `groupId` = '%u' WHERE `groupId` = '%u'", groupId, *i);
            CharacterDatabase.PExecute("UPDATE `group_member` SET `groupId` = '%u' WHERE `groupId` = '%u'", groupId, *i);
            CharacterDatabase.CommitTransaction();
        }

        ++groupId;
        bar.step();
    }

    sMint.GroupIds().Set(groupId);

    sLog.outString(">> Group Ids remapped, next group id is %u", groupId);
    sLog.outString();
}

void ObjectMgr::SetHighestGuids()
{
    QueryResult* result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `characters`");
    if (result)
    {
        sMint.PlayerGuids().Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = WorldDatabase.Query("SELECT MAX(`guid`) FROM `creature`");
    if (result)
    {
        sMint.FirstTemporaryCreature((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `item_instance`");
    if (result)
    {
        sMint.ItemGuids().Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` >= '%u'", sMint.ItemGuids().NextAfterMaxUsed());
    CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` >= '%u'", sMint.ItemGuids().NextAfterMaxUsed());
    CharacterDatabase.PExecute("DELETE FROM `auction` WHERE `itemguid` >= '%u'", sMint.ItemGuids().NextAfterMaxUsed());
    CharacterDatabase.CommitTransaction();

    result = WorldDatabase.Query("SELECT MAX(`guid`) FROM `gameobject`");
    if (result)
    {
        sMint.FirstTemporaryGameObject((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `auction`");
    if (result)
    {
        sMint.AuctionIds().Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `mail`");
    if (result)
    {
        sMint.MailIds().Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `corpse`");
    if (result)
    {
        sMint.CorpseGuids().Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`guildid`) FROM `guild`");
    if (result)
    {
        sMint.GuildIds().Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`groupId`) FROM `groups`");
    if (result)
    {
        sMint.GroupIds().Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    sMint.StaticCreatureGuids().Set(sMint.FirstTemporaryCreature());
    sMint.FirstTemporaryCreature(sMint.FirstTemporaryCreature()
                                 + sWorld.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE));

    sMint.StaticGameObjectGuids().Set(sMint.FirstTemporaryGameObject());
    sMint.FirstTemporaryGameObject(sMint.FirstTemporaryGameObject()
                                   + sWorld.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT));
}

void ObjectMgr::LoadExplorationBaseXP()
{
    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `level`,`basexp` FROM `exploration_basexp`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u BaseXP definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        uint32 level  = fields[0].GetUInt32();
        uint32 basexp = fields[1].GetUInt32();
        mBaseXPTable[level] = basexp;
        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u BaseXP definitions", count);
    sLog.outString();
}

uint32 ObjectMgr::GetBaseXP(uint32 level) const
{
    BaseXPMap::const_iterator itr = mBaseXPTable.find(level);
    return itr != mBaseXPTable.end() ? itr->second : 0;
}

uint32 ObjectMgr::GetXPForLevel(uint32 level) const
{
    if (level < mPlayerXPperLevel.size())
    {
        return mPlayerXPperLevel[level];
    }
    return 0;
}

void ObjectMgr::LoadPetNames()
{
    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `word`,`entry`,`half` FROM `pet_name_generation`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u pet name parts", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        std::string word = fields[0].GetString();
        uint32 entry     = fields[1].GetUInt32();
        bool   half      = fields[2].GetBool();
        if (half)
        {
            PetHalfName1[entry].push_back(word);
        }
        else
        {
            PetHalfName0[entry].push_back(word);
        }
        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u pet name parts", count);
    sLog.outString();
}

void ObjectMgr::LoadPetNumber()
{
    QueryResult* result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `character_pet`");
    if (result)
    {
        Field* fields = result->Fetch();
        sMint.PetNumbers().Set(fields[0].GetUInt32() + 1);
        delete result;
    }

    BarGoLink bar(1);
    bar.step();

    sLog.outString(">> Loaded the max pet number: %d", sMint.PetNumbers().NextAfterMaxUsed() - 1);
    sLog.outString();
}

std::string ObjectMgr::GeneratePetName(uint32 entry)
{
    std::vector<std::string>& list0 = PetHalfName0[entry];
    std::vector<std::string>& list1 = PetHalfName1[entry];

    if (list0.empty() || list1.empty())
    {
        CreatureInfo const* cinfo = GetCreatureTemplate(entry);
        char const* petname = GetPetName(cinfo->Family, sWorld.GetDefaultDbcLocale());
        if (!petname)
        {
            petname = cinfo->Name;
        }
        return std::string(petname);
    }

    return *(list0.begin() + urand(0, list0.size() - 1)) + *(list1.begin() + urand(0, list1.size() - 1));
}

void ObjectMgr::LoadCorpses()
{
    uint32 count = 0;
    QueryResult* result = CharacterDatabase.Query(

            "SELECT `corpse`.`guid`, `player`, `corpse`.`position_x`, `corpse`.`position_y`, `corpse`.`position_z`, `corpse`.`orientation`, `corpse`.`map`, "

            "`time`, `corpse_type`, `instance`, `gender`, `race`, `class`, `playerBytes`, `playerBytes2`, `equipmentCache`, `guildId`, `playerFlags` FROM `corpse` "
            "JOIN `characters` ON `player` = `characters`.`guid` "
            "LEFT JOIN `guild_member` ON `player`=`guild_member`.`guid` WHERE `corpse_type` <> 0");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u corpses", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 guid = fields[0].GetUInt32();

        Corpse* corpse = new Corpse;
        if (!corpse->LoadFromDB(guid, fields))
        {
            delete corpse;
            continue;
        }

        sCorpseManager.Add(corpse);

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u corpses", count);
    sLog.outString();
}

void ObjectMgr::LoadPointsOfInterest()
{
    mPointsOfInterest.clear();

    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 Points of Interest definitions. DB table `points_of_interest` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 point_id = fields[0].GetUInt32();

        PointOfInterest POI;
        POI.x                    = fields[1].GetFloat();
        POI.y                    = fields[2].GetFloat();
        POI.icon                 = fields[3].GetUInt32();
        POI.flags                = fields[4].GetUInt32();
        POI.data                 = fields[5].GetUInt32();
        POI.icon_name            = fields[6].GetCppString();

        if (!MaNGOS::IsValidMapCoord(POI.x, POI.y))
        {
            sLog.outErrorDb("Table `points_of_interest` (Entry: %u) have invalid coordinates (X: %f Y: %f), ignored.", point_id, POI.x, POI.y);
            continue;
        }

        mPointsOfInterest[point_id] = POI;

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u Points of Interest definitions", count);
    sLog.outString();
}

void ObjectMgr::DeleteCreatureData(uint32 guid)
{

    CreatureData const* data = GetCreatureData(guid);
    if (data)
    {
        RemoveCreatureFromGrid(guid, data);
    }

    mCreatureDataMap.erase(guid);
}

void ObjectMgr::DeleteGOData(uint32 guid)
{

    GameObjectData const* data = GetGOData(guid);
    if (data)
    {
        RemoveGameobjectFromGrid(guid, data);
    }

    mGameObjectDataMap.erase(guid);
}

void ObjectMgr::AddCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid, uint32 instance)
{

    CellObjectGuids& cell_guids = mMapObjectGuids[mapid][cellid];
    cell_guids.corpses[player_guid] = instance;
}

void ObjectMgr::DeleteCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid)
{

    CellObjectGuids& cell_guids = mMapObjectGuids[mapid][cellid];
    cell_guids.corpses.erase(player_guid);
}

int ObjectMgr::GetIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
    {
        return -1;
    }

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
    {
        if (m_LocalForIndex[i] == loc)
        {
            return i;
        }
    }
    return -1;
}

LocaleConstant ObjectMgr::GetLocaleForIndex(int i)
{
    if (i < 0 || i >= (int32)m_LocalForIndex.size())
    {
        return LOCALE_enUS;
    }

    return m_LocalForIndex[i];
}

int ObjectMgr::GetOrNewIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
    {
        return -1;
    }

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
    {
        if (m_LocalForIndex[i] == loc)
        {
            return i;
        }
    }

    m_LocalForIndex.push_back(loc);
    return m_LocalForIndex.size() - 1;
}

inline void _DoStringError(int32 entry, char const* text, ...)
{
    MANGOS_ASSERT(text);

    char buf[256];
    va_list ap;
    va_start(ap, text);
    vsnprintf(buf, 256, text, ap);
    va_end(ap);

    if (entry <= MAX_CREATURE_AI_TEXT_STRING_ID)
    {
        sLog.outErrorScriptLib("%s", buf);
    }
    else if (entry <= MIN_CREATURE_AI_TEXT_STRING_ID)
    {
        sLog.outErrorEventAI("%s", buf);
    }
    else if (entry < MIN_DB_SCRIPT_STRING_ID)
    {
        sLog.outError("%s", buf);
    }
    else
    {
        sLog.outErrorDb("DB-SCRIPTS: %s", buf);
    }
}

bool ObjectMgr::LoadMangosStrings(DatabaseType& db, char const* table, int32 min_value, int32 max_value, bool extra_content)
{
    int32 start_value = min_value;
    int32 end_value   = max_value;

    if (start_value < 0)
    {
        if (end_value >= start_value)
        {
            sLog.outErrorDb("Table '%s' attempt loaded with invalid range (%d - %d), strings not loaded.", table, min_value, max_value);
            return false;
        }

        std::swap(start_value, end_value);
        ++start_value;
        ++end_value;
    }
    else
    {
        if (start_value >= end_value)
        {
            sLog.outErrorDb("Table '%s' attempt loaded with invalid range (%d - %d), strings not loaded.", table, min_value, max_value);
            return false;
        }
    }

    for (MangosStringLocaleMap::iterator itr = mMangosStringLocaleMap.begin(); itr != mMangosStringLocaleMap.end();)
    {
        if (itr->first >= start_value && itr->first < end_value)
        {
            mMangosStringLocaleMap.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    sLog.outString("Loading texts from %s%s", table, extra_content ? ", with additional data" : "");

    QueryResult* result = db.PQuery("SELECT `entry`,`content_default`,`content_loc1`,`content_loc2`,`content_loc3`,`content_loc4`,`content_loc5`,`content_loc6`,`content_loc7`,`content_loc8` %s FROM %s",
        extra_content ? ",sound,type,language,emote" : "", table);

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        if (min_value == MIN_MANGOS_STRING_ID)
        {
            sLog.outErrorDb(">> Loaded 0 mangos strings. DB table `%s` is empty. Can not continue.", table);
        }
        else
        {
            sLog.outString(">> Loaded 0 string templates. DB table `%s` is empty.", table);
        }
        return false;
    }

    uint32 count = 0;

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        int32 entry = fields[0].GetInt32();

        if (entry == 0)
        {
            _DoStringError(start_value, "Table `%s` contain reserved entry 0, ignored.", table);
            continue;
        }
        else if (entry < start_value || entry >= end_value)
        {
            _DoStringError(start_value, "Table `%s` contain entry %i out of allowed range (%d - %d), ignored.", table, entry, min_value, max_value);
            continue;
        }

        MangosStringLocale& data = mMangosStringLocaleMap[entry];

        if (!data.Content.empty())
        {
            _DoStringError(entry, "Table `%s` contain data for already loaded entry  %i (from another table?), ignored.", table, entry);
            continue;
        }

        data.Content.resize(1);
        ++count;

        data.Content[0] = fields[1].GetCppString();

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {

                    if ((int32)data.Content.size() <= idx + 1)
                    {
                        data.Content.resize(idx + 2);
                    }

                    data.Content[idx + 1] = str;
                }
            }
        }

        if (extra_content)
        {
            data.SoundId     = fields[10].GetUInt32();
            data.Type        = fields[11].GetUInt32();
            data.LanguageId  = Language(fields[12].GetUInt32());
            data.Emote       = fields[13].GetUInt32();

            if (data.SoundId && !sSoundEntriesStore.LookupEntry(data.SoundId))
            {
                _DoStringError(entry, "Entry %i in table `%s` has soundId %u but sound does not exist.", entry, table, data.SoundId);
                data.SoundId = 0;
            }

            if (!GetLanguageDescByID(data.LanguageId))
            {
                _DoStringError(entry, "Entry %i in table `%s` using Language %u but Language does not exist.", entry, table, uint32(data.LanguageId));
                data.LanguageId = LANG_UNIVERSAL;
            }

            if (data.Type > CHAT_TYPE_ZONE_YELL)
            {
                _DoStringError(entry, "Entry %i in table `%s` has Type %u but this Chat Type does not exist.", entry, table, data.Type);
                data.Type = CHAT_TYPE_SAY;
            }

            if (data.Emote && !sEmotesStore.LookupEntry(data.Emote))
            {
                _DoStringError(entry, "Entry %i in table `%s` has Emote %u but emote does not exist.", entry, table, data.Emote);
                data.Emote = EMOTE_ONESHOT_NONE;
            }
        }
    }
    while (result->NextRow());

    delete result;

    if (min_value == MIN_MANGOS_STRING_ID)
    {
        sLog.outString(">> Loaded %u MaNGOS strings from table %s", count, table);
    }
    else
    {
        sLog.outString(">> Loaded %u %s templates from %s", count, extra_content ? "text" : "string", table);
    }
    sLog.outString();

    m_loadedStringCount[min_value] = count;

    return true;
}

const char* ObjectMgr::GetMangosString(int32 entry, int locale_idx) const
{

    if (MangosStringLocale const* msl = GetMangosStringLocale(entry))
    {
        if ((int32)msl->Content.size() > locale_idx + 1 && !msl->Content[locale_idx + 1].empty())
        {
            return msl->Content[locale_idx + 1].c_str();
        }
        else
        {
            return msl->Content[0].c_str();
        }
    }

    _DoStringError(entry, "Entry %i not found but requested", entry);

    return "<error>";
}

bool ObjectMgr::IsPlayerMeetToCondition(uint16 conditionId, Player const* pPlayer, Map const* map, Occupant const* source, ConditionSource conditionSourceType, ConditionEntry* entry) const
{
    if (const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(conditionId))
    {
        return condition->Meets(pPlayer, map, source, conditionSourceType, entry);
    }

    return false;
}

char const* conditionSourceToStr[] =
{
        "loot system",
        "referencing loot",
        "gossip menu",
        "gossip menu option",
        "event AI",
        "hardcoded",
        "vendor's item check",
        "spell_area check",
        "npc_spellclick_spells check",
        "DBScript engine",
        "area trigger check"
};

bool PlayerCondition::Meets(Player const* player, Map const* map, Occupant const* source, ConditionSource conditionSourceType, ConditionEntry* entry) const
{
    DEBUG_LOG("Condition-System: Check condition %u, type %i - called from %s with params plr: %s, map %i, src %s",
        m_entry, m_condition, conditionSourceToStr[conditionSourceType], player ? player->GetGuidStr().c_str() : "<nullptr>", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "<nullptr>");

    if (entry)
    {
        entry->type = m_condition;
        entry->param1 = m_value1;
        entry->param2 = m_value2;
    }

    if (!CheckParamRequirements(player, map, source, conditionSourceType))
    {
        return false;
    }

    switch (m_condition)
    {
        case CONDITION_NOT:

            return !sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(player, map, source, conditionSourceType, entry);
        case CONDITION_OR:

            return sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(player, map, source, conditionSourceType, entry) || sConditionStorage.LookupEntry<PlayerCondition>(m_value2)->Meets(player, map, source, conditionSourceType, entry);
        case CONDITION_AND:

            return sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(player, map, source, conditionSourceType, entry) && sConditionStorage.LookupEntry<PlayerCondition>(m_value2)->Meets(player, map, source, conditionSourceType, entry);
        case CONDITION_NONE:
            return true;
        case CONDITION_AURA:
            return player->HasAura(m_value1, SpellEffectIndex(m_value2));
        case CONDITION_ITEM:
            return player->HasItemCount(m_value1, m_value2);
        case CONDITION_ITEM_EQUIPPED:
            return player->HasItemWithIdEquipped(m_value1, 1);
        case CONDITION_AREAID:
        {
            uint32 zone, area;
            Occupant const* searcher = source ? source : player;
            searcher->GetTerrain()->GetZoneAndAreaId(zone, area, searcher->Where().X(), searcher->Where().Y(), searcher->Where().Z());
            return (zone == m_value1 || area == m_value1) == (m_value2 == 0);
        }
        case CONDITION_REPUTATION_RANK_MIN:
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(m_value1);
            return faction && player->GetReputationMgr().GetRank(faction) >= ReputationRank(m_value2);
        }
        case CONDITION_TEAM:
        {
            if (conditionSourceType == CONDITION_FROM_REFERING_LOOT && sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
            {
                return true;
            }
            else
            {
                return uint32(player->GetTeam()) == m_value1;
            }
        }
        case CONDITION_SKILL:
            return player->HasSkill(m_value1) && player->GetBaseSkillValue(m_value1) >= m_value2;
        case CONDITION_QUESTREWARDED:
            return player->GetQuestRewardStatus(m_value1);
        case CONDITION_QUESTTAKEN:
            return player->IsCurrentQuest(m_value1, m_value2);
        case CONDITION_AD_COMMISSION_AURA:
        {
            Unit::SpellAuraHolderMap const& auras = player->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
            {
                if ((itr->second->GetSpellProto()->HasAttribute(SPELL_ATTR_CASTABLE_WHILE_MOUNTED) || itr->second->GetSpellProto()->HasAttribute(SPELL_ATTR_ABILITY)) && itr->second->GetSpellProto()->SpellVisualID == 3580)
                {
                    return true;
                }
            }
            return false;
        }
        case CONDITION_NO_AURA:
            return !player->HasAura(m_value1, SpellEffectIndex(m_value2));
        case CONDITION_ACTIVE_GAME_EVENT:
            return sGameEventMgr.IsActiveEvent(m_value1);
        case CONDITION_AREA_FLAG:
        {
            Occupant const* searcher = source ? source : player;
            if (AreaTableEntry const* pAreaEntry = GetAreaEntryByAreaID(searcher->GetTerrain()->GetAreaId(searcher->Where().X(), searcher->Where().Y(), searcher->Where().Z())))
            {
                if ((!m_value1 || (pAreaEntry->Flags & m_value1)) && (!m_value2 || !(pAreaEntry->Flags & m_value2)))
                {
                    return true;
                }
            }
            return false;
        }
        case CONDITION_RACE_CLASS:
            if ((!m_value1 || (player->getRaceMask() & m_value1)) && (!m_value2 || (player->getClassMask() & m_value2)))
            {
                return true;
            }
            return false;
        case CONDITION_LEVEL:
        {
            switch (m_value2)
            {
                case 0: return player->getLevel() == m_value1;
                case 1: return player->getLevel() >= m_value1;
                case 2: return player->getLevel() <= m_value1;
            }
            return false;
        }
        case CONDITION_NOITEM:
            return !player->HasItemCount(m_value1, m_value2);
        case CONDITION_SPELL:
        {
            switch (m_value2)
            {
                case 0: return player->HasSpell(m_value1);
                case 1: return !player->HasSpell(m_value1);
            }
            return false;
        }
        case CONDITION_INSTANCE_SCRIPT:
        {
            if (!map)
            {
                map = player ? player->GetMap() : source->GetMap();
            }

            if (InstanceData* data = map->GetInstanceData())
            {
                return data->CheckConditionCriteriaMeet(player, m_value1, source, conditionSourceType);
            }
            return false;
        }
        case CONDITION_QUESTAVAILABLE:
        {
            return player->CanTakeQuest(sObjectMgr.GetQuestTemplate(m_value1), false);
        }
        case CONDITION_RESERVED_1:
        case CONDITION_RESERVED_2:
        case CONDITION_RESERVED_3:
        case CONDITION_RESERVED_4:
            return false;
        case CONDITION_QUEST_NONE:
        {
            if (!player->IsCurrentQuest(m_value1) && !player->GetQuestRewardStatus(m_value1))
            {
                return true;
            }
            return false;
        }
        case CONDITION_ITEM_WITH_BANK:
            return player->HasItemCount(m_value1, m_value2, true);
        case CONDITION_NOITEM_WITH_BANK:
            return !player->HasItemCount(m_value1, m_value2, true);
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
            return !sGameEventMgr.IsActiveEvent(m_value1);
        case CONDITION_ACTIVE_HOLIDAY:
            return sGameEventMgr.IsActiveHoliday(HolidayIds(m_value1));
        case CONDITION_NOT_ACTIVE_HOLIDAY:
            return !sGameEventMgr.IsActiveHoliday(HolidayIds(m_value1));
        case CONDITION_LEARNABLE_ABILITY:
        {

            if (player->HasSpell(m_value1))
            {
                return false;
            }

            if (m_value2)
            {

                if (player->HasItemCount(m_value2, 1, true))
                {
                    return false;
                }
            }

            bool isSkillOk = false;

            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(m_value1);

            for (SkillLineAbilityMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                const SkillLineAbilityEntry* skillInfo = itr->second;

                if (!skillInfo)
                {
                    continue;
                }

                if (!player->HasSkill(skillInfo->SkillLine))
                {
                    return false;
                }

                if (skillInfo->ClassMask && (skillInfo->ClassMask & player->getClassMask()) == 0)
                {
                    return false;
                }

                if (skillInfo->RaceMask && (skillInfo->RaceMask & player->getRaceMask()) == 0)
                {
                    return false;
                }

                if (skillInfo->TrivialSkillLineRankLow > player->GetSkillValue(skillInfo->SkillLine))
                {
                    return false;
                }

                isSkillOk = true;
                break;
            }

            if (isSkillOk)
            {
                return true;
            }

            return false;
        }
        case CONDITION_SKILL_BELOW:
        {
            if (m_value2 == 1)
            {
                return !player->HasSkill(m_value1);
            }
            else
            {
                return player->HasSkill(m_value1) && player->GetBaseSkillValue(m_value1) < m_value2;
            }
        }
        case CONDITION_REPUTATION_RANK_MAX:
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(m_value1);
            return faction && player->GetReputationMgr().GetRank(faction) <= ReputationRank(m_value2);
        }
        case CONDITION_SOURCE_AURA:
        {
            if (!IsType(source, TYPEMASK_UNIT))
            {
                sLog.outErrorDb("CONDITION_SOURCE_AURA (entry %u) is used for non unit source (source %s) by %s", m_entry, source->GetGuidStr().c_str(), player->GetGuidStr().c_str());
                return false;
            }
            return ((Unit*)source)->HasAura(m_value1, SpellEffectIndex(m_value2));
        }
        case CONDITION_LAST_WAYPOINT:
        {
            if (!IsCreature(source))
            {
                sLog.outErrorDb("CONDITION_LAST_WAYPOINT (entry %u) is used for non creature source (source %s) by %s", m_entry, source->GetGuidStr().c_str(), player->GetGuidStr().c_str());
                return false;
            }
            uint32 lastReachedWp = ((Creature*)source)->GetMotionMaster()->getLastReachedWaypoint();
            switch (m_value2)
            {
                case 0: return m_value1 == lastReachedWp;
                case 1: return m_value1 <= lastReachedWp;
                case 2: return m_value1 > lastReachedWp;
            }
            return false;
        }
        case CONDITION_GENDER:
            return player->getGender() == m_value1;
        case CONDITION_DEAD_OR_AWAY:
            switch (m_value1)
            {
                case 0:
                    return !player || !player->IsAlive() || (m_value2 && source && !InReach(*source, *player, m_value2));
                case 1:
                    if (!player)
                    {
                        return true;
                    }
                    if (Group const* grp = player->GetGroup())
                    {
                        for (GroupReference const* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
                        {
                            Player const* pl = itr->getSource();
                            if (pl && pl->IsAlive() && !pl->isGameMaster() && (!m_value2 || !source || InReach(*source, *pl, m_value2)))
                            {
                                return false;
                            }
                        }
                        return true;
                    }
                    else
                    {
                        return !player->IsAlive() || (m_value2 && source && !InReach(*source, *player, m_value2));
                    }
                case 2:
                    for (Map::PlayerList::const_iterator itr = map->GetPlayers().begin(); itr != map->GetPlayers().end(); ++itr)
                    {
                        Player const* plr = itr->getSource();
                        if (plr && plr->IsAlive() && !plr->isGameMaster() && (!m_value2 || !source || InReach(*source, *plr, m_value2)))
                        {
                            return false;
                        }
                    }
                    return true;
                case 3:
                    return !source || !IsCreature(source) || !((Unit*)source)->IsAlive();
            }
        case CONDITION_CREATURE_IN_RANGE:
        {
            Creature* creature = nullptr;

            MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck creature_check(*player, m_value1, true, false, m_value2, true);
            MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(creature, creature_check);
            Cell::VisitGridObjects(player, searcher, m_value2);

            return creature;
        }
        case CONDITION_GAMEOBJECT_IN_RANGE:
        {
            GameObject* pGo = nullptr;

            if (source)
            {
                MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*source, m_value1, m_value2);
                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> searcher(pGo, go_check);

                Cell::VisitGridObjects(source, searcher, m_value2);
            }
            return pGo;
        }
        case CONDITION_PVP_RANK:
        {
            switch (m_value2)
            {
                case 0: return player->GetHonorRankInfo().rank == m_value1;
                case 1: return player->GetHonorRankInfo().rank >= m_value1;
                case 2: return player->GetHonorRankInfo().rank <= m_value1;
            }
        }
        default:
            return false;
    }
}

bool PlayerCondition::CheckParamRequirements(Player const* pPlayer, Map const* map, Occupant const* source, ConditionSource conditionSourceType) const
{
    switch (m_condition)
    {
        case CONDITION_NOT:
        case CONDITION_AND:
        case CONDITION_OR:
        case CONDITION_NONE:
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
            break;
        case CONDITION_AREAID:
        case CONDITION_AREA_FLAG:
            if (!pPlayer && !source)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                    m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "nullptr", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "nullptr");
                return false;
            }
            break;
        case CONDITION_INSTANCE_SCRIPT:
            if (!pPlayer && !source && !map)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                    m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "nullptr", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "nullptr");
                return false;
            }
            break;
        case CONDITION_SOURCE_AURA:
        case CONDITION_LAST_WAYPOINT:
            if (!source)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                    m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "nullptr", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "nullptr");
                return false;
            }
            break;
        case CONDITION_DEAD_OR_AWAY:
            switch (m_value1)
            {
                case 0:
                case 1:
                case 2:
                    if (m_value2 && !source)
                    {
                        sLog.outErrorDb("CONDITION_DEAD_OR_AWAY %u - called from %s without source, but source expected for range check", m_entry, conditionSourceToStr[conditionSourceType]);
                        return false;
                    }
                    if (m_value1 != 2)
                    {
                        return true;
                    }

                    if (!map && (pPlayer || source))
                    {
                        map = source ? source->GetMap() : pPlayer->GetMap();
                    }
                    if (!map || !map->Instanceable())
                    {
                        sLog.outErrorDb("CONDITION_DEAD_OR_AWAY %u (Player in instance case) - called from %s without map param or from non-instanceable map %i", m_entry,  conditionSourceToStr[conditionSourceType], map ? map->GetId() : -1);
                        return false;
                    }
                case 3:
                    return true;
            }
            break;
        case CONDITION_PVP_RANK:
            if (!pPlayer)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with %s, map %i, src %s",
                    m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "nullptr", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "nullptr");
                return false;
            }
            break;
        default:
            if (!pPlayer)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                    m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "nullptr", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "nullptr");
                return false;
            }
            break;
    }
    return true;
}

bool PlayerCondition::IsValid(uint16 entry, ConditionType condition, uint32 value1, uint32 value2)
{
    switch (condition)
    {
        case CONDITION_NOT:
        {
            if (value1 >= entry)
            {
                sLog.outErrorDb("CONDITION_NOT (entry %u, type %d) has invalid value1 %u, must be lower than entry, skipped", entry, condition, value1);
                return false;
            }
            const PlayerCondition* condition1 = sConditionStorage.LookupEntry<PlayerCondition>(value1);
            if (!condition1)
            {
                sLog.outErrorDb("CONDITION_NOT (entry %u, type %d) has value1 %u without proper condition, skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_OR:
        case CONDITION_AND:
        {
            if (value1 >= entry)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has invalid value1 %u, must be lower than entry, skipped", entry, condition, value1);
                return false;
            }
            if (value2 >= entry)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has invalid value2 %u, must be lower than entry, skipped", entry, condition, value2);
                return false;
            }
            const PlayerCondition* condition1 = sConditionStorage.LookupEntry<PlayerCondition>(value1);
            if (!condition1)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has value1 %u without proper condition, skipped", entry, condition, value1);
                return false;
            }
            const PlayerCondition* condition2 = sConditionStorage.LookupEntry<PlayerCondition>(value2);
            if (!condition2)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has value2 %u without proper condition, skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_AURA:
        case CONDITION_SOURCE_AURA:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }
            if (value2 >= MAX_EFFECT_INDEX)
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing effect index (%u) (must be 0..%u), skipped", entry, condition, value2, MAX_EFFECT_INDEX - 1);
                return false;
            }
            break;
        }
        case CONDITION_ITEM:
        case CONDITION_NOITEM:
        case CONDITION_ITEM_WITH_BANK:
        case CONDITION_NOITEM_WITH_BANK:
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
            if (!proto)
            {
                sLog.outErrorDb("Item condition (entry %u, type %u) requires to have non existing item (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 < 1)
            {
                sLog.outErrorDb("Item condition (entry %u, type %u) useless with count < 1, skipped", entry, condition);
                return false;
            }
            break;
        }
        case CONDITION_ITEM_EQUIPPED:
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
            if (!proto)
            {
                sLog.outErrorDb("ItemEquipped condition (entry %u, type %u) requires to have non existing item (%u) equipped, skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_AREAID:
        {
            AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(value1);
            if (!areaEntry)
            {
                sLog.outErrorDb("Zone condition (entry %u, type %u) requires to be in non existing area (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 1)
            {
                sLog.outErrorDb("Zone condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_REPUTATION_RANK_MIN:
        case CONDITION_REPUTATION_RANK_MAX:
        {
            FactionEntry const* factionEntry = sFactionStore.LookupEntry(value1);
            if (!factionEntry)
            {
                sLog.outErrorDb("Reputation condition (entry %u, type %u) requires to have reputation non existing faction (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 >= MAX_REPUTATION_RANK)
            {
                sLog.outErrorDb("Reputation condition (entry %u, type %u) has invalid rank requirement (value2 = %u) - must be between %u and %u, skipped", entry, condition, value2, MIN_REPUTATION_RANK, MAX_REPUTATION_RANK - 1);
                return false;
            }
            break;
        }
        case CONDITION_TEAM:
        {
            if (value1 != ALLIANCE && value1 != HORDE)
            {
                sLog.outErrorDb("Team condition (entry %u, type %u) specifies unknown team (%u), skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_SKILL:
        case CONDITION_SKILL_BELOW:
        {
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(value1);
            if (!pSkill)
            {
                sLog.outErrorDb("Skill condition (entry %u, type %u) specifies non-existing skill (%u), skipped", entry, condition, value1);
                return false;
            }
            if (value2 < 1 || value2 > sWorld.GetConfigMaxSkillValue())
            {
                sLog.outErrorDb("Skill condition (entry %u, type %u) specifies invalid skill value (%u), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_QUESTREWARDED:
        case CONDITION_QUESTTAKEN:
        case CONDITION_QUESTAVAILABLE:
        case CONDITION_QUEST_NONE:
        {
            Quest const* Quest = sObjectMgr.GetQuestTemplate(value1);
            if (!Quest)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) specifies non-existing quest (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 && condition != CONDITION_QUESTTAKEN)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value2 (%u)!", entry, condition, value2);
            }
            break;
        }
        case CONDITION_AD_COMMISSION_AURA:
        {
            if (value1)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value1 (%u)!", entry, condition, value1);
            }
            if (value2)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value2 (%u)!", entry, condition, value2);
            }
            break;
        }
        case CONDITION_NO_AURA:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }
            if (value2 > MAX_EFFECT_INDEX)
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing effect index (%u) (must be 0..%u), skipped", entry, condition, value2, MAX_EFFECT_INDEX - 1);
                return false;
            }
            break;
        }
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        {
            if (!sGameEventMgr.IsValidEvent(value1))
            {
                sLog.outErrorDb("(Not)Active event condition (entry %u, type %u) requires existing event id (%u), skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_AREA_FLAG:
        {
            if (!value1 && !value2)
            {
                sLog.outErrorDb("Area flag condition (entry %u, type %u) has both values like 0, skipped", entry, condition);
                return false;
            }
            break;
        }
        case CONDITION_RACE_CLASS:
        {
            if (!value1 && !value2)
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has both values like 0, skipped", entry, condition);
                return false;
            }

            if (value1 && !(value1 & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has invalid player class %u, skipped", entry, condition, value1);
                return false;
            }

            if (value2 && !(value2 & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has invalid race mask %u, skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_LEVEL:
        {
            if (!value1 || value1 > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                sLog.outErrorDb("Level condition (entry %u, type %u)has invalid level %u, skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 2)
            {
                sLog.outErrorDb("Level condition (entry %u, type %u) has invalid argument %u (must be 0..2), skipped", entry, condition, value2);
                return false;
            }

            break;
        }
        case CONDITION_SPELL:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Spell condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 1)
            {
                sLog.outErrorDb("Spell condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value2);
                return false;
            }

            break;
        }
        case CONDITION_INSTANCE_SCRIPT:
            break;
        case CONDITION_RESERVED_1:
        case CONDITION_RESERVED_2:
        case CONDITION_RESERVED_3:
        case CONDITION_RESERVED_4:
        {
            sLog.outErrorDb("Condition (%u) reserved for later versions, skipped", condition);
            return false;
        }
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:

            break;
        case CONDITION_LEARNABLE_ABILITY:
        {
            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(value1);

            if (bounds.first == bounds.second)
            {
                sLog.outErrorDb("Learnable ability condition (entry %u, type %u) has spell id %u defined, but this spell is not listed in SkillLineAbility and can not be used, skipping.", entry, condition, value1);
                return false;
            }

            if (value2)
            {
                ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value2);
                if (!proto)
                {
                    sLog.outErrorDb("Learnable ability condition (entry %u, type %u) has item entry %u defined but item does not exist, skipping.", entry, condition, value2);
                    return false;
                }
            }

            break;
        }
        case CONDITION_LAST_WAYPOINT:
        {
            if (value2 > 2)
            {
                sLog.outErrorDb("Last Waypoint condition (entry %u, type %u) has an invalid value in value2. (Has %u, supported 0, 1, or 2), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_GENDER:
        {
            if (value1 >= MAX_GENDER)
            {
                sLog.outErrorDb("Gender condition (entry %u, type %u) has an invalid value in value1. (Has %u, must be smaller than %u), skipping.", entry, condition, value1, MAX_GENDER);
                return false;
            }
            break;
        }
        case CONDITION_DEAD_OR_AWAY:
        {
            if (value1 >= 4)
            {
                sLog.outErrorDb("Dead condition (entry %u, type %u) has an invalid value in value1. (Has %u, must be smaller than 4), skipping.", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_CREATURE_IN_RANGE:
        {
            if (!sCreatureStorage.LookupEntry<CreatureInfo> (value1))
            {
                sLog.outErrorDb("Creature in range condition (entry %u, type %u) has an invalid value in value1. (Creature %u does not exist in the database), skipping.", entry, condition, value1);
                return false;
            }
            if (value2 <= 0)
            {
                sLog.outErrorDb("Creature in range condition (entry %u, type %u) has an invalid value in value2. (Range %u must be greater than 0), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_GAMEOBJECT_IN_RANGE:
        {
            if (!sGOStorage.LookupEntry<GameObjectInfo>(value1))
            {
                sLog.outErrorDb("Game object in range condition (entry %u, type %u) has an invalid value in value1 (gameobject). (Game object %u does not exist in the database), skipping.", entry, condition, value1);
                return false;
            }
            if (value2 <= 0)
            {
                sLog.outErrorDb("Game object in range condition (entry %u, type %u) has an invalid value in value2 (range). (Range %u must be greater than 0), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_PVP_RANK:
        {
            if (value2 > 2)
            {
                sLog.outErrorDb("PVP rank condition (entry %u, type %u) has invalid argument %u (must be 0..2), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_NONE:
            break;
        default:
            sLog.outErrorDb("Condition entry %u has bad type of %d, skipped ", entry, condition);
            return false;
    }
    return true;
}

bool PlayerCondition::CanBeUsedWithoutPlayer(uint16 entry)
{
    PlayerCondition const* condition = sConditionStorage.LookupEntry<PlayerCondition>(entry);
    if (!condition)
    {
        return false;
    }

    switch (condition->m_condition)
    {
        case CONDITION_NOT:
            return CanBeUsedWithoutPlayer(condition->m_value1);
        case CONDITION_AND:
        case CONDITION_OR:
            return CanBeUsedWithoutPlayer(condition->m_value1) && CanBeUsedWithoutPlayer(condition->m_value2);
        case CONDITION_NONE:
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
        case CONDITION_AREAID:
        case CONDITION_AREA_FLAG:
        case CONDITION_INSTANCE_SCRIPT:
        case CONDITION_SOURCE_AURA:
        case CONDITION_LAST_WAYPOINT:
            return true;
        default:
            return false;
    }
}

SkillRangeType GetSkillRangeType(SkillLineEntry const* pSkill, bool racial)
{
    switch (pSkill->CategoryID)
    {
        case SKILL_CATEGORY_LANGUAGES:
            return SKILL_RANGE_LANGUAGE;
        case SKILL_CATEGORY_WEAPON:
            if (pSkill->ID != SKILL_FIST_WEAPONS)
            {
                return SKILL_RANGE_LEVEL;
            }
            else
            {
                return SKILL_RANGE_MONO;
            }
        case SKILL_CATEGORY_ARMOR:
        case SKILL_CATEGORY_CLASS:
            if (pSkill->ID != SKILL_POISONS && pSkill->ID != SKILL_LOCKPICKING)
            {
                return SKILL_RANGE_MONO;
            }
            else
            {
                return SKILL_RANGE_LEVEL;
            }
        case SKILL_CATEGORY_SECONDARY:
        case SKILL_CATEGORY_PROFESSION:

            if (IsProfessionSkill(pSkill->ID))
            {
                return SKILL_RANGE_RANK;
            }
            else if (racial)
            {
                return SKILL_RANGE_NONE;
            }
            else
            {
                return SKILL_RANGE_MONO;
            }
        default:
        case SKILL_CATEGORY_ATTRIBUTES:
        case SKILL_CATEGORY_GENERIC:
            return SKILL_RANGE_NONE;
    }
}

void ObjectMgr::LoadCoreSideGossipTextIdCache()
{
    m_mCacheNpcTextIdMap.clear();

    QueryResult* result = WorldDatabase.Query("SELECT `ct`.`Entry`, "
        "`gm`.`text_id` "
        "FROM `creature_template` `ct` "
        "LEFT JOIN ("
        "    SELECT "
        "    `entry`, MIN(`text_id`) as `text_id` "
        "    FROM `gossip_menu` "
        "    GROUP BY `entry` "
        ") As gm on `ct`.`GossipMenuId` = `gm`.`entry` "
        "WHERE `ct`.GossipMenuId <> 0 "
        "ORDER BY `ct`.`Entry` ASC, `gm`.`entry` ASC, `gm`.`text_id` ASC"
    );

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded core side gossip text id cache, NO DATA FOUND !");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        m_mCacheNpcTextIdMap[fields[0].GetUInt32()] =  fields[1].GetUInt32();

        ++count;
    }
    while (result->NextRow());

    sLog.outString(">> Loaded %u core side gossip text id in cache", count);
    sLog.outString();

}

void ObjectMgr::AddVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime)
{
    VendorItemData& vList = m_mCacheVendorItemMap[entry];
    vList.AddItem(item, maxcount, incrtime, 0);

    WorldDatabase.PExecuteLog("INSERT INTO `npc_vendor` (`entry`,`item`,`maxcount`,`incrtime`) VALUES('%u','%u','%u','%u')", entry, item, maxcount, incrtime);
}

bool ObjectMgr::RemoveVendorItem(uint32 entry, uint32 item)
{
    CacheVendorItemMap::iterator  iter = m_mCacheVendorItemMap.find(entry);
    if (iter == m_mCacheVendorItemMap.end())
    {
        return false;
    }

    if (!iter->second.RemoveItem(item))
    {
        return false;
    }

    WorldDatabase.PExecuteLog("DELETE FROM `npc_vendor` WHERE `entry`='%u' AND `item`='%u'", entry, item);
    return true;
}

bool ObjectMgr::IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item_id, uint32 maxcount, uint32 incrtime, uint16 conditionId, Player* pl, std::set<uint32>* skip_vendors) const
{
    char const* idStr = isTemplate ? "vendor template" : "vendor";
    CreatureInfo const* cInfo = nullptr;

    if (!isTemplate)
    {
        cInfo = GetCreatureTemplate(vendor_entry);
        if (!cInfo)
        {
            if (pl)
            {
                ChatHandler(pl).SendSysMessage(LANG_COMMAND_VENDORSELECTION);
            }
            else
            {
                sLog.outErrorDb("Table `%s` has data for nonexistent creature (Entry: %u), ignoring", tableName, vendor_entry);
            }
            return false;
        }

        if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR))
        {
            if (!skip_vendors || skip_vendors->count(vendor_entry) == 0)
            {
                if (pl)
                {
                    ChatHandler(pl).SendSysMessage(LANG_COMMAND_VENDORSELECTION);
                }
                else
                {
                    sLog.outErrorDb("Table `%s` has data for creature (Entry: %u) without vendor flag, ignoring", tableName, vendor_entry);
                }

                if (skip_vendors)
                {
                    skip_vendors->insert(vendor_entry);
                }
            }
            return false;
        }
    }

    if (!GetItemPrototype(item_id))
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage(LANG_ITEM_NOT_FOUND, item_id);
        }
        else
        {
            sLog.outErrorDb("Table `%s` for %s %u contain nonexistent item (%u), ignoring",
                tableName, idStr, vendor_entry, item_id);
        }
        return false;
    }

    if (maxcount > 0 && incrtime == 0)
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage("MaxCount!=0 (%u) but IncrTime==0", maxcount);
        }
        else
        {
            sLog.outErrorDb("Table `%s` has `maxcount` (%u) for item %u of %s %u but `incrtime`=0, ignoring",
                tableName, maxcount, item_id, idStr, vendor_entry);
        }
        return false;
    }
    else if (maxcount == 0 && incrtime > 0)
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage("MaxCount==0 but IncrTime<>=0");
        }
        else
        {
            sLog.outErrorDb("Table `%s` has `maxcount`=0 for item %u of %s %u but `incrtime`<>0, ignoring",
                tableName, item_id, idStr, vendor_entry);
        }
        return false;
    }

    if (conditionId && !sConditionStorage.LookupEntry<PlayerCondition>(conditionId))
    {
        sLog.outErrorDb("Table `%s` has `condition_id`=%u for item %u of %s %u but this condition is not valid, ignoring", tableName, conditionId, item_id, idStr, vendor_entry);
        return false;
    }

    VendorItemData const* vItems = isTemplate ? GetNpcVendorTemplateItemList(vendor_entry) : GetNpcVendorItemList(vendor_entry);
    VendorItemData const* tItems = isTemplate ? nullptr : GetNpcVendorTemplateItemList(vendor_entry);

    if (!vItems && !tItems)
    {
        return true;
    }

    if (vItems && vItems->FindItem(item_id))
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage(LANG_ITEM_ALREADY_IN_LIST, item_id);
        }
        else
        {
            sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u, ignoring",
                tableName, item_id, idStr, vendor_entry);
        }
        return false;
    }

    if (!isTemplate)
    {
        if (tItems && tItems->GetItem(item_id))
        {
            if (pl)
            {
                ChatHandler(pl).PSendSysMessage(LANG_ITEM_ALREADY_IN_LIST, item_id);
            }
            else
            {
                if (!cInfo->VendorTemplateId)
                {
                    sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u, ignoring",
                        tableName, item_id, idStr, vendor_entry);
                }
                else
                {
                    sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u (or possible in vendor template %u), ignoring",
                        tableName, item_id, idStr, vendor_entry, cInfo->VendorTemplateId);
                }
            }
            return false;
        }
    }

    uint32 countItems = vItems ? vItems->GetItemCount() : 0;
    countItems += tItems ? tItems->GetItemCount() : 0;

    if (countItems >= MAX_VENDOR_ITEMS)
    {
        if (pl)
        {
            ChatHandler(pl).SendSysMessage(LANG_COMMAND_ADDVENDORITEMITEMS);
        }
        else
        {
            sLog.outErrorDb("Table `%s` has too many items (%u >= %i) for %s %u, ignoring",
                tableName, countItems, MAX_VENDOR_ITEMS, idStr, vendor_entry);
        }
        return false;
    }

    return true;
}

void ObjectMgr::AddGroup(Group* group)
{
    mGroupMap[group->GetId()] = group ;
}

void ObjectMgr::RemoveGroup(Group* group)
{
    mGroupMap.erase(group->GetId());
}

bool LoadMangosStrings(DatabaseType& db, char const* table, int32 start_value, int32 end_value, bool extra_content)
{

    if (start_value > MAX_DB_SCRIPT_STRING_ID || end_value >= start_value)
    {
        sLog.outErrorDb("Table '%s' attempt loaded with reserved by mangos range (%d - %d), strings not loaded.", table, start_value, end_value + 1);
        return false;
    }

    return sObjectMgr.LoadMangosStrings(db, table, start_value, end_value, extra_content);
}

CreatureInfo const* GetCreatureTemplateStore(uint32 entry)
{
    return sCreatureStorage.LookupEntry<CreatureInfo>(entry);
}

Quest const* GetQuestTemplateStore(uint32 entry)
{
    return sObjectMgr.GetQuestTemplate(entry);
}

MangosStringLocale const* GetMangosStringData(int32 entry)
{
    return sObjectMgr.GetMangosStringLocale(entry);
}

bool FindCreatureData::operator()(CreatureDataPair const& dataPair)
{

    if (i_id && dataPair.second.id != i_id)
    {
        return false;
    }

    if (!i_anyData)
    {
        i_anyData = &dataPair;
    }

    if (!i_player)
    {
        return true;
    }

    if (dataPair.second.mapid != i_player->GetMapId())
    {
        return false;
    }

    float new_dist = i_player->Where().DistanceTo(Geometry::Vector2(dataPair.second.posX, dataPair.second.posY));

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    uint16 pool_id = sPoolMgr.IsPartOfAPool<Creature>(dataPair.first);
    if (pool_id && !i_player->GetMap()->GetPersistentState()->IsSpawnedPoolObject<Creature>(dataPair.first))
    {
        return false;
    }

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

CreatureDataPair const* FindCreatureData::GetResult() const
{
    if (i_spawnedData)
    {
        return i_spawnedData;
    }

    if (i_mapData)
    {
        return i_mapData;
    }

    return i_anyData;
}

bool FindGOData::operator()(GameObjectDataPair const& dataPair)
{

    if (i_id && dataPair.second.id != i_id)
    {
        return false;
    }

    if (!i_anyData)
    {
        i_anyData = &dataPair;
    }

    if (!i_player)
    {
        return true;
    }

    if (dataPair.second.mapid != i_player->GetMapId())
    {
        return false;
    }

    float new_dist = i_player->Where().DistanceTo(Geometry::Vector2(dataPair.second.posX, dataPair.second.posY));

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    uint16 pool_id = sPoolMgr.IsPartOfAPool<GameObject>(dataPair.first);
    if (pool_id && !i_player->GetMap()->GetPersistentState()->IsSpawnedPoolObject<GameObject>(dataPair.first))
    {
        return false;
    }

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

GameObjectDataPair const* FindGOData::GetResult() const
{
    if (i_mapData)
    {
        return i_mapData;
    }

    if (i_spawnedData)
    {
        return i_spawnedData;
    }

    return i_anyData;
}

bool DoDisplayText(Occupant* source, int32 entry, Unit const* target )
{
    MangosStringLocale const* data = sObjectMgr.GetMangosStringLocale(entry);

    if (!data)
    {
        _DoStringError(entry, "DoScriptText with source %s could not find text entry %i.", source->GetGuidStr().c_str(), entry);
        return false;
    }

    if (data->SoundId)
    {
        if (data->Type == CHAT_TYPE_ZONE_YELL)
        {
            PlaySoundToMap(*source->GetMap(), data->SoundId, source->GetTerrain()->GetZoneId(source->Where().X(), source->Where().Y(), source->Where().Z()));
        }
        else if (data->Type == CHAT_TYPE_WHISPER || data->Type == CHAT_TYPE_BOSS_WHISPER)
        {

            if (target &&IsPlayer(target))
            {
                PlaySound(*source, SoundKind::Flat, data->SoundId, static_cast<Player const*>(target));
            }
        }
        else
        {
            PlaySound(*source, SoundKind::Flat, data->SoundId);
        }
    }

    if (data->Emote)
    {
        if (IsCreature(source) ||IsPlayer(source))
        {
            ((Unit*)source)->HandleEmote(data->Emote);
        }
        else
        {
            _DoStringError(entry, "DoDisplayText entry %i tried to process emote for invalid source %s", entry, source->GetGuidStr().c_str());
            return false;
        }
    }

    if ((data->Type == CHAT_TYPE_WHISPER || data->Type == CHAT_TYPE_BOSS_WHISPER) && (!target || !IsPlayer(target)))
    {
        _DoStringError(entry, "DoDisplayText entry %i can not whisper without target unit (TYPEID_PLAYER).", entry);
        return false;
    }

    Utter(*source, data, target);
    return true;
}
