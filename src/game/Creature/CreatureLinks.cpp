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

#include "CreatureLinks.h"

#include "Creature.h"
#include "Map.h"

void CreatureLinks::Enrol(Map& map)
{
    if (sCreatureLinkingMgr.GetLinkedTriggerInformation(&m_owner))
    {
        map.GetCreatureLinkingHolder()->AddSlaveToHolder(&m_owner);
    }

    if (sCreatureLinkingMgr.IsLinkedEventTrigger(&m_owner))
    {
        m_listenedTo = true;
        map.GetCreatureLinkingHolder()->AddMasterToHolder(&m_owner);
    }
}

bool CreatureLinks::MayRespawn() const
{
    return !m_spawnWaits || m_owner.GetMap()->GetCreatureLinkingHolder()->CanSpawn(&m_owner);
}

void CreatureLinks::Tell(CreatureLinkingEvent what, Unit* enemy)
{
    if (!m_listenedTo)
    {
        return;
    }

    m_owner.GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(what, &m_owner, enemy);
}

void CreatureLinks::Aggroed(Unit* enemy)
{
    Tell(LINKING_EVENT_AGGRO, enemy);
}

void CreatureLinks::Evaded()
{
    Tell(LINKING_EVENT_EVADE);
}

void CreatureLinks::Died()
{
    Tell(LINKING_EVENT_DIE);
}

void CreatureLinks::Respawned()
{
    Tell(LINKING_EVENT_RESPAWN);
}

void CreatureLinks::Despawned()
{
    Tell(LINKING_EVENT_DESPAWN);
}

bool CreatureLinks::RefollowMaster()
{
    return m_listenedTo && m_owner.GetMap()->GetCreatureLinkingHolder()->TryFollowMaster(&m_owner);
}
