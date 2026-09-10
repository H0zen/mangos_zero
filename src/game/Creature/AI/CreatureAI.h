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

#include <vector>
#include "Platform/Define.h"
#include "Dynamic/FactoryHolder.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "ObjectMgr.h"

class Occupant;
class GameObject;
class Unit;
class Creature;
class Player;
struct SpellEntry;
class ChatHandler;

#define TIME_INTERVAL_LOOK   5000
#define VISIBILITY_RANGE    10000

enum CanCastResult
{
    CAST_OK                     = 0,
    CAST_FAIL_IS_CASTING        = 1,
    CAST_FAIL_OTHER             = 2,
    CAST_FAIL_TOO_FAR           = 3,
    CAST_FAIL_TOO_CLOSE         = 4,
    CAST_FAIL_POWER             = 5,
    CAST_FAIL_STATE             = 6,
    CAST_FAIL_TARGET_AURA       = 7,
    CAST_FAIL_NO_LOS            = 8,
    CAST_FAIL_SILENCED          = 9
};

enum CastFlags
{
    CAST_INTERRUPT_PREVIOUS     = 0x01,
    CAST_TRIGGERED              = 0x02,
    CAST_FORCE_CAST             = 0x04,
    CAST_NO_MELEE_IF_OOM        = 0x08,
    CAST_FORCE_TARGET_SELF      = 0x10,
    CAST_AURA_NOT_PRESENT       = 0x20,
};

enum SpellListCastFlags
{
    CF_INTERRUPT_PREVIOUS     = 0x01,
    CF_TRIGGERED              = 0x02,
    CF_FORCE_CAST             = 0x04,
    CF_MAIN_RANGED_SPELL      = 0x08,
    CF_TARGET_UNREACHABLE     = 0x10,
    CF_AURA_NOT_PRESENT       = 0x20,
    CF_ONLY_IN_MELEE          = 0x40,
    CF_NOT_IN_MELEE           = 0x80,
};

enum CombatMovementFlags
{
    COMBAT_MOVEMENT_SCRIPT      = 0x01,
    CM_SPELL  = 0x02,
};

enum AIEventType
{

    AI_EVENT_JUST_DIED          = 0,
    AI_EVENT_CRITICAL_HEALTH    = 1,
    AI_EVENT_LOST_HEALTH        = 2,
    AI_EVENT_LOST_SOME_HEALTH   = 3,
    AI_EVENT_GOT_FULL_HEALTH    = 4,
    AI_EVENT_CUSTOM_EVENTAI_A   = 5,
    AI_EVENT_CUSTOM_EVENTAI_B   = 6,
    AI_EVENT_GOT_CCED           = 7,
    MAXIMAL_AI_EVENT_EVENTAI    = 8,

    AI_EVENT_CALL_ASSISTANCE    = 10,

    AI_EVENT_START_ESCORT       = 100,
    AI_EVENT_START_ESCORT_B     = 101,
    AI_EVENT_START_EVENT        = 102,
    AI_EVENT_START_EVENT_A      = 103,
    AI_EVENT_START_EVENT_B      = 104,

    AI_EVENT_CUSTOM_A           = 1000,
    AI_EVENT_CUSTOM_B           = 1001,
    AI_EVENT_CUSTOM_C           = 1002,
    AI_EVENT_CUSTOM_D           = 1003,
    AI_EVENT_CUSTOM_E           = 1004,
    AI_EVENT_CUSTOM_F           = 1005,
};

struct CreatureAISpellsEntry : CreatureSpellsEntry
{
    uint32 cooldown;
    CreatureAISpellsEntry(CreatureSpellsEntry const& EntryStruct) : CreatureSpellsEntry(EntryStruct), cooldown(urand(EntryStruct.delayInitialMin, EntryStruct.delayInitialMax)) {}
};

class CreatureAI
{
    public:
        explicit CreatureAI(Creature* creature);
        virtual ~CreatureAI();

        virtual void GetAIInformation(ChatHandler& ) {}

        virtual void MoveInLineOfSight(Unit* ) {}

        virtual void EnterCombat(Unit* ) {}

        virtual void EnterEvadeMode();

        virtual void JustReachedHome() {}

        virtual void HealedBy(Unit * , uint32& ) {}

        virtual void DamageDeal(Unit* , uint32& ) {}

        virtual void DamageTaken(Unit* , uint32& ) {}

        virtual void JustDied(Unit* ) {}

        virtual void CorpseRemoved(uint32& ) {}

        virtual void SummonedCreatureJustDied(Creature* ) {}

        virtual void KilledUnit(Unit* ) {}

        virtual void OwnerKilledUnit(Unit* ) {}

        virtual void JustSummoned(Creature* ) {}

        virtual void JustSummoned(GameObject* ) {}

        virtual void SummonedCreatureDespawn(Creature* ) {}

        virtual void SpellHit(Unit* , const SpellEntry* ) {}

        virtual void OnSpellCastChange(const SpellEntry* , SpellCastResult ) {}

        virtual void SpellHitTarget(Unit* , const SpellEntry* ) {}

        virtual void AttackedBy(Unit* pAttacker);

        virtual void JustRespawned() {}

        virtual void MovementInform(uint32 , uint32 ) {}

        virtual void SummonedMovementInform(Creature* , uint32 , uint32 ) {}

        virtual void ReceiveEmote(Player* , uint32 ) {}

        virtual void AttackStart(Unit* ) {}

        virtual void UpdateAI(const uint32 ) {}

        virtual bool IsVisible(Unit* ) const { return false; }

        virtual bool canReachByRangeAttack(Unit*) { return false; }

        bool DoMeleeAttackIfReady();

        CanCastResult CanCastSpell(Unit* pTarget, const SpellEntry* pSpell, bool isTriggered);

        virtual CanCastResult DoCastSpellIfCan(Unit* pTarget, uint32 uiSpell, uint32 uiCastFlags = 0, ObjectGuid OriginalCasterGuid = 0);

        void SetSpellsList(uint32 entry);
        void SetSpellsList(CreatureSpellsList const* pSpellsList);

        void UpdateSpellsList(uint32 const uiDiff);
        void DoSpellsListCasts(uint32 const uiDiff);

        void SetMeleeAttack(bool enabled);

        void SetCombatMovement(bool enable, bool stopOrStartMovement = false);
        bool IsCombatMovement() const { return m_combatMovement != 0; }
        uint8 GetCombatMovementFlags() const { return m_combatMovement; }
        void SetCombatMovementFlag(uint8 flag, bool setFlag = true);

        void SendAIEventAround(AIEventType eventType, Unit* pInvoker, uint32 uiDelay, float fRadius, uint32 miscValue = 0) const;

        void SendAIEvent(AIEventType eventType, Unit* pInvoker, Creature* pReceiver, uint32 miscValue = 0) const;

        virtual void ReceiveAIEvent(AIEventType , Creature* , Unit* , uint32 ) {}

        virtual void Reset()
        {
            m_combatMovement = COMBAT_MOVEMENT_SCRIPT;
        }

    protected:
        void HandleMovementOnAttackStart(Unit* victim);
        void SetChase(bool chase);

        Creature* const m_creature;

        float m_attackDistance;
        float m_attackAngle;

        bool m_meleeAttack;
        uint8 m_combatMovement;
        uint32 m_uiCastingDelay;
        std::vector<CreatureAISpellsEntry> m_CreatureSpells;
};

struct SelectableAI : public FactoryHolder<CreatureAI>, public Permissible<Creature>
{
    SelectableAI(const char* id) : FactoryHolder<CreatureAI>(id) {}
};

template<class REAL_AI>
    struct CreatureAIFactory : public SelectableAI
{
    CreatureAIFactory(const char* name) : SelectableAI(name) {}

    CreatureAI* Create(void*) const override;

    int Permit(const Creature* c) const override { return REAL_AI::Permissible(c); }
};

enum Permitions
{
    PERMIT_BASE_NO                 = -1,
    PERMIT_BASE_IDLE               = 1,
    PERMIT_BASE_REACTIVE           = 100,
    PERMIT_BASE_PROACTIVE          = 200,
    PERMIT_BASE_FACTION_SPECIFIC   = 400,
    PERMIT_BASE_SPECIAL            = 800
};

typedef FactoryHolder<CreatureAI> CreatureAICreator;
typedef FactoryHolder<CreatureAI>::FactoryHolderRegistry CreatureAIRegistry;
typedef FactoryHolder<CreatureAI>::FactoryHolderRepository CreatureAIRepository;
