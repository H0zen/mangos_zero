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
#include <ctime>
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Util.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "MapPersistentStateMgr.h"
#include "LootMgr.h"
#include "LFGMgr.h"
#include "LFGHandler.h"

bool Group::_addMember(ObjectGuid guid, const char* name, bool isAssistant)
{

    uint8 groupid = 0;
    if (m_subGroupsCounts)
    {
        bool groupFound = false;
        for (; groupid < MAX_RAID_SUBGROUPS; ++groupid)
        {
            if (m_subGroupsCounts[groupid] < MAX_GROUP_SIZE)
            {
                groupFound = true;
                break;
            }
        }

        if (!groupFound)
        {
            return false;
        }
    }

    return _addMember(guid, name, isAssistant, groupid);
}

bool Group::_addMember(ObjectGuid guid, const char* name, bool isAssistant, uint8 group)
{
    if (IsFull())
    {
        return false;
    }

    if (!guid)
    {
        return false;
    }

    Player* player = sObjectMgr.GetPlayer(guid, false);

    MemberSlot member;
    member.guid      = guid;
    member.name      = name;
    member.group     = group;
    member.assistant = isAssistant;
    member.joinTime = time(nullptr);
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(group);

    if (player)
    {
        player->Invites().ToParty(nullptr);

        if (player->GetGroup() && isBGGroup())
        {
            player->SetBattleGroundRaid(this, group);
        }

        else if (player->GetGroup())
        {
            player->SetOriginalGroup(this, group);
        }

        else
        {
            player->SetGroup(this, group);
        }

        if (player->IsInWorld())
        {

            if (DungeonHold* bind = m_binds.To(player->GetMapId()))
            {
                if (bind->state->GetInstanceId() == player->GetInstanceId())
                {
                    player->Binds().StillWelcome(true);
                }
            }
        }
    }

    if (!isRaidGroup())
    {
        for (int i = 0; i < TARGET_ICON_COUNT; ++i)
        {
            m_targetIcons[i] = 0;
        }
    }

    if (!isBGGroup())
    {

        CharacterDatabase.PExecute("INSERT INTO `group_member` (`groupId`,`memberGuid`,`assistant`,`subgroup`) VALUES ('%u','%u','%u','%u')",
            m_Id, GuidCounter(member.guid), ((member.assistant == 1) ? 1 : 0), member.group);
    }

    return true;
}

bool Group::_removeMember(ObjectGuid guid)
{
    Player* player = sObjectMgr.GetPlayer(guid);
    if (player)
    {

        if (isBGGroup())
        {
            player->RemoveFromBattleGroundRaid();
        }
        else
        {

            if (player->GetOriginalGroup() == this)
            {
                player->SetOriginalGroup(nullptr);
            }
            else
            {
                player->SetGroup(nullptr);
            }
        }
    }

    _removeRolls(guid);

    member_witerator slot = _getMemberWSlot(guid);
    if (slot != m_memberSlots.end())
    {
        SubGroupCounterDecrease(slot->group);

        m_memberSlots.erase(slot);
    }

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid`='%u'", GuidCounter(guid));
    }

    if (m_leaderGuid == guid)
    {
        if (GetMembersCount() > 0)
        {
            _setLeader(m_memberSlots.front().guid);
        }
        return true;
    }

    return false;
}

void Group::_setLeader(ObjectGuid guid)
{
    member_citerator slot = _getMemberCSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return;
    }

    if (!isBGGroup())
    {
        uint32 slot_lowguid = GuidCounter(slot->guid);

        uint32 leader_lowguid = GuidCounter(m_leaderGuid);

        CharacterDatabase.BeginTransaction();

        CharacterDatabase.PExecute(
                "DELETE FROM `group_instance` WHERE `leaderguid`='%u' AND (`permanent` = 1 OR "
                "`instance` IN (SELECT `instance` FROM `character_instance` WHERE `guid` = '%u')"
                ")", leader_lowguid, slot_lowguid);

        Player* player = sObjectMgr.GetPlayer(slot->guid);

        if (player)
        {
            m_binds.ReleasePermanent();
        }

        CharacterDatabase.PExecute("UPDATE `group_instance` SET `leaderGuid`='%u' WHERE `leaderGuid` = '%u'",
            slot_lowguid, leader_lowguid);

        Player::ConvertInstancesToGroup(player, this, slot->guid);

        CharacterDatabase.PExecute("UPDATE `groups` SET `leaderGuid`='%u' WHERE `groupId`='%u'", slot_lowguid, m_Id);
        CharacterDatabase.CommitTransaction();
    }

    m_leaderGuid = slot->guid;
    m_leaderName = slot->name;
}

void Group::_removeRolls(ObjectGuid guid)
{
    for (Rolls::iterator it = RollId.begin(); it != RollId.end();)
    {
        Roll* roll = *it;
        Roll::PlayerVote::iterator itr2 = roll->playerVote.find(guid);
        if (itr2 == roll->playerVote.end())
        {
            ++it;
            continue;
        }

        if (itr2->second == ROLL_GREED)
        {
            --roll->totalGreed;
        }
        if (itr2->second == ROLL_NEED)
        {
            --roll->totalNeed;
        }
        if (itr2->second == ROLL_PASS)
        {
            --roll->totalPass;
        }
        if (itr2->second != ROLL_NOT_VALID)
        {
            --roll->totalPlayersRolling;
        }

        roll->playerVote.erase(itr2);

        if (!CountRollVote(guid, it, ROLL_NOT_EMITED_YET))
        {
            ++it;
        }
    }
}

bool Group::_setMembersGroup(ObjectGuid guid, uint8 group)
{
    member_witerator slot = _getMemberWSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return false;
    }

    slot->group = group;

    SubGroupCounterIncrease(group);

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `group_member` SET `subgroup`='%u' WHERE `memberGuid`='%u'", group, GuidCounter(guid));
    }

    return true;
}

bool Group::_setAssistantFlag(ObjectGuid guid, const bool& state)
{
    member_witerator slot = _getMemberWSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return false;
    }

    slot->assistant = state;
    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `group_member` SET `assistant`='%u' WHERE `memberGuid`='%u'", (state == true) ? 1 : 0, GuidCounter(guid));
    }
    return true;
}

bool Group::_setMainTank(ObjectGuid guid)
{
    if (m_mainTankGuid == guid)
    {
        return false;
    }

    if (guid)
    {
        member_citerator slot = _getMemberCSlot(guid);
        if (slot == m_memberSlots.end())
        {
            return false;
        }

        if (m_mainAssistantGuid == guid)
        {
            _setMainAssistant(0);
        }
    }

    m_mainTankGuid = guid;

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `mainTank`='%u' WHERE `groupId`='%u'", GuidCounter(m_mainTankGuid), m_Id);
    }

    return true;
}

bool Group::_setMainAssistant(ObjectGuid guid)
{
    if (m_mainAssistantGuid == guid)
    {
        return false;
    }

    if (guid)
    {
        member_witerator slot = _getMemberWSlot(guid);
        if (slot == m_memberSlots.end())
        {
            return false;
        }

        if (m_mainTankGuid == guid)
        {
            _setMainTank(0);
        }
    }

    m_mainAssistantGuid = guid;

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `mainAssistant`='%u' WHERE `groupId`='%u'",
            GuidCounter(m_mainAssistantGuid), m_Id);
    }

    return true;
}

bool Group::SameSubGroup(Player const* member1, Player const* member2) const
{
    if (!member1 || !member2)
    {
        return false;
    }
    if (member1->GetGroup() != this || member2->GetGroup() != this)
    {
        return false;
    }
    else
    {
        return member1->GetSubGroup() == member2->GetSubGroup();
    }
}

void Group::ChangeMembersGroup(ObjectGuid guid, uint8 group)
{
    if (!isRaidGroup())
    {
        return;
    }

    Player* player = sObjectMgr.GetPlayer(guid);

    if (!player)
    {
        uint8 prevSubGroup = GetMemberGroup(guid);
        if (prevSubGroup == group)
        {
            return;
        }

        if (_setMembersGroup(guid, group))
        {
            SubGroupCounterDecrease(prevSubGroup);
            SendUpdate();
        }
    }
    else

    {
        ChangeMembersGroup(player, group);
    }
}

void Group::ChangeMembersGroup(Player* player, uint8 group)
{
    if (!player || !isRaidGroup())
    {
        return;
    }

    uint8 prevSubGroup = player->GetSubGroup();
    if (prevSubGroup == group)
    {
        return;
    }

    if (_setMembersGroup(player->GetObjectGuid(), group))
    {
        if (player->GetGroup() == this)
        {
            player->GetGroupRef().setSubGroup(group);
        }

        else
        {
            prevSubGroup = player->GetOriginalSubGroup();
            player->GetOriginalGroupRef().setSubGroup(group);
        }
        SubGroupCounterDecrease(prevSubGroup);

        SendUpdate();
    }
}
