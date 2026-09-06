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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "BattleGroundStay.h"

#include "BattleGround/BattleGroundMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"

Team BattleGroundStay::Side() const
{
    return m_side ? m_side : m_owner.GetTeam();
}

BattleGround* BattleGroundStay::Ground() const
{
    if (!InOne())
    {
        return nullptr;
    }

    return sBattleGroundMgr.GetBattleGround(m_instanceId, m_kind);
}

bool BattleGroundStay::TeleportBack()
{
    return m_owner.TeleportTo(m_cameFrom);
}

bool BattleGroundStay::MayJoin() const
{
    // the deserter's debuff keeps him out
    return m_owner.GetDummyAura(26013) == nullptr;
}

void BattleGroundStay::RecordTheWayBack(Player* leader /*= nullptr*/)
{
    // a chat command, or joining alone, means he leads himself back
    if (!leader || !leader->IsInWorld() || leader->IsTaxiFlying() ||
        leader->GetMap()->IsDungeon() || leader->GetMap()->IsBattleGround())
    {
        leader = &m_owner;
    }

    if (leader->IsInWorld() && !leader->IsTaxiFlying())
    {
        if (leader->GetMap()->IsDungeon())
        {
            // out of a dungeon he comes back to that dungeon's graveyard
            if (WorldSafeLocsEntry const* grave = sObjectMgr.GetClosestGraveYard(
                    leader->Where().X(), leader->Where().Y(), leader->Where().Z(),
                    leader->GetMapId(), leader->GetTeam()))
            {
                m_cameFrom = Geometry::Placement::Somewhere(grave->map_id,
                                                            Geometry::Vector3(grave->x, grave->y, grave->z), 0.0f);
                m_unsaved = true;
                return;
            }

            sLog.outError("BattleGroundStay: Dungeon map %u has no linked graveyard, setting home location as entry point.",
                          leader->GetMapId());
        }
        else if (!leader->GetMap()->IsBattleGround())
        {
            m_cameFrom = Geometry::Placement::Somewhere(leader->GetMapId(),
                                                        Geometry::Vector3(leader->Where().X(), leader->Where().Y(), leader->Where().Z()),
                                                        leader->Where().Facing());
            m_unsaved = true;
            return;
        }
    }

    // where nothing else can be worked out, his inn
    m_cameFrom = Geometry::Placement::Somewhere(m_owner.Home().MapId(),
                                                Geometry::Vector3(m_owner.Home().X(), m_owner.Home().Y(), m_owner.Home().Z()),
                                                0.0f);
    m_unsaved = true;
}

void BattleGroundStay::Leave(bool teleportBack /*= true*/)
{
    BattleGround* ground = Ground();
    if (!ground)
    {
        return;
    }

    ground->RemovePlayerAtLeave(m_owner.GetObjectGuid(), teleportBack, true);

    // after the removal, so that he is alive again for the cast
    if (m_owner.isGameMaster() || !sWorld.getConfig(CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER))
    {
        return;
    }

    if (ground->GetStatus() != STATUS_IN_PROGRESS && ground->GetStatus() != STATUS_WAIT_JOIN)
    {
        return;
    }

    // he may already be on his way out, in which case the debuff waits for him
    if (m_owner.IsBeingTeleportedFar())
    {
        m_owner.ScheduleDelayedOperation(DELAYED_SPELL_CAST_DESERTER);
        return;
    }

    m_owner.CastSpell(&m_owner, 26013, true);               // Deserter
}
