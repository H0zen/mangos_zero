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
#include "Stats/Experience.h"
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
#include "Corpse.h"

bool Player::isHonorOrXPTarget(Unit* pVictim) const
{
    uint32 v_level = pVictim->getLevel();
    uint32 k_grey  = xp::GreyLevel(getLevel());

    if (v_level <= k_grey)
    {
        return false;
    }

    if (IsCreature(pVictim))
    {
        if (((Creature*)pVictim)->IsTotem() ||
            ((Creature*)pVictim)->IsPet() ||
            ((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_XP_AT_KILL)
        {
            return false;
        }
    }
    return true;
}

void Player::RewardSinglePlayerAtKill(Unit* pVictim)
{
    bool PvP = pVictim->IsCharmedOwnedByPlayerOrPlayer();

    uint32 xp = PvP ? 0
                    : xp::FromKill(getLevel(), xp::QuarryOf(*pVictim),
                                   sWorld.getConfig(CONFIG_FLOAT_RATE_XP_KILL));

    RewardHonor(pVictim, 1);

    if (!PvP)
    {
        RewardReputation(pVictim, 1);
        GiveXP(xp, pVictim);

        if (Pet* pet = GetPet())
        {
            pet->GivePetXP(xp);
        }

        if (IsCreature(pVictim))
        {
            m_journal.CreatureKilled(((Creature*)pVictim)->GetCreatureInfo(), pVictim->GetObjectGuid());
        }
    }
}

void Player::RewardPlayerAndGroupAtEvent(uint32 creature_id, Occupant* pRewardSource)
{
    MANGOS_ASSERT((!GetGroup() || pRewardSource));

    ObjectGuid creature_guid = pRewardSource &&IsCreature(pRewardSource) ? pRewardSource->GetObjectGuid() : 0;

    if (Group* pGroup = GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
            {
                continue;
            }

            if (!pGroupGuy->IsAtGroupRewardDistance(pRewardSource))
            {
                continue;
            }

            if (pGroupGuy->IsAlive() || !pGroupGuy->HasPlayerFlag(PLAYER_FLAGS_GHOST))
            {
                pGroupGuy->Journal().KillCredited(creature_id, creature_guid);
            }
        }
    }
    else
    {
        m_journal.KillCredited(creature_id, creature_guid);
    }
}

void Player::RewardPlayerAndGroupAtCast(Occupant* pRewardSource, uint32 spellid)
{

    if (Group* pGroup = GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
            {
                continue;
            }

            if (!pGroupGuy->IsAtGroupRewardDistance(pRewardSource))
            {
                continue;
            }

            if (pGroupGuy->IsAlive() || !pGroupGuy->HasPlayerFlag(PLAYER_FLAGS_GHOST))
            {
                pGroupGuy->Journal().CastCredited(pRewardSource->GetEntry(), pRewardSource->GetObjectGuid(), spellid, pGroupGuy == this);
            }
        }
    }
    else
    {
        m_journal.CastCredited(pRewardSource->GetEntry(), pRewardSource->GetObjectGuid(), spellid);
    }
}

bool Player::IsAtGroupRewardDistance(Occupant const* pRewardSource) const
{
    if (InReach(*pRewardSource, *this, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE)))
    {
        return true;
    }

    if (IsAlive())
    {
        return false;
    }

    Corpse* corpse = GetCorpse();
    if (!corpse)
    {
        return false;
    }

    return InReach(*pRewardSource, *corpse, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE));
}
