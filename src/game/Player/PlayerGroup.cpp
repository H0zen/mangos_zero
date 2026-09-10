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

#include "Reaction.h"
#include "Utilities/Errors.h"
#include <vector>
#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
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
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>

bool Player::IsGroupVisibleFor(Player* p) const
{
    switch (sWorld.getConfig(CONFIG_UINT32_GROUP_VISIBILITY))
    {
        default:
            return IsInSameGroupWith(p);
        case 1:
            return IsInSameRaidWith(p);
        case 2:
            return GetTeam() == p->GetTeam();
    }
}

bool Player::IsInSameGroupWith(Player const* p) const
{
    return (p == this ||
        (GetGroup() != nullptr &&
        GetGroup()->SameSubGroup(this, p)));
}

void Player::UninviteFromGroup()
{
    Group* group = Invites().ToParty();
    if (!group)
    {
        return;
    }

    group->RemoveInvite(this);

    if (group->GetMembersCount() <= 1)
    {
        if (group->IsCreated())
        {
            group->Disband(true);
            sObjectMgr.RemoveGroup(group);
        }
        else
        {
            group->RemoveAllInvites();
        }

        delete group;
    }
}

void Player::RemoveFromGroup(Group* group, ObjectGuid guid, uint8 removeMethod)
{
    if (group)
    {
        if (group->RemoveMember(guid, removeMethod) <= 1)
        {

            sObjectMgr.RemoveGroup(group);
            delete group;

        }
    }
}

void Player::SetGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
    {
        m_group.unlink();
    }
    else
    {

        MANGOS_ASSERT(subgroup >= 0);
        m_group.link(group, this);
        m_group.setSubGroup((uint8)subgroup);
    }

}

void Player::SendUpdateToOutOfRangeGroupMembers()
{
    if (m_groupUpdateMask == GROUP_UPDATE_FLAG_NONE)
    {
        return;
    }
    if (Group* group = GetGroup())
    {
        group->UpdatePlayerOutOfRange(this);
    }

    m_groupUpdateMask = GROUP_UPDATE_FLAG_NONE;
    m_auraUpdateMask = 0;
    if (Pet* pet = GetPet())
    {
        pet->ResetAuraUpdateMask();
    }
}

Player* Player::GetNextRandomRaidMember(float radius)
{
    Group* pGroup = GetGroup();
    if (!pGroup)
    {
        return nullptr;
    }

    std::vector<Player*> nearMembers;
    nearMembers.reserve(pGroup->GetMembersCount());

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* Target = itr->getSource();

        if (Target && Target != this && InReach(*this, *Target, radius) &&
            !Target->HasInvisibilityAura() && !IsHostile(*this, *Target))
        {
            nearMembers.push_back(Target);
        }
    }

    if (nearMembers.empty())
    {
        return nullptr;
    }

    uint32 randTarget = urand(0, nearMembers.size() - 1);
    return nearMembers[randTarget];
}

PartyResult Player::CanUninviteFromGroup() const
{
    const Group* grp = GetGroup();
    if (!grp)
    {
        return ERR_NOT_IN_GROUP;
    }

    if (!grp->IsLeader(GetObjectGuid()) && !grp->IsAssistant(GetObjectGuid()))
    {
        return ERR_NOT_LEADER;
    }

    if (Battle().InOne())
    {
        return ERR_NOT_IN_GROUP;
    }

    return ERR_PARTY_RESULT_OK;
}

void Player::SetOriginalGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
    {
        m_originalGroup.unlink();
    }
    else
    {

        MANGOS_ASSERT(subgroup >= 0);
        m_originalGroup.link(group, this);
        m_originalGroup.setSubGroup((uint8)subgroup);
    }
}
