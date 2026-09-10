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
#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MovementGenerator.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Movement/Spline/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include <math.h>
#include <stdarg.h>

bool Unit::CanHaveThreatList(bool ignoreAliveState) const
{

    if (!IsCreature(this))
    {
        return false;
    }

    if (!IsAlive() && !ignoreAliveState)
    {
        return false;
    }

    Creature const* creature = ((Creature const*)this);

    if (creature->IsTotem())
    {
        return false;
    }

    if (creature->IsPet() && (creature->GetOwnerGuid() != 0 && GuidHigh(creature->GetOwnerGuid()) == HIGHGUID_PLAYER))
    {
        return false;
    }

    if ((creature->GetCharmerGuid() != 0 && GuidHigh(creature->GetCharmerGuid()) == HIGHGUID_PLAYER))
    {
        return false;
    }

    return true;
}

float Unit::ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask)
{
    if (!HasAuraType(SPELL_AURA_MOD_THREAT))
    {
        return threat;
    }

    if (schoolMask == SPELL_SCHOOL_MASK_NONE)
    {
        return threat;
    }

    SpellSchools school = GetFirstSchoolInMask(schoolMask);

    return threat * m_threatModifier[school];
}

void Unit::AddThreat(Unit* pVictim, float threat , bool crit , SpellSchoolMask schoolMask , SpellEntry const* threatSpell )
{

    if (CanHaveThreatList())
    {
        m_ThreatManager.addThreat(pVictim, threat, crit, schoolMask, threatSpell);
    }
}

void Unit::DeleteThreatList()
{
    m_ThreatManager.clearReferences();
}

void Unit::TauntApply(Unit* taunter)
{
    MANGOS_ASSERT(IsCreature(this));

    if (!taunter || (IsPlayer(taunter) && ((Player*)taunter)->isGameMaster()))
    {
        return;
    }

    if (!CanHaveThreatList())
    {
        return;
    }

    Unit* target = getVictim();

    if (target && target == taunter)
    {
        return;
    }

    if (!hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED) && !IsSecondChoiceTarget(taunter, true))
    {
        if (GetTargetGuid() || !target)
        {
            SetInFront(taunter);
        }

        if (((Creature*)this)->AI())
        {
            ((Creature*)this)->AI()->AttackStart(taunter);
        }
    }

    m_ThreatManager.tauntApply(taunter);
}

void Unit::TauntFadeOut(Unit* taunter)
{
    MANGOS_ASSERT(IsCreature(this));

    if (!taunter || (IsPlayer(taunter) && ((Player*)taunter)->isGameMaster()))
    {
        return;
    }

    if (!CanHaveThreatList())
    {
        return;
    }

    Unit* target = getVictim();

    if (!target || target != taunter)
    {
        return;
    }

    if (m_ThreatManager.isThreatListEmpty())
    {
        m_fixateTargetGuid = 0;

        if (((Creature*)this)->AI())
        {
            ((Creature*)this)->AI()->EnterEvadeMode();
        }

        if (InstanceData* mapInstance = GetInstanceData())
        {
            mapInstance->OnCreatureEvade((Creature*)this);
        }

        static_cast<Creature*>(this)->Links().Evaded();

        return;
    }

    m_ThreatManager.tauntFadeOut(taunter);
    target = m_ThreatManager.getHostileTarget();

    if (target && target != taunter)
    {
        if (GetTargetGuid())
        {
            SetInFront(target);
        }

        if (((Creature*)this)->AI())
        {
            ((Creature*)this)->AI()->AttackStart(target);
        }
    }
}

void Unit::FixateTarget(Unit* pVictim)
{
    if (!pVictim)
    {
        m_fixateTargetGuid = 0;
    }
    else if (pVictim->IsTargetableForAttack())
    {
        m_fixateTargetGuid = pVictim->GetObjectGuid();
    }

    SelectHostileTarget();
}

bool Unit::IsSecondChoiceTarget(Unit* pTarget, bool checkThreatArea)
{
    MANGOS_ASSERT(pTarget && IsCreature(this));

    return pTarget->IsImmuneToDamage(GetMeleeDamageSchoolMask()) ||
        pTarget->hasNegativeAuraWithInterruptFlag(AURA_INTERRUPT_FLAG_DAMAGE) ||
        (checkThreatArea && ((Creature*)this)->IsOutOfThreatArea(pTarget));
}

bool Unit::SelectHostileTarget()
{

    MANGOS_ASSERT(IsCreature(this));

    if (!this->IsAlive())
    {
        return false;
    }

    if (!((Creature*)this)->AI())
    {
        return false;
    }

    Unit* target = nullptr;
    Unit* oldTarget = getVictim();

    if (m_fixateTargetGuid)
    {
        if (oldTarget && oldTarget->GetObjectGuid() == m_fixateTargetGuid)
        {
            target = oldTarget;
        }
        else
        {
            Unit* pFixateTarget = GetMap()->GetUnit(m_fixateTargetGuid);
            if (pFixateTarget && pFixateTarget->IsAlive() && !IsSecondChoiceTarget(pFixateTarget, true))
            {
                target = pFixateTarget;
            }
        }
    }

    if (!target)
    {

        for (const auto* aura : GetAurasByRecency(SPELL_AURA_MOD_TAUNT))
        {
            Unit* caster = aura->GetCaster();
            if (caster && caster->Where().ShareFrame(this->Where()) &&
                caster->IsTargetableForAttack() && caster->isInAccessablePlaceFor(static_cast<Creature*>(this)) &&
                !IsSecondChoiceTarget(caster, true))
            {
                target = caster;
                break;
            }
        }
    }

    if (!target && !m_ThreatManager.isThreatListEmpty())
    {
        target = m_ThreatManager.getHostileTarget();
    }

    if (target)
    {
        if (!hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
        {
            SetInFront(target);
            if (oldTarget != target)
            {
                ((Creature*)this)->AI()->AttackStart(target);
            }

            if (!GetMotionMaster()->GetCurrent()->IsReachable())
            {

                RemoveAurasOfType(SPELL_AURA_MOD_TAUNT);

                if (m_ThreatManager.getThreatList().size() < 2)
                {

                    ((Creature*)this)->AI()->EnterEvadeMode();
                }
                else
                {

                    m_HostileRefManager.deleteReference(target);
                    m_ThreatManager.modifyThreatPercent(target, -101);

                    AttackStop(true);
                }

                return false;
            }
        }
        return true;
    }

    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_TAUNT) || m_dummyCombatState)
    {
        return false;
    }

    if (GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
    {
        for (AttackerSet::const_iterator itr = m_attackers.begin(); itr != m_attackers.end(); ++itr)
        {
            if ((*itr)->Where().ShareFrame(this->Where()) && (*itr)->IsTargetableForAttack() && (*itr)->isInAccessablePlaceFor((Creature*)this))
            {
                return false;
            }
        }
    }

    m_fixateTargetGuid = 0;
    ((Creature*)this)->AI()->EnterEvadeMode();

    if (InstanceData* mapInstance = GetInstanceData())
    {
        mapInstance->OnCreatureEvade((Creature*)this);
    }

    static_cast<Creature*>(this)->Links().Evaded();

    return false;
}
