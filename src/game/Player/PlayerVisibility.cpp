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

#include <set>
#include "Reaction.h"
#include "Metrics/ServerMetrics.h"
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
#include "Corpse.h"

bool Player::IsVisibleInGridForPlayer(Player* pl) const
{

    if (pl->isGameMaster() && GetSession()->GetSecurity() <= pl->GetSession()->GetSecurity())
    {
        return true;
    }

    if (IsInSameRaidWith(pl))
    {
        return true;
    }

    if (pl->IsAlive() || pl->m_deathTimer > 0)
    {
        return IsAlive() || m_deathTimer > 0;
    }

    if (!(IsAlive() || m_deathTimer > 0) && IsFriendly(*this, *pl))
    {
        return true;
    }

    if (IsAlive())
    {
        if (Corpse* corpse = pl->GetCorpse())
        {

            if (InReach(*corpse, *this, (20 + 25) * sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO)))
            {
                return true;
            }
        }
    }

    return false;
}

bool Player::IsVisibleGloballyFor(Player* u) const
{
    if (!u)
    {
        return false;
    }

    if (u == this)
    {
        return true;
    }

    if (GetVisibility() == VISIBILITY_ON)
    {
        return true;
    }

    if (u->GetSession()->GetSecurity() > SEC_PLAYER)
    {
        return GetSession()->GetSecurity() <= u->GetSession()->GetSecurity();
    }

    if (GetVisibility() == VISIBILITY_OFF)
    {
        return false;
    }

    return true;
}

inline void BeforeVisibilityDestroy(Occupant* o, Player* p)
{
    if (Creature* t = static_cast<Creature*>(o))
    {
        if (p->GetPetGuid() == t->GetObjectGuid() && t->IsPet())
        {
            ((Pet*)t)->Unsummon(PET_SAVE_REAGENTS);
        }
    }
}

void Player::Remember(Occupant* target)
{

    GameObject* platform = static_cast<GameObject*>(target);
    if (platform && platform->IsMovingPlatform())
    {
        m_clientPlatforms.insert(platform->GetObjectGuid());
        return;
    }

    m_clientGUIDs.insert(target->GetObjectGuid());
}

void Player::UpdateVisibilityOf(Occupant const* viewPoint, Occupant* target)
{
    if (HaveAtClient(target))
    {
        if (!target->IsVisibleForInState(this, viewPoint, true))
        {
            ObjectGuid t_guid = target->GetObjectGuid();

            if (IsCreature(target))
            {
                BeforeVisibilityDestroy(target, this);
            }

            target->DestroyForPlayer(this);
            m_clientGUIDs.erase(t_guid);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(2p): %s out of range for player %u. Distance = %f", GuidString(t_guid).c_str(), GetGUIDLow(), Where().DistanceTo(target->Where()));
        }
    }
    else
    {
        if (target->IsVisibleForInState(this, viewPoint, false))
        {
            target->SendCreateUpdateToPlayer(this);
            Remember(target);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(2p): %s is visible now for player %u. Distance = %f", target->GetGuidStr().c_str(), GetGUIDLow(), Where().DistanceTo(target->Where()));

            if (target != this && IsType(target, TYPEMASK_UNIT))
            {
                SendAuraDurationsForTarget((Unit*)target);
            }
        }
    }
}

void Player::UpdateVisibilityOf(Occupant const* viewPoint, Occupant* target, UpdateData& data, std::set<Occupant*>& visibleNow)
{
    if (HaveAtClient(target))
    {
        if (!target->IsVisibleForInState(this, viewPoint, true))
        {
            BeforeVisibilityDestroy(target, this);

            ObjectGuid t_guid = target->GetObjectGuid();

            target->BuildOutOfRangeUpdateBlock(&data);
            m_clientGUIDs.erase(t_guid);
            metrics::Server().sightDestroys.Add();

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(4p): %s is out of range for %s. Distance = %f", GuidString(t_guid).c_str(), GetGuidStr().c_str(), Where().DistanceTo(target->Where()));
        }
    }
    else
    {
        if (target->IsVisibleForInState(this, viewPoint, false))
        {
            visibleNow.insert(target);
            target->BuildCreateUpdateBlockForPlayer(&data, this);
            metrics::Server().sightCreates.Add();
            Remember(target);
            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(4p): %s is visible now for %s. Distance = %f", target->GetGuidStr().c_str(), GetGuidStr().c_str(), Where().DistanceTo(target->Where()));
        }
    }
}
