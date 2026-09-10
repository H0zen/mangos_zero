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

#pragma once

#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "SharedDefines.h"
#include "Utilities/LinkedReference/Reference.h"
#include "UnitEvents.h"
#include "ObjectGuid.h"
#include <list>

class Unit;
class Creature;
class ThreatManager;
struct SpellEntry;

class ThreatCalcHelper
{
    public:

        static float CalcThreat(Unit* pHatedUnit, Unit* pHatingUnit, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell);
};

class HostileReference : public Reference<Unit, ThreatManager>
{
    public:

        HostileReference(Unit* pUnit, ThreatManager* pThreatManager, float pThreat);

        void addThreat(float pMod);

        void setThreat(float pThreat) { addThreat(pThreat - getThreat()); }

        void addThreatPercent(int32 pPercent)
        {

            addThreat(pPercent == -100 ? -iThreat : iThreat * pPercent / 100.0f);
        }

        float getThreat() const { return iThreat; }

        bool isOnline() const { return iOnline; }

        bool isAccessable() const { return iAccessible; }

        void setTempThreat(float pThreat) { iTempThreatModifyer = pThreat - getThreat(); if (iTempThreatModifyer != 0.0f) { addThreat(iTempThreatModifyer); } }

        void resetTempThreat()
        {
            if (iTempThreatModifyer != 0.0f)
            {
                addThreat(-iTempThreatModifyer); iTempThreatModifyer = 0.0f;
            }
        }

        float getTempThreatModifyer()
        {
            return iTempThreatModifyer;
        }

        void updateOnlineStatus();

        void setOnlineOfflineState(bool pIsOnline);

        void setAccessibleState(bool pIsAccessible);

        bool operator ==(const HostileReference& pHostileReference) const { return pHostileReference.getUnitGuid() == getUnitGuid(); }

        ObjectGuid getUnitGuid() const { return iUnitGuid; }

        Unit* getSourceUnit();

        void removeReference();

        HostileReference* next()
        {
            return ((HostileReference*) Reference<Unit, ThreatManager>::next());
        }

        void targetObjectBuildLink() override;

        void targetObjectDestroyLink() override;

        void sourceObjectDestroyLink() override;

    private:

        void fireStatusChanged(ThreatRefStatusChangeEvent& pThreatRefStatusChangeEvent);

    private:
        float iThreat;
        float iTempThreatModifyer;
        ObjectGuid iUnitGuid = 0;
        bool iOnline;
        bool iAccessible;
};

class ThreatManager;

typedef std::list<HostileReference*> ThreatList;

class ThreatContainer
{
    private:
        ThreatList iThreatList;
        bool iDirty;

    protected:
        friend class ThreatManager;

        void remove(HostileReference* pRef) { iThreatList.remove(pRef); }

        void addReference(HostileReference* pHostileReference) { iThreatList.push_back(pHostileReference); }

        void clearReferences();

        void update();

    public:

        ThreatContainer()
        {
            iDirty = false;
        }

        ~ThreatContainer()
        {
            clearReferences();
        }

        HostileReference* addThreat(Unit* pVictim, float pThreat);

        void modifyThreatPercent(Unit* pVictim, int32 percent);

        HostileReference* selectNextVictim(Creature* pAttacker, HostileReference* pCurrentVictim);

        void setDirty(bool pDirty) { iDirty = pDirty; }

        bool isDirty() const { return iDirty; }

        bool empty() const { return(iThreatList.empty()); }

        HostileReference* getMostHated()
        {
            return iThreatList.empty() ? nullptr : iThreatList.front();
        }

        HostileReference* getReferenceByTarget(Unit* pVictim);

        ThreatList const& getThreatList() const { return iThreatList; }
};

class ThreatManager
{
    public:
        friend class HostileReference;

        explicit ThreatManager(Unit* pOwner);

        ~ThreatManager()
        {
            clearReferences();
        }

        void clearReferences();

        void addThreat(Unit* pVictim, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell);

        void addThreat(Unit* pVictim, float threat) { addThreat(pVictim, threat, false, SPELL_SCHOOL_MASK_NONE, nullptr); }

        void addThreatDirectly(Unit* pVictim, float threat);

        void modifyThreatPercent(Unit* pVictim, int32 pPercent);

        float getThreat(Unit* pVictim, bool pAlsoSearchOfflineList = false);

        bool isThreatListEmpty() const { return iThreatContainer.empty(); }

        void processThreatEvent(ThreatRefStatusChangeEvent* threatRefStatusChangeEvent);

        HostileReference* getCurrentVictim()
        {
            return iCurrentVictim;
        }

        Unit* getOwner() const
        {
            return iOwner;
        }

        Unit* getHostileTarget();

        void tauntApply(Unit* pTaunter);

        void tauntFadeOut(Unit* pTaunter);

        void setCurrentVictim(HostileReference* pHostileReference);

        void setDirty(bool pDirty) { iThreatContainer.setDirty(pDirty); }

        ThreatList const& getThreatList() const { return iThreatContainer.getThreatList(); }

    private:
        HostileReference* iCurrentVictim;
        Unit* iOwner;
        ThreatContainer iThreatContainer;
        ThreatContainer iThreatOfflineContainer;
};
