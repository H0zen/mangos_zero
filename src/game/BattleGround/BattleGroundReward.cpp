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

#include <string>
#include "BattleGround.h"
#include "Object.h"
#include "Player.h"
#include "BattleGroundMgr.h"
#include "Creature.h"
#include "Language.h"
#include "SpellAuras.h"
#include "World.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Mail.h"
#include "WorldPacket.h"
#include "Util.h"
#include "Formulas.h"
#include "GridNotifiersImpl.h"
#include "Chat.h"

void BattleGround::RewardHonorToTeam(uint32 Honor, Team teamId)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);

        if (!plr)
        {
            sLog.outError("BattleGround:RewardHonorToTeam: %s not found!", GuidString(itr->first).c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            UpdatePlayerScore(plr, SCORE_BONUS_HONOR, Honor);
        }
    }
}

void BattleGround::RewardReputationToTeam(uint32 faction_id, uint32 Reputation, Team teamId)
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

    if (!factionEntry)
    {
        return;
    }

    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);

        if (!plr)
        {
            sLog.outError("BattleGround:RewardReputationToTeam: %s not found!", GuidString(itr->first).c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            plr->GetReputationMgr().ModifyReputation(factionEntry, Reputation);
        }
    }
}

void BattleGround::UpdateWorldState(uint32 Field, uint32 Value)
{
    WorldPacket data;
    sBattleGroundMgr.BuildUpdateWorldStatePacket(&data, Field, Value);
    SendPacketToAll(&data);
}

void BattleGround::UpdateWorldStateForPlayer(uint32 Field, uint32 Value, Player* Source)
{
    WorldPacket data;
    sBattleGroundMgr.BuildUpdateWorldStatePacket(&data, Field, Value);
    Source->GetSession()->SendPacket(&data);
}

void BattleGround::EndBattleGround(Team winner)
{
    this->RemoveFromBGFreeSlotQueue();

    uint32 winner_rating = 0;
    WorldPacket data;
    int32 winmsg_id = 0;

    uint32 bgScoresWinner = TEAM_INDEX_NEUTRAL;
    uint64 battleground_id = 1;

    if (winner == ALLIANCE)
    {
        winmsg_id = LANG_BG_A_WINS;
        PlaySoundToAll(SOUND_ALLIANCE_WINS);

        bgScoresWinner = TEAM_INDEX_HORDE;
    }
    else if (winner == HORDE)
    {
        winmsg_id = LANG_BG_H_WINS;
        PlaySoundToAll(SOUND_HORDE_WINS);

        bgScoresWinner = TEAM_INDEX_ALLIANCE;
    }

    if (sWorld.getConfig(CONFIG_BOOL_BATTLEGROUND_SCORE_STATISTICS))
    {
        static SqlStatementID insPvPstatsBattleground;
        QueryResult* result;

        SqlStatement stmt = CharacterDatabase.CreateStatement(insPvPstatsBattleground, "INSERT INTO `pvpstats_battlegrounds` (`id`, `winner_team`, `bracket_id`, `type`, `date`) VALUES (?, ?, ?, ?, NOW())");

        uint8 battleground_bracket = GetMinLevel() / 10;
        uint8 battleground_type = (uint8)GetTypeID();

        result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `pvpstats_battlegrounds`");
        if (result)
        {
            Field* fields = result->Fetch();
            battleground_id = fields[0].GetUInt64() + 1;
            delete result;
        }

        stmt.PExecute(battleground_id, bgScoresWinner, battleground_bracket, battleground_type);
    }

    SetWinner(winner);

    SetStatus(STATUS_WAIT_LEAVE);

    m_EndTime = TIME_TO_AUTOREMOVE;

    for (BattleGroundPlayerMap::iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        Team team = itr->second.PlayerTeam;

        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);
        if (!plr)
        {
            sLog.outError("BattleGround:EndBattleGround %s not found!", GuidString(itr->first).c_str());
            continue;
        }

        if (plr->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            plr->RemoveAurasOfType(SPELL_AURA_MOD_SHAPESHIFT);
        }

        if (!plr->IsAlive())
        {
            plr->ResurrectPlayer(1.0f);
            plr->SpawnCorpseBones();
        }
        else
        {

            plr->CombatStop();
            plr->GetHostileRefManager().deleteReferences();
        }

        if (sWorld.getConfig(CONFIG_BOOL_BATTLEGROUND_SCORE_STATISTICS))
        {
            static SqlStatementID insPvPstatsPlayer;
            BattleGroundScoreMap::iterator score = m_PlayerScores.find(itr->first);
            SqlStatement stmt = CharacterDatabase.CreateStatement(insPvPstatsPlayer, "INSERT INTO `pvpstats_players` (`battleground_id`, `character_guid`, `score_killing_blows`, `score_deaths`, `score_honorable_kills`, `score_bonus_honor`, `score_damage_done`, `score_healing_done`, `attr_1`, `attr_2`, `attr_3`, `attr_4`, `attr_5`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

            stmt.addUInt32(battleground_id);
            stmt.addUInt32(plr->GetGUIDLow());
            stmt.addUInt32(score->second->GetKillingBlows());
            stmt.addUInt32(score->second->GetDeaths());
            stmt.addUInt32(score->second->GetHonorableKills());
            stmt.addUInt32(score->second->GetBonusHonor());
            stmt.addUInt32(score->second->GetDamageDone());
            stmt.addUInt32(score->second->GetHealingDone());
            stmt.addUInt32(score->second->GetAttr1());
            stmt.addUInt32(score->second->GetAttr2());
            stmt.addUInt32(score->second->GetAttr3());
            stmt.addUInt32(score->second->GetAttr4());
            stmt.addUInt32(score->second->GetAttr5());

            stmt.Execute();
        }

        if (team == winner)
        {
            RewardMark(plr, ITEM_WINNER_COUNT);
            RewardQuestComplete(plr);
        }
        else
        {
            RewardMark(plr, ITEM_LOSER_COUNT);
        }

        plr->CombatStopWithPets(true);

        BlockMovement(plr);

        sBattleGroundMgr.BuildPvpLogDataPacket(&data, this);
        plr->GetSession()->SendPacket(&data);

        BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(GetTypeID());
        sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, this, plr->Queues().SlotOf(bgQueueTypeId), STATUS_IN_PROGRESS, TIME_TO_AUTOREMOVE, GetStartTime());
        plr->GetSession()->SendPacket(&data);
    }

    if (winmsg_id)
    {
        SendMessageToAll(winmsg_id, CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }
}

uint32 BattleGround::GetBonusHonorFromKill(uint32 kills) const
{

    return (uint32)MaNGOS::Honor::hk_honor_at_level(GetMaxLevel(), kills);
}

uint32 BattleGround::GetBattlemasterEntry() const
{
    switch (GetTypeID())
    {
        case BATTLEGROUND_AV: return 15972;
        case BATTLEGROUND_WS: return 14623;
        case BATTLEGROUND_AB: return 14879;
        default:              return 0;
    }
}

void BattleGround::RewardMark(Player* plr, uint32 count)
{
    switch (GetTypeID())
    {
        case BATTLEGROUND_AV:
            if (count == ITEM_WINNER_COUNT)
            {
                RewardSpellCast(plr, SPELL_AV_MARK_WINNER);
            }
            else
            {
                RewardSpellCast(plr, SPELL_AV_MARK_LOSER);
            }
            break;
        case BATTLEGROUND_WS:
            if (count == ITEM_WINNER_COUNT)
            {
                RewardSpellCast(plr, SPELL_WS_MARK_WINNER);
            }
            else
            {
                RewardSpellCast(plr, SPELL_WS_MARK_LOSER);
            }
            break;
        case BATTLEGROUND_AB:
            if (count == ITEM_WINNER_COUNT)
            {
                RewardSpellCast(plr, SPELL_AB_MARK_WINNER);
            }
            else
            {
                RewardSpellCast(plr, SPELL_AB_MARK_LOSER);
            }
            break;
        default:
            break;
    }
}

void BattleGround::RewardSpellCast(Player* plr, uint32 spell_id)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        sLog.outError("Battleground reward casting spell %u not exist.", spell_id);
        return;
    }

    plr->CastSpell(plr, spellInfo, true);
}

void BattleGround::RewardItem(Player* plr, uint32 item_id, uint32 count)
{
    ItemPosCountVec dest;
    uint32 no_space_count = 0;
    uint8 msg = plr->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item_id, count, &no_space_count);

    if (msg == EQUIP_ERR_ITEM_NOT_FOUND)
    {
        sLog.outErrorDb("Battleground reward item (Entry %u) not exist in `item_template`.", item_id);
        return;
    }

    if (msg != EQUIP_ERR_OK)
    {
        count -= no_space_count;
    }

    if (count != 0 && !dest.empty())
    {
        if (Item* item = plr->StoreNewItem(dest, item_id, true, 0))
        {
            plr->SendNewItem(item, count, true, false);
        }
    }

    if (no_space_count > 0)
    {
        SendRewardMarkByMail(plr, item_id, no_space_count);
    }
}

void BattleGround::SendRewardMarkByMail(Player* plr, uint32 mark, uint32 count) const
{
    uint32 bmEntry = GetBattlemasterEntry();
    if (!bmEntry)
    {
        return;
    }

    ItemPrototype const* markProto = ObjectMgr::GetItemPrototype(mark);
    if (!markProto)
    {
        return;
    }

    if (Item* markItem = Item::CreateItem(mark, count, plr))
    {

        markItem->SaveToDB();

        int loc_idx = plr->GetSession()->GetSessionDbLocaleIndex();

        std::string subject = markProto->Name1;
        sObjectMgr.GetItemLocaleStrings(markProto->ItemId, loc_idx, &subject);

        std::string textFormat = plr->GetSession()->GetMangosString(LANG_BG_MARK_BY_MAIL);
        char textBuf[300];
        snprintf(textBuf, 300, textFormat.c_str(), GetName(), GetName());

        MailDraft(subject, textBuf)
            .AddItem(markItem)
            .SendMailTo(plr, MailSender(MAIL_CREATURE, bmEntry));
    }
}

void BattleGround::RewardQuestComplete(Player* plr)
{
    uint32 quest;
    switch (GetTypeID())
    {
        case BATTLEGROUND_AV:
            quest = SPELL_AV_QUEST_REWARD;
            break;
        case BATTLEGROUND_WS:
            quest = SPELL_WS_QUEST_REWARD;
            break;
        case BATTLEGROUND_AB:
            quest = SPELL_AB_QUEST_REWARD;
            break;
        default:
            return;
    }

    RewardSpellCast(plr, quest);
}
