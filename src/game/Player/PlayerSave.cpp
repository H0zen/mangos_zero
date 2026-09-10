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

#include <cmath>
#include "Utilities/Errors.h"
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include "Utilities/PackedValues.h"
#include "Player.h"
#include "TransportMap.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "LFGMgr.h"
#include "DisableMgr.h"
#include "Cast/Recipe/RecipeBook.h"

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))

#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)

#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define SKILL_VALUE(x)         PAIR32_LOPART(x)

#define SKILL_MAX(x)           PAIR32_HIPART(x)

#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v,m)

#define SKILL_TEMP_BONUS(x)    int16(PAIR32_LOPART(x))

#define SKILL_PERM_BONUS(x)    int16(PAIR32_HIPART(x))

#define MAKE_SKILL_BONUS(t, p) MAKE_PAIR32(t,p)

void Player::SaveToDB()
{

    m_nextSave = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_SAVE_PLAYER);
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s at save: ", m_name.c_str());
    outDebugStatsValues();

    CharacterDatabase.BeginTransaction();

    UpdateHonor();

    static SqlStatementID delChar ;
    static SqlStatementID insChar ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delChar, "DELETE FROM `characters` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());

    SqlStatement uberInsert = CharacterDatabase.CreateStatement(insChar, "INSERT INTO `characters` (`guid`,`account`,`name`,`race`,`class`,`gender`, "
        "`level`,`xp`,`money`,`playerBytes`,`playerBytes2`,`playerFlags`,"
        "`map`, `position_x`, `position_y`, `position_z`, `orientation`, "
        "`taximask`, `online`, `cinematic`, "
        "`totaltime`, `leveltime`, `rest_bonus`, `logout_time`, `is_logout_resting`, `resettalents_cost`, `resettalents_time`, "
        "`trans_x`, `trans_y`, `trans_z`, `trans_o`, `transguid`, `extra_flags`, `stable_slots`, `at_login`, `zone`, "
        "`death_expire_time`, `taxi_path`, "
        "`honor_highest_rank`, `honor_standing`, `stored_honor_rating`, `stored_dishonorable_kills`, `stored_honorable_kills`, "
        "`watchedFaction`, `drunk`, `health`, `power1`, `power2`, `power3`, "
        "`power4`, `power5`, `exploredZones`, `equipmentCache`, `ammoId`, `actionBars`, `createdDate`) "
        "VALUES ( ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, "
        "?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, "
        "?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?) ");

    uberInsert.addUInt32(GetGUIDLow());
    uberInsert.addUInt32(GetSession()->GetAccountId());
    uberInsert.addString(m_name.c_str());
    uberInsert.addUInt8(getRace());
    uberInsert.addUInt8(getClass());
    uberInsert.addUInt8(getGender());
    uberInsert.addUInt32(getLevel());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_XP));
    uberInsert.addUInt32(GetMoney());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES));
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES_2));
    uberInsert.addUInt32(GetPlayerFlags());

    if (!IsBeingTeleported())
    {

        uint32 savedMap = GetMapId();
        float savedX = Where().X(), savedY = Where().Y();
        float savedZ = Where().Z(), savedO = Where().Facing();

        Transport* vessel = m_transport;
        if (!vessel)
        {
            if (Map* on = FindMap())
            {
                if (TransportMap* hull = on->AsTransport())
                {
                    vessel = hull->Vessel();
                }
            }
        }

        if (vessel)
        {
            savedMap = vessel->GetMapId();
            savedX = vessel->Where().X();
            savedY = vessel->Where().Y();
            savedZ = vessel->Where().Z();
            savedO = vessel->Where().Facing();
        }

        uberInsert.addUInt32(savedMap);
        uberInsert.addFloat(finiteAlways(savedX));
        uberInsert.addFloat(finiteAlways(savedY));
        uberInsert.addFloat(finiteAlways(savedZ));
        uberInsert.addFloat(finiteAlways(savedO));
    }
    else
    {
        uberInsert.addUInt32(GetTeleportDest().MapId());
        uberInsert.addFloat(finiteAlways(GetTeleportDest().X()));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().Y()));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().Z()));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().Facing()));
    }

    std::ostringstream ss;
    ss << m_taxi;
    uberInsert.addString(ss);

    uberInsert.addUInt32(IsInWorld() ? 1 : 0);

    uberInsert.addUInt32(m_cinematic);

    uberInsert.addUInt32(Played().Total());
    uberInsert.addUInt32(Played().AtThisLevel());

    uberInsert.addFloat(finiteAlways(Resting().Bonus()));
    uberInsert.addUInt64(uint64(time(nullptr)));
    uberInsert.addUInt32(HasPlayerFlag(PLAYER_FLAGS_RESTING) ? 1 : 0);

    uberInsert.addUInt32(m_resetTalentsCost);
    uberInsert.addUInt64(uint64(m_resetTalentsTime));

    Position const* transportPosition = m_movementInfo.GetTransportPos();
    uberInsert.addFloat(finiteAlways(transportPosition->x));
    uberInsert.addFloat(finiteAlways(transportPosition->y));
    uberInsert.addFloat(finiteAlways(transportPosition->z));
    uberInsert.addFloat(finiteAlways(transportPosition->o));

    if (m_transport)
    {
        uberInsert.addUInt32(m_transport->GetGUIDLow());
    }
    else
    {
        uberInsert.addUInt32(0);
    }

    uberInsert.addUInt32(m_ExtraFlags);

    uberInsert.addUInt32(uint32(GetStableSlots()));

    uberInsert.addUInt32(uint32(m_atLoginFlags));

    uberInsert.addUInt32(IsInWorld() ? GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z()) : GetCachedZoneId());

    uberInsert.addUInt64(uint64(m_deathExpireTime));

    ss << m_taxi.SaveTaxiDestinationsToString();
    uberInsert.addString(ss);

    uberInsert.addUInt32(uint32(m_honor.HighestRank().rank));
    uberInsert.addInt32(m_honor.LastWeekPlace());
    uberInsert.addFloat(finiteAlways(m_honor.Stored()));
    uberInsert.addUInt32(m_honor.Kills(false));
    uberInsert.addUInt32(m_honor.Kills(true));

    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX));

    uberInsert.addUInt16(static_cast<uint16>(GetDrunkAndGender() & 0xFFFE));

    uberInsert.addUInt32(GetHealth());

    for (uint32 i = 0; i < MAX_POWERS; ++i)
    {
        uberInsert.addUInt32(GetPower(Powers(i)));
    }

    for (uint32 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i)
    {
        ss << GetExploredZones(i) << " ";
    }
    uberInsert.addString(ss);

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        ss << GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET) << " ";

        uint32 ench1 = GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET + 1 + PERM_ENCHANTMENT_SLOT);
        uint32 ench2 = GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET + 1 + TEMP_ENCHANTMENT_SLOT);
        ss << uint32(MAKE_PAIR32(ench1, ench2)) << " ";
    }
    uberInsert.addString(ss);

    uberInsert.addUInt32(GetUInt32Value(PLAYER_AMMO_ID));

    uberInsert.addUInt32(uint32(GetActionBars()));
    uberInsert.addUInt32(GetCreatedDate());

    uberInsert.Execute();

    if (Post().Changed())
    {
        SaveMail();
    }

    _SaveBGData();
    _SaveInventory();
    _SaveQuestStatus();
    _SaveSpells();
    _SaveSpellCooldowns();
    _SaveActions();
    _SaveAuras();
    _SaveSkills();
    m_reputationMgr.SaveToDB();
    _SaveHonorCP();
    GetSession()->SaveTutorialsData();

    CharacterDatabase.CommitTransaction();

    if (m_session->isLogingOut() || !sWorld.getConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT))
    {
        _SaveStats();
    }

    if (Pet* pet = GetPet())
    {
        pet->SavePetToDB(PET_SAVE_AS_CURRENT);
    }
}

void Player::SaveInventoryAndGoldToDB()
{
    _SaveInventory();
    SaveGoldToDB();
}

void Player::SaveGoldToDB()
{
    static SqlStatementID updateGold ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(updateGold, "UPDATE `characters` SET `money` = ? WHERE `guid` = ?");
    stmt.PExecute(GetMoney(), GetGUIDLow());
}

void Player::_SaveActions()
{
    static SqlStatementID insertAction ;
    static SqlStatementID updateAction ;
    static SqlStatementID deleteAction ;

    for (ActionButtonList::iterator itr = m_actionButtons.begin(); itr != m_actionButtons.end();)
    {
        switch (itr->second.uState)
        {
            case ACTIONBUTTON_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertAction, "INSERT INTO `character_action` (`guid`,`button`,`action`,`type`) VALUES (?, ?, ?, ?)");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.addUInt32(itr->second.GetAction());
                stmt.addUInt32(uint32(itr->second.GetType()));
                stmt.Execute();
                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
            }
            break;
            case ACTIONBUTTON_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateAction, "UPDATE `character_action` SET `action` = ?, `type` = ? WHERE `guid` = ? AND `button` = ?");
                stmt.addUInt32(itr->second.GetAction());
                stmt.addUInt32(uint32(itr->second.GetType()));
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.Execute();
                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
            }
            break;
            case ACTIONBUTTON_DELETED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAction, "DELETE FROM `character_action` WHERE `guid` = ? AND `button` = ?");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.Execute();
                m_actionButtons.erase(itr++);
            }
            break;
            default:
                ++itr;
                break;
        }
    }
}

void Player::_SaveAuras()
{
    static SqlStatementID deleteAuras ;
    static SqlStatementID insertAuras ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAuras, "DELETE FROM `character_aura` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();

    if (auraHolders.empty())
    {
        return;
    }

    stmt = CharacterDatabase.CreateStatement(insertAuras, "INSERT INTO `character_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
        "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        bool selfCastHolder = holder->GetCasterGuid() == GetObjectGuid();
        TrackedAuraType trackedType = holder->GetTrackedAuraType();
        if (!holder->IsPassive() && !(cast::RecipeOf(*holder->GetSpellProto()).Starts() == cast::Start::Channelled) &&
            (trackedType == TRACK_AURA_TYPE_NOT_TRACKED || (trackedType == TRACK_AURA_TYPE_SINGLE_TARGET && selfCastHolder)))
        {
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {

                    if (aur->IsAreaAura() && holder->GetCasterGuid() != GetObjectGuid())
                    {
                        continue;
                    }

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
            {
                continue;
            }

            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt64(holder->GetCasterGuid());
            stmt.addUInt32(GuidCounter(holder->GetCastItemGuid()));
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addInt32(damage[i]);
            }

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addUInt32(periodicTime[i]);
            }

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();
        }
    }
}

void Player::_SaveInventory()
{

    for (uint8 i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
    {
        Item* item = m_inventory.Own(i);
        if (!item || item->GetState() == ITEM_NEW)
        {
            continue;
        }

        static SqlStatementID delInv ;
        static SqlStatementID delItemInst ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delInv, "DELETE FROM `character_inventory` WHERE `item` = ?");
        stmt.PExecute(item->GetGUIDLow());

        stmt = CharacterDatabase.CreateStatement(delItemInst, "DELETE FROM `item_instance` WHERE `guid` = ?");
        stmt.PExecute(item->GetGUIDLow());

        m_inventory.Own(i)->FSetState(ITEM_NEW);
    }

    m_inventory.SettleClocks();

    if (m_inventory.Saves().IsEmpty())
    {
        return;
    }

    bool error = false;
    for (Item* item : m_inventory.Saves().Waiting())
    {
        if (!item || item->GetState() == ITEM_REMOVED)
        {
            continue;
        }
        Item* test = GetItemByPos(item->GetBagSlot(), item->GetSlot());

        if (test == nullptr)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the player doesn't have an item at that position!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow());
            error = true;
        }
        else if (test != item)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the item with guid %d is there instead!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow(), test->GetGUIDLow());
            error = true;
        }
    }

    if (error)
    {
        sLog.outError("Player::_SaveInventory - one or more errors occurred save aborted!");
        ChatHandler(this).SendSysMessage(LANG_ITEM_SAVE_FAILED);
        return;
    }

    static SqlStatementID insertInventory ;
    static SqlStatementID updateInventory ;
    static SqlStatementID deleteInventory ;

    for (Item* item : m_inventory.Saves().Waiting())
    {
        if (!item)
        {
            continue;
        }

        Bag* container = item->GetContainer();
        uint32 bag_guid = container ? container->GetGUIDLow() : 0;

        switch (item->GetState())
        {
            case ITEM_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertInventory, "INSERT INTO `character_inventory` (`guid`,`bag`,`slot`,`item`,`item_template`) VALUES (?, ?, ?, ?, ?)");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(bag_guid);
                stmt.addUInt8(item->GetSlot());
                stmt.addUInt32(item->GetGUIDLow());
                stmt.addUInt32(item->GetEntry());
                stmt.Execute();
            }
            break;
            case ITEM_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateInventory, "UPDATE `character_inventory` SET `guid` = ?, `bag` = ?, `slot` = ?, `item_template` = ? WHERE `item` = ?");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(bag_guid);
                stmt.addUInt8(item->GetSlot());
                stmt.addUInt32(item->GetEntry());
                stmt.addUInt32(item->GetGUIDLow());
                stmt.Execute();
            }
            break;
            case ITEM_REMOVED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteInventory, "DELETE FROM `character_inventory` WHERE `item` = ?");
                stmt.PExecute(item->GetGUIDLow());
            }
            break;
            case ITEM_UNCHANGED:
                break;
        }

        item->SaveToDB();
    }
    m_inventory.Saves().Clear();
}

void Player::_SaveQuestStatus()
{
    static SqlStatementID insertQuestStatus ;

    static SqlStatementID updateQuestStatus ;

    for (auto i = m_journal.All().begin(); i != m_journal.All().end(); ++i)
    {
        QuestStatusData &questStatus = i->second;
        switch (questStatus.uState)
        {
            case QUEST_NEW :
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertQuestStatus, "INSERT INTO `character_queststatus` (`guid`,`quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4`) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(i->first);
                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + sWorld.GetGameTime()));
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_creatureOrGOcount[k]);
                }
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_itemcount[k]);
                }
                stmt.Execute();
            }
            break;
            case QUEST_CHANGED :
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateQuestStatus, "UPDATE `character_queststatus` SET `status` = ?,`rewarded` = ?,`explored` = ?,`timer` = ?,"
                    "`mobcount1` = ?,`mobcount2` = ?,`mobcount3` = ?,`mobcount4` = ?,`itemcount1` = ?,`itemcount2` = ?,`itemcount3` = ?,`itemcount4` = ?  WHERE `guid` = ? AND `quest` = ?");

                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + sWorld.GetGameTime()));
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_creatureOrGOcount[k]);
                }
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_itemcount[k]);
                }
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(i->first);
                stmt.Execute();
            }
            break;
            case QUEST_UNCHANGED:
                break;
        };
        questStatus.uState = QUEST_UNCHANGED;
    }
}

void Player::_SaveSkills()
{
    static SqlStatementID delSkills ;
    static SqlStatementID insSkills ;
    static SqlStatementID updSkills ;

    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end();)
    {
        if (itr->second.uState == SKILL_UNCHANGED)
        {
            ++itr;
            continue;
        }

        if (itr->second.uState == SKILL_DELETED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(delSkills, "DELETE FROM `character_skills` WHERE `guid` = ? AND `skill` = ?");
            stmt.PExecute(GetGUIDLow(), itr->first);
            mSkillStatus.erase(itr++);
            continue;
        }

        uint32 valueData = GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos));
        uint16 value = SKILL_VALUE(valueData);
        uint16 max = SKILL_MAX(valueData);

        switch (itr->second.uState)
        {
            case SKILL_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insSkills, "INSERT INTO `character_skills` (`guid`, `skill`, `value`, `max`) VALUES (?, ?, ?, ?)");
                stmt.PExecute(GetGUIDLow(), itr->first, value, max);
            }
            break;
            case SKILL_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updSkills, "UPDATE `character_skills` SET `value` = ?, `max` = ? WHERE `guid` = ? AND `skill` = ?");
                stmt.PExecute(value, max, GetGUIDLow(), itr->first);
            }
            break;
            case SKILL_UNCHANGED:
            case SKILL_DELETED:
                MANGOS_ASSERT(false);
                break;
        };
        itr->second.uState = SKILL_UNCHANGED;

        ++itr;
    }
}

void Player::_SaveSpells()
{
    static SqlStatementID delSpells ;
    static SqlStatementID insSpells ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM `character_spell` WHERE `guid` = ? and `spell` = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insSpells, "INSERT INTO `character_spell` (`guid`,`spell`,`active`,`disabled`) VALUES (?, ?, ?, ?)");

    for (PlayerSpellMap::iterator itr = m_spells.begin(), next = m_spells.begin(); itr != m_spells.end();)
    {
        PlayerSpell& playerSpell = itr->second;

        if (playerSpell.state == PLAYERSPELL_REMOVED || playerSpell.state == PLAYERSPELL_CHANGED)
        {
            stmtDel.PExecute(GetGUIDLow(), itr->first);
        }

        if (!playerSpell.dependent && (playerSpell.state == PLAYERSPELL_NEW || playerSpell.state == PLAYERSPELL_CHANGED))
        {
            stmtIns.PExecute(GetGUIDLow(), itr->first, uint8(playerSpell.active ? 1 : 0), uint8(playerSpell.disabled ? 1 : 0));
        }

        if (playerSpell.state == PLAYERSPELL_REMOVED)
        {
            m_spells.erase(itr++);
        }
        else
        {
            playerSpell.state = PLAYERSPELL_UNCHANGED;
            ++itr;
        }
    }
}

void Player::_SaveStats()
{

    if (!sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE) || getLevel() < sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE))
    {
        return;
    }

    static SqlStatementID delStats ;
    static SqlStatementID insertStats ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delStats, "DELETE FROM `character_stats` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());

    stmt = CharacterDatabase.CreateStatement(insertStats, "INSERT INTO `character_stats` (`guid`, `maxhealth`, `maxpower1`, `maxpower2`, `maxpower3`, `maxpower4`, `maxpower5`, "
        "`strength`, `agility`, `stamina`, `intellect`, `spirit`, `armor`, `resHoly`, `resFire`, `resNature`, `resFrost`, `resShadow`, `resArcane`, "
        "`blockPct`, `dodgePct`, `parryPct`, `critPct`, `rangedCritPct`, `attackPower`, `rangedAttackPower`) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    stmt.addUInt32(GetGUIDLow());
    stmt.addUInt32(GetMaxHealth());
    for (int i = 0; i < MAX_POWERS; ++i)
    {
        stmt.addUInt32(GetMaxPower(Powers(i)));
    }
    for (int i = 0; i < MAX_STATS; ++i)
    {
        stmt.addFloat(GetStat(Stats(i)));
    }

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        stmt.addUInt32(GetResistance(SpellSchools(i)));
    }
    stmt.addFloat(GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_DODGE_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_PARRY_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_CRIT_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_ATTACK_POWER));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER));

    stmt.Execute();
}

void Player::outDebugStatsValues() const
{

    if (!sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG) || sLog.HasLogFilter(LOG_FILTER_PLAYER_STATS))
    {
        return;
    }

    sLog.outDebug("HP is: \t\t\t%u\t\tMP is: \t\t\t%u", GetMaxHealth(), GetMaxPower(POWER_MANA));
    sLog.outDebug("AGILITY is: \t\t%f\t\tSTRENGTH is: \t\t%f", GetStat(STAT_AGILITY), GetStat(STAT_STRENGTH));
    sLog.outDebug("INTELLECT is: \t\t%f\t\tSPIRIT is: \t\t%f", GetStat(STAT_INTELLECT), GetStat(STAT_SPIRIT));
    sLog.outDebug("STAMINA is: \t\t%f", GetStat(STAT_STAMINA));
    sLog.outDebug("Armor is: \t\t%u\t\tBlock is: \t\t%f", GetArmor(), GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    sLog.outDebug("HolyRes is: \t\t%u\t\tFireRes is: \t\t%u", GetResistance(SPELL_SCHOOL_HOLY), GetResistance(SPELL_SCHOOL_FIRE));
    sLog.outDebug("NatureRes is: \t\t%u\t\tFrostRes is: \t\t%u", GetResistance(SPELL_SCHOOL_NATURE), GetResistance(SPELL_SCHOOL_FROST));
    sLog.outDebug("ShadowRes is: \t\t%u\t\tArcaneRes is: \t\t%u", GetResistance(SPELL_SCHOOL_SHADOW), GetResistance(SPELL_SCHOOL_ARCANE));
    sLog.outDebug("MIN_DAMAGE is: \t\t%f\tMAX_DAMAGE is: \t\t%f", GetShownDamage(false, false), GetShownDamage(false, true));
    sLog.outDebug("MIN_OFFHAND_DAMAGE is: \t%f\tMAX_OFFHAND_DAMAGE is: \t%f", GetShownDamage(true, false), GetShownDamage(true, true));
    sLog.outDebug("MIN_RANGED_DAMAGE is: \t%f\tMAX_RANGED_DAMAGE is: \t%f", GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
    sLog.outDebug("ATTACK_TIME is: \t%u\t\tRANGE_ATTACK_TIME is: \t%u", GetAttackTime(BASE_ATTACK), GetAttackTime(RANGED_ATTACK));
}

void Player::UpdateSpeakTime()
{

    if (GetSession()->GetSecurity() > SEC_PLAYER)
    {
        return;
    }

    time_t current = time(nullptr);
    if (m_speakTime > current)
    {
        uint32 max_count = sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT);
        if (!max_count)
        {
            return;
        }

        ++m_speakCount;
        if (m_speakCount >= max_count)
        {

            time_t new_mute = current + sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MUTE_TIME);
            if (GetSession()->m_muteTime < new_mute)
            {
                GetSession()->m_muteTime = new_mute;
            }

            m_speakCount = 0;
        }
    }
    else
    {
        m_speakCount = 0;
    }

    m_speakTime = current + sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY);
}

bool Player::CanSpeak() const
{
    return  GetSession()->m_muteTime <= time(nullptr);
}

void Player::SendAttackSwingNotInRange()
{
    WorldPacket data(SMSG_ATTACKSWING_NOTINRANGE, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendAttackSwingDeadTarget()
{
    WorldPacket data(SMSG_ATTACKSWING_DEADTARGET, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendAttackSwingCantAttack()
{
    WorldPacket data(SMSG_ATTACKSWING_CANT_ATTACK, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendAttackSwingCancelAttack()
{
    WorldPacket data(SMSG_CANCEL_COMBAT, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendAttackSwingBadFacingAttack()
{
    WorldPacket data(SMSG_ATTACKSWING_BADFACING, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendAutoRepeatCancel()
{
    WorldPacket data(SMSG_CANCEL_AUTO_REPEAT, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendExplorationExperience(uint32 Area, uint32 Experience)
{
    WorldPacket data(SMSG_EXPLORATION_EXPERIENCE, 8);
    data << uint32(Area);
    data << uint32(Experience);
    GetSession()->SendPacket(&data);
}

void Player::SendResetFailedNotify(uint32 mapid)
{
    WorldPacket data(SMSG_RESET_FAILED_NOTIFY, 4);
    data << uint32(mapid);
    GetSession()->SendPacket(&data);
}

void Player::SendResetInstanceSuccess(uint32 MapId)
{
    WorldPacket data(SMSG_INSTANCE_RESET, 4);
    data << uint32(MapId);
    GetSession()->SendPacket(&data);
}

void Player::SendResetInstanceFailed(uint32 reason, uint32 MapId)
{

    WorldPacket data(SMSG_INSTANCE_RESET_FAILED, 8);
    data << uint32(reason);
    data << uint32(MapId);
    GetSession()->SendPacket(&data);
}

void Player::UpdateContestedPvP(uint32 diff)
{
    if (!m_contestedPvPTimer || IsInCombat())
    {
        return;
    }
    if (m_contestedPvPTimer <= diff)
    {
        ResetContestedPvP();
    }
    else
    {
        m_contestedPvPTimer -= diff;
    }
}

void Player::UpdatePvPFlag(time_t currTime)
{
    if (!IsPvP())
    {
        return;
    }
    if (pvpInfo.endTimer == 0 || currTime < (pvpInfo.endTimer + 300))
    {
        return;
    }

    UpdatePvP(false);
}

void Player::Say(const std::string& text, const uint32 language)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_SAY, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    Deliver(Audience::Within(*this, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY)).AndSubject(), &data);
}

void Player::Yell(const std::string& text, const uint32 language)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_YELL, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    Deliver(Audience::Within(*this, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL)).AndSubject(), &data);
}

void Player::TextEmote(const std::string& text)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_EMOTE, text.c_str(), LANG_UNIVERSAL, GetChatTag(), GetObjectGuid(), GetName());
    Deliver(Audience::Within(*this, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE)).AndSubject().OwnTeamOnly(!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT)), &data);
}

void Player::LogWhisper(const std::string& text, ObjectGuid receiver)
{
    WhisperLoggingLevels loggingLevel = WhisperLoggingLevels(sWorld.getConfig(CONFIG_UINT32_LOG_WHISPERS));

    if (loggingLevel == WHISPER_LOGGING_NONE)
    {
        return;
    }

    GMTicket* ticket = sTicketMgr.GetGMTicket(GetObjectGuid());
    if (!ticket)
    {
        ticket = sTicketMgr.GetGMTicket(receiver);
    }

    uint32 ticketId = 0;
    if (ticket)
    {
        ticketId = ticket->GetId();
    }

    bool isSomeoneGM = false;

    if (GetSession()->GetSecurity() >= SEC_GAMEMASTER)
    {
        isSomeoneGM = true;
    }
    else
    {
        Player* pRecvPlayer = sObjectMgr.GetPlayer(receiver);
        if (pRecvPlayer && pRecvPlayer->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
        {
            isSomeoneGM = true;
        }
    }

    if ((loggingLevel == WHISPER_LOGGING_TICKETS && ticket && isSomeoneGM) ||
        loggingLevel == WHISPER_LOGGING_EVERYTHING)
    {
        static SqlStatementID wlog;
        SqlStatement stmt = CharacterDatabase.CreateStatement(wlog, "INSERT INTO `character_whispers` (`to_guid`, `from_guid`, `message`, `regarding_ticket_id`) VALUES (?, ?, ?, ?)");
        stmt.addUInt32(GuidCounter(receiver));
        stmt.addUInt32(GuidCounter(GetObjectGuid()));
        stmt.addString(text.c_str());
        stmt.addUInt32(ticketId);
        stmt.Execute();
    }
}

void Player::Whisper(const std::string& text, uint32 language, ObjectGuid receiver)
{
    if (language != LANG_ADDON)
    {
        language = LANG_UNIVERSAL;
    }

    Player* rPlayer = sObjectMgr.GetPlayer(receiver);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    rPlayer->GetSession()->SendPacket(&data);

    if (language != LANG_ADDON)
    {
        data.clear();
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER_INFORM, text.c_str(), Language(language), CHAT_TAG_NONE, rPlayer->GetObjectGuid());
        LogWhisper(text, receiver);
        GetSession()->SendPacket(&data);
    }

    if (!isAcceptWhispers())
    {
        SetAcceptWhispers(true);
        ChatHandler(this).SendSysMessage(LANG_COMMAND_WHISPERON);
    }

    if (rPlayer->isAFK())
    {

        ChatHandler(this).PSendSysMessage(LANG_PLAYER_AFK, rPlayer->GetName(), rPlayer->autoReplyMsg.c_str());
    }
    else if (rPlayer->isDND())
    {

        ChatHandler(this).PSendSysMessage(LANG_PLAYER_DND, rPlayer->GetName(), rPlayer->autoReplyMsg.c_str());
    }
}

void Player::PetSpellInitialize()
{
    Pet* pet = GetPet();

    if (!pet)
    {
        return;
    }

    DEBUG_LOG("Pet Spells Groups");

    CharmInfo* charmInfo = pet->GetCharmInfo();

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 1 + 1 + 2 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << pet->GetObjectGuid();
    data << uint32(0);
    data << uint8(charmInfo->GetReactState()) << uint8(charmInfo->GetCommandState()) << uint16(0);

    charmInfo->BuildActionBar(&data);

    size_t spellsCountPos = data.wpos();

    uint8 addlist = 0;
    data << uint8(addlist);

    if (pet->IsPermanentPetFor(this))
    {

        for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
            {
                continue;
            }

            data << uint32(MAKE_UNIT_ACTION_BUTTON(itr->first, itr->second.active));
            ++addlist;
        }
    }

    data.put<uint8>(spellsCountPos, addlist);

    uint8 cooldownsCount = uint8(pet->Knowing().StillDown().size() + pet->Knowing().CategoriesUsed().size());
    data << uint8(cooldownsCount);

    time_t curTime = time(nullptr);

    for (auto const& down : pet->Knowing().StillDown())
    {
        time_t const left = down.second > curTime ? (down.second - curTime) * IN_MILLISECONDS : 0;

        data << uint16(down.first);
        data << uint16(0);
        data << uint32(left);
        data << uint32(0);
    }

    for (auto const& held : pet->Knowing().CategoriesUsed())
    {
        time_t const left = held.second > curTime ? (held.second - curTime) * IN_MILLISECONDS : 0;

        data << uint16(held.first);
        data << uint16(0);
        data << uint32(0);
        data << uint32(left);
    }

    GetSession()->SendPacket(&data);
}

void Player::PossessSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
    {
        return;
    }

    CharmInfo* charmInfo = charm->GetCharmInfo();

    if (!charmInfo)
    {
        sLog.outError("Player::PossessSpellInitialize(): charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 4 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << charm->GetObjectGuid();
    data << uint32(0);
    data << uint32(0);

    charmInfo->BuildActionBar(&data);

    data << uint8(0);
    data << uint8(0);

    GetSession()->SendPacket(&data);
}

void Player::CharmSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
    {
        return;
    }

    CharmInfo* charmInfo = charm->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("Player::CharmSpellInitialize(): the player's charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    uint8 addlist = 0;

    if (!IsPlayer(charm))
    {
        CreatureInfo const* cinfo = ((Creature*)charm)->GetCreatureInfo();

        if (cinfo && cinfo->CreatureType == CREATURE_TYPE_DEMON && getClass() == CLASS_WARLOCK)
        {
            for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
            {
                if (charmInfo->GetCharmSpell(i)->GetAction())
                {
                    ++addlist;
                }
            }
        }
    }

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 1 + 1 + 2 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 4 * addlist + 1);
    data << charm->GetObjectGuid();
    data << uint32(0x00000000);

    if (!IsPlayer(charm))
    {
        data << uint8(charmInfo->GetReactState()) << uint8(charmInfo->GetCommandState()) << uint16(0);
    }
    else
    {
        data << uint8(0) << uint8(0) << uint16(0);
    }

    charmInfo->BuildActionBar(&data);

    data << uint8(addlist);

    if (addlist)
    {
        for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            CharmSpellEntry* cspell = charmInfo->GetCharmSpell(i);
            if (cspell->GetAction())
            {
                data << uint32(cspell->packedData);
            }
        }
    }

    data << uint8(0);

    GetSession()->SendPacket(&data);
}

void Player::SendProficiency(ItemClass itemClass, uint32 itemSubclassMask)
{
    WorldPacket data(SMSG_SET_PROFICIENCY, 1 + 4);
    data << uint8(itemClass) << uint32(itemSubclassMask);
    GetSession()->SendPacket(&data);
}

void Player::RemovePetitionsAndSigns(ObjectGuid guid)
{
    uint32 lowguid = GuidCounter(guid);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '%u'", lowguid);
    if (result)
    {
        do
        {

            Field* fields = result->Fetch();
            ObjectGuid ownerguid   = MakeGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            ObjectGuid petitionguid = MakeGuid(HIGHGUID_ITEM, fields[1].GetUInt32());

            Player* owner = sObjectMgr.GetPlayer(ownerguid);
            if (owner)
            {
                owner->GetSession()->SendPetitionQueryOpcode(petitionguid);
            }
        }
        while (result->NextRow());

        delete result;

        CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `playerguid` = '%u'", lowguid);
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `ownerguid` = '%u'", lowguid);
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `ownerguid` = '%u'", lowguid);
    CharacterDatabase.CommitTransaction();
}

void Player::HandleStealthedUnitsDetection()
{
    std::list<Unit*> stealthedUnits;

    MaNGOS::AnyStealthedCheck u_check(this);
    MaNGOS::UnitListSearcher<MaNGOS::AnyStealthedCheck > searcher(stealthedUnits, u_check);
    Cell::VisitAllObjects(this, searcher, MAX_PLAYER_STEALTH_DETECT_RANGE);

    Occupant const* viewPoint = GetCamera().GetBody();

    for (std::list<Unit*>::const_iterator i = stealthedUnits.begin(); i != stealthedUnits.end(); ++i)
    {
        if ((*i) == this)
        {
            continue;
        }

        bool hasAtClient = HaveAtClient((*i));
        bool hasDetected = (*i)->IsVisibleForOrDetect(this, viewPoint, true);

        if (hasDetected)
        {
            if (!hasAtClient)
            {
                ObjectGuid i_guid = (*i)->GetObjectGuid();
                (*i)->SendCreateUpdateToPlayer(this);
                m_clientGUIDs.insert(i_guid);

                DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(): %s is detected in stealth by player %u. Distance = %f", GuidString(i_guid).c_str(), GetGUIDLow(), Where().DistanceTo((*i)->Where()));

                if ((*i) != this && IsType(*i, TYPEMASK_UNIT))
                {
                    SendAuraDurationsForTarget(*i);
                }
            }
        }
        else
        {
            if (hasAtClient)
            {
                (*i)->DestroyForPlayer(this);
                m_clientGUIDs.erase((*i)->GetObjectGuid());
            }
        }
    }
}

bool Player::ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc , uint32 spellid )
{
    if (nodes.size() < 2)
    {
        return false;
    }

    if (GetSession()->isLogingOut() || IsInCombat())
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
        return false;
    }

    if (HasUnitFlag(UNIT_FLAG_CLIENT_CONTROL_LOST))
    {
        return false;
    }

    if (npc)
    {

        if (IsMounted())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERALREADYMOUNTED);
            return false;
        }

        if (IsInDisallowedMountForm())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERSHAPESHIFTED);
            return false;
        }

        if (IsNonMeleeSpellCasted(false))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
            return false;
        }
    }

    else
    {
        RemoveAurasOfType(SPELL_AURA_MOUNTED);

        if (IsInDisallowedMountForm())
        {
            RemoveAurasOfType(SPELL_AURA_MOD_SHAPESHIFT);
        }

        if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
        {
            if (spell->m_spellInfo->ID != spellid)
            {
                InterruptSpell(CURRENT_GENERIC_SPELL, false);
            }
        }

        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, false);

        if (Spell* spell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            if (spell->m_spellInfo->ID != spellid)
            {
                InterruptSpell(CURRENT_CHANNELED_SPELL, true);
            }
        }
    }

    uint32 sourcenode = nodes[0];

    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(sourcenode);
    if (!node)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
        return false;
    }

    if (node->x != 0.0f || node->y != 0.0f || node->z != 0.0f)
    {
        if (node->map_id != GetMapId() ||
            (node->x - Where().X()) * (node->x - Where().X()) +
            (node->y - Where().Y()) * (node->y - Where().Y()) +
            (node->z - Where().Z()) * (node->z - Where().Z()) >
            (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXITOOFARAWAY);
            return false;
        }
    }

    else if (npc)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);
        return false;
    }

    CombatStop();

    TradeCancel(true);

    m_taxi.ClearTaxiDestinations();

    m_taxi.AddTaxiDestination(sourcenode);

    uint32 sourcepath = 0;
    uint32 totalcost = 0;

    uint32 prevnode = sourcenode;

    for (uint32 i = 1; i < nodes.size(); ++i)
    {
        uint32 path, cost;

        uint32 lastnode = nodes[i];
        sObjectMgr.GetTaxiPath(prevnode, lastnode, path, cost);

        if (!path)
        {
            m_taxi.ClearTaxiDestinations();
            return false;
        }

        totalcost += cost;

        if (prevnode == sourcenode)
        {
            sourcepath = path;
        }

        m_taxi.AddTaxiDestination(lastnode);

        prevnode = lastnode;
    }

    uint32 mount_display_id = sObjectMgr.GetTaxiMountDisplayId(sourcenode, GetTeam(), npc == nullptr);

    if ((mount_display_id == 0 && spellid == 0) || sourcepath == 0)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);

        m_taxi.ClearTaxiDestinations();
        return false;
    }

    uint32 money = GetMoney();

    if (npc)
    {
        totalcost = (uint32)ceil(totalcost * GetReputationPriceDiscount(npc));
    }

    if (money < totalcost)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOTENOUGHMONEY);

        m_taxi.ClearTaxiDestinations();
        return false;
    }

    ModifyMoney(-(int32)totalcost);

    RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);

    if (sWorld.getConfig(CONFIG_BOOL_INSTANT_TAXI))
    {
        TaxiNodesEntry const* lastnode = sTaxiNodesStore.LookupEntry(nodes[nodes.size() - 1]);
        m_taxi.ClearTaxiDestinations();
        TeleportTo(lastnode->map_id, lastnode->x, lastnode->y, lastnode->z, Where().Facing());
        return false;
    }
    else
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIOK);
        GetSession()->SendDoFlight(mount_display_id, sourcepath);
    }

    return true;
}

bool Player::ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid )
{
    TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(taxi_path_id);
    if (!entry)
    {
        return false;
    }

    std::vector<uint32> nodes;

    nodes.resize(2);
    nodes[0] = entry->from;
    nodes[1] = entry->to;

    return ActivateTaxiPathTo(nodes, nullptr, spellid);
}

void Player::ContinueTaxiFlight()
{
    uint32 sourceNode = m_taxi.GetTaxiSource();
    if (!sourceNode)
    {
        return;
    }

    DEBUG_LOG("WORLD: Restart character %u taxi flight", GetGUIDLow());

    uint32 mountDisplayId = sObjectMgr.GetTaxiMountDisplayId(sourceNode, GetTeam(), true);
    uint32 path = m_taxi.GetCurrentTaxiPath();

    uint32 startNode = 0;

    TaxiPathNodeList const& nodeList = sTaxiPathNodesByPath[path];

    float distNext =
        (nodeList[0].LocX - Where().X()) * (nodeList[0].LocX - Where().X()) +
        (nodeList[0].LocY - Where().Y()) * (nodeList[0].LocY - Where().Y()) +
        (nodeList[0].LocZ - Where().Z()) * (nodeList[0].LocZ - Where().Z());

    for (uint32 i = 1; i < nodeList.size(); ++i)
    {
        TaxiPathNodeEntry const& node = nodeList[i];
        TaxiPathNodeEntry const& prevNode = nodeList[i - 1];

        if (node.ContinentID != GetMapId())
        {
            continue;
        }

        float distPrev = distNext;

        distNext =
            (node.LocX - Where().X()) * (node.LocX - Where().X()) +
            (node.LocY - Where().Y()) * (node.LocY - Where().Y()) +
            (node.LocZ - Where().Z()) * (node.LocZ - Where().Z());

        float distNodes =
            (node.LocX - prevNode.LocX) * (node.LocX - prevNode.LocX) +
            (node.LocY - prevNode.LocY) * (node.LocY - prevNode.LocY) +
            (node.LocZ - prevNode.LocZ) * (node.LocZ - prevNode.LocZ);

        if (distNext + distPrev < distNodes)
        {
            startNode = i;
            break;
        }
    }

    GetSession()->SendDoFlight(mountDisplayId, path, startNode);
}

void Player::_SaveBGData()
{

    if (!Battle().Unsaved())
    {
        return;
    }

    static SqlStatementID delBGData ;
    static SqlStatementID insBGData ;

    SqlStatement stmt =  CharacterDatabase.CreateStatement(delBGData, "DELETE FROM `character_battleground_data` WHERE `guid` = ?");

    stmt.PExecute(GetGUIDLow());

    if (Battle().InOne())
    {
        stmt = CharacterDatabase.CreateStatement(insBGData, "INSERT INTO `character_battleground_data` VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.addUInt32(GetGUIDLow());
        stmt.addUInt32(Battle().Id());
        stmt.addUInt32(uint32(Battle().SideAsSet()));
        stmt.addFloat(Battle().CameFrom().X());
        stmt.addFloat(Battle().CameFrom().Y());
        stmt.addFloat(Battle().CameFrom().Z());
        stmt.addFloat(Battle().CameFrom().Facing());
        stmt.addUInt32(Battle().CameFrom().MapId());

        stmt.Execute();
    }

    Battle().Saved();
}
