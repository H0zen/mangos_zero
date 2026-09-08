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

/**
 * @file SpellTargetPickers.cpp
 * @brief The rules by which a slot's target code finds who it means.
 *
 * One function per rule that takes more than a line to state: a chain, a
 * cone, a party, a spot on the ground. Each is handed what it needs and
 * adds to the list it is given.
 */

#include <algorithm>
#include <iterator>
#include <list>
#include "Reaction.h"
#include "Utilities/MathDefines.h"
#include "Spell.h"
#include "Cast/Targets/Trim.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "PlayerRegistry.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "Corpse.h"
#include "Cast/Recipe/RecipeBook.h"

// Helper for Chain Healing
// Spell target first
// Raidmates then descending by injury suffered (MaxHealth - Health)
// Other players/mobs then descending by injury suffered (MaxHealth - Health)
struct ChainHealingOrder
{
    const Unit* MainTarget;
    explicit ChainHealingOrder(Unit const* Target) : MainTarget(Target) {};
    // functor for operator ">"
    bool operator()(Unit const* _Left, Unit const* _Right) const
    {
        return (ChainHealingHash(_Left) < ChainHealingHash(_Right));
    }

    int32 ChainHealingHash(Unit const* Target) const
    {
        if (Target == MainTarget)
        {
            return 0;
        }
        else if (Target->IsPlayer() && MainTarget->IsPlayer() &&
            ((Player const*)Target)->IsInSameRaidWith((Player const*)MainTarget))
        {
            if (Target->GetHealth() == Target->GetMaxHealth())
            {
                return 40000;
            }
            else
            {
                return 20000 - Target->GetMaxHealth() + Target->GetHealth();
            }
        }
        else
        {
            return 40000 - Target->GetMaxHealth() + Target->GetHealth();
        }
    }
};

class ChainHealingFullHealth
{
    public:
        const Unit* MainTarget;
        explicit ChainHealingFullHealth(const Unit* Target) : MainTarget(Target) {};

        bool operator()(const Unit* Target)
        {
            return (Target != MainTarget && Target->GetHealth() == Target->GetMaxHealth());
        }
};

// Helper for targets nearest to the spell target
// The spell target is always first unless there is a target at _completely_ the same position (unbelievable case)
struct TargetDistanceOrderNear
{
    const Unit* MainTarget;
    explicit TargetDistanceOrderNear(const Unit* Target) : MainTarget(Target) {};
    // functor for operator ">"
    bool operator()(const Unit* _Left, const Unit* _Right) const
    {
        return MainTarget->Where().IsNearer(_Left->Where(), _Right->Where());
    }
};

/**
 * @brief Picks one unit at random in the area and then a chain of others near it.
 *
 * Whether that first one has to be an enemy, a friend or anyone at all is the
 * only thing the three target codes disagree about.
 */
void Spell::PickARandomChainInTheArea(uint32 targetMode, UnitList& targetUnitMap, float radius, uint32 chainTargets, uint32& mayHit)
{
        m_targets.m_targetMask = 0;
        mayHit = chainTargets;
        float max_range = radius + mayHit * CHAIN_SPELL_JUMP_RADIUS;

        UnitList tempTargetUnitMap;

        switch (targetMode)
        {
            case TARGET_RANDOM_ENEMY_CHAIN_IN_AREA:
            {
                MaNGOS::AnyAoETargetUnitInObjectRangeCheck u_check(m_caster, max_range);
                MaNGOS::UnitListSearcher<MaNGOS::AnyAoETargetUnitInObjectRangeCheck> searcher(tempTargetUnitMap, u_check);
                Cell::VisitAllObjects(m_caster, searcher, max_range);
                return;
            }
            case TARGET_RANDOM_UNIT_CHAIN_IN_AREA: // This works the same as Target_random_friend_chain_in_area but is named differently for some reason
            case TARGET_RANDOM_FRIEND_CHAIN_IN_AREA:
            {
                MaNGOS::AnyFriendlyUnitInObjectRangeCheck u_check(m_caster, max_range);
                MaNGOS::UnitListSearcher<MaNGOS::AnyFriendlyUnitInObjectRangeCheck> searcher(tempTargetUnitMap, u_check);
                Cell::VisitAllObjects(m_caster, searcher, max_range);
                return;
            }
        }

        if (tempTargetUnitMap.empty())
        {
            return;
        }

        tempTargetUnitMap.sort(TargetDistanceOrderNear(m_caster));

        // Now to get us a random target that's in the initial range of the spell
        uint32 t = 0;
        UnitList::iterator itr = tempTargetUnitMap.begin();
        while (itr != tempTargetUnitMap.end() && (*itr)->Where().WithinDist(m_caster->Where(), radius))
        {
            ++t, ++itr;
        }

        if (!t)
        {
            return;
        }

        itr = tempTargetUnitMap.begin();
        std::advance(itr, rand() % t);
        Unit* pUnitTarget = *itr;
        targetUnitMap.push_back(pUnitTarget);

        tempTargetUnitMap.erase(itr);

        tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

        t = mayHit - 1;
        Unit* prev = pUnitTarget;
        UnitList::iterator next = tempTargetUnitMap.begin();

        while (t && next != tempTargetUnitMap.end())
        {
            if (!prev->Where().WithinDist((*next)->Where(), CHAIN_SPELL_JUMP_RADIUS))
            {
                return;
            }

            if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, nullptr, SPELL_DISABLE_LOS) && !HasLineOfSight(*prev, *(*next)))
            {
                ++next;
                continue;
            }
            prev = *next;
            targetUnitMap.push_back(prev);
            tempTargetUnitMap.erase(next);
            tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
            next = tempTargetUnitMap.begin();
            --t;
        }
}

/**
 * @brief Picks the unit the caster aimed at and the chain that jumps on from it.
 */
void Spell::PickTheChainFromTheVictim(const cast::Operation& operation, UnitList& targetUnitMap, float radius, uint32 chainTargets, uint32& mayHit)
{
    const SpellEffectIndex effIndex = SpellEffectIndex(operation.slot);

        if (chainTargets <= 1)
        {
            if (Unit* pUnitTarget = m_caster->SelectMagnetTarget(m_targets.getUnitTarget(), this, effIndex))
            {
                m_targets.setUnitTarget(pUnitTarget);
                targetUnitMap.push_back(pUnitTarget);
            }
        }
        else
        {
            Unit* pUnitTarget = m_targets.getUnitTarget();
            Occupant* originalCaster = GetAffectiveCasterObject();
            if (!pUnitTarget || !originalCaster)
            {
                return;
            }

            mayHit = chainTargets;

            float max_range;
            if (m_spellInfo->DefenseType == SPELL_DAMAGE_CLASS_MELEE)
            {
                max_range = radius;
            }
            else
                // FIXME: This very like horrible hack and wrong for most spells
            {
                max_range = radius + mayHit * CHAIN_SPELL_JUMP_RADIUS;
            }

            UnitList tempTargetUnitMap;
            {
                MaNGOS::AnyAoEVisibleTargetUnitInObjectRangeCheck u_check(pUnitTarget, originalCaster, max_range);
                MaNGOS::UnitListSearcher<MaNGOS::AnyAoEVisibleTargetUnitInObjectRangeCheck> searcher(tempTargetUnitMap, u_check);
                Cell::VisitAllObjects(m_caster, searcher, max_range);
            }

            if (tempTargetUnitMap.empty())
            {
                return;
            }

            tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

            if (*tempTargetUnitMap.begin() == pUnitTarget)
            {
                tempTargetUnitMap.erase(tempTargetUnitMap.begin());
            }

            targetUnitMap.push_back(pUnitTarget);
            uint32 t = mayHit - 1;
            Unit* prev = pUnitTarget;
            UnitList::iterator next = tempTargetUnitMap.begin();

            while (t && next != tempTargetUnitMap.end())
            {
                if (!prev->Where().WithinDist((*next)->Where(), CHAIN_SPELL_JUMP_RADIUS))
                {
                    return;
                }

                if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, nullptr, SPELL_DISABLE_LOS) && !HasLineOfSight(*prev, *(*next)))
                {
                    ++next;
                    continue;
                }

                prev = *next;
                targetUnitMap.push_back(prev);
                tempTargetUnitMap.erase(next);
                tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
                next = tempTargetUnitMap.begin();

                --t;
            }
        }
}

/**
 * @brief Picks everyone in the area, on the side the slot's own verb calls for.
 *
 * Completing a quest reaches anyone standing there; a helpful slot reaches
 * friends; everything else reaches enemies.
 */
void Spell::PickTheAreaTheVerbWants(const cast::Operation& operation, UnitList& targetUnitMap, float radius)
{
    const SpellEffectIndex effIndex = SpellEffectIndex(operation.slot);

        cast::Side targetB = cast::Side::HostileForArea;
        switch (operation.verb)
        {
            case SPELL_EFFECT_QUEST_COMPLETE:
                targetB = cast::Side::Anyone;
                return;
            default:
                // Select friendly targets for positive effect
                if (operation.positive)
                {
                    targetB = cast::Side::Friendly;
                }
                return;
        }

        UnitList tempTargetUnitMap;
        SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->ID);

        // fill real target list if no spell script target defined
        FillAreaTargets(bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap,
            radius, cast::Around::Spot, bounds.first != bounds.second ? cast::Side::Anyone : targetB);

        if (!tempTargetUnitMap.empty())
        {
            for (UnitList::const_iterator iter = tempTargetUnitMap.begin(); iter != tempTargetUnitMap.end(); ++iter)
            {
                if (!(*iter)->IsCreature())
                {
                    continue;
                }

                for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                {
                    if (i_spellST->CanNotHitWithSpellEffect(effIndex))
                    {
                        continue;
                    }

                    // only creature entries supported for this target type
                    if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
                    {
                        continue;
                    }

                    if ((*iter)->GetEntry() == i_spellST->targetEntry)
                    {
                        if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)(*iter))->IsCorpse())
                        {
                            targetUnitMap.push_back((*iter));
                        }
                        else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && (*iter)->IsAlive())
                        {
                            targetUnitMap.push_back((*iter));
                        }

                        return;
                    }
                }
            }
        }
}

/**
 * @brief Picks whoever is in the area, and where the spell names particular creature
 * entries, only those of them.
 *
 * A lasting ground effect picks nobody, because it is the ground it is laid
 * on; a summon picks the caster.
 */
void Spell::PickTheNamedCreaturesInTheArea(const cast::Operation& operation, UnitList& targetUnitMap, float radius)
{
    const SpellEffectIndex effIndex = SpellEffectIndex(operation.slot);

        if (operation.verb == SPELL_EFFECT_PERSISTENT_AREA_AURA)
        {
            return;
        }
        else if (operation.verb == SPELL_EFFECT_SUMMON)
        {
            targetUnitMap.push_back(m_caster);
            return;
        }

        UnitList tempTargetUnitMap;
        SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->ID);
        // fill real target list if no spell script target defined
        FillAreaTargets(bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap, radius, cast::Around::Spot, cast::Side::Anyone);

        if (!tempTargetUnitMap.empty())
        {
            for (UnitList::const_iterator iter = tempTargetUnitMap.begin(); iter != tempTargetUnitMap.end(); ++iter)
            {
                if (!(*iter)->IsCreature())
                {
                    continue;
                }

                for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                {
                    if (i_spellST->CanNotHitWithSpellEffect(effIndex))
                    {
                        continue;
                    }

                    // only creature entries supported for this target type
                    if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
                    {
                        continue;
                    }

                    if ((*iter)->GetEntry() == i_spellST->targetEntry)
                    {
                        if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)(*iter))->IsCorpse())
                        {
                            targetUnitMap.push_back((*iter));
                        }
                        else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && (*iter)->IsAlive())
                        {
                            targetUnitMap.push_back((*iter));
                        }

                        return;
                    }
                }
            }
        }
        else
        {
            // remove not targetable units if spell has no script targets
            for (UnitList::iterator itr = targetUnitMap.begin(); itr != targetUnitMap.end();)
            {
                if (!(*itr)->IsTargetableForAttack(Recipe().Says().castOnDead))
                {
                    targetUnitMap.erase(itr++);
                }
                else
                {
                    ++itr;
                }
            }
        }
}

/**
 * @brief Picks the gameobjects standing around a spot, either the one the cast came
 * from or the one it was aimed at.
 */
void Spell::PickTheObjectsAroundTheSpot(SpellEffectIndex effIndex, uint32 targetMode, std::list<GameObject*>& found, float radius)
{
        float x, y, z;

        if (targetMode == TARGET_AREAEFFECT_GO_AROUND_SOURCE)
        {
            if (m_targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
            {
                m_targets.getSource(x, y, z);
            }
            else
            {
                x = m_caster->Where().X();
                y = m_caster->Where().Y();
                z = m_caster->Where().Z();
            }
        }
        else
        {
            m_targets.getDestination(x, y, z);
        }

        // It may be possible to fill targets for some spell effects
        // automatically (SPELL_EFFECT_WMO_REPAIR(88) for example) but
        // for some/most spells we clearly need/want to limit with spell_target_script

        // Some spells untested, for affected GO type 33. May need further adjustments for spells related.

        SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->ID);
        for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
        {
            if (i_spellST->CanNotHitWithSpellEffect(effIndex))
            {
                continue;
            }

            if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
            {
                // search all GO's with entry, within range of m_destN
                MaNGOS::GameObjectEntryInPosRangeCheck go_check(*m_caster, i_spellST->targetEntry, x, y, z, radius);
                MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectEntryInPosRangeCheck> checker(found, go_check);
                Cell::VisitGridObjects(m_caster, checker, radius);
            }
        }
}

/**
 * @brief Picks the one party member a spell may be laid on, never the caster.
 *
 * A groupmate's pet counts, and so does the caster's own master when it is a
 * pet doing the casting.
 */
void Spell::PickTheOneGroupmate(UnitList& targetUnitMap)
{
        Unit* target = m_targets.getUnitTarget();
        // Those spells apparently can't be casted on the caster.
        if (target && target != m_caster)
        {
            // Can only be casted on group's members or its pets
            Group*  pGroup = nullptr;

            Unit* owner = m_caster->GetCharmerOrOwner();
            Unit* targetOwner = target->GetCharmerOrOwner();
            if (owner)
            {
                if (owner->IsPlayer())
                {
                    if (target == owner)
                    {
                        targetUnitMap.push_back(target);
                        return;
                    }
                    pGroup = ((Player*)owner)->GetGroup();
                }
            }
            else if (m_caster->IsPlayer())
            {
                if (targetOwner == m_caster && target->IsCreature() && ((Creature*)target)->IsPet())
                {
                    targetUnitMap.push_back(target);
                    return;
                }
                pGroup = ((Player*)m_caster)->GetGroup();
            }

            if (pGroup)
            {
                // Our target can also be a player's pet who's grouped with us or our pet. But can't be controlled player
                if (targetOwner)
                {
                    if (targetOwner->IsPlayer() &&
                        target->IsCreature() && (((Creature*)target)->IsPet()) &&
                        target->GetOwnerGuid() == targetOwner->GetObjectGuid() &&
                        pGroup->IsMember(((Player*)targetOwner)->GetObjectGuid()))
                    {
                        targetUnitMap.push_back(target);
                    }
                }
                // 1Our target can be a player who is on our group
                else if (target->IsPlayer() && pGroup->IsMember(((Player*)target)->GetObjectGuid()))
                {
                    targetUnitMap.push_back(target);
                }
            }
        }
}

/**
 * @brief Picks everyone in the cone the caster faces.
 *
 * Two spells open a cone of their own: one sweeps behind the caster and one
 * opens a cone narrow enough to be a line.
 */
void Spell::PickTheConeThisSpellOpens(const cast::Operation& operation, UnitList& targetUnitMap, float radius)
{
    const SpellEffectIndex effIndex = SpellEffectIndex(operation.slot);

        cast::Side targetB = cast::Side::HostileForArea;

        if (operation.verb == SPELL_EFFECT_SCRIPT_EFFECT)
        {
            targetB = cast::Side::Anyone;
        }

        UnitList tempTargetUnitMap;
        SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->ID);

        // fill real target list if no spell script target defined
        FillAreaTargets(bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap,
            radius, cast::Around::CasterInFront15, bounds.first != bounds.second ? cast::Side::Anyone : targetB);

        if (!tempTargetUnitMap.empty())
        {
            for (UnitList::const_iterator iter = tempTargetUnitMap.begin(); iter != tempTargetUnitMap.end(); ++iter)
            {
                if (!(*iter)->IsCreature())
                {
                    continue;
                }

                for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                {
                    if (i_spellST->CanNotHitWithSpellEffect(effIndex))
                    {
                        continue;
                    }

                    // only creature entries supported for this target type
                    if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
                    {
                        continue;
                    }

                    if ((*iter)->GetEntry() == i_spellST->targetEntry)
                    {
                        if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)(*iter))->IsCorpse())
                        {
                            targetUnitMap.push_back((*iter));
                        }
                        else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && (*iter)->IsAlive())
                        {
                            targetUnitMap.push_back((*iter));
                        }
                        return;
                    }
                }
            }
        }
}

/**
 * @brief Picks the party standing around the caster, or around whoever owns him.
 */
void Spell::PickThePartyAround(UnitList& targetUnitMap, float radius)
{
        Unit* owner = m_caster->GetCharmerOrOwner();
        Player* pTarget = nullptr;

        if (owner)
        {
            targetUnitMap.push_back(m_caster);
            if (owner->IsPlayer())
            {
                pTarget = (Player*)owner;
            }
        }
        else if (m_caster->IsPlayer())
        {
            if (Unit* target = m_targets.getUnitTarget())
            {
                if (!target->IsPlayer())
                {
                    if (((Creature*)target)->IsPet())
                    {
                        Unit* targetOwner = target->GetOwner();
                        if (targetOwner->IsPlayer())
                        {
                            pTarget = (Player*)targetOwner;
                        }
                    }
                }
                else
                {
                    pTarget = (Player*)target;
                }
            }
        }

        Group* pGroup = pTarget ? pTarget->GetGroup() : nullptr;

        if (pGroup)
        {
            uint8 subgroup = pTarget->GetSubGroup();

            for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* Target = itr->getSource();

                // IsHostileTo check duel and controlled by enemy
                if (Target && Target->GetSubGroup() == subgroup && !IsHostile(*m_caster, *Target))
                {
                    if (InReach(*pTarget, *Target, radius))
                    {
                        targetUnitMap.push_back(Target);
                    }

                    if (Pet* pet = Target->GetPet())
                    {
                        if (InReach(*pTarget, *pet, radius))
                        {
                            targetUnitMap.push_back(pet);
                        }
                    }
                }
            }
        }
        else if (owner)
        {
            if (InReach(*m_caster, *owner, radius))
            {
                targetUnitMap.push_back(owner);
            }
        }
        else if (pTarget)
        {
            targetUnitMap.push_back(pTarget);

            if (Pet* pet = pTarget->GetPet())
            {
                if (InReach(*m_caster, *pet, radius))
                {
                    targetUnitMap.push_back(pet);
                }
            }
        }
}

/**
 * @brief Picks the wounded a healing chain jumps between, worst hurt first.
 */
void Spell::PickTheChainOfWounded(UnitList& targetUnitMap, float radius, uint32 chainTargets, uint32& mayHit)
{
        Unit* pUnitTarget = m_targets.getUnitTarget();
        if (!pUnitTarget)
        {
            return;
        }

        if (chainTargets <= 1)
        {
            targetUnitMap.push_back(pUnitTarget);
        }
        else
        {
            mayHit = chainTargets;
            float max_range = radius + mayHit * CHAIN_SPELL_JUMP_RADIUS;

            UnitList tempTargetUnitMap;

            FillAreaTargets(tempTargetUnitMap, max_range, cast::Around::Caster, cast::Side::Friendly);

            if (m_caster != pUnitTarget && std::find(tempTargetUnitMap.begin(), tempTargetUnitMap.end(), m_caster) == tempTargetUnitMap.end())
            {
                tempTargetUnitMap.push_front(m_caster);
            }

            tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

            if (tempTargetUnitMap.empty())
            {
                return;
            }

            if (*tempTargetUnitMap.begin() == pUnitTarget)
            {
                tempTargetUnitMap.erase(tempTargetUnitMap.begin());
            }

            targetUnitMap.push_back(pUnitTarget);
            uint32 t = mayHit - 1;
            Unit* prev = pUnitTarget;
            UnitList::iterator next = tempTargetUnitMap.begin();

            while (t && next != tempTargetUnitMap.end())
            {
                if (!prev->Where().WithinDist((*next)->Where(), CHAIN_SPELL_JUMP_RADIUS))
                {
                    return;
                }

                if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, nullptr, SPELL_DISABLE_LOS) && !HasLineOfSight(*prev, *(*next)))
                {
                    ++next;
                    continue;
                }

                if ((*next)->GetHealth() == (*next)->GetMaxHealth())
                {
                    next = tempTargetUnitMap.erase(next);
                    continue;
                }

                prev = *next;
                targetUnitMap.push_back(prev);
                tempTargetUnitMap.erase(next);
                tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
                next = tempTargetUnitMap.begin();

                --t;
            }
        }
}

/**
 * @brief Picks the party members of the aimed-at player who share his class.
 */
void Spell::PickThePartyOfTheTargetsClass(UnitList& targetUnitMap, float radius)
{
        Player* targetPlayer = m_targets.getUnitTarget() && m_targets.getUnitTarget()->IsPlayer()
            ? (Player*)m_targets.getUnitTarget() : nullptr;

        Group* pGroup = targetPlayer ? targetPlayer->GetGroup() : nullptr;
        if (pGroup)
        {
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* Target = itr->getSource();

                // IsHostileTo check duel and controlled by enemy
                if (Target && InReach(*targetPlayer, *Target, radius) &&
                    targetPlayer->getClass() == Target->getClass() &&
                    !IsHostile(*m_caster, *Target))
                {
                    targetUnitMap.push_back(Target);
                }
            }
        }
        else if (m_targets.getUnitTarget())
        {
            targetUnitMap.push_back(m_targets.getUnitTarget());
        }
}

/**
 * @brief Picks the spot beside the caster a lasting ground effect is put down on,
 * in front of him, behind him or to either side.
 */
void Spell::PickTheSpotBesideTheCaster(const cast::Operation& operation, uint32 targetMode, UnitList& targetUnitMap, float radius)
{
        if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
        {
            // General override, we don't want to use max spell range here.
            // Note: 0.0 radius is also for index 36. It is possible that 36 must be defined as
            // "at the base of", in difference to 0 which appear to be "directly in front of".
            // TODO: some summoned will make caster be half inside summoned object. Need to fix
            // that in the below code (nearpoint vs closepoint, etc).
            if (operation.radiusIndex == 0)
            {
                radius = 0.0f;
            }

            float angle = m_caster->Where().Facing();
            switch (targetMode)
            {
                case TARGET_DYNAMIC_OBJECT_FRONT:                           break;
                case TARGET_DYNAMIC_OBJECT_BEHIND:      angle += M_PI_F;      break;
                case TARGET_DYNAMIC_OBJECT_LEFT_SIDE:   angle += M_PI_F / 2;  break;
                case TARGET_DYNAMIC_OBJECT_RIGHT_SIDE:  angle -= M_PI_F / 2;  break;
            }

            float x, y;
            const Geometry::Vector3 near_ = PointNear(*m_caster, radius + m_caster->Where().Extent(), angle);
            x = near_.x;
            y = near_.y;
            m_targets.setDestination(x, y, m_caster->Where().Z());
        }

        targetUnitMap.push_back(m_caster);
}
