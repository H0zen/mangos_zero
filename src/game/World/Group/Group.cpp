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

#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <string>
#include <vector>
#include <ctime>
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "ObjectGuid.h"
#include "Group.h"
#include "Formulas.h"
#include "Stats/Experience.h"
#include "BattleGround/BattleGround.h"
#include "MapPersistentStateMgr.h"
#include "LootMgr.h"
#include "PerKind.h"
#include "LFGMgr.h"
#include "LFGHandler.h"

#define LOOT_ROLL_TIMEOUT  (1*MINUTE*IN_MILLISECONDS)

void Roll::targetObjectBuildLink()
{

    getTarget()->addLootValidatorRef(this);
}

Group::Group() : m_Id(0), m_groupType(GROUPTYPE_NORMAL),
    m_bgGroup(nullptr), m_lootMethod(FREE_FOR_ALL), m_lootThreshold(ITEM_QUALITY_UNCOMMON),
    m_binds(*this), m_subGroupsCounts(nullptr), m_LFGAreaId(0)
{
}

Group::~Group()
{
    if (m_bgGroup)
    {
        DEBUG_LOG("Group::~Group: battleground group being deleted.");
        if (m_bgGroup->GetBgRaid(ALLIANCE) == this)
        {
            m_bgGroup->SetBgRaid(ALLIANCE, nullptr);
        }
        else if (m_bgGroup->GetBgRaid(HORDE) == this)
        {
            m_bgGroup->SetBgRaid(HORDE, nullptr);
        }
        else
        {
            sLog.outError("Group::~Group: battleground group is not linked to the correct battleground.");
        }
    }
    Rolls::iterator itr;
    while (!RollId.empty())
    {
        itr = RollId.begin();
        Roll* r = *itr;
        RollId.erase(itr);
        delete(r);
    }

    delete[] m_subGroupsCounts;
}

bool Group::Create(ObjectGuid guid, const char* name)
{
    m_leaderGuid = guid;
    m_leaderName = name;

    m_groupType  = isBGGroup() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
    {
        _initRaidSubGroupsCounter();
    }

    m_lootMethod = GROUP_LOOT;
    m_lootThreshold = ITEM_QUALITY_UNCOMMON;
    m_looterGuid = guid;

    if (!isBGGroup())
    {
        m_Id = sMint.GroupIds().Next();

        Player* leader = sObjectMgr.GetPlayer(guid);

        Player::ConvertInstancesToGroup(leader, this, guid);

        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId` ='%u'", m_Id);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId` ='%u'", m_Id);

        CharacterDatabase.PExecute("INSERT INTO `groups` (`groupId`,`leaderGuid`,`mainTank`,`mainAssistant`,`lootMethod`,`looterGuid`,`lootThreshold`,`icon1`,`icon2`,`icon3`,`icon4`,`icon5`,`icon6`,`icon7`,`icon8`,`isRaid`) "
            "VALUES ('%u','%u','%u','%u','%u','%u','%u','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','%u')",
            m_Id, GuidCounter(m_leaderGuid), GuidCounter(m_mainTankGuid), GuidCounter(m_mainAssistantGuid), uint32(m_lootMethod),
            GuidCounter(m_looterGuid), uint32(m_lootThreshold),
            m_targetIcons[0], m_targetIcons[1],
            m_targetIcons[2], m_targetIcons[3],
            m_targetIcons[4], m_targetIcons[5],
            m_targetIcons[6], m_targetIcons[7],
            isRaidGroup());
    }

    if (!AddMember(guid, name))
    {
        return false;
    }

    if (!isBGGroup())
    {
        CharacterDatabase.CommitTransaction();
    }

    return true;
}

bool Group::LoadGroupFromDB(Field* fields)
{

    m_Id = fields[15].GetUInt32();
    m_leaderGuid = MakeGuid(HIGHGUID_PLAYER, fields[14].GetUInt32());

    if (!sObjectMgr.GetPlayerNameByGUID(m_leaderGuid, m_leaderName))
    {
        return false;
    }

    m_groupType  = fields[13].GetBool() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
    {
        _initRaidSubGroupsCounter();
    }

    m_mainTankGuid = MakeGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    m_mainAssistantGuid = MakeGuid(HIGHGUID_PLAYER, fields[1].GetUInt32());
    m_lootMethod = LootMethod(fields[2].GetUInt8());
    m_looterGuid = MakeGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
    m_lootThreshold = ItemQualities(fields[4].GetUInt16());

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        m_targetIcons[i] = static_cast<ObjectGuid>(fields[5 + i].GetUInt64());
    }

    return true;
}

bool Group::LoadMemberFromDB(uint32 guidLow, uint8 subgroup, bool assistant)
{
    MemberSlot member;
    member.guid      = MakeGuid(HIGHGUID_PLAYER, guidLow);

    if (!sObjectMgr.GetPlayerNameByGUID(member.guid, member.name))
    {
        return false;
    }

    member.group     = subgroup;
    member.assistant = assistant;
    member.joinTime = time(nullptr);
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(subgroup);

    return true;
}

void Group::ConvertToRaid()
{
    m_groupType = GROUPTYPE_RAID;

    _initRaidSubGroupsCounter();

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `isRaid` = 1 WHERE `groupId`='%u'", m_Id);
    }
    SendUpdate();

    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {

        if (Player* player = sObjectMgr.GetPlayer(citr->guid))
        {
            player->UpdateForQuestObjects();
        }
    }
}

bool Group::AddInvite(Player* player)
{
    if (!player || player->Invites().ToParty())
    {
        return false;
    }
    Group* group = player->GetGroup();
    if (group && group->isBGGroup())
    {
        group = player->GetOriginalGroup();
    }
    if (group)
    {
        return false;
    }

    RemoveInvite(player);

    m_invitees.insert(player);

    player->Invites().ToParty(this);

    return true;
}

bool Group::AddLeaderInvite(Player* player)
{
    if (!AddInvite(player))
    {
        return false;
    }

    m_leaderGuid = player->GetObjectGuid();
    m_leaderName = player->GetName();
    return true;
}

uint32 Group::RemoveInvite(Player* player)
{
    m_invitees.erase(player);

    player->Invites().ToParty(nullptr);
    return GetMembersCount();
}

void Group::RemoveAllInvites()
{
    for (InvitesList::iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
    {
        (*itr)->Invites().ToParty(nullptr);
    }

    m_invitees.clear();
}

Player* Group::GetInvited(ObjectGuid guid) const
{
    for (InvitesList::const_iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
    {
        if ((*itr)->GetObjectGuid() == guid)
        {
            return (*itr);
        }
    }
    return nullptr;
}

Player* Group::GetInvited(const std::string& name) const
{
    for (InvitesList::const_iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
    {
        if ((*itr)->GetName() == name)
        {
            return (*itr);
        }
    }
    return nullptr;
}

bool Group::AddMember(ObjectGuid guid, const char* name, uint8 joinMethod)
{
    if (!_addMember(guid, name))
    {
        return false;
    }

    SendUpdate();

    if (Player* player = sObjectMgr.GetPlayer(guid))
    {
        if (!IsLeader(player->GetObjectGuid()) && !isBGGroup())
        {

            player->Binds().Reset(INSTANCE_RESET_GROUP_JOIN);
        }
        player->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        UpdatePlayerOutOfRange(player);

        if (isRaidGroup())
        {
            player->UpdateForQuestObjects();
        }

        if (isInLFG())
        {
            if (joinMethod == GROUP_LFG)
            {

            }
            else
            {
                player->GetSession()->SendMeetingstoneSetqueue(m_LFGAreaId, MEETINGSTONE_STATUS_JOINED_QUEUE);

                sLFGMgr.UpdateGroup(m_Id);
            }
        }
    }

    return true;
}

uint32 Group::RemoveMember(ObjectGuid guid, uint8 removeMethod)
{

    if (GetMembersCount() > uint32(isBGGroup() ? 1 : 2))
    {
        bool leaderChanged = _removeMember(guid);

        if (Player* player = sObjectMgr.GetPlayer(guid))
        {

            if (isRaidGroup())
            {
                player->UpdateForQuestObjects();
            }

            WorldPacket data;

            if (removeMethod == GROUP_KICK)
            {
                data.Initialize(SMSG_GROUP_UNINVITE, 0);
                player->GetSession()->SendPacket(&data);

                if (isInLFG())
                {
                    data.Initialize(SMSG_MEETINGSTONE_SETQUEUE, 5);
                    data << 0 << uint8(MEETINGSTONE_STATUS_PARTY_MEMBER_REMOVED_PARTY_REMOVED);

                    BroadcastPacket(&data, true);
                    sLFGMgr.RemoveGroupFromQueue(m_Id);

                    player->GetSession()->SendMeetingstoneSetqueue(m_LFGAreaId, MEETINGSTONE_STATUS_LOOKING_FOR_NEW_PARTY_IN_QUEUE);
                    sLFGMgr.AddToQueue(player, m_LFGAreaId);
                }
            }

            if (removeMethod == GROUP_LEAVE && isInLFG())
            {
                player->GetSession()->SendMeetingstoneSetqueue(0, MEETINGSTONE_STATUS_NONE);

                data.Initialize(SMSG_MEETINGSTONE_SETQUEUE, 5);
                data << m_LFGAreaId << uint8(MEETINGSTONE_STATUS_PARTY_MEMBER_LEFT_LFG);
                BroadcastPacket(&data, true);
            }

            if (Group* group = player->GetGroup())
            {
                group->SendUpdate();
            }
            else
            {
                data.Initialize(SMSG_GROUP_LIST, 24);
                data << uint64(0) << uint64(0) << uint64(0);
                player->GetSession()->SendPacket(&data);
            }

            _homebindIfInstance(player);
        }

        if (leaderChanged)
        {
            WorldPacket data(SMSG_GROUP_SET_LEADER, (m_memberSlots.front().name.size() + 1));
            data << m_memberSlots.front().name;
            BroadcastPacket(&data, true);

            sLFGMgr.RemoveGroupFromQueue(m_Id);
        }

        if (isInLFG())
        {
            sLFGMgr.UpdateGroup(m_Id);
        }

        SendUpdate();
    }

    else
    {
        Disband(true);
    }

    return m_memberSlots.size();
}

void Group::ChangeLeader(ObjectGuid guid)
{
    member_citerator slot = _getMemberCSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return;
    }

    _setLeader(guid);

    WorldPacket data(SMSG_GROUP_SET_LEADER, slot->name.size() + 1);
    data << slot->name;
    BroadcastPacket(&data, true);
    SendUpdate();
}

void Group::Disband(bool hideDestroy)
{
    Player* player;

    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        player = sObjectMgr.GetPlayer(citr->guid);
        if (!player)
        {
            continue;
        }

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

        if (isRaidGroup())
        {
            player->UpdateForQuestObjects();
        }

        if (!player->GetSession())
        {
            continue;
        }

        WorldPacket data;
        if (!hideDestroy)
        {
            data.Initialize(SMSG_GROUP_DESTROYED, 0);
            player->GetSession()->SendPacket(&data);
        }

        if (Group* group = player->GetGroup())
        {
            group->SendUpdate();
        }
        else
        {
            data.Initialize(SMSG_GROUP_LIST, 24);
            data << uint64(0) << uint64(0) << uint64(0);
            player->GetSession()->SendPacket(&data);

            if (isInLFG())
            {
                sLFGMgr.RemoveGroupFromQueue(m_Id);

                data.Initialize(SMSG_MEETINGSTONE_SETQUEUE, 5);
                data << 0 << MEETINGSTONE_STATUS_NONE;

                player->GetSession()->SendPacket(&data);
            }
        }

        _homebindIfInstance(player);
    }
    RollId.clear();
    m_memberSlots.clear();

    RemoveAllInvites();

    if (!isBGGroup())
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId`='%u'", m_Id);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId`='%u'", m_Id);
        CharacterDatabase.CommitTransaction();
        m_binds.Reset(INSTANCE_RESET_GROUP_DISBAND, nullptr);
    }

    m_leaderGuid = 0;
    m_leaderName.clear();
}

void Group::SendUpdateToPlayer(Player* pPlayer)
{
    if (!pPlayer || !pPlayer->GetSession() || !pPlayer->IsInWorld() || pPlayer->GetGroup() != this)
    {
        return;
    }

    if (pPlayer->GetGroupUpdateFlag() == GROUP_UPDATE_FLAG_NONE)
    {
        return;
    }

    uint8 subGroup;

    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        if (citr->guid == pPlayer->GetObjectGuid())
        {
            subGroup=citr->group;
        }
    }

    WorldPacket data(SMSG_GROUP_LIST, (1 + 1 + 1 + 4 + GetMembersCount() * 20) + 8 + 1 + 8 + 1);
    data << (uint8)m_groupType;
    data << (uint8)(subGroup | (IsAssistant(pPlayer->GetObjectGuid()) ? 0x80 : 0));

    data << uint32(GetMembersCount() - 1);
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        if (citr->guid == pPlayer->GetObjectGuid())
        {
            continue;
        }

        Player* member = sObjectMgr.GetPlayer(citr->guid);
        uint8 onlineState = (member) ? MEMBER_STATUS_ONLINE : MEMBER_STATUS_OFFLINE;
        onlineState = onlineState | ((isBGGroup()) ? MEMBER_STATUS_PVP : 0);

        data << citr->name;
        data << citr->guid;

        data << uint8(sObjectMgr.GetPlayer(citr->guid) ? 1 : 0);
        data << (uint8)(citr->group | (citr->assistant ? 0x80 : 0));
    }

    data << m_leaderGuid;
    if (GetMembersCount() - 1)
    {
        data << uint8(m_lootMethod);
        if (m_lootMethod == MASTER_LOOT)
        {
            data << m_looterGuid;
        }
        else
        {
            data << uint64(0);
        }
        data << uint8(m_lootThreshold);
    }

    pPlayer->GetSession()->SendPacket(&data);
}

void Group::CalculateLFGRoles(LFGGroupQueueInfo& data)
{
    uint32 m_initRoles = (LFG_ROLE_TANK | LFG_ROLE_DPS | LFG_ROLE_HEALER);
    std::vector<ObjectGuid> m_processed;
    uint32 dpsCount = 0;

    for (member_citerator citr = GetMemberSlots().begin(); citr != GetMemberSlots().end(); ++citr)
    {
        ClassRoles lfgRole;

        lfgRole = sLFGMgr.CalculateRoles((Classes)sObjectMgr.GetPlayerClassByGUID(citr->guid));

        if ((sLFGMgr.canPerformRole(lfgRole, LFG_ROLE_TANK) & m_initRoles) == LFG_ROLE_TANK && !inLFGGroup(m_processed, citr->guid))
        {
            FillPremadeLFG(citr->guid, LFG_ROLE_TANK, m_initRoles, dpsCount, m_processed);
        }

        if ((sLFGMgr.canPerformRole(lfgRole, LFG_ROLE_HEALER) & m_initRoles) == LFG_ROLE_HEALER && !inLFGGroup(m_processed, citr->guid))
        {
            FillPremadeLFG(citr->guid, LFG_ROLE_HEALER, m_initRoles, dpsCount, m_processed);
        }

        if ((sLFGMgr.canPerformRole(lfgRole, LFG_ROLE_DPS) & m_initRoles) == LFG_ROLE_DPS && !inLFGGroup(m_processed, citr->guid))
        {
            FillPremadeLFG(citr->guid, LFG_ROLE_DPS, m_initRoles, dpsCount, m_processed);
        }
    }

    data.availableRoles = (ClassRoles)m_initRoles;
    data.dpsCount = dpsCount;
}

void Group::FillPremadeLFG(ObjectGuid plrGuid, ClassRoles requiredRole, uint32& InitRoles, uint32& DpsCount, std::vector<ObjectGuid>& playersProcessed)
{
    Classes plrClass = (Classes)sObjectMgr.GetPlayerClassByGUID(plrGuid);

    if (sLFGMgr.getPriority(plrClass, requiredRole) >= LFG_PRIORITY_HIGH && !inLFGGroup(playersProcessed, plrGuid))
    {
        switch (requiredRole)
        {
            case LFG_ROLE_TANK:
            {
                InitRoles &= ~LFG_ROLE_TANK;
                break;
            }

            case LFG_ROLE_HEALER:
            {
                InitRoles &= ~LFG_ROLE_HEALER;
                break;
            }

            case LFG_ROLE_DPS:
            {
                if (DpsCount < 3)
                {
                    ++DpsCount;

                    if (DpsCount >= 3)
                    {
                        InitRoles &= ~LFG_ROLE_DPS;
                    }
                }
                break;
            }
            default:
                break;
        }
        playersProcessed.push_back(plrGuid);
    }
    else if (sLFGMgr.getPriority(plrClass, requiredRole) < LFG_PRIORITY_HIGH && !inLFGGroup(playersProcessed, plrGuid))
    {
        bool hasFoundPriority = false;

        for (member_citerator citr = GetMemberSlots().begin(); citr != GetMemberSlots().end(); ++citr)
        {
            if (plrGuid == citr->guid)
            {
                continue;
            }

            Classes memberClass = (Classes)sObjectMgr.GetPlayerClassByGUID(plrGuid);

            if (sLFGMgr.getPriority(plrClass, requiredRole) < sLFGMgr.getPriority(memberClass, requiredRole) && !inLFGGroup(playersProcessed, plrGuid))
            {
                hasFoundPriority = true;
            }
        }

        if (!hasFoundPriority)
        {
            switch (requiredRole)
            {
                case LFG_ROLE_TANK:
                {
                    InitRoles &= ~LFG_ROLE_TANK;
                    break;
                }

                case LFG_ROLE_HEALER:
                {
                    InitRoles &= ~LFG_ROLE_HEALER;
                    break;

                }

                case LFG_ROLE_DPS:
                {
                    if (DpsCount < 3)
                    {
                        ++DpsCount;

                        if (DpsCount >= 3)
                        {
                            InitRoles &= ~LFG_ROLE_DPS;
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            playersProcessed.push_back(plrGuid);
        }
    }
}

void Group::SendLootStartRoll(uint32 CountDown, const Roll& r)
{
    WorldPacket data(SMSG_LOOT_START_ROLL, (8 + 4 + 4 + 4 + 4 + 4));
    data << r.lootedTargetGUID;
    data << uint32(r.itemSlot);
    data << uint32(r.itemid);
    data << uint32(0);
    data << uint32(r.itemRandomPropId);
    data << uint32(CountDown);

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second == ROLL_NOT_VALID)
        {
            continue;
        }

        p->GetSession()->SendPacket(&data);
    }
}

void Group::SendLootRoll(ObjectGuid targetGuid, uint8 rollNumber, uint8 rollType, const Roll& r)
{
    WorldPacket data(SMSG_LOOT_ROLL, (8 + 4 + 8 + 4 + 4 + 4 + 1 + 1));
    data << r.lootedTargetGUID;
    data << uint32(r.itemSlot);
    data << targetGuid;
    data << uint32(r.itemid);
    data << uint32(0);
    data << uint32(r.itemRandomPropId);
    data << uint8(rollNumber);
    data << uint8(rollType);

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

void Group::SendLootRollWon(ObjectGuid targetGuid, uint8 rollNumber, RollVote rollType, const Roll& r)
{
    WorldPacket data(SMSG_LOOT_ROLL_WON, (8 + 4 + 4 + 4 + 4 + 8 + 1 + 1));
    data << r.lootedTargetGUID;
    data << uint32(r.itemSlot);
    data << uint32(r.itemid);
    data << uint32(0);
    data << uint32(r.itemRandomPropId);
    data << targetGuid;
    data << uint8(rollNumber);
    data << uint8(rollType);

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

void Group::SendLootAllPassed(Roll const& r)
{
    WorldPacket data(SMSG_LOOT_ALL_PASSED, (8 + 4 + 4 + 4 + 4));
    data << r.lootedTargetGUID;
    data << uint32(r.itemSlot);
    data << uint32(r.itemid);
    data << uint32(r.itemRandomPropId);
    data << uint32(0);

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

void Group::GroupLoot(Occupant* pSource, Loot* loot)
{
    for (uint8 itemSlot = 0; itemSlot < loot->items.size(); ++itemSlot)
    {
        LootItem& lootItem = loot->items[itemSlot];
        ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(lootItem.itemid);
        if (!itemProto)
        {
            DEBUG_LOG("Group::GroupLoot: missing item prototype for item with id: %d", lootItem.itemid);
            continue;
        }

        if (itemProto->Quality >= uint32(m_lootThreshold) && !lootItem.freeforall)
        {
            lootItem.is_underthreshold = 0;
            StartLootRoll(pSource, GROUP_LOOT, loot, itemSlot);
        }
        else
        {
            lootItem.is_underthreshold = 1;
        }
    }
}

void Group::NeedBeforeGreed(Occupant* pSource, Loot* loot)
{
    for (uint8 itemSlot = 0; itemSlot < loot->items.size(); ++itemSlot)
    {
        LootItem& lootItem = loot->items[itemSlot];
        ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(lootItem.itemid);
        if (!itemProto)
        {
            DEBUG_LOG("Group::NeedBeforeGreed: missing item prototype for item with id: %d", lootItem.itemid);
            continue;
        }

        if (itemProto->Quality >= uint32(m_lootThreshold) && !lootItem.freeforall)
        {
            lootItem.is_underthreshold = 0;
            StartLootRoll(pSource, NEED_BEFORE_GREED, loot, itemSlot);
        }
        else
        {
            lootItem.is_underthreshold = 1;
        }
    }
}

void Group::MasterLoot(Occupant* pSource, Loot* loot)
{
    for (LootItemList::iterator i = loot->items.begin(); i != loot->items.end(); ++i)
    {
        ItemPrototype const* item = ObjectMgr::GetItemPrototype(i->itemid);
        if (!item)
        {
            continue;
        }
        if (item->Quality >= uint32(m_lootThreshold))
        {
            i->is_underthreshold = 0;
        }
    }

    uint32 real_count = 0;

    WorldPacket data(SMSG_LOOT_MASTER_LIST, 330);
    data << uint8(GetMembersCount());

    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* looter = itr->getSource();
        if (!looter->IsInWorld())
        {
            continue;
        }

        if (looter->Where().WithinDist(pSource->Where(), sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
        {
            data << looter->GetObjectGuid();
            ++real_count;
        }
    }

    data.put<uint8>(0, real_count);

    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* looter = itr->getSource();
        if (looter->Where().WithinDist(pSource->Where(), sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
        {
            looter->GetSession()->SendPacket(&data);
        }
    }
}

bool Group::CountRollVote(Player* player, ObjectGuid lootedTarget, uint32 itemSlot, RollVote vote)
{
    Rolls::iterator rollI = RollId.begin();
    for (; rollI != RollId.end(); ++rollI)
    {
        if ((*rollI)->isValid() && (*rollI)->lootedTargetGUID == lootedTarget && (*rollI)->itemSlot == itemSlot)
        {
            break;
        }
    }

    if (rollI == RollId.end())
    {
        return false;
    }

    CountRollVote(player->GetObjectGuid(), rollI, vote);
    return true;
}

bool Group::CountRollVote(ObjectGuid playerGUID, Rolls::iterator& rollI, RollVote vote)
{
    Roll* roll = *rollI;

    Roll::PlayerVote::iterator itr = roll->playerVote.find(playerGUID);

    if (itr == roll->playerVote.end())
    {
        return true;
    }

    if (roll->getLoot())
    {
        if (roll->getLoot()->items.empty())
        {
            return false;
        }
    }

    switch (vote)
    {
        case ROLL_PASS:
        {
            SendLootRoll(playerGUID, 128, 128, *roll);
            ++roll->totalPass;
            itr->second = ROLL_PASS;
            break;
        }
        case ROLL_NEED:
        {
            SendLootRoll(playerGUID, 0, 0, *roll);
            ++roll->totalNeed;
            itr->second = ROLL_NEED;
            break;
        }
        case ROLL_GREED:
        {
            SendLootRoll(playerGUID, 128, 2, *roll);
            ++roll->totalGreed;
            itr->second = ROLL_GREED;
            break;
        }
        default:
            break;
    }

    if (roll->totalPass + roll->totalNeed + roll->totalGreed >= roll->totalPlayersRolling)
    {
        CountTheRoll(rollI);
        return true;
    }

    return false;
}

void Group::StartLootRoll(Occupant* lootTarget, LootMethod method, Loot* loot, uint8 itemSlot)
{
    if (itemSlot >= loot->items.size())
    {
        return;
    }

    LootItem const& lootItem = loot->items[itemSlot];

    ItemPrototype const* item = ObjectMgr::GetItemPrototype(lootItem.itemid);

    Roll* r = new Roll(lootTarget->GetObjectGuid(), lootItem);

    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* playerToRoll = itr->getSource();
        if (!playerToRoll || !playerToRoll->GetSession())
        {
            continue;
        }

        if ((method != NEED_BEFORE_GREED || playerToRoll->CanUseItem(item) == EQUIP_ERR_OK) && lootItem.AllowedForPlayer(playerToRoll, lootTarget))
        {
            if (InReach(*playerToRoll, *lootTarget, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
            {
                r->playerVote[playerToRoll->GetObjectGuid()] = ROLL_NOT_EMITED_YET;
                ++r->totalPlayersRolling;
            }
        }
    }

    if (r->totalPlayersRolling > 0)
    {
        r->setLoot(loot);
        r->itemSlot = itemSlot;

        if (r->totalPlayersRolling == 1)
        {
            r->playerVote.begin()->second = ROLL_NEED;
        }
        else
        {

            MANGOS_ASSERT(IsType(lootTarget, TYPEMASK_CREATURE_OR_GAMEOBJECT));

            SendLootStartRoll(LOOT_ROLL_TIMEOUT, *r);
            loot->items[itemSlot].is_blocked = true;

            if (LootClaim* claim = ClaimOn(*lootTarget))
            {
                claim->StartRoll(this, LOOT_ROLL_TIMEOUT);
            }
        }

        RollId.push_back(r);
    }
    else
    {
        delete r;
    }
}

void Group::EndRoll()
{
    while (!RollId.empty())
    {

        Rolls::iterator itr = RollId.begin();
        CountTheRoll(itr);
    }
}

void Group::CountTheRoll(Rolls::iterator& rollI)
{
    Roll* roll = *rollI;

    if (!roll->isValid())
    {
        rollI = RollId.erase(rollI);
        delete roll;
        return;
    }

    bool won = false;
    if (roll->totalNeed > 0)
    {
        if (!roll->playerVote.empty())
        {
            uint8 maxresul = 0;
            ObjectGuid maxguid  = (*roll->playerVote.begin()).first;

            for (Roll::PlayerVote::const_iterator itr = roll->playerVote.begin(); itr != roll->playerVote.end(); ++itr)
            {
                if (itr->second != ROLL_NEED)
                {
                    continue;
                }

                uint8 randomN = urand(1, 100);
                SendLootRoll(itr->first, randomN, ROLL_NEED, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                }
            }

            if (Player* player = sObjectMgr.GetPlayer(maxguid))
            {
                if (Occupant* object = player->GetMap()->GetOccupant(roll->lootedTargetGUID))
                {
                    SendLootRollWon(maxguid, maxresul, ROLL_NEED, *roll);
                    won = true;
                    if (player->GetSession())
                    {
                        ItemPosCountVec dest;
                        LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
                        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count);
                        if (msg == EQUIP_ERR_OK)
                        {
                            item->is_looted = true;
                            roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                            --roll->getLoot()->unlootedCount;
                            Item* newitem = player->StoreNewItem(dest, roll->itemid, true, item->randomPropertyId);
                            player->SendNewItem(newitem, uint32(item->count), false, false, true);

                            if (Creature* creature = static_cast<Creature*>(object))
                            {

                                if (creature->loot.isLooted())
                                {
                                    creature->RemoveDynFlag(UNIT_DYNFLAG_LOOTABLE);
                                }
                            }
                        }
                        else
                        {
                            item->is_blocked = false;
                            player->SendEquipError(msg, nullptr, nullptr, roll->itemid);
                            item->winner = player->GetObjectGuid();
                        }
                    }
                }
            }
        }
    }

    if (!won && roll->totalGreed > 0)
    {
        if (!roll->playerVote.empty())
        {
            uint8 maxresul = 0;
            ObjectGuid maxguid = (*roll->playerVote.begin()).first;

            Roll::PlayerVote::iterator itr;
            for (itr = roll->playerVote.begin(); itr != roll->playerVote.end(); ++itr)
            {
                if (itr->second != ROLL_GREED)
                {
                    continue;
                }

                uint8 randomN = urand(1, 100);
                SendLootRoll(itr->first, randomN, itr->second, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                }
            }

            if (Player* player = sObjectMgr.GetPlayer(maxguid))
            {
                if (Occupant* object = player->GetMap()->GetOccupant(roll->lootedTargetGUID))
                {
                    SendLootRollWon(maxguid, maxresul, ROLL_GREED, *roll);
                    won = true;
                    if (player->GetSession())
                    {
                        ItemPosCountVec dest;
                        LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
                        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count);
                        if (msg == EQUIP_ERR_OK)
                        {
                            item->is_looted = true;
                            roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                            --roll->getLoot()->unlootedCount;
                            Item* newitem = player->StoreNewItem(dest, roll->itemid, true, item->randomPropertyId);
                            player->SendNewItem(newitem, uint32(item->count), false, false, true);
                            if (Creature* creature = static_cast<Creature*>(object))
                            {

                                if (creature->loot.isLooted())
                                {
                                    creature->RemoveDynFlag(UNIT_DYNFLAG_LOOTABLE);
                                }
                            }
                        }
                        else
                        {
                            item->is_blocked = false;
                            player->SendEquipError(msg, nullptr, nullptr, roll->itemid);

                            item->winner = player->GetObjectGuid();
                        }
                    }
                }
            }
        }
    }

    if (!won)
    {
        SendLootAllPassed(*roll);
        LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
        if (item)
        {
            item->is_blocked = false;
        }
    }

    rollI = RollId.erase(rollI);
    delete roll;
}

bool Group::IsRollDoneForItem(Occupant * pObject, const LootItem * pItem)
{
    if (RollId.empty())
    {
        return true;
    }

    for (Rolls::iterator i = RollId.begin(); i != RollId.end(); ++i)
    {
        Roll *roll = *i;
        if (roll->lootedTargetGUID == pObject->GetObjectGuid() && roll->itemid == pItem->itemid && roll->totalPlayersRolling > 1)
        {
            return false;
        }
    }

    return true;
}

void Group::SetTargetIcon(uint8 id, ObjectGuid targetGuid)
{
    if (id >= TARGET_ICON_COUNT)
    {
        return;
    }

    if (targetGuid)
    {
        for (int i = 0; i < TARGET_ICON_COUNT; ++i)
        {
            if (m_targetIcons[i] == targetGuid)
            {
                SetTargetIcon(i, 0);
            }
        }
    }

    m_targetIcons[id] = targetGuid;

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (1 + 1 + 8));
    data << uint8(0);
    data << uint8(id);
    data << targetGuid;
    BroadcastPacket(&data, true);
}

static void GetDataForXPAtKill_helper(Player* player, Unit const* victim, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level)
{
    sum_level += player->getLevel();
    if (!member_with_max_level || member_with_max_level->getLevel() < player->getLevel())
    {
        member_with_max_level = player;
    }

    uint32 gray_level = xp::GreyLevel(player->getLevel());
    if (victim->getLevel() > gray_level && (!not_gray_member_with_max_level ||
        not_gray_member_with_max_level->getLevel() < player->getLevel()))
    {
        not_gray_member_with_max_level = player;
    }
}

void Group::GetDataForXPAtKill(Unit const* victim, uint32& count, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level, Player* additional)
{
    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->getSource();
        if (!member || !member->IsAlive())
        {
            continue;
        }

        if (member == additional)
        {
            continue;
        }

        if (!member->IsAtGroupRewardDistance(victim))
        {
            continue;
        }

        ++count;
        GetDataForXPAtKill_helper(member, victim, sum_level, member_with_max_level, not_gray_member_with_max_level);
    }

    if (additional)
    {
        if (additional->IsAtGroupRewardDistance(victim))
        {
            ++count;
            GetDataForXPAtKill_helper(additional, victim, sum_level, member_with_max_level, not_gray_member_with_max_level);
        }
    }
}

void Group::SendTargetIconList(WorldSession* session)
{
    if (!session)
    {
        return;
    }

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (1 + TARGET_ICON_COUNT * 9));
    data << uint8(1);

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        if (!m_targetIcons[i])
        {
            continue;
        }

        data << uint8(i);
        data << m_targetIcons[i];
    }

    session->SendPacket(&data);
}

void Group::SendUpdate()
{
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        Player* player = sObjectMgr.GetPlayer(citr->guid);
        if (!player || !player->GetSession() || player->GetGroup() != this)
        {
            continue;
        }

        WorldPacket data(SMSG_GROUP_LIST, (1 + 1 + 1 + 4 + GetMembersCount() * 20) + 8 + 1 + 8 + 1);
        data << (uint8)m_groupType;
        data << (uint8)(citr->group | (citr->assistant ? 0x80 : 0));

        data << uint32(GetMembersCount() - 1);
        for (member_citerator citr2 = m_memberSlots.begin(); citr2 != m_memberSlots.end(); ++citr2)
        {
            if (citr->guid == citr2->guid)
            {
                continue;
            }
            Player* member = sObjectMgr.GetPlayer(citr2->guid);
            uint8 onlineState = (member) ? MEMBER_STATUS_ONLINE : MEMBER_STATUS_OFFLINE;
            onlineState = onlineState | ((isBGGroup()) ? MEMBER_STATUS_PVP : 0);

            data << citr2->name;
            data << citr2->guid;

            data << uint8(sObjectMgr.GetPlayer(citr2->guid) ? 1 : 0);
            data << (uint8)(citr2->group | (citr2->assistant ? 0x80 : 0));
        }

        data << m_leaderGuid;
        if (GetMembersCount() - 1)
        {
            data << uint8(m_lootMethod);
            if (m_lootMethod == MASTER_LOOT)
            {
                data << m_looterGuid;
            }
            else
            {
                data << uint64(0);
            }
            data << uint8(m_lootThreshold);
        }
        player->GetSession()->SendPacket(&data);
    }
}

void Group::UpdatePlayerOutOfRange(Player* pPlayer)
{
    if (!pPlayer || !pPlayer->IsInWorld())
    {
        return;
    }

    if (pPlayer->GetGroupUpdateFlag() == GROUP_UPDATE_FLAG_NONE)
    {
        return;
    }

    WorldPacket data;
    pPlayer->GetSession()->BuildPartyMemberStatsChangedPacket(pPlayer, &data);

    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* player = itr->getSource())
        {
            if (player != pPlayer && !player->HaveAtClient(pPlayer))
            {
                player->GetSession()->SendPacket(&data);
            }
        }
    }
}

void Group::BroadcastPacket(WorldPacket* packet, bool ignorePlayersInBGRaid, int group, ObjectGuid ignore)
{
    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pl = itr->getSource();
        if (!pl || (ignore && pl->GetObjectGuid() == ignore) || (ignorePlayersInBGRaid && pl->GetGroup() != this))
        {
            continue;
        }

        if (pl->GetSession() && (group == -1 || itr->getSubGroup() == group))
        {
            pl->GetSession()->SendPacket(packet);
        }
    }
}

void Group::BroadcastReadyCheck(WorldPacket* packet)
{
    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pl = itr->getSource();
        if (pl && pl->GetSession())
        {
            if (IsLeader(pl->GetObjectGuid()) || IsAssistant(pl->GetObjectGuid()))
            {
                pl->GetSession()->SendPacket(packet);
            }
        }
    }
}

void Group::OfflineReadyCheck()
{
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        Player* pl = sObjectMgr.GetPlayer(citr->guid);
        if (!pl || !pl->GetSession())
        {
            WorldPacket data(MSG_RAID_READY_CHECK_CONFIRM, 9);
            data << citr->guid;
            data << uint8(0);
            BroadcastReadyCheck(&data);
        }
    }
}
