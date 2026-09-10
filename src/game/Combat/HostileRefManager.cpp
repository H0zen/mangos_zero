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

#include "HostileRefManager.h"
#include "ThreatManager.h"
#include "Unit.h"
#include "DBCStructure.h"
#include "SpellMgr.h"

HostileRefManager::HostileRefManager(Unit* pOwner) : iOwner(pOwner)
{
}

HostileRefManager::~HostileRefManager()
{
    deleteReferences();
}

void HostileRefManager::threatAssist(Unit* pVictim, float pThreat, SpellEntry const* pThreatSpell, bool pSingleTarget)
{
    uint32 size = pSingleTarget ? 1 : getSize();
    float threat = pThreat / size;
    HostileReference* ref = getFirst();
    while (ref)
    {
        ref->getSource()->addThreat(pVictim, threat, false, (pThreatSpell ? GetSpellSchoolMask(pThreatSpell) : SPELL_SCHOOL_MASK_NORMAL), pThreatSpell);

        ref = ref->next();
    }
}

void HostileRefManager::addThreatPercent(int32 pValue)
{
    HostileReference* ref;

    ref = getFirst();
    while (ref != nullptr)
    {
        ref->addThreatPercent(pValue);
        ref = ref->next();
    }
}

void HostileRefManager::setOnlineOfflineState(bool pIsOnline)
{
    HostileReference* ref;

    ref = getFirst();
    while (ref != nullptr)
    {
        ref->setOnlineOfflineState(pIsOnline);
        ref = ref->next();
    }
}

void HostileRefManager::updateThreatTables()
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        ref->updateOnlineStatus();
        ref = ref->next();
    }
}

void HostileRefManager::deleteReferences()
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        ref->removeReference();
        delete ref;
        ref = nextRef;
    }
}

void HostileRefManager::deleteReferencesForFaction(uint32 faction)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner()->getFactionTemplateEntry()->Faction == faction)
        {
            ref->removeReference();
            delete ref;
        }
        ref = nextRef;
    }
}

void HostileRefManager::deleteReference(Unit* pCreature)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner() == pCreature)
        {
            ref->removeReference();
            delete ref;
            break;
        }
        ref = nextRef;
    }
}

void HostileRefManager::setOnlineOfflineState(Unit* pCreature, bool pIsOnline)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner() == pCreature)
        {
            ref->setOnlineOfflineState(pIsOnline);
            break;
        }
        ref = nextRef;
    }
}
