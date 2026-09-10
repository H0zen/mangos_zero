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

#include "ReputationMgr.h"
#include "DBCStores.h"
#include "Player.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"

const int32 ReputationMgr::PointsInRank[MAX_REPUTATION_RANK] = {36000, 3000, 3000, 3000, 6000, 12000, 21000, 1000};

ReputationRank ReputationMgr::ReputationToRank(int32 standing)
{
    int32 limit = Reputation_Cap + 1;
    for (int i = MAX_REPUTATION_RANK - 1; i >= MIN_REPUTATION_RANK; --i)
    {
        limit -= PointsInRank[i];
        if (standing >= limit)
        {
            return ReputationRank(i);
        }
    }
    return MIN_REPUTATION_RANK;
}

int32 ReputationMgr::GetReputation(uint32 faction_id) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

    if (!factionEntry)
    {
        sLog.outError("ReputationMgr::GetReputation: Can't get reputation of %s for unknown faction (faction id) #%u.", m_player->GetName(), faction_id);
        return 0;
    }

    return GetReputation(factionEntry);
}

int32 ReputationMgr::GetBaseReputation(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
    {
        return 0;
    }

    uint32 raceMask = m_player->getRaceMask();
    uint32 classMask = m_player->getClassMask();

    int idx = factionEntry->GetIndexFitTo(raceMask, classMask);

    return idx >= 0 ? factionEntry->ReputationBase[idx] : 0;
}

int32 ReputationMgr::GetReputation(FactionEntry const* factionEntry) const
{

    if (!factionEntry)
    {
        return 0;
    }

    if (FactionState const* state = GetState(factionEntry))
    {
        return GetBaseReputation(factionEntry) + state->Standing;
    }

    return 0;
}

ReputationRank ReputationMgr::GetRank(FactionEntry const* factionEntry) const
{
    int32 reputation = GetReputation(factionEntry);
    return ReputationToRank(reputation);
}

ReputationRank ReputationMgr::GetBaseRank(FactionEntry const* factionEntry) const
{
    int32 reputation = GetBaseReputation(factionEntry);
    return ReputationToRank(reputation);
}

void ReputationMgr::ApplyForceReaction(uint32 faction_id, ReputationRank rank, bool apply)
{
    if (apply)
    {
        m_forcedReactions[faction_id] = rank;
    }
    else
    {
        m_forcedReactions.erase(faction_id);
    }
}

uint32 ReputationMgr::GetDefaultStateFlags(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
    {
        return 0;
    }

    uint32 raceMask = m_player->getRaceMask();
    uint32 classMask = m_player->getClassMask();

    int idx = factionEntry->GetIndexFitTo(raceMask, classMask);

    return idx >= 0 ? factionEntry->ReputationFlags[idx] : 0;
}

void ReputationMgr::SendForceReactions()
{
    WorldPacket data;
    data.Initialize(SMSG_SET_FORCED_REACTIONS, 4 + m_forcedReactions.size() * (4 + 4));
    data << uint32(m_forcedReactions.size());
    for (ForcedReactions::const_iterator itr = m_forcedReactions.begin(); itr != m_forcedReactions.end(); ++itr)
    {
        data << uint32(itr->first);
        data << uint32(itr->second);
    }
    m_player->SendDirectMessage(&data);
}

void ReputationMgr::SendState(FactionState const* faction)
{
    uint32 count = 1;

    WorldPacket data(SMSG_SET_FACTION_STANDING, (16));
    size_t p_count = data.wpos();
    data << (uint32) count;

    data << (uint32) faction->ReputationListID;
    data << (uint32) faction->Standing;

    for (FactionStateList::iterator itr = m_factions.begin(); itr != m_factions.end(); ++itr)
    {
        FactionState &subFaction = itr->second;
        if (subFaction.needSend)
        {
            subFaction.needSend = false;
            if (subFaction.ReputationListID != faction->ReputationListID)
            {
                data << uint32(subFaction.ReputationListID);
                data << uint32(subFaction.Standing);

                ++count;
            }
        }
    }

    data.put<uint32>(p_count, count);
    m_player->SendDirectMessage(&data);
}

struct rep
{
    uint8 flags;
    uint32 standing;
};

void ReputationMgr::SendInitialReputations()
{
    WorldPacket data(SMSG_INITIALIZE_FACTIONS, (4 + 64 * 5));
    data << uint32(0x00000040);

    RepListID a = 0;

    for (FactionStateList::iterator itr = m_factions.begin(); itr != m_factions.end(); ++itr)
    {

        for (; a != itr->first; ++a)
        {
            data << uint8(0x00);
            data << uint32(0x00000000);
        }

        data << uint8(itr->second.Flags);
        data << uint32(itr->second.Standing);

        itr->second.needSend = false;

        ++a;
    }

    for (; a != 64; ++a)
    {
        data << uint8(0x00);
        data << uint32(0x00000000);
    }

    m_player->SendDirectMessage(&data);
}

void ReputationMgr::SendVisible(FactionState const* faction) const
{
    if (m_player->GetSession()->PlayerLoading())
    {
        return;
    }

    WorldPacket data(SMSG_SET_FACTION_VISIBLE, 4);
    data << faction->ReputationListID;
    m_player->SendDirectMessage(&data);
}

void ReputationMgr::Initialize()
{
    m_factions.clear();

    for (unsigned int i = 1; i < sFactionStore.GetNumRows(); ++i)
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry(i);

        if (factionEntry && (factionEntry->ReputationIndex >= 0))
        {
            FactionState newFaction;
            newFaction.ID = factionEntry->ID;
            newFaction.ReputationListID = factionEntry->ReputationIndex;
            newFaction.Standing = 0;
            newFaction.Flags = GetDefaultStateFlags(factionEntry);
            newFaction.needSend = true;
            newFaction.needSave = true;

            m_factions[newFaction.ReputationListID] = newFaction;
        }
    }
}

bool ReputationMgr::SetReputation(FactionEntry const* factionEntry, int32 standing, bool incremental)
{

    bool res = false;

    if (const RepSpilloverTemplate* repTemplate = sObjectMgr.GetRepSpilloverTemplate(factionEntry->ID))
    {
        for (uint32 i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
        {
            if (repTemplate->faction[i])
            {
                if (m_player->GetReputationRank(repTemplate->faction[i]) <= ReputationRank(repTemplate->faction_rank[i]))
                {

                    int32 spilloverRep = standing * repTemplate->faction_rate[i];
                    SetOneFactionReputation(sFactionStore.LookupEntry(repTemplate->faction[i]), spilloverRep, incremental);
                }
            }
        }
    }

    FactionStateList::iterator faction = m_factions.find(factionEntry->ReputationIndex);
    if (faction != m_factions.end())
    {
        res = SetOneFactionReputation(factionEntry, standing, incremental);

        SendState(&faction->second);
    }
    return res;
}

bool ReputationMgr::SetOneFactionReputation(FactionEntry const* factionEntry, int32 standing, bool incremental)
{
    FactionStateList::iterator itr = m_factions.find(factionEntry->ReputationIndex);
    if (itr != m_factions.end())
    {
        FactionState &faction = itr->second;
        int32 BaseRep = GetBaseReputation(factionEntry);

        if (incremental)
        {
            standing += faction.Standing + BaseRep;
        }

        if (standing > Reputation_Cap)
        {
            standing = Reputation_Cap;
        }
        else if (standing < Reputation_Bottom)
        {
            standing = Reputation_Bottom;
        }

        ReputationRank oldRank = ReputationToRank(faction.Standing + BaseRep);

        faction.Standing = standing - BaseRep;
        faction.needSend = true;
        faction.needSave = true;

        SetVisible(&faction);

        ReputationRank newRank = ReputationToRank(standing);

        if (newRank != oldRank)
        {
            if (newRank <= REP_HOSTILE)
            {
                SetAtWar(&itr->second, true);
            }
            else if (newRank >= REP_FRIENDLY)
            {
                SetAtWar(&itr->second, false);
            }
        }

        m_player->Journal().ReputationNowIs(factionEntry);

        return true;
    }
    return false;
}

void ReputationMgr::SetVisible(FactionTemplateEntry const* factionTemplateEntry)
{
    if (!factionTemplateEntry->Faction)
    {
        return;
    }

    if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionTemplateEntry->Faction))
    {
        SetVisible(factionEntry);
    }
}

void ReputationMgr::SetVisible(FactionEntry const* factionEntry)
{
    if (factionEntry->ReputationIndex < 0)
    {
        return;
    }

    FactionStateList::iterator itr = m_factions.find(factionEntry->ReputationIndex);
    if (itr == m_factions.end())
    {
        return;
    }

    SetVisible(&itr->second);
}

void ReputationMgr::SetVisible(FactionState* faction)
{

    if (faction->Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN))
    {
        return;
    }

    if (faction->Flags & FACTION_FLAG_VISIBLE)
    {
        return;
    }

    faction->Flags |= FACTION_FLAG_VISIBLE;
    faction->needSend = true;
    faction->needSave = true;

    SendVisible(faction);
}

void ReputationMgr::SetAtWar(RepListID repListID, bool on)
{
    FactionStateList::iterator itr = m_factions.find(repListID);
    if (itr == m_factions.end())
    {
        return;
    }

    if (itr->second.Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN))
    {
        return;
    }

    SetAtWar(&itr->second, on);
}

void ReputationMgr::SetAtWar(FactionState* faction, bool atWar)
{

    if (atWar && (faction->Flags & FACTION_FLAG_PEACE_FORCED) && ReputationToRank(faction->Standing) > REP_HATED)
    {
        return;
    }

    if (((faction->Flags & FACTION_FLAG_AT_WAR) != 0) == atWar)
    {
        return;
    }

    if (atWar)
    {
        faction->Flags |= FACTION_FLAG_AT_WAR;
    }
    else
    {
        faction->Flags &= ~FACTION_FLAG_AT_WAR;
    }

    faction->needSend = true;
    faction->needSave = true;
}

void ReputationMgr::SetInactive(RepListID repListID, bool on)
{
    FactionStateList::iterator itr = m_factions.find(repListID);
    if (itr == m_factions.end())
    {
        return;
    }

    SetInactive(&itr->second, on);
}

void ReputationMgr::SetInactive(FactionState* faction, bool inactive)
{

    if (inactive && ((faction->Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN)) || !(faction->Flags & FACTION_FLAG_VISIBLE)))
    {
        return;
    }

    if (((faction->Flags & FACTION_FLAG_INACTIVE) != 0) == inactive)
    {
        return;
    }

    if (inactive)
    {
        faction->Flags |= FACTION_FLAG_INACTIVE;
    }
    else
    {
        faction->Flags &= ~FACTION_FLAG_INACTIVE;
    }

    faction->needSend = true;
    faction->needSave = true;
}

void ReputationMgr::LoadFromDB(QueryResult* result)
{

    Initialize();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            FactionEntry const* factionEntry = sFactionStore.LookupEntry(fields[0].GetUInt32());
            if (factionEntry && (factionEntry->ReputationIndex >= 0))
            {
                FactionState* faction = &m_factions[factionEntry->ReputationIndex];

                faction->Standing = int32(fields[1].GetUInt32());

                uint32 dbFactionFlags = fields[2].GetUInt32();

                if (dbFactionFlags & FACTION_FLAG_VISIBLE)
                {
                    SetVisible(faction);
                }

                if (dbFactionFlags & FACTION_FLAG_INACTIVE)
                {
                    SetInactive(faction, true);
                }

                if (dbFactionFlags & FACTION_FLAG_AT_WAR)
                {
                    SetAtWar(faction, true);
                }
                else
                {

                    if (faction->Flags & FACTION_FLAG_VISIBLE)
                    {
                        SetAtWar(faction, false);
                    }
                }

                ForcedReactions::const_iterator forceItr = m_forcedReactions.find(factionEntry->ID);
                if (forceItr != m_forcedReactions.end())
                {
                    if (forceItr->second <= REP_HOSTILE)
                    {
                        SetAtWar(faction, true);
                    }
                }
                else if (GetRank(factionEntry) <= REP_HOSTILE)
                {
                    SetAtWar(faction, true);
                }

                if (faction->Flags == dbFactionFlags)
                {
                    faction->needSend = false;
                    faction->needSave = false;
                }
            }
        }
        while (result->NextRow());

        delete result;
    }
}

void ReputationMgr::SaveToDB()
{
    static SqlStatementID delRep ;
    static SqlStatementID insRep ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delRep, "DELETE FROM `character_reputation` WHERE `guid` = ? AND `faction`=?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insRep, "INSERT INTO `character_reputation` (`guid`,`faction`,`standing`,`flags`) VALUES (?, ?, ?, ?)");

    for (FactionStateList::iterator itr = m_factions.begin(); itr != m_factions.end(); ++itr)
    {
        FactionState &faction = itr->second;
        if (faction.needSave)
        {
            stmtDel.PExecute(m_player->GetGUIDLow(), faction.ID);
            stmtIns.PExecute(m_player->GetGUIDLow(), faction.ID, faction.Standing, faction.Flags);
            faction.needSave = false;
        }
    }
}
