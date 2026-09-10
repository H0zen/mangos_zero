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

#include "Group.h"
#include "Platform/Define.h"
#include <set>
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Formulas.h"
#include "Stats/Experience.h"
#include "PlayerRegistry.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "MapPersistentStateMgr.h"
#include "Util.h"
#include "LootMgr.h"
#include "LFGMgr.h"
#include "LFGHandler.h"

void Group::UpdateLooterGuid(Occupant* pSource, bool ifneed)
{
    switch (GetLootMethod())
    {
        case MASTER_LOOT:
        case FREE_FOR_ALL:
            return;
        default:

            break;
    }

    member_citerator guid_itr = _getMemberCSlot(GetLooterGuid());
    if (guid_itr != m_memberSlots.end())
    {
        if (ifneed)
        {

            Player* looter = sPlayerRegistry.Find(guid_itr->guid);
            if (looter && looter->Where().WithinDist(pSource->Where(), sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
            {
                return;
            }
        }
        ++guid_itr;
    }

    if (guid_itr != m_memberSlots.end())
    {
        for (member_citerator itr = guid_itr; itr != m_memberSlots.end(); ++itr)
        {
            if (Player* pl = sPlayerRegistry.Find(itr->guid))
            {
                if (pl->Where().WithinDist(pSource->Where(), sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
                {
                    bool refresh = pl->GetLootGuid() == pSource->GetObjectGuid();

                    SetLooterGuid(pl->GetObjectGuid());
                    SendUpdate();
                    if (refresh)
                    {
                        pl->SendLoot(pSource->GetObjectGuid(), LOOT_CORPSE);
                    }
                    return;
                }
            }
        }
    }

    for (member_citerator itr = m_memberSlots.begin(); itr != guid_itr; ++itr)
    {
        if (Player* pl = sPlayerRegistry.Find(itr->guid))
        {
            if (pl->Where().WithinDist(pSource->Where(), sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
            {
                bool refresh = pl->GetLootGuid() == pSource->GetObjectGuid();

                SetLooterGuid(pl->GetObjectGuid());
                SendUpdate();
                if (refresh)
                {
                    pl->SendLoot(pSource->GetObjectGuid(), LOOT_CORPSE);
                }
                return;
            }
        }
    }

    SetLooterGuid(0);
    SendUpdate();
}

uint32 Group::CanJoinBattleGroundQueue(BattleGroundTypeId bgTypeId, BattleGroundQueueTypeId bgQueueTypeId, uint32 MinPlayerCount, uint32 MaxPlayerCount)
{

    uint32 memberscount = GetMembersCount();
    if (memberscount < MinPlayerCount)
    {
        return BG_JOIN_ERR_GROUP_NOT_ENOUGH;
    }
    if (memberscount > MaxPlayerCount)
    {
        return BG_JOIN_ERR_GROUP_TOO_MANY;
    }

    Player* reference = GetFirstMember()->getSource();

    if (!reference)
    {
        return BG_JOIN_ERR_OFFLINE_MEMBER;
    }

    BattleGroundBracketId bracket_id = reference->GetBattleGroundBracketIdFromLevel(bgTypeId);
    Team team = reference->GetTeam();

    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->getSource();

        if (!member)
        {
            return BG_JOIN_ERR_OFFLINE_MEMBER;
        }

        if (member->GetTeam() != team)
        {
            return BG_JOIN_ERR_MIXED_FACTION;
        }

        if (member->GetBattleGroundBracketIdFromLevel(bgTypeId) != bracket_id)
        {
            return BG_JOIN_ERR_MIXED_LEVELS;
        }

        if (member->Queues().Holds(bgQueueTypeId))
        {
            return BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE;
        }

        if (!member->Battle().MayJoin())
        {
            return BG_JOIN_ERR_GROUP_DESERTER;
        }

        if (!member->Queues().AnyFree())
        {
            return BG_JOIN_ERR_ALL_QUEUES_USED;
        }
    }
    return BG_JOIN_ERR_OK;
}

bool Group::InCombatToInstance(uint32 instanceId)
{
    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pPlayer = itr->getSource();
        if (pPlayer->getAttackers().size() && pPlayer->GetInstanceId() == instanceId)
        {
            return true;
        }
    }
    return false;
}

void Group::_homebindIfInstance(Player* player)
{
    if (player && !player->isGameMaster())
    {
        Map* map = player->GetMap();
        if (map->IsDungeon())
        {

            DungeonHold* playerHold = player->Binds().To(map->GetId());
            if (!playerHold || !playerHold->permanent)
            {
                player->Binds().StillWelcome(false);
            }
        }
    }
}

static void RewardGroupAtKill_helper(Player* pGroupGuy, Unit* pVictim, uint32 count, bool PvP, float group_rate, uint32 sum_level, bool is_dungeon, Player* not_gray_member_with_max_level, Player* member_with_max_level, uint32 xp)
{

    if (pGroupGuy->IsAlive())
    {
        pGroupGuy->RewardHonor(pVictim, count);
    }

    if (!PvP)
    {
        float rate = group_rate * float(pGroupGuy->getLevel()) / sum_level;

        pGroupGuy->RewardReputation(pVictim, is_dungeon ? 1.0f : rate);

        if (pGroupGuy->IsAlive() && not_gray_member_with_max_level &&
            pGroupGuy->getLevel() <= not_gray_member_with_max_level->getLevel())
        {
            uint32 itr_xp = (member_with_max_level == not_gray_member_with_max_level) ? uint32(xp * rate) : uint32((xp * rate / 2) + 1);

            pGroupGuy->GiveXP(itr_xp, pVictim);
            if (Pet* pet = pGroupGuy->GetPet())
            {
                pet->GivePetXP(itr_xp);
            }
        }

        if (pGroupGuy->IsAlive() || !pGroupGuy->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        {

            if (IsCreature(pVictim))
            {
                pGroupGuy->Journal().CreatureKilled(((Creature*)pVictim)->GetCreatureInfo(), pVictim->GetObjectGuid());
            }
        }
    }
}

void Group::RewardGroupAtKill(Unit* pVictim, Player* player_tap)
{
    bool PvP = pVictim->IsCharmedOwnedByPlayerOrPlayer();

    uint32 count = 0;
    uint32 sum_level = 0;
    Player* member_with_max_level = nullptr;
    Player* not_gray_member_with_max_level = nullptr;

    GetDataForXPAtKill(pVictim, count, sum_level, member_with_max_level, not_gray_member_with_max_level, player_tap);

    if (member_with_max_level)
    {

        uint32 xp = (PvP || !not_gray_member_with_max_level)
                  ? 0
                  : xp::FromKill(not_gray_member_with_max_level->getLevel(), xp::QuarryOf(*pVictim),
                                 sWorld.getConfig(CONFIG_FLOAT_RATE_XP_KILL));

        bool is_raid = PvP ? false : sMapStore.LookupEntry(pVictim->GetMapId())->IsRaid() && isRaidGroup();
        bool is_dungeon = PvP ? false : sMapStore.LookupEntry(pVictim->GetMapId())->IsDungeon();
        float group_rate = xp::GroupShare(count, is_raid);

        for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
            {
                continue;
            }

            if (pGroupGuy == player_tap)
            {
                continue;
            }

            if (!pGroupGuy->IsAtGroupRewardDistance(pVictim))
            {
                continue;
            }

            RewardGroupAtKill_helper(pGroupGuy, pVictim, count, PvP, group_rate, sum_level, is_dungeon, not_gray_member_with_max_level, member_with_max_level, xp);
        }

        if (player_tap)
        {

            if (player_tap->IsAtGroupRewardDistance(pVictim))
            {
                RewardGroupAtKill_helper(player_tap, pVictim, count, PvP, group_rate, sum_level, is_dungeon, not_gray_member_with_max_level, member_with_max_level, xp);
            }
        }
    }
}
