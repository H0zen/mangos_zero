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

#include <vector>
#include <list>
#include "Reaction.h"
#include "Utilities/MathDefines.h"
#include "PetAI.h"
#include "Errors.h"
#include "Pet.h"
#include "Player.h"
#include "DBCStores.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Creature.h"
#include "World.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

#include <cfloat>
#include "Cast/Recipe/RecipeBook.h"

int PetAI::Permissible(const Creature* creature)
{
    if (creature->IsPet())
    {
        return PERMIT_BASE_SPECIAL;
    }

    return PERMIT_BASE_NO;
}

PetAI::PetAI(Creature* c) : CreatureAI(c), i_tracker(TIME_INTERVAL_LOOK), inCombat(false), m_loiterUntilTime(0)
{
    m_AllySet.clear();
    UpdateAllies();
}

void PetAI::MoveInLineOfSight(Unit* u)
{
    if (m_creature->getVictim())
    {
        return;
    }

    if (m_creature->IsPet() && ((Pet*)m_creature)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
    {
        return;
    }

    if (!m_creature->GetCharmInfo() || !m_creature->GetCharmInfo()->HasReactState(REACT_AGGRESSIVE))
    {
        return;
    }

    if (u->IsTargetableForAttack() && IsHostile(*m_creature, *u) &&
        u->isInAccessablePlaceFor(m_creature))
    {
        float attackRadius = m_creature->GetAttackDistance(u);
        if (InReach(*m_creature, *u, attackRadius) && m_creature->Where().HeightGapTo(u->Where()) <= CREATURE_Z_ATTACK_RANGE)
        {
            if (HasLineOfSight(*m_creature, *u))
            {
                AttackStart(u);
            }
        }
    }
}

void PetAI::AttackStart(Unit* u)
{
    if (!u || (m_creature->IsPet() && ((Pet*)m_creature)->getPetType() == MINI_PET))
    {
        return;
    }

    if (m_creature->Attack(u, true))
    {

        m_creature->Pacing().Reckon(MOVE_RUN, false);

        inCombat = true;
    }
}

void PetAI::EnterEvadeMode()
{
}

bool PetAI::IsVisible(Unit* pl) const
{
    return _isVisible(pl);
}

bool PetAI::_needToStop() const
{

    if (m_creature->IsCharmed() && m_creature->getVictim() == m_creature->GetCharmer())
    {
        return true;
    }

    return !m_creature->getVictim()->IsTargetableForAttack();
}

void PetAI::_stopAttack()
{
    if (inCombat)
    {

        m_loiterUntilTime = getMSTime() + urand(1000, 2500);
        inCombat = false;
    }

    m_creature->GetMotionMaster()->Clear(false);
    m_creature->GetMotionMaster()->MoveIdle();
    m_creature->AttackStop();
}

void PetAI::SelectNextTarget(Unit* owner)
{
    if (!m_creature->GetCharmInfo()->HasReactState(REACT_PASSIVE) && !m_creature->GetCharmInfo()->HasCommandState(COMMAND_STAY))
    {
        std::list<Unit*> candidates;

        if (m_creature->GetCharmInfo()->HasReactState(REACT_DEFENSIVE))
        {
            for (Unit* attacker : owner->getAttackers())
            {
                candidates.push_back(attacker);
            }

            for (Unit* attacker : m_creature->getAttackers())
            {
                candidates.push_back(attacker);
            }
        }
        else if (m_creature->GetCharmInfo()->HasReactState(REACT_AGGRESSIVE))
        {
            float radius = m_creature->GetAttackDistance(m_creature);
            MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck u_check(m_creature, radius);
            MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck>
                searcher(candidates, u_check);
            Cell::VisitAllObjects(m_creature, searcher, radius);
        }

        Unit* nextTarget = nullptr;
        float closestDist = FLT_MAX;
        for (Unit* candidate : candidates)
        {
            if (candidate->IsAlive() &&
                candidate->IsTargetableForAttack() &&
                _isVisible(candidate) &&
                candidate->isInAccessablePlaceFor(m_creature) &&
                HasLineOfSight(*m_creature, *candidate))
            {
                float dist = m_creature->Where().DistanceTo(candidate->Where());
                if (dist < closestDist)
                {
                    closestDist = dist;
                    nextTarget = candidate;
                }
            }
        }

        if (nextTarget)
        {
            AttackStart(nextTarget);
        }
    }

}

void PetAI::UpdateAI(const uint32 diff)
{
    if (!m_creature->IsAlive())
    {
        return;
    }

    Unit* owner = m_creature->GetCharmerOrOwner();

    if (m_updateAlliesTimer <= diff)

    {
        UpdateAllies();
    }
    else
    {
        m_updateAlliesTimer -= diff;
    }

    if (inCombat && (!m_creature->getVictim() || (m_creature->IsPet() && ((Pet*)m_creature)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)))
    {
        _stopAttack();
    }

    if (m_creature->getVictim())
    {
        if (_needToStop())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "PetAI (guid = %u) is stopping attack.", m_creature->GetGUIDLow());
            _stopAttack();
            return;
        }

        bool meleeReach = InMeleeReach(*m_creature, *m_creature->getVictim());

        if (m_creature->IsStopped() || meleeReach)
        {

            if (m_creature->IsStopped() && m_creature->IsNonMeleeSpellCasted(false))
            {
                if (m_creature->hasUnitState(UNIT_STAT_FOLLOW_MOVE))
                {
                    m_creature->InterruptNonMeleeSpells(false);
                }
                else
                {
                    return;
                }
            }

            else if (DoMeleeAttackIfReady())
            {
                if (!m_creature->getVictim())
                {
                    return;
                }

                m_creature->getVictim()->AddThreat(m_creature);

                if (_needToStop())
                {
                    _stopAttack();
                }
            }
        }
    }
    else if (owner && m_creature->GetCharmInfo() && ((m_loiterUntilTime == 0) || (getMSTime() > m_loiterUntilTime)))
    {

        if (m_loiterUntilTime > 0)
        {
            m_loiterUntilTime = 0;
            SelectNextTarget(owner);

        }
        else if (owner->IsInCombat() && !(m_creature->GetCharmInfo()->HasReactState(REACT_PASSIVE) || m_creature->GetCharmInfo()->HasCommandState(COMMAND_STAY)))
        {
            AttackStart(owner->getAttackerForHelper());
        }
        else if (m_creature->GetCharmInfo()->HasCommandState(COMMAND_FOLLOW))
        {
            if (!m_creature->hasUnitState(UNIT_STAT_FOLLOW))
            {
                m_creature->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
            }
        }
    }

    if (!m_creature->IsNonMeleeSpellCasted(false))
    {
        typedef std::vector<std::pair<Unit*, Spell*> > TargetSpellList;
        TargetSpellList targetSpellStore;

        float maxOutOfRangeDistance = 0.0f;
        for (uint8 i = 0; i < m_creature->GetPetAutoSpellSize(); ++i)
        {
            uint32 spellID = m_creature->GetPetAutoSpellOnPos(i);
            if (!spellID)
            {
                continue;
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellID);
            if (!spellInfo)
            {
                continue;
            }

            if (m_creature->GetCharmInfo() && m_creature->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
            {
                continue;
            }

            if (!inCombat)
            {

                if (!cast::RecipeOf(*spellInfo).IsPositive())
                {
                    continue;
                }

                if (!IsNonCombatSpell(spellInfo))
                {

                    int32 duration = cast::RecipeOf(*spellInfo).DurationMs();
                    if ((spellInfo->ManaCost || spellInfo->ManaCostPct || spellInfo->ManaPerSecond) && duration > 0)
                    {
                        continue;
                    }

                    int32 cooldown = GetSpellRecoveryTime(spellInfo);
                    if (cooldown >= 0 && duration >= 0 && cooldown > duration)
                    {
                        continue;
                    }

                    if (spellInfo->HasSpellEffect(SPELL_EFFECT_INSTAKILL))
                    {
                        continue;
                    }
                }
            }
            else
            {

                if (IsNonCombatSpell(spellInfo))
                {
                    continue;
                }
            }

            Spell* spell = new Spell(m_creature, spellInfo, false);

            if (inCombat && spell->CanAutoCast(m_creature->getVictim()))
            {
                targetSpellStore.push_back(TargetSpellList::value_type(m_creature->getVictim(), spell));
                continue;
            }
            else
            {
                bool spellUsed = false;
                for (GuidSet::const_iterator tar = m_AllySet.begin(); tar != m_AllySet.end(); ++tar)
                {
                    Unit* Target = m_creature->GetMap()->GetUnit(*tar);

                    if (!Target)
                    {
                        continue;
                    }

                    if (spell->CanAutoCast(Target))
                    {
                        targetSpellStore.push_back(TargetSpellList::value_type(Target, spell));
                        spellUsed = true;
                        break;
                    }
                }

                if (!spellUsed && inCombat && m_creature->getVictim() && !cast::RecipeOf(*spellInfo).IsPositive())
                {
                    SpellCastResult failReason = spell->CheckPetCast(m_creature->getVictim());
                    if (failReason == SPELL_FAILED_OUT_OF_RANGE)
                    {
                        SpellRangeEntry const* spellRange = sSpellRangeStore.LookupEntry(spellInfo->RangeIndex);
                        if (spellRange)
                        {
                            float spellMaxRange =  GetSpellMaxRange(spellRange);
                            if (spellMaxRange >= 7.0f)
                            {
                                maxOutOfRangeDistance = spellMaxRange - 1.0f;
                            }
                        }
                    }
                }

                if (!spellUsed)
                {
                    delete spell;
                }
            }
        }

        if (!targetSpellStore.empty())
        {
            uint32 index = urand(0, targetSpellStore.size() - 1);

            Spell* spell  = targetSpellStore[index].second;
            Unit*  target = targetSpellStore[index].first;

            targetSpellStore.erase(targetSpellStore.begin() + index);

            SpellCastTargets targets;
            targets.setUnitTarget(target);

            if (!m_creature->Where().HasInArc(target->Where(), M_PI_F))
            {
                m_creature->SetInFront(target);
                if (IsPlayer(target))
                {
                    m_creature->SendCreateUpdateToPlayer((Player*)target);
                }

                if (owner &&IsPlayer(owner))
                {
                    m_creature->SendCreateUpdateToPlayer((Player*)owner);
                }
            }

            m_creature->AddCreatureSpellCooldown(spell->m_spellInfo->ID);
            if (m_creature->IsPet())
            {
                ((Pet*)m_creature)->CheckLearning(spell->m_spellInfo->ID);
            }

            spell->prepare(&targets);
        }
        else if (maxOutOfRangeDistance > 0.0f && inCombat && m_creature->getVictim() && (m_attackDistance != maxOutOfRangeDistance))
        {

            m_attackDistance = maxOutOfRangeDistance;
            HandleMovementOnAttackStart(m_creature->getVictim());
        }
        else if (inCombat && m_creature->getVictim())
        {

            if (m_attackDistance > 0.0f || !m_creature->hasUnitState(UNIT_STAT_CHASE))
            {
                m_attackDistance = 0.0f;
                HandleMovementOnAttackStart(m_creature->getVictim());
            }
        }

        for (TargetSpellList::const_iterator itr = targetSpellStore.begin(); itr != targetSpellStore.end(); ++itr)
        {
            delete itr->second;
        }
    }
}

bool PetAI::_isVisible(Unit* u) const
{
    return m_creature->Where().WithinDist(u->Where(), sWorld.getConfig(CONFIG_FLOAT_SIGHT_GUARDER)) &&
        u->IsVisibleForOrDetect(m_creature, m_creature, true);
}

void PetAI::UpdateAllies()
{
    Unit* owner = m_creature->GetCharmerOrOwner();
    Group* pGroup = nullptr;

    m_updateAlliesTimer = 10 * IN_MILLISECONDS;

    if (!owner)
    {
        return;
    }
    else if (IsPlayer(owner))
    {
        pGroup = ((Player*)owner)->GetGroup();
    }

    if (m_AllySet.size() == 2 && !pGroup)
    {
        return;
    }

    if (pGroup && !pGroup->isRaidGroup() && m_AllySet.size() == (pGroup->GetMembersCount() + 2))
    {
        return;
    }

    m_AllySet.clear();
    m_AllySet.insert(m_creature->GetObjectGuid());
    if (pGroup)
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* target = itr->getSource();
            if (!target || !pGroup->SameSubGroup((Player*)owner, target))
            {
                continue;
            }

            if (target->GetObjectGuid() == owner->GetObjectGuid())
            {
                continue;
            }

            m_AllySet.insert(target->GetObjectGuid());
        }
    }
    else
    {
        m_AllySet.insert(owner->GetObjectGuid());
    }
}

void PetAI::AttackedBy(Unit* attacker)
{

    if (!m_creature->getVictim() && m_creature->GetCharmInfo() && !m_creature->GetCharmInfo()->HasReactState(REACT_PASSIVE) &&
        (!m_creature->GetCharmInfo()->HasCommandState(COMMAND_STAY) || InMeleeReach(*m_creature, *attacker)))
    {
        AttackStart(attacker);
    }
}
