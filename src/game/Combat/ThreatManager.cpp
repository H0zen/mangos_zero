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
#include <list>
#include "ThreatManager.h"
#include "Unit.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "Player.h"
#include "ObjectLookup.h"
#include "UnitEvents.h"
#include "Cast/Recipe/RecipeBook.h"

float ThreatCalcHelper::CalcThreat(Unit* pHatedUnit, Unit* , float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* pThreatSpell)
{

    if (!threat)
    {
        return 0.0f;
    }

    if (pThreatSpell)
    {
        if (cast::RecipeOf(*pThreatSpell).Says().makesNoThreat)
        {
            return 0.0f;
        }

        if (Player* modOwner = pHatedUnit->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(pThreatSpell->ID, SPELLMOD_THREAT, threat);
        }

        if (crit)
        {
            threat *= pHatedUnit->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRITICAL_THREAT, schoolMask);
        }
    }

    threat = pHatedUnit->ApplyTotalThreatModifier(threat, schoolMask);
    return threat;
}

HostileReference::HostileReference(Unit* pUnit, ThreatManager* pThreatManager, float pThreat)
{
    iThreat = pThreat;
    iTempThreatModifyer = 0.0f;
    link(pUnit, pThreatManager);
    iUnitGuid = pUnit->GetObjectGuid();
    iOnline = true;
    iAccessible = true;
}

void HostileReference::targetObjectBuildLink()
{
    getTarget()->AddHatedBy(this);
}

void HostileReference::targetObjectDestroyLink()
{
    getTarget()->RemoveHatedBy(this);
}

void HostileReference::sourceObjectDestroyLink()
{
    setOnlineOfflineState(false);
}

void HostileReference::fireStatusChanged(ThreatRefStatusChangeEvent& pThreatRefStatusChangeEvent)
{
    if (getSource())
    {
        getSource()->processThreatEvent(&pThreatRefStatusChangeEvent);
    }
}

void HostileReference::addThreat(float pMod)
{
    iThreat += pMod;

    if (!isOnline())
    {
        updateOnlineStatus();
    }
    if (pMod != 0.0f)
    {
        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_THREAT_CHANGE, this, pMod);
        fireStatusChanged(event);
    }

    if (isValid() && pMod >= 0)
    {
        Unit* victim_owner = getTarget()->GetOwner();
        if (victim_owner && victim_owner->IsAlive())
        {
            getSource()->addThreat(victim_owner, 0.0f);
        }
    }
}

void HostileReference::updateOnlineStatus()
{
    bool online = false;
    bool accessible = false;

    if (!isValid())
    {
        if (Unit* target = ObjectLookup::GetUnit(*getSourceUnit(), getUnitGuid()))
        {
            link(target, getSource());
        }
    }

    if (isValid() &&
        ((!IsPlayer(getTarget()) || !((Player*)getTarget())->isGameMaster()) ||
        !getTarget()->IsTaxiFlying()))
    {
        Creature* creature = (Creature*) getSourceUnit();
        online = getTarget()->isInAccessablePlaceFor(creature);
        if (!online)
        {
            if (creature->AI()->canReachByRangeAttack(getTarget()))
            {
                online = true;
            }
        }
        else
        {
            accessible = true;
        }
    }
    setAccessibleState(accessible);
    setOnlineOfflineState(online);
}

void HostileReference::setOnlineOfflineState(bool pIsOnline)
{
    if (iOnline != pIsOnline)
    {
        iOnline = pIsOnline;
        if (!iOnline)
        {
            setAccessibleState(false);
        }

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_ONLINE_STATUS, this);
        fireStatusChanged(event);
    }
}

void HostileReference::setAccessibleState(bool pIsAccessible)
{
    if (iAccessible != pIsAccessible)
    {
        iAccessible = pIsAccessible;

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_ASSECCIBLE_STATUS, this);
        fireStatusChanged(event);
    }
}

void HostileReference::removeReference()
{
    invalidate();

    ThreatRefStatusChangeEvent event(UEV_THREAT_REF_REMOVE_FROM_LIST, this);
    fireStatusChanged(event);
}

Unit* HostileReference::getSourceUnit()
{
    return (getSource()->getOwner());
}

void ThreatContainer::clearReferences()
{
    for (ThreatList::const_iterator i = iThreatList.begin(); i != iThreatList.end(); ++i)
    {
        (*i)->unlink();
        delete(*i);
    }
    iThreatList.clear();
}

HostileReference* ThreatContainer::getReferenceByTarget(Unit* pVictim)
{
    HostileReference* result = nullptr;
    ObjectGuid guid = pVictim->GetObjectGuid();
    for (ThreatList::const_iterator i = iThreatList.begin(); i != iThreatList.end(); ++i)
    {
        if ((*i)->getUnitGuid() == guid)
        {
            result = (*i);
            break;
        }
    }

    return result;
}

HostileReference* ThreatContainer::addThreat(Unit* pVictim, float pThreat)
{
    HostileReference* ref = getReferenceByTarget(pVictim);
    if (ref)
    {
        ref->addThreat(pThreat);
    }
    return ref;
}

void ThreatContainer::modifyThreatPercent(Unit* pVictim, int32 pPercent)
{
    if (HostileReference* ref = getReferenceByTarget(pVictim))
    {
        if (pPercent < -100)
        {
            ref->removeReference();
            delete ref;
        }
        else
        {
            ref->addThreatPercent(pPercent);
        }
    }
}

bool HostileReferenceSortPredicate(const HostileReference* lhs, const HostileReference* rhs)
{

    return lhs->getThreat() > rhs->getThreat();
}

void ThreatContainer::update()
{
    if (iDirty && iThreatList.size() > 1)
    {
        iThreatList.sort(HostileReferenceSortPredicate);
    }
    iDirty = false;
}

HostileReference* ThreatContainer::selectNextVictim(Creature* pAttacker, HostileReference* pCurrentVictim)
{
    HostileReference* pCurrentRef = nullptr;
    bool found = false;
    bool onlySecondChoiceTargetsFound = false;
    bool checkedCurrentVictim = false;

    ThreatList::const_iterator lastRef = iThreatList.end();
    --lastRef;

    for (ThreatList::const_iterator iter = iThreatList.begin(); iter != iThreatList.end() && !found;)
    {
        pCurrentRef = (*iter);

        Unit* pTarget = pCurrentRef->getTarget();
        MANGOS_ASSERT(pTarget);

        if (!onlySecondChoiceTargetsFound && pAttacker->IsSecondChoiceTarget(pTarget, pCurrentRef == pCurrentVictim))
        {
            if (iter != lastRef)
            {
                ++iter;
            }
            else
            {

                onlySecondChoiceTargetsFound = true;
                iter = iThreatList.begin();
            }

            if (pCurrentRef == pCurrentVictim)
            {
                pCurrentVictim = nullptr;
            }

            continue;
        }

        if (!pAttacker->IsOutOfThreatArea(pTarget))
        {
            if (pCurrentVictim)
            {

                if (pCurrentVictim == pCurrentRef)
                {
                    found = true;
                    break;
                }

                if (!checkedCurrentVictim)
                {
                    Unit* pCurrentTarget = pCurrentVictim->getTarget();
                    MANGOS_ASSERT(pCurrentTarget);
                    if (pAttacker->IsSecondChoiceTarget(pCurrentTarget, true))
                    {

                        found = true;
                        break;
                    }
                    checkedCurrentVictim = true;
                }

                if (pCurrentRef->getThreat() <= 1.1f * pCurrentVictim->getThreat())
                {
                    pCurrentRef = pCurrentVictim;
                    found = true;
                    break;
                }

                if (pCurrentRef->getThreat() > 1.3f * pCurrentVictim->getThreat() ||
                    (pCurrentRef->getThreat() > 1.1f * pCurrentVictim->getThreat() && InMeleeReach(*pAttacker, *pTarget)))
                {

                    found = true;
                    break;
                }
            }
            else
            {
                found = true;
                break;
            }
        }
        ++iter;
    }
    if (!found)
    {
        pCurrentRef = nullptr;
    }

    return pCurrentRef;
}

ThreatManager::ThreatManager(Unit* owner)
    : iCurrentVictim(nullptr), iOwner(owner)
{
}

void ThreatManager::clearReferences()
{
    iThreatContainer.clearReferences();
    iThreatOfflineContainer.clearReferences();
    iCurrentVictim = nullptr;
}

void ThreatManager::addThreat(Unit* pVictim, float pThreat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* pThreatSpell)
{

    if (pVictim == getOwner())
    {
        return;
    }

    if (!pVictim || (IsPlayer(pVictim) && ((Player*)pVictim)->isGameMaster()))
    {
        return;
    }

    if (!pVictim->IsAlive() || !getOwner()->IsAlive())
    {
        return;
    }

    MANGOS_ASSERT(IsCreature(getOwner()));

    float threat = ThreatCalcHelper::CalcThreat(pVictim, iOwner, pThreat, crit, schoolMask, pThreatSpell);

    addThreatDirectly(pVictim, threat);
}

void ThreatManager::addThreatDirectly(Unit* pVictim, float threat)
{
    HostileReference* ref = iThreatContainer.addThreat(pVictim, threat);

    if (!ref)
    {
        ref = iThreatOfflineContainer.addThreat(pVictim, threat);
    }

    if (!ref)
    {

        HostileReference* hostileReference = new HostileReference(pVictim, this, 0);
        iThreatContainer.addReference(hostileReference);
        hostileReference->addThreat(threat);
        if (IsPlayer(pVictim) && ((Player*)pVictim)->isGameMaster())
        {
            hostileReference->setOnlineOfflineState(false);
        }
    }
}

void ThreatManager::modifyThreatPercent(Unit* pVictim, int32 pPercent)
{
    iThreatContainer.modifyThreatPercent(pVictim, pPercent);
}

Unit* ThreatManager::getHostileTarget()
{
    iThreatContainer.update();
    HostileReference* nextVictim = iThreatContainer.selectNextVictim((Creature*) getOwner(), getCurrentVictim());
    setCurrentVictim(nextVictim);
    return getCurrentVictim() != nullptr ? getCurrentVictim()->getTarget() : nullptr;
}

float ThreatManager::getThreat(Unit* pVictim, bool pAlsoSearchOfflineList)
{
    float threat = 0.0f;
    HostileReference* ref = iThreatContainer.getReferenceByTarget(pVictim);
    if (!ref && pAlsoSearchOfflineList)
    {
        ref = iThreatOfflineContainer.getReferenceByTarget(pVictim);
    }
    if (ref)
    {
        threat = ref->getThreat();
    }
    return threat;
}

void ThreatManager::tauntApply(Unit* pTaunter)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(pTaunter))
    {
        if (getCurrentVictim() && (ref->getThreat() < getCurrentVictim()->getThreat()))
        {

            if (ref->getTempThreatModifyer() == 0.0f)
            {
                ref->setTempThreat(getCurrentVictim()->getThreat());
            }
        }
    }
}

void ThreatManager::tauntFadeOut(Unit* pTaunter)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(pTaunter))
    {
        ref->resetTempThreat();
    }
}

void ThreatManager::setCurrentVictim(HostileReference* pHostileReference)
{
    iCurrentVictim = pHostileReference;
}

void ThreatManager::processThreatEvent(ThreatRefStatusChangeEvent* threatRefStatusChangeEvent)
{
    threatRefStatusChangeEvent->setThreatManager(this);

    HostileReference* hostileReference = threatRefStatusChangeEvent->getReference();

    switch (threatRefStatusChangeEvent->getType())
    {
        case UEV_THREAT_REF_THREAT_CHANGE:
            if ((getCurrentVictim() == hostileReference && threatRefStatusChangeEvent->getFValue() < 0.0f) ||
                (getCurrentVictim() != hostileReference && threatRefStatusChangeEvent->getFValue() > 0.0f))
            {
                setDirty(true);
            }
            break;
        case UEV_THREAT_REF_ONLINE_STATUS:
            if (!hostileReference->isOnline())
            {
                if (hostileReference == getCurrentVictim())
                {
                    setCurrentVictim(nullptr);
                    setDirty(true);
                }
                iThreatContainer.remove(hostileReference);
                iThreatOfflineContainer.addReference(hostileReference);
            }
            else
            {
                if (getCurrentVictim() && hostileReference->getThreat() > (1.1f * getCurrentVictim()->getThreat()))
                {
                    setDirty(true);
                }
                iThreatContainer.addReference(hostileReference);
                iThreatOfflineContainer.remove(hostileReference);
            }
            break;
        case UEV_THREAT_REF_REMOVE_FROM_LIST:
            if (hostileReference == getCurrentVictim())
            {
                setCurrentVictim(nullptr);
                setDirty(true);
            }
            if (hostileReference->isOnline())
            {
                iThreatContainer.remove(hostileReference);
            }
            else
            {
                iThreatOfflineContainer.remove(hostileReference);
            }
            break;
    }
}
