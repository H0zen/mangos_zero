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

#include "Position.h"
#include <unordered_map>
#include <utility>
#include "Platform/Define.h"
#include <string>
#include <map>
#include <set>
#include <ctime>
#include "ItemPrototype.h"
#include "Occupant.h"
#include "Opcodes.h"
#include "SpellAuraDefines.h"
#include "AuraIndex.h"
#include "AuraBook.h"
#include "Immunities.h"
#include "Combat/Blow.h"
#include "Unit/Auras/Diminishing.h"
#include "UpdateFields.h"
#include "SharedDefines.h"
#include "Conjurations.h"
#include "Pace.h"
#include "Retinue.h"
#include "Stats/StatSheet.h"
#include "ThreatManager.h"
#include "HostileRefManager.h"
#include "FollowerReference.h"
#include "FollowerRefManager.h"
#include "Utilities/EventProcessor.h"
#include "Movement/MovementInfo.h"
#include "GlobalCooldown.h"
#include "CharmInfo.h"
#include "Stats/Modifiers.h"
#include "Stats/Tallies.h"
#include "Stats/Recovery.h"
#include "Creature/CreatureRecord.h"
#include "Cell.h"
#include "Creature/VendorStock.h"
#include "Creature/Disguise.h"
#include "Creature/Repertoire.h"
#include "LootClaim.h"
#include "Creature/Pickings.h"
#include "Creature/Station.h"
#include "Creature/Vigil.h"
#include "MotionMaster.h"
#include "DBCStructure.h"
#include "WorldPacket.h"
#include "Timer.h"
#include "Log.h"

#include <list>
#include <optional>

enum SpellInterruptFlags
{
    SPELL_INTERRUPT_FLAG_MOVEMENT = 0x01,
    SPELL_INTERRUPT_FLAG_DAMAGE = 0x02,
    SPELL_INTERRUPT_FLAG_INTERRUPT = 0x04,
    SPELL_INTERRUPT_FLAG_AUTOATTACK = 0x08,
    SPELL_INTERRUPT_FLAG_ABORT_ON_DMG = 0x10

};

enum SpellChannelInterruptFlags
{
    CHANNEL_FLAG_DAMAGE = 0x0002,
    CHANNEL_FLAG_MOVEMENT = 0x0008,
    CHANNEL_FLAG_TURNING = 0x0010,
    CHANNEL_FLAG_DAMAGE2 = 0x0080,
    CHANNEL_FLAG_DELAY = 0x4000
};

enum SpellAuraInterruptFlags
{
    AURA_INTERRUPT_FLAG_UNK0 = 0x00000001,
    AURA_INTERRUPT_FLAG_DAMAGE = 0x00000002,
    AURA_INTERRUPT_FLAG_UNK2 = 0x00000004,
    AURA_INTERRUPT_FLAG_MOVE = 0x00000008,
    AURA_INTERRUPT_FLAG_TURNING = 0x00000010,
    AURA_INTERRUPT_FLAG_ENTER_COMBAT = 0x00000020,
    AURA_INTERRUPT_FLAG_NOT_MOUNTED = 0x00000040,
    AURA_INTERRUPT_FLAG_NOT_ABOVEWATER = 0x00000080,
    AURA_INTERRUPT_FLAG_NOT_UNDERWATER = 0x00000100,
    AURA_INTERRUPT_FLAG_NOT_SHEATHED = 0x00000200,
    AURA_INTERRUPT_FLAG_UNK10 = 0x00000400,
    AURA_INTERRUPT_FLAG_UNK11 = 0x00000800,
    AURA_INTERRUPT_FLAG_MELEE_ATTACK = 0x00001000,
    AURA_INTERRUPT_FLAG_UNK13 = 0x00002000,
    AURA_INTERRUPT_FLAG_UNK14 = 0x00004000,
    AURA_INTERRUPT_FLAG_UNK15 = 0x00008000,
    AURA_INTERRUPT_FLAG_UNK16 = 0x00010000,
    AURA_INTERRUPT_FLAG_MOUNTING = 0x00020000,
    AURA_INTERRUPT_FLAG_NOT_SEATED = 0x00040000,
    AURA_INTERRUPT_FLAG_CHANGE_MAP = 0x00080000,
    AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION = 0x00100000,
    AURA_INTERRUPT_FLAG_UNK21 = 0x00200000,
    AURA_INTERRUPT_FLAG_UNK22 = 0x00400000,
    AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT = 0x00800000,
    AURA_INTERRUPT_FLAG_DIRECT_DAMAGE = 0x01000000
};

enum SpellModOp
{
    SPELLMOD_DAMAGE = 0,
    SPELLMOD_DURATION = 1,
    SPELLMOD_THREAT = 2,
    SPELLMOD_ATTACK_POWER = 3,
    SPELLMOD_CHARGES = 4,
    SPELLMOD_RANGE = 5,
    SPELLMOD_RADIUS = 6,
    SPELLMOD_CRITICAL_CHANCE = 7,
    SPELLMOD_ALL_EFFECTS = 8,
    SPELLMOD_NOT_LOSE_CASTING_TIME = 9,
    SPELLMOD_CASTING_TIME = 10,
    SPELLMOD_COOLDOWN = 11,
    SPELLMOD_SPEED = 12,

    SPELLMOD_COST = 14,
    SPELLMOD_CRIT_DAMAGE_BONUS = 15,
    SPELLMOD_RESIST_MISS_CHANCE = 16,
    SPELLMOD_JUMP_TARGETS = 17,
    SPELLMOD_CHANCE_OF_SUCCESS = 18,
    SPELLMOD_ACTIVATION_TIME = 19,
    SPELLMOD_EFFECT_PAST_FIRST = 20,
    SPELLMOD_CASTING_TIME_OLD = 21,
    SPELLMOD_DOT = 22,
    SPELLMOD_HASTE = 23,
    SPELLMOD_SPELL_BONUS_DAMAGE = 24,

    SPELLMOD_MULTIPLE_VALUE = 27,
    SPELLMOD_RESIST_DISPEL_CHANCE = 28
};

#define MAX_SPELLMOD 32

enum SpellFacingFlags
{
    SPELL_FACING_FLAG_INFRONT = 0x0001
};

#define BASE_MELEERANGE_OFFSET 1.33f
#define BASE_MINDAMAGE 1.0f
#define BASE_MAXDAMAGE 2.0f
#define BASE_ATTACK_TIME 2000

enum UnitStandStateType
{
    UNIT_STAND_STATE_STAND             = 0,
    UNIT_STAND_STATE_SIT               = 1,
    UNIT_STAND_STATE_SIT_CHAIR         = 2,
    UNIT_STAND_STATE_SLEEP             = 3,
    UNIT_STAND_STATE_SIT_LOW_CHAIR     = 4,
    UNIT_STAND_STATE_SIT_MEDIUM_CHAIR  = 5,
    UNIT_STAND_STATE_SIT_HIGH_CHAIR    = 6,
    UNIT_STAND_STATE_DEAD              = 7,
    UNIT_STAND_STATE_KNEEL             = 8,
};

#define MAX_UNIT_STAND_STATE             9

enum UnitBytes1_Flags
{
    UNIT_BYTE1_FLAG_ALWAYS_STAND = 0x01,
    UNIT_BYTE1_FLAGS_CREEP       = 0x02,
    UNIT_BYTE1_FLAG_UNTRACKABLE  = 0x04,
    UNIT_BYTE1_FLAG_ALL          = 0xFF
};

enum SheathState
{

    SHEATH_STATE_UNARMED  = 0,

    SHEATH_STATE_MELEE    = 1,

    SHEATH_STATE_RANGED   = 2
};

#define MAX_SHEATH_STATE    3

enum Swing
{
    NOSWING                    = 0,
    SINGLEHANDEDSWING          = 1,
    TWOHANDEDSWING             = 2
};

enum VictimState
{
    VICTIMSTATE_UNAFFECTED     = 0,
    VICTIMSTATE_NORMAL         = 1,
    VICTIMSTATE_DODGE          = 2,
    VICTIMSTATE_PARRY          = 3,
    VICTIMSTATE_INTERRUPT      = 4,
    VICTIMSTATE_BLOCKS         = 5,
    VICTIMSTATE_EVADES         = 6,
    VICTIMSTATE_IS_IMMUNE      = 7,
    VICTIMSTATE_DEFLECTS       = 8
};

enum HitInfo
{
    HITINFO_NORMALSWING         = 0x00000000,
    HITINFO_UNK0                = 0x00000001,
    HITINFO_NORMALSWING2        = 0x00000002,
    HITINFO_LEFTSWING           = 0x00000004,
    HITINFO_UNK3                = 0x00000008,
    HITINFO_MISS                = 0x00000010,
    HITINFO_ABSORB              = 0x00000020,
    HITINFO_RESIST              = 0x00000040,
    HITINFO_CRITICALHIT         = 0x00000080,
    HITINFO_UNK8                = 0x00000100,
    HITINFO_BLOCK               = 0x00000800,
    HITINFO_UNK9                = 0x00002000,
    HITINFO_GLANCING            = 0x00004000,
    HITINFO_CRUSHING            = 0x00008000,
    HITINFO_NOACTION            = 0x00010000,
    HITINFO_SWINGNOHITSOUND     = 0x00080000
};

struct FactionTemplateEntry;
struct Modifier;
struct SpellEntry;

namespace cast
{
    class Recipe;
    struct Operation;
}

struct SpellEntryExt;

class Aura;
class SpellAuraHolder;
class Creature;
class Spell;
class DynamicObject;
class GameObject;
class Item;
class Pet;
class PetAura;
class Totem;

enum WeaponDamageRange
{
    MINDAMAGE,
    MAXDAMAGE
};

enum DamageTypeToSchool
{
    RESISTANCE,
    DAMAGE_DEALT,
    DAMAGE_TAKEN
};

enum AuraRemoveMode
{
    AURA_REMOVE_BY_DEFAULT,
    AURA_REMOVE_BY_STACK,
    AURA_REMOVE_BY_CANCEL,
    AURA_REMOVE_BY_DISPEL,
    AURA_REMOVE_BY_DEATH,
    AURA_REMOVE_BY_DELETE,
    AURA_REMOVE_BY_SHIELD_BREAK,
    AURA_REMOVE_BY_EXPIRE,
    AURA_REMOVE_BY_TRACKING
};
enum BaseModGroup
{
    CRIT_PERCENTAGE,
    RANGED_CRIT_PERCENTAGE,
    OFFHAND_CRIT_PERCENTAGE,
    SHIELD_BLOCK_VALUE,
    BASEMOD_END
};

enum BaseModType
{
    FLAT_MOD,
    PCT_MOD,
    MOD_END,
};

enum DeathState
{
    ALIVE          = 0,
    JUST_DIED      = 1,
    CORPSE         = 2,
    DEAD           = 3,
    JUST_ALIVED    = 4
};

enum UnitState
{

    UNIT_STAT_MELEE_ATTACKING = 0x00000001,
    UNIT_STAT_ATTACK_PLAYER   = 0x00000002,
    UNIT_STAT_DIED            = 0x00000004,
    UNIT_STAT_STUNNED         = 0x00000008,
    UNIT_STAT_ROOT            = 0x00000010,
    UNIT_STAT_ISOLATED        = 0x00000020,
    UNIT_STAT_CONTROLLED      = 0x00000040,

    UNIT_STAT_TAXI_FLIGHT     = 0x00000080,
    UNIT_STAT_DISTRACTED      = 0x00000100,

    UNIT_STAT_CONFUSED        = 0x00000200,
    UNIT_STAT_CONFUSED_MOVE   = 0x00000400,
    UNIT_STAT_ROAMING         = 0x00000800,
    UNIT_STAT_ROAMING_MOVE    = 0x00001000,
    UNIT_STAT_CHASE           = 0x00002000,
    UNIT_STAT_CHASE_MOVE      = 0x00004000,
    UNIT_STAT_FOLLOW          = 0x00008000,
    UNIT_STAT_FOLLOW_MOVE     = 0x00010000,
    UNIT_STAT_FLEEING         = 0x00020000,
    UNIT_STAT_FLEEING_MOVE    = 0x00040000,

    UNIT_STAT_NO_COMBAT_MOVEMENT    = 0x01000000,
    UNIT_STAT_RUNNING               = 0x02000000,
    UNIT_STAT_WAYPOINT_PAUSED       = 0x04000000,

    UNIT_STAT_IGNORE_PATHFINDING    = 0x10000000,

    UNIT_STAT_CAN_NOT_MOVE    = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED,

    UNIT_STAT_NOT_MOVE        = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED |
    UNIT_STAT_DISTRACTED,

    UNIT_STAT_NO_FREE_MOVE    = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED |
    UNIT_STAT_TAXI_FLIGHT |
    UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING,

    UNIT_STAT_CAN_NOT_REACT   = UNIT_STAT_STUNNED | UNIT_STAT_DIED |
    UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING,

    UNIT_STAT_LOST_CONTROL    = UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING | UNIT_STAT_CONTROLLED,

    UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL  = UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_LOST_CONTROL,

    UNIT_STAT_MOVING          = UNIT_STAT_ROAMING_MOVE | UNIT_STAT_CHASE_MOVE | UNIT_STAT_FOLLOW_MOVE | UNIT_STAT_FLEEING_MOVE,

    UNIT_STAT_RUNNING_STATE   = UNIT_STAT_CHASE_MOVE | UNIT_STAT_FLEEING_MOVE | UNIT_STAT_RUNNING,

    UNIT_STAT_ALL_STATE       = 0xFFFFFFFF,
    UNIT_STAT_ALL_DYN_STATES  = UNIT_STAT_ALL_STATE & ~(UNIT_STAT_NO_COMBAT_MOVEMENT | UNIT_STAT_RUNNING | UNIT_STAT_WAYPOINT_PAUSED | UNIT_STAT_IGNORE_PATHFINDING)
};

enum UnitAuraFlags
{
    UNIT_AURAFLAG_ALIVE_INVISIBLE   = 0x1,
};

enum UnitVisibility
{
    VISIBILITY_OFF                = 0,
    VISIBILITY_ON                 = 1,
    VISIBILITY_GROUP_STEALTH      = 2,
    VISIBILITY_GROUP_INVISIBILITY = 3,
    VISIBILITY_GROUP_NO_DETECT    = 4,
    VISIBILITY_REMOVE_CORPSE      = 5
};

enum UnitFlags
{
    UNIT_FLAG_NONE                  = 0x00000000,
    UNIT_FLAG_UNK_0                 = 0x00000001,
    UNIT_FLAG_NON_ATTACKABLE        = 0x00000002,
    UNIT_FLAG_CLIENT_CONTROL_LOST   = 0x00000004,

    UNIT_FLAG_PLAYER_CONTROLLED     = 0x00000008,

    UNIT_FLAG_RENAME                = 0x00000010,

    UNIT_FLAG_ABANDON               = 0x00000020,
    UNIT_FLAG_UNK_6                 = 0x00000040,
    UNIT_FLAG_OOC_NOT_ATTACKABLE    = 0x00000100,
    UNIT_FLAG_PASSIVE               = 0x00000200,
    UNIT_FLAG_PVP                   = 0x00001000,
    UNIT_FLAG_SILENCED              = 0x00002000,
    UNIT_FLAG_UNK_14                = 0x00004000,
    UNIT_FLAG_UNK_15                = 0x00008000,
    UNIT_FLAG_UNK_16                = 0x00010000,
    UNIT_FLAG_PACIFIED              = 0x00020000,
    UNIT_FLAG_IN_COMBAT             = 0x00080000,
    UNIT_FLAG_NOT_SELECTABLE        = 0x02000000,
    UNIT_FLAG_SKINNABLE             = 0x04000000,
    UNIT_FLAG_AURAS_VISIBLE         = 0x08000000,
    UNIT_FLAG_SHEATHE               = 0x40000000,

    UNIT_FLAG_NOT_ATTACKABLE_1      = 0x00000080,
    UNIT_FLAG_LOOTING               = 0x00000400,
    UNIT_FLAG_PET_IN_COMBAT         = 0x00000800,
    UNIT_FLAG_STUNNED               = 0x00040000,
    UNIT_FLAG_TAXI_FLIGHT           = 0x00100000,
    UNIT_FLAG_DISARMED              = 0x00200000,
    UNIT_FLAG_CONFUSED              = 0x00400000,
    UNIT_FLAG_FLEEING               = 0x00800000,
    UNIT_FLAG_POSSESSED             = 0x01000000,
    UNIT_FLAG_UNK_28                = 0x10000000,
    UNIT_FLAG_UNK_29                = 0x20000000
};

enum NPCFlags
{
    UNIT_NPC_FLAG_NONE                  = 0x00000000,
    UNIT_NPC_FLAG_GOSSIP                = 0x00000001,
    UNIT_NPC_FLAG_QUESTGIVER            = 0x00000002,
    UNIT_NPC_FLAG_VENDOR                = 0x00000004,
    UNIT_NPC_FLAG_FLIGHTMASTER          = 0x00000008,
    UNIT_NPC_FLAG_TRAINER               = 0x00000010,
    UNIT_NPC_FLAG_SPIRITHEALER          = 0x00000020,
    UNIT_NPC_FLAG_SPIRITGUIDE           = 0x00000040,
    UNIT_NPC_FLAG_INNKEEPER             = 0x00000080,
    UNIT_NPC_FLAG_BANKER                = 0x00000100,
    UNIT_NPC_FLAG_PETITIONER            = 0x00000200,
    UNIT_NPC_FLAG_TABARDDESIGNER        = 0x00000400,
    UNIT_NPC_FLAG_BATTLEMASTER          = 0x00000800,
    UNIT_NPC_FLAG_AUCTIONEER            = 0x00001000,
    UNIT_NPC_FLAG_STABLEMASTER          = 0x00002000,
    UNIT_NPC_FLAG_REPAIR                = 0x00004000,
    UNIT_NPC_FLAG_SPELLCLICK            = 0x01000000,
    UNIT_NPC_FLAG_OUTDOORPVP            = 0x20000000
};

inline ByteBuffer& operator<< (ByteBuffer& buf, MovementInfo const& mi)
{
    mi.Write(buf);
    return buf;
}

inline ByteBuffer& operator>> (ByteBuffer& buf, MovementInfo& mi)
{
    mi.Read(buf);
    return buf;
}

namespace Movement
{
    class MoveSpline;
}

enum MeleeHitOutcome
{
    MELEE_HIT_EVADE     = 0,
    MELEE_HIT_MISS      = 1,
    MELEE_HIT_DODGE     = 2,
    MELEE_HIT_BLOCK     = 3,
    MELEE_HIT_PARRY     = 4,
    MELEE_HIT_GLANCING  = 5,
    MELEE_HIT_CRIT      = 6,
    MELEE_HIT_CRUSHING  = 7,
    MELEE_HIT_NORMAL    = 8,
    MELEE_HIT_BLOCK_CRIT = 9,
};

struct CleanDamage
{
    CleanDamage(uint32 _damage, WeaponAttackType _attackType, MeleeHitOutcome _hitOutCome)
        : damage(_damage), attackType(_attackType), hitOutCome(_hitOutCome) {}

    uint32 damage;
    WeaponAttackType attackType;
    MeleeHitOutcome hitOutCome;
};

struct CalcDamageInfo
{

    Unit*  attacker;

    Unit*  target;
    SpellSchoolMask damageSchoolMask;

    uint32 damage;

    uint32 absorb;

    uint32 resist;

    uint32 blocked_amount;

    uint32 HitInfo;

    uint32 TargetState;

    WeaponAttackType attackType;

    uint32 procAttacker;

    uint32 procVictim;

    uint32 procEx;

    uint32 cleanDamage;

    MeleeHitOutcome hitOutCome;
};

struct SpellNonMeleeDamage
{
    SpellNonMeleeDamage(Unit* _attacker, Unit* _target, uint32 _SpellID, SpellSchools _school)
        : target(_target), attacker(_attacker), SpellID(_SpellID), damage(0), school(_school),
        absorb(0), resist(0), physicalLog(false), unused(false), blocked(0), HitInfo(0)
    {}

    Unit*   target;
    Unit*   attacker;
    uint32 SpellID;
    uint32 damage;
    SpellSchools school;
    uint32 absorb;
    uint32 resist;
    bool   physicalLog;
    bool   unused;
    uint32 blocked;
    uint32 HitInfo;
};

struct SpellPeriodicAuraLogInfo
{
    SpellPeriodicAuraLogInfo(Aura* _aura, uint32 _damage, uint32 _absorb, uint32 _resist, float _multiplier)
        : aura(_aura), damage(_damage), absorb(_absorb), resist(_resist), multiplier(_multiplier) {}

    Aura*   aura;
    uint32 damage;
    uint32 absorb;
    uint32 resist;
    float  multiplier;
};

uint32 createProcExtendMask(SpellNonMeleeDamage* damageInfo, SpellMissInfo missCondition);

enum SpellAuraProcResult
{
    SPELL_AURA_PROC_OK              = 0,
    SPELL_AURA_PROC_FAILED          = 1,
    SPELL_AURA_PROC_CANT_TRIGGER    = 2
};

typedef SpellAuraProcResult(Unit::*pAuraProcHandler)(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
extern pAuraProcHandler AuraProcHandler[TOTAL_AURAS];

enum CurrentSpellTypes
{
    CURRENT_MELEE_SPELL             = 0,
    CURRENT_GENERIC_SPELL           = 1,
    CURRENT_AUTOREPEAT_SPELL        = 2,
    CURRENT_CHANNELED_SPELL         = 3
};

#define CURRENT_FIRST_NON_MELEE_SPELL 1
#define CURRENT_MAX_SPELL             4

enum ControlledUnitMask
{
    CONTROLLED_PET       = 0x01,
    CONTROLLED_MINIPET   = 0x02,
    CONTROLLED_GUARDIANS = 0x04,
    CONTROLLED_CHARM     = 0x08,
    CONTROLLED_TOTEMS    = 0x10,
};

#define REACTIVE_TIMER_START 4000

enum ReactiveType
{
    REACTIVE_DEFENSE      = 1,
    REACTIVE_HUNTER_PARRY = 2,
    REACTIVE_OVERPOWER    = 5
};

#define MAX_REACTIVE 6

#define ATTACK_DISPLAY_DELAY 200
#define MAX_PLAYER_STEALTH_DETECT_RANGE 45.0f
#define MAX_CREATURE_ATTACK_RADIUS 45.0f

#define REGEN_TIME_FULL     2000

enum PowerDefaults
{
    POWER_RAGE_DEFAULT              = 1000,
    POWER_FOCUS_DEFAULT             = 100,
    POWER_ENERGY_DEFAULT            = 100,
    POWER_HAPPINESS_DEFAULT         = 1000000,
};

struct SpellProcEventEntry;

float CombatReachBetween(Unit const& attacker, Unit const& victim, bool forMeleeRange = true,
                         float flat_mod = 0.0f);
float CombatDistanceBetween(Unit const& attacker, Unit const& target, bool forMeleeRange);
bool InMeleeReach(Unit const& attacker, Unit const& victim, float flat_mod = 0.0f);

class Unit : public Occupant
{
    public:
        typedef std::set<Unit*> AttackerSet;

        typedef ::SpellAuraHolderMap SpellAuraHolderMap;
        typedef ::SpellAuraHolderBounds SpellAuraHolderBounds;
        typedef ::SpellAuraHolderConstBounds SpellAuraHolderConstBounds;
        typedef ::SpellAuraHolderList SpellAuraHolderList;
        typedef ::AuraList AuraList;

        typedef std::set < uint32  > ComboPointHolderSet;
        typedef ::TrackedAuraTargetMap TrackedAuraTargetMap;

        virtual ~Unit();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        void CleanupsBeforeDelete() override;

        bool IsControlledByPlayer() const override
        {
            return IsCharmerOrOwnerPlayerOrPlayerItself();
        }

        bool OutlivesItsGrid() const override { return IsPet(); }

        virtual void MovedTo(float x, float y, float z, float o) = 0;

        virtual bool MovesItself() const = 0;

        float ComputeBoundingRadius() const override
        {
            return GetFloatValue(UNIT_FIELD_BOUNDINGRADIUS);
        }

        unit::Diminishing& Diminishing() { return m_diminishing; }
        const unit::Diminishing& Diminishing() const { return m_diminishing; }

        void Update(uint32 update_diff, uint32 time) override;

        void setAttackTimer(WeaponAttackType type, uint32 time) { m_attackTimer[type] = time; }

        void resetAttackTimer(WeaponAttackType type = BASE_ATTACK);

        uint32 getAttackTimer(WeaponAttackType type) const { return m_attackTimer[type]; }

        bool isAttackReady(WeaponAttackType type = BASE_ATTACK) const { return m_attackTimer[type] == 0; }

        bool haveOffhandWeapon() const;

        bool UpdateMeleeAttackingState();

        bool CanUseEquippedWeapon(WeaponAttackType attackType) const
        {
            if (IsInFeralForm())
            {
                return false;
            }

            switch (attackType)
            {
                default:
                case BASE_ATTACK:
                    return !HasUnitFlag(UNIT_FLAG_DISARMED);
                case OFF_ATTACK:
                case RANGED_ATTACK:
                    return true;
            }
        }

        uint32 m_extraAttacks;

        std::vector<combat::SplitShare> m_owedSplits;
        SpellSchoolMask m_owedSplitSchool = SPELL_SCHOOL_MASK_NORMAL;

        void _addAttacker(Unit* pAttacker)
        {
            AttackerSet::const_iterator itr = m_attackers.find(pAttacker);
            if (itr == m_attackers.end())
            {
                m_attackers.insert(pAttacker);
            }
        }

        void _removeAttacker(Unit* pAttacker)
        {
            m_attackers.erase(pAttacker);
        }

        Unit* getAttackerForHelper()
        {
            if (getVictim() != nullptr)
            {
                return getVictim();
            }

            if (!m_attackers.empty())
            {
                return *(m_attackers.begin());
            }

            return nullptr;
        }

        bool Attack(Unit* victim, bool meleeAttack);

        void AttackedBy(Unit* attacker);

        void CastStop(uint32 except_spellid = 0);

        bool AttackStop(bool targetSwitch = false);

        void RemoveAllAttackers();

        AttackerSet const& getAttackers() const { return m_attackers; }

        bool isAttackingPlayer() const;

        Unit* getVictim() const { return m_attacking; }

        void CombatStop(bool includingCast = false);

        void CombatStopWithPets(bool includingCast = false);

        void StopAttackFaction(uint32 faction_id);

        Unit* SelectRandomUnfriendlyTarget(Unit* except = nullptr, float radius = ATTACK_DISTANCE) const;

        Unit* SelectRandomFriendlyTarget(Unit* except = nullptr, float radius = ATTACK_DISTANCE) const;

        Unit* FindLowestHpFriendlyUnit(float fRange, uint32 uiMinHPDiff = 1, bool bPercent = false, Unit* except = nullptr) const;

        Unit* FindFriendlyUnitMissingBuff(float range, uint32 spellid, Unit* except = nullptr) const;

        Unit* FindFriendlyUnitCC(float range) const;

        bool hasNegativeAuraWithInterruptFlag(uint32 flag);

        void SendMeleeAttackStop(Unit* victim);

        void SendMeleeAttackStart(Unit* pVictim);

        void addUnitState(uint32 f) { m_state |= f; }

        bool hasUnitState(uint32 f) const { return (m_state & f); }

        void clearUnitState(uint32 f) { m_state &= ~f; }

        bool CanFreeMove() const
        {
            return !hasUnitState(UNIT_STAT_NO_FREE_MOVE) && !GetOwnerGuid();
        }

        uint32 getLevel() const { return GetUInt32Value(UNIT_FIELD_LEVEL); }

        virtual uint32 GetLevelForTarget(Unit const* ) const { return getLevel(); }

        void SetLevel(uint32 lvl);

        uint8 getRace() const { return GetByteValue(UNIT_FIELD_BYTES_0, 0); }
        void SetRace(uint8 race) { SetByteValue(UNIT_FIELD_BYTES_0, 0, race); }

        uint32 getRaceMask() const { return 1 << (getRace() - 1); }

        uint8 getClass() const { return GetByteValue(UNIT_FIELD_BYTES_0, 1); }
        void SetClass(uint8 unitClass) { SetByteValue(UNIT_FIELD_BYTES_0, 1, unitClass); }

        uint32 getClassMask() const { return 1 << (getClass() - 1); }

        uint8 getGender() const { return GetByteValue(UNIT_FIELD_BYTES_0, 2); }
        void SetGender(uint8 gender) { SetByteValue(UNIT_FIELD_BYTES_0, 2, gender); }

        float GetStat(Stats stat) const { return float(GetUInt32Value(UNIT_FIELD_STAT0 + stat)); }

        void SetStat(Stats stat, int32 val) { SetStatInt32Value(UNIT_FIELD_STAT0 + stat, val); }

        uint32 GetArmor() const { return GetResistance(SPELL_SCHOOL_NORMAL) ; }

        void SetArmor(int32 val) { SetResistance(SPELL_SCHOOL_NORMAL, val); }

        uint32 GetResistance(SpellSchools school) const { return GetUInt32Value(UNIT_FIELD_RESISTANCES + school); }

        void SetResistance(SpellSchools school, int32 val) { SetStatInt32Value(UNIT_FIELD_RESISTANCES + school, val); }

        uint32 GetHealth()    const { return m_health; }

        float GetCastSpeedMod() const { return GetFloatValue(UNIT_MOD_CAST_SPEED); }
        void SetCastSpeedMod(float factor) { SetFloatValue(UNIT_MOD_CAST_SPEED, factor); }
        void ApplyCastSpeedMod(float percent, bool apply)
        {
            ApplyPercentModFloatValue(UNIT_MOD_CAST_SPEED, percent, apply);
        }

        float GetPowerCostMultiplier(uint32 school) const
        {
            return GetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + school);
        }
        void SetPowerCostMultiplier(uint32 school, float value)
        {
            SetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + school, value);
        }
        void ApplyPowerCostMultiplier(uint32 school, float value, bool apply)
        {
            ApplyModSignedFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + school, value, apply);
        }

        void SetBoundingRadius(float radius) { SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, radius); }
        void SetCombatReach(float reach) { SetFloatValue(UNIT_FIELD_COMBATREACH, reach); }
        float GetCombatReachValue() const { return GetFloatValue(UNIT_FIELD_COMBATREACH); }

        float GetShownDamage(bool offHand, bool maximum) const
        {
            uint16 const field = offHand ? (maximum ? UNIT_FIELD_MAXOFFHANDDAMAGE : UNIT_FIELD_MINOFFHANDDAMAGE)
                                         : (maximum ? UNIT_FIELD_MAXDAMAGE : UNIT_FIELD_MINDAMAGE);
            return GetFloatValue(field);
        }

        uint8 GetVirtualItemInfo(uint8 slot, uint8 part) const
        {
            return GetByteValue(UNIT_VIRTUAL_ITEM_INFO + slot * 2, part);
        }

        void SetAttackPower(bool ranged, int32 base, int32 bonus, float multiplier)
        {
            uint16 const bonusField = ranged ? UNIT_FIELD_RANGED_ATTACK_POWER_MODS
                                             : UNIT_FIELD_ATTACK_POWER_MODS;
            SetInt32Value(ranged ? UNIT_FIELD_RANGED_ATTACK_POWER : UNIT_FIELD_ATTACK_POWER, base);
            SetInt16Value(bonusField, 0, static_cast<int16>(bonus > 0 ? bonus : 0));
            SetInt16Value(bonusField, 1, static_cast<int16>(bonus < 0 ? bonus : 0));
            SetFloatValue(ranged ? UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER
                                 : UNIT_FIELD_ATTACK_POWER_MULTIPLIER, multiplier);
        }

        int32 GetAttackPowerBase(bool ranged) const
        {
            return GetInt32Value(ranged ? UNIT_FIELD_RANGED_ATTACK_POWER : UNIT_FIELD_ATTACK_POWER);
        }

        int32 GetAttackPowerBonus(bool ranged) const
        {
            uint16 const field = ranged ? UNIT_FIELD_RANGED_ATTACK_POWER_MODS
                                        : UNIT_FIELD_ATTACK_POWER_MODS;
            return static_cast<int16>(GetUInt16Value(field, 0))
                 + static_cast<int16>(GetUInt16Value(field, 1));
        }

        float GetAttackPowerMultiplier(bool ranged) const
        {
            return GetFloatValue(ranged ? UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER
                                        : UNIT_FIELD_ATTACK_POWER_MULTIPLIER);
        }

        bool HasUnitFlag(uint32 flag) const { return HasFlag(UNIT_FIELD_FLAGS, flag); }
        void SetUnitFlag(uint32 flag) { SetFlag(UNIT_FIELD_FLAGS, flag); }
        void RemoveUnitFlag(uint32 flag) { RemoveFlag(UNIT_FIELD_FLAGS, flag); }
        void ApplyUnitFlag(uint32 flag, bool apply) { ApplyModFlag(UNIT_FIELD_FLAGS, flag, apply); }
        uint32 GetUnitFlags() const { return GetUInt32Value(UNIT_FIELD_FLAGS); }
        void SetAllUnitFlags(uint32 flags) { SetUInt32Value(UNIT_FIELD_FLAGS, flags); }

        bool HasNpcFlag(uint32 flag) const { return HasFlag(UNIT_NPC_FLAGS, flag); }
        void SetNpcFlag(uint32 flag) { SetFlag(UNIT_NPC_FLAGS, flag); }
        void RemoveNpcFlag(uint32 flag) { RemoveFlag(UNIT_NPC_FLAGS, flag); }
        void ApplyNpcFlag(uint32 flag, bool apply) { ApplyModFlag(UNIT_NPC_FLAGS, flag, apply); }

        bool HasDynFlag(uint32 flag) const { return HasFlag(UNIT_DYNAMIC_FLAGS, flag); }
        void SetDynFlag(uint32 flag) { SetFlag(UNIT_DYNAMIC_FLAGS, flag); }
        void RemoveDynFlag(uint32 flag) { RemoveFlag(UNIT_DYNAMIC_FLAGS, flag); }
        void ApplyDynFlag(uint32 flag, bool apply) { ApplyModFlag(UNIT_DYNAMIC_FLAGS, flag, apply); }

        bool HealthAbovePctHealed(int32 pct, uint32 heal) const { return uint64(GetHealth()) + uint64(heal) > CountPctFromMaxHealth(pct); }
        uint32 GetMaxHealth() const { return m_maxHealth; }
        bool IsFullHealth() const { return GetHealth() == GetMaxHealth(); }
        bool HealthBelowPct(int32 pct) const { return GetHealth() < CountPctFromMaxHealth(pct); }
        bool HealthBelowPctDamaged(int32 pct, uint32 damage) const { return int64(GetHealth()) - int64(damage) < int64(CountPctFromMaxHealth(pct)); }
        bool HealthAbovePct(int32 pct) const { return GetHealth() > CountPctFromMaxHealth(pct); }

        float GetHealthPercent() const { return (GetHealth() * 100.0f) / GetMaxHealth(); }
        uint32 CountPctFromMaxHealth(int32 pct) const { return (GetMaxHealth() * static_cast<float>(pct) / 100.0f); }
        uint32 CountPctFromCurHealth(int32 pct) const { return (GetHealth() * static_cast<float>(pct) / 100.0f); }

        void SetHealth(uint32 val);

        void SetMaxHealth(uint32 val);

        void SetHealthPercent(float percent);

        int32 ModifyHealth(int32 val);

        Powers GetPowerType() const { return Powers(GetByteValue(UNIT_FIELD_BYTES_0, 3)); }

        void SetPowerKind(Powers kind) { SetByteValue(UNIT_FIELD_BYTES_0, 3, kind); }
        void SetPowerType(Powers power);
        uint32 GetPower(Powers power) const { return GetUInt32Value(UNIT_FIELD_POWER1 + power); }
        uint32 GetMaxPower(Powers power) const { return GetUInt32Value(UNIT_FIELD_MAXPOWER1 + power); }
        void SetPower(Powers power, uint32 val);
        void SetMaxPower(Powers power, uint32 val);
        int32 ModifyPower(Powers power, int32 val);

        void ApplyPowerMod(Powers power, uint32 val, bool apply);

        void ApplyMaxPowerMod(Powers power, uint32 val, bool apply);

        uint32 GetAttackTime(WeaponAttackType att) const { return (uint32)(GetFloatValue(UNIT_FIELD_BASEATTACKTIME + att) / m_modAttackSpeedPct[att]); }

        void SetAttackTime(WeaponAttackType att, uint32 val) { SetFloatValue(UNIT_FIELD_BASEATTACKTIME + att, val * m_modAttackSpeedPct[att]); }

        void ApplyAttackTimePercentMod(WeaponAttackType att, float val, bool apply);

        void ApplyCastTimePercentMod(float val, bool apply);

        SheathState GetSheath() const { return SheathState(GetByteValue(UNIT_FIELD_BYTES_2, 0)); }

        virtual void SetSheath(SheathState sheathed) { SetByteValue(UNIT_FIELD_BYTES_2, 0, sheathed); }

        uint32 getFaction() const { return GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE); }

        void setFaction(uint32 faction) { SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, faction); }
        FactionTemplateEntry const* getFactionTemplateEntry() const;

        bool IsContestedGuard() const
        {
            if (FactionTemplateEntry const* entry = getFactionTemplateEntry())
            {
                return entry->IsContestedGuardFaction();
            }

            return false;
        }

        bool IsPvP() const { return HasUnitFlag(UNIT_FLAG_PVP); }

        void SetPvP(bool state);

        uint32 GetCreatureType() const;

        CreatureRecord Record() const;

        CreatureInfo const* GetCreatureInfo() const { return m_creatureInfo; }

        Vigil& Watch() { return m_vigil; }
        Vigil const& Watch() const { return m_vigil; }

        time_t GetRespawnTime() const { return m_vigil.RespawnsAt(); }
        time_t GetRespawnTimeEx() const { return m_vigil.BackAt(time(nullptr)); }
        void SetRespawnTime(uint32 respawn) { m_vigil.RespawnsAt(respawn ? time(nullptr) + respawn : 0); }

        uint32 GetRespawnDelay() const { return m_vigil.RespawnDelay(); }
        void SetRespawnDelay(uint32 delay) { m_vigil.RespawnDelay(delay); }

        uint32 GetCorpseDelay() const { return m_vigil.CorpseDelay(); }
        void SetCorpseDelay(uint32 delay) { m_vigil.CorpseDelay(delay); }

        float GetRespawnRadius() const { return m_station.Radius(); }
        void SetRespawnRadius(float dist) { m_station.Radius(dist); }

        Station& Stationed() { return m_station; }
        Station const& Stationed() const { return m_station; }

        LootClaim& Claim() { return m_claim; }
        LootClaim const& Claim() const { return m_claim; }

        Pickings& Taking() { return m_pickings; }
        Pickings const& Taking() const { return m_pickings; }

        void SetNoCallAssistance(bool called) { m_calledForHelp = called; }
        void SetNoSearchAssistance(bool searched) { m_searchedForHelp = searched; }
        bool HasSearchedAssistance() const { return m_searchedForHelp; }

        Disguise& Colours() { return m_disguise; }
        Disguise const& Colours() const { return m_disguise; }

        Repertoire& Knowing() { return m_repertoire; }
        Repertoire const& Knowing() const { return m_repertoire; }

        void _AddCreatureSpellCooldown(uint32 spellId, time_t readyAt) { m_repertoire.ReadyAt(spellId, readyAt); }
        void _AddCreatureCategoryCooldown(uint32 category, time_t usedAt) { m_repertoire.CategoryUsedAt(category, usedAt); }
        void AddCreatureSpellCooldown(uint32 spellId);
        bool HasSpellCooldown(uint32 spellId) const;
        bool HasCategoryCooldown(uint32 spellId) const;
        uint32 GetCreatureSpellCooldownDelay(uint32 spellId) const { return m_repertoire.Left(spellId, time(nullptr)); }

        bool IsVendor()       const { return HasNpcFlag(UNIT_NPC_FLAG_VENDOR); }

        bool IsTrainer()      const { return HasNpcFlag(UNIT_NPC_FLAG_TRAINER); }

        bool IsQuestGiver()   const { return HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER); }

        bool IsGossip()       const { return HasNpcFlag(UNIT_NPC_FLAG_GOSSIP); }

        bool IsTaxi()         const { return HasNpcFlag(UNIT_NPC_FLAG_FLIGHTMASTER); }

        bool IsBanker()       const { return HasNpcFlag(UNIT_NPC_FLAG_BANKER); }

        bool IsInnkeeper()    const { return HasNpcFlag(UNIT_NPC_FLAG_INNKEEPER); }

        bool IsSpiritGuide()  const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITGUIDE); }

        bool IsTabardDesigner()const { return HasNpcFlag(UNIT_NPC_FLAG_TABARDDESIGNER); }

        bool IsServiceProvider() const
        {
            return HasNpcFlag(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_FLIGHTMASTER |
                UNIT_NPC_FLAG_PETITIONER | UNIT_NPC_FLAG_BATTLEMASTER | UNIT_NPC_FLAG_BANKER |
                UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_SPIRITHEALER |
                UNIT_NPC_FLAG_SPIRITGUIDE | UNIT_NPC_FLAG_TABARDDESIGNER | UNIT_NPC_FLAG_AUCTIONEER);
        }

        bool IsSpiritService() const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITHEALER | UNIT_NPC_FLAG_SPIRITGUIDE); }

        bool IsGuildMaster()  const { return HasNpcFlag(UNIT_NPC_FLAG_PETITIONER); }

        bool IsBattleMaster() const { return HasNpcFlag(UNIT_NPC_FLAG_BATTLEMASTER); }

        bool IsArmorer()      const { return HasNpcFlag(UNIT_NPC_FLAG_REPAIR); }

        bool IsSpiritHealer() const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITHEALER); }

        uint32 GetEquipmentId() const { return m_equipmentId; }
        uint32 GetCurrentEquipmentId() const { return m_equipmentId; }

        uint32 GetOriginalEntry() const { return m_originalEntry; }
        void SetOriginalEntry(uint32 entry) { m_originalEntry = entry; }

        SpellSchoolMask GetMeleeDamageSchoolMask() const { return m_meleeDamageSchoolMask; }
        void SetMeleeDamageSchool(SpellSchools school) { m_meleeDamageSchoolMask = GetSchoolMask(school); }

        bool IsReputationGainDisabled() const { return m_noReputationGain; }
        void SetDisableReputationGain(bool disable) { m_noReputationGain = disable; }

        bool AiLocked() const { return m_aiLocked; }
        void AiLocked(bool locked) { m_aiLocked = locked; }

        uint32 GetVendorItemCurrentCount(VendorItem const* vItem);
        uint32 UpdateVendorItemCurrentCount(VendorItem const* vItem, uint32 used_count);

        void SetFactionTemporary(uint32 factionId, uint32 flags = TEMPFACTION_ALL) { m_disguise.Wear(factionId, flags); }
        void ClearTemporaryFaction() { m_disguise.TakeOff(); }
        uint32 GetTemporaryFactionFlags() const { return m_disguise.Flags(); }

        Geometry::Placement const& Spawn() const { return m_station.Where(); }

        Geometry::Vector3 const& CombatAnchor() const { return m_station.Anchor(); }
        void SetCombatAnchor(Geometry::Vector3 const& at) { m_station.Anchor(at); }

        MovementGeneratorType GetDefaultMovementType() const { return m_station.Wander(); }
        void SetDefaultMovementType(MovementGeneratorType mgt) { m_station.Wander(mgt); }

        time_t GetKilledTime() const { return m_vigil.KilledAt(); }
        void SetKilledTime(time_t when) { m_vigil.KilledAt(when); }

        bool IsDeadByDefault() const { return m_vigil.DeadByDefault(); }
        void SetDeadByDefault(bool dead) { m_vigil.DeadByDefault(dead); }

        CreatureSubtype GetSubtype() const { return m_subtype; }
        bool IsPet() const { return m_subtype == CREATURE_SUBTYPE_PET; }
        bool IsTotem() const { return m_subtype == CREATURE_SUBTYPE_TOTEM; }
        bool IsTemporarySummon() const { return m_subtype == CREATURE_SUBTYPE_TEMPORARY_SUMMON; }

        uint32 GetCreatureTypeMask() const
        {
            uint32 creatureType = GetCreatureType();
            return (creatureType >= 1) ? (1 << (creatureType - 1)) : 0;
        }

        bool isVendor()       const { return HasNpcFlag(UNIT_NPC_FLAG_VENDOR); }
        bool isTrainer()      const { return HasNpcFlag(UNIT_NPC_FLAG_TRAINER); }
        bool isQuestGiver()   const { return HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER); }
        bool isGossip()       const { return HasNpcFlag(UNIT_NPC_FLAG_GOSSIP); }
        bool isTaxi()         const { return HasNpcFlag(UNIT_NPC_FLAG_FLIGHTMASTER); }
        bool isGuildMaster()  const { return HasNpcFlag(UNIT_NPC_FLAG_PETITIONER); }
        bool isBattleMaster() const { return HasNpcFlag(UNIT_NPC_FLAG_BATTLEMASTER); }
        bool isBanker()       const { return HasNpcFlag(UNIT_NPC_FLAG_BANKER); }
        bool isInnkeeper()    const { return HasNpcFlag(UNIT_NPC_FLAG_INNKEEPER); }
        bool isSpiritHealer() const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITHEALER); }
        bool isSpiritGuide()  const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITGUIDE); }
        bool isTabardDesigner()const { return HasNpcFlag(UNIT_NPC_FLAG_TABARDDESIGNER); }
        bool isArmorer()      const { return HasNpcFlag(UNIT_NPC_FLAG_REPAIR); }
        bool isServiceProvider() const
        {
            return HasNpcFlag(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_FLIGHTMASTER |
                UNIT_NPC_FLAG_PETITIONER | UNIT_NPC_FLAG_BATTLEMASTER | UNIT_NPC_FLAG_BANKER |
                UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_SPIRITHEALER |
                UNIT_NPC_FLAG_SPIRITGUIDE | UNIT_NPC_FLAG_TABARDDESIGNER | UNIT_NPC_FLAG_AUCTIONEER);
        }
        bool isSpiritService() const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITHEALER | UNIT_NPC_FLAG_SPIRITGUIDE); }

        uint8 getStandState() const { return GetByteValue(UNIT_FIELD_BYTES_1, 0); }

        bool IsAlwaysStanding() const { return HasByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND); }
        void SetAlwaysStanding(bool on) { ApplyModByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND, on); }

        bool IsCreeping() const { return HasByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAGS_CREEP); }
        void SetCreeping(bool on) { ApplyModByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAGS_CREEP, on); }

        bool IsUntrackable() const { return HasByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_UNTRACKABLE); }
        void SetUntrackable(bool on) { ApplyModByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_UNTRACKABLE, on); }

        uint8 GetBearing() const { return GetByteValue(UNIT_FIELD_BYTES_1, 3); }
        void SetBearing(uint8 bits) { SetByteValue(UNIT_FIELD_BYTES_1, 3, bits); }

        uint8 GetLoyaltyByte() const { return GetByteValue(UNIT_FIELD_BYTES_1, 1); }
        void SetLoyaltyByte(uint8 value) { SetByteValue(UNIT_FIELD_BYTES_1, 1, value); }

        bool IsSitState() const;

        bool IsStandState() const;

        bool IsSeatedState() const;

        void SetStandState(uint8 state);

        bool IsMounted() const { return GetMountID(); }

        uint32 GetMountID() const { return GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID); }

        virtual void Mount(uint32 mount, uint32 spellId = 0);

        virtual void Unmount(bool from_aura = false);

        uint16 GetMaxSkillValueForLevel(Unit const* target = nullptr) const { return (target ? GetLevelForTarget(target) : getLevel()) * 5; }

        void DealDamageMods(Unit* pVictim, uint32& damage, uint32* absorb);

        uint32 DealDamage(Unit* pVictim, uint32 damage, CleanDamage const* cleanDamage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto, bool durabilityLoss);

        int32 DealHeal(Unit* pVictim, uint32 addhealth, SpellEntry const* spellProto, bool critical = false);

        void PetOwnerKilledUnit(Unit* pVictim);

        static constexpr uint32 MAX_PROC_DEPTH = 8;

        void OweSplits(const std::vector<combat::SplitShare>& splits, SpellSchoolMask school);

        void DeliverOwedSplits();

        void ProcDamageAndSpell(Unit* pVictim, uint32 procAttacker, uint32 procVictim, uint32 procEx, uint32 amount, WeaponAttackType attType = BASE_ATTACK, SpellEntry const* procSpell = nullptr);

        void ProcDamageAndSpellFor(bool isVictim, Unit* pTarget, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, SpellEntry const* procSpell, uint32 damage);

        void HandleEmote(uint32 emote_id);

        void HandleEmoteCommand(uint32 emote_id);

        void HandleEmoteState(uint32 emote_id);

        void AttackerStateUpdate(Unit* pVictim, WeaponAttackType attType = BASE_ATTACK, bool extra = false);

        float MeleeMissChanceCalc(const Unit* pVictim, WeaponAttackType attType) const;

        void CalculateMeleeDamage(Unit* pVictim, CalcDamageInfo* damageInfo, WeaponAttackType attackType = BASE_ATTACK);

        void DealMeleeDamage(CalcDamageInfo* damageInfo, bool durabilityLoss);

        void HandleProcExtraAttackFor(Unit* victim);

        void CalculateSpellDamage(SpellNonMeleeDamage* damageInfo, int32 damage, SpellEntry const* spellInfo, WeaponAttackType attackType = BASE_ATTACK);

        void DealSpellDamage(SpellNonMeleeDamage* damageInfo, bool durabilityLoss);

        float  MeleeSpellMissChance(Unit* pVictim, WeaponAttackType attType, int32 skillDiff, SpellEntry const* spell);

        SpellMissInfo MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell);

        SpellMissInfo MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell);

        SpellMissInfo SpellHitResult(Unit* pVictim, SpellEntry const* spell, bool canReflect = false);

        float GetUnitDodgeChance()    const;

        float GetUnitParryChance()    const;

        float GetUnitBlockChance()    const;

        float GetUnitCriticalChance(WeaponAttackType attackType, const Unit* pVictim) const;

        uint32 GetUnitMeleeSkill(Unit const* target = nullptr) const { return (target ? GetLevelForTarget(target) : getLevel()) * 5; }

        uint32 GetDefenseSkillValue(Unit const* target = nullptr) const;

        uint32 GetWeaponSkillValue(WeaponAttackType attType, Unit const* target = nullptr) const;

        float GetWeaponProcChance() const;

        float GetPPMProcChance(uint32 WeaponSpeed, float PPM) const;

        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType) const;

        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType, int32 crit_chance, int32 miss_chance, int32 dodge_chance, int32 parry_chance, int32 block_chance, bool SpellCasted) const;

        bool isAuctioner()    const { return HasNpcFlag(UNIT_NPC_FLAG_AUCTIONEER); }

        bool IsTaxiFlying()  const { return hasUnitState(UNIT_STAT_TAXI_FLIGHT); }

        bool IsInCombat()  const { return HasUnitFlag(UNIT_FLAG_IN_COMBAT); }

        void SetInCombatState(bool PvP, Unit* enemy = nullptr);
        void SetInDummyCombatState(bool state);

        void SetInCombatWith(Unit* enemy);

        void ClearInCombat();

        uint32 GetCombatTimer() const { return m_CombatTimer; }

        SpellAuraHolderBounds GetSpellAuraHolderBounds(uint32 spell_id)
        {
            return m_auras.Of(spell_id);
        }

        SpellAuraHolderConstBounds GetSpellAuraHolderBounds(uint32 spell_id) const
        {
            return m_auras.Of(spell_id);
        }

        bool HasAuraType(AuraType auraType) const;

        bool HasAffectedAura(AuraType auraType, SpellEntry const* spellProto) const;

        bool HasAura(uint32 spellId, SpellEffectIndex effIndex) const;

        bool HasAura(uint32 spellId) const
        {
            return m_auras.Holds(spellId);
        }

        virtual bool HasSpell(uint32 spellId) const { return m_repertoire.Knows(spellId); }

        bool HasStealthAura()      const { return HasAuraType(SPELL_AURA_MOD_STEALTH); }

        bool HasInvisibilityAura() const { return HasAuraType(SPELL_AURA_MOD_INVISIBILITY); }

        bool IsFeared()  const { return HasAuraType(SPELL_AURA_MOD_FEAR); }

        bool IsInRoots() const { return HasAuraType(SPELL_AURA_MOD_ROOT); }

        bool IsPolymorphed() const;

        bool IsFrozen() const;

        bool IsTargetableForAttack(bool inverseAlive = false) const;

        bool isPassiveToHostile()
        {
            return HasUnitFlag(UNIT_FLAG_PASSIVE);
        }

        virtual bool IsInWater() const;

        virtual bool IsUnderWater() const;

        bool isInAccessablePlaceFor(Creature const* c) const;

        void SendHealSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, bool critical = false);

        void SendEnergizeSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype);

        void EnergizeBySpell(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype);

        uint32 SpellNonMeleeDamageLog(Unit* pVictim, uint32 spellID, uint32 damage);

        void CastSpell(Unit* Victim, uint32 spellId, bool triggered, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = 0, SpellEntry const* triggeredBy = nullptr);

        void CastSpell(Unit* Victim, SpellEntry const* spellInfo, bool triggered, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = 0, SpellEntry const* triggeredBy = nullptr);

        void CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = 0, SpellEntry const* triggeredBy = nullptr);

        void CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = 0, SpellEntry const* triggeredBy = nullptr);

        void CastSpell(float x, float y, float z, uint32 spellId, bool triggered, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = 0, SpellEntry const* triggeredBy = nullptr);

        void CastSpell(float x, float y, float z, SpellEntry const* spellInfo, bool triggered, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = 0, SpellEntry const* triggeredBy = nullptr);

        void DeMorph();

        void SendAttackStateUpdate(CalcDamageInfo* damageInfo);

        void SendAttackStateUpdate(uint32 HitInfo, Unit* target, SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage, uint32 Resist, VictimState TargetState, uint32 BlockedAmount);

        void SendSpellNonMeleeDamageLog(SpellNonMeleeDamage* log);

        void SendSpellNonMeleeDamageLog(Unit* target, uint32 SpellID, uint32 Damage, SpellSchoolMask damageSchoolMask, uint32 AbsorbedDamage, uint32 Resist, bool PhysicalDamage, uint32 Blocked, bool CriticalHit = false);

        void SendPeriodicAuraLog(SpellPeriodicAuraLogInfo* pInfo);

        void SendSpellMiss(Unit* target, uint32 spellID, SpellMissInfo missInfo);

        void NearTeleportTo(float x, float y, float z, float orientation, bool casting = false);

        void MonsterMoveWithSpeed(float x, float y, float z, float speed, bool generatePath = false, bool forceDestination = false);

        void SendHeartBeat();

        void WriteMovementInfo(ByteBuffer& out) const;

        bool IsLevitating() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING); }

        bool IsWalking() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE); }

        bool IsRooted() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_ROOT); }
        virtual void SetLevitate(bool ) {}
        virtual void SetSwim(bool ) {}
        virtual void SetCanFly(bool ) {}
        virtual void SetFeatherFall(bool ) {}
        virtual void SetHover(bool ) {}

        virtual void SetRoot(bool ) {}

        virtual void SetWaterWalk(bool ) {}

        void SetInFront(Unit const* target);

        void SetFacingTo(float ori);

        void SetFacingToObject(Occupant* pObject);

        bool IsAlive() const { return (m_deathState == ALIVE); };
        bool IsDying() const { return (m_deathState == JUST_DIED); }

        bool IsDead() const { return (m_deathState == DEAD || m_deathState == CORPSE); };

        DeathState GetDeathState() const { return m_deathState; };

        virtual void SetDeathState(DeathState s);

        ObjectGuid GetOwnerGuid() const { return  GetGuidValue(UNIT_FIELD_SUMMONEDBY); }
        void SetOwnerGuid(ObjectGuid owner) { SetGuidValue(UNIT_FIELD_SUMMONEDBY, owner); }
        ObjectGuid GetCreatorGuid() const { return GetGuidValue(UNIT_FIELD_CREATEDBY); }
        void SetCreatorGuid(ObjectGuid creator) { SetGuidValue(UNIT_FIELD_CREATEDBY, creator); }
        ObjectGuid GetPetGuid() const { return GetGuidValue(UNIT_FIELD_SUMMON); }
        void SetPetGuid(ObjectGuid pet) { SetGuidValue(UNIT_FIELD_SUMMON, pet); }
        ObjectGuid GetCharmerGuid() const { return GetGuidValue(UNIT_FIELD_CHARMEDBY); }
        void SetCharmerGuid(ObjectGuid owner) { SetGuidValue(UNIT_FIELD_CHARMEDBY, owner); }
        ObjectGuid GetCharmGuid() const { return GetGuidValue(UNIT_FIELD_CHARM); }
        void SetCharmGuid(ObjectGuid charm) { SetGuidValue(UNIT_FIELD_CHARM, charm); }
        ObjectGuid GetTargetGuid() const { return GetGuidValue(UNIT_FIELD_TARGET); }
        void SetTargetGuid(ObjectGuid targetGuid) { SetGuidValue(UNIT_FIELD_TARGET, targetGuid); }
        ObjectGuid GetChannelObjectGuid() const { return GetGuidValue(UNIT_FIELD_CHANNEL_OBJECT); }
        void SetChannelObjectGuid(ObjectGuid targetGuid) { SetGuidValue(UNIT_FIELD_CHANNEL_OBJECT, targetGuid); }

        virtual Pet* GetMiniPet() const { return nullptr; }

        ObjectGuid GetCharmerOrOwnerGuid() const { return GetCharmerGuid() ? GetCharmerGuid() : GetOwnerGuid(); }

        ObjectGuid GetCharmerOrOwnerOrOwnGuid() const
        {
            if (ObjectGuid guid = GetCharmerOrOwnerGuid())
            {
                return guid;
            }
            return GetObjectGuid();
        }

        bool IsCharmedOwnedByPlayerOrPlayer() const { return (GetCharmerOrOwnerOrOwnGuid() != 0 && GuidHigh(GetCharmerOrOwnerOrOwnGuid()) == HIGHGUID_PLAYER); }

        Player* GetSpellModOwner() const;

        Unit* GetOwner() const;

        Pet* GetPet() const;

        Unit* GetCharmer() const;

        Unit* GetCharm() const;

        void Uncharm();

        Unit* GetCharmerOrOwner() const { return GetCharmerGuid() ? GetCharmer() : GetOwner(); }

        Unit* GetCharmerOrOwnerOrSelf()
        {
            if (Unit* u = GetCharmerOrOwner())
            {
                return u;
            }

            return this;
        }
        bool IsCharmerOrOwnerPlayerOrPlayerItself() const;
        Player* GetCharmerOrOwnerPlayerOrPlayerItself();
        Player const* GetCharmerOrOwnerPlayerOrPlayerItself() const;

        void SetPet(Pet* pet);

        void SetCharm(Unit* pet);

        Retinue& Retainers() { return m_retinue; }
        Retinue const& Retainers() const { return m_retinue; }

        bool IsCharmed() const { return !(GetCharmerGuid() == 0); }

        CharmInfo* GetCharmInfo()
        {
            return m_charmInfo ? &m_charmInfo.value() : nullptr;
        }
        CharmInfo const* GetCharmInfo() const
        {
            return m_charmInfo ? &m_charmInfo.value() : nullptr;
        }

        CharmInfo& InitCharmInfo();

        template<typename Func>
            void CallForAllControlledUnits(Func const& func, uint32 controlledMask);

        template<typename Func>
            bool CheckAllControlledUnits(Func const& func, uint32 controlledMask) const;

        bool AddSpellAuraHolder(SpellAuraHolder* holder);

        void AddAuraToModList(Aura* aura);

        void RemoveAura(Aura* aura, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        void RemoveAura(uint32 spellId, SpellEffectIndex effindex, Aura* except = nullptr);

        void RemoveHolder(SpellAuraHolder* holder, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        void RemoveAuraEffect(SpellAuraHolder* holder, SpellEffectIndex index, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        void RemoveAuraEffect(uint32 id, SpellEffectIndex index, ObjectGuid casterGuid, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        void RemoveAuras(uint32 spellId, SpellAuraHolder* except = nullptr, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        void RemoveAurasFromItem(Item* castItem, uint32 spellId);

        void RemoveAurasByCaster(ObjectGuid casterGuid);

        void RemoveAurasCastBy(uint32 spellId, ObjectGuid casterGuid);

        void CancelAuras(uint32 spellId);

        void RemoveTrackedAurasOfOthers();

        void RemoveAurasAtMechanicImmunity(uint32 mechMask, uint32 exceptSpellId, bool non_positive = false);

        void RemoveAurasOfType(AuraType auraType);

        void RemoveAurasOfType(AuraType auraType, SpellAuraHolder* except);

        void RemoveAurasOfType(AuraType auraType, ObjectGuid casterGuid);

        void RemoveOtherRanks(uint32 spellId);

        bool RemoveConflictingAuras(SpellAuraHolder* holder);

        void RemoveAurasWithInterruptFlags(uint32 flags);

        void RemoveAurasWithAttribute(uint32 flags);

        void RemoveAurasWithDispelType(DispelType type, ObjectGuid casterGuid = 0);

        void RemoveAllAuras(AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        void RemoveAllAurasOnDeath();

        void RemoveAllAurasOnEvade();

        void RemoveStacks(uint32 spellId, uint32 stackAmount = 1, ObjectGuid casterGuid = 0, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        void DelaySpellAuraHolder(uint32 spellId, int32 delaytime, ObjectGuid casterGuid);

        void SetCreateHealth(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_HEALTH, val); }
        uint32 GetCreateHealth() const { return GetUInt32Value(UNIT_FIELD_BASE_HEALTH); }
        void SetCreateMana(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_MANA, val); }
        uint32 GetCreateMana() const { return GetUInt32Value(UNIT_FIELD_BASE_MANA); }
        uint32 GetCreatePowers(Powers power) const;

        void SetCurrentCastedSpell(Spell* pSpell);
        virtual void ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs);
        bool IsSchoolLockedOut(SpellSchoolMask schoolMask) const;
        void InterruptSpell(CurrentSpellTypes spellType, bool withDelayed = true);
        void FinishSpell(CurrentSpellTypes spellType, bool ok = true);

        bool IsClientControlled(Player const* exactClient = nullptr) const;

        bool IsNonMeleeSpellCasted(bool withDelayed, bool skipChanneled = false, bool skipAutorepeat = false, bool forMovement = false, bool forAutoIgnore = false) const;

        void InterruptNonMeleeSpells(bool withDelayed, uint32 spellid = 0);

        Spell* GetCurrentSpell(CurrentSpellTypes spellType) const { return m_currentSpells[spellType]; }
        Spell* GetCurrentSpell(uint32 spellType) const { return m_currentSpells[spellType]; }
        Spell* FindCurrentSpellBySpellId(uint32 spell_id) const;

        bool CheckAndIncreaseCastCounter();
        void DecreaseCastCounter()
        {
            if (m_castCounter)
            {
                --m_castCounter;
            }
        }

        ObjectGuid m_ObjectSlotGuid[4] = {};
        uint32 m_detectInvisibilityMask;
        uint32 m_invisibilityMask;

        void SeesInvisibility(uint8 kind, bool can)
        {
            if (can)
            {
                m_detectInvisibilityMask |= (1 << kind);
            }
            else
            {
                m_detectInvisibilityMask &= ~(1 << kind);
            }
        }

        ShapeshiftForm GetShapeshiftForm() const { return ShapeshiftForm(GetByteValue(UNIT_FIELD_BYTES_1, 2)); }
        void  SetShapeshiftForm(ShapeshiftForm form) { SetByteValue(UNIT_FIELD_BYTES_1, 2, form); }

        bool IsInFeralForm() const
        {
            ShapeshiftForm form = GetShapeshiftForm();
            return form == FORM_CAT || form == FORM_BEAR || form == FORM_DIREBEAR;
        }

        bool IsInDisallowedMountForm() const
        {
            ShapeshiftForm form = GetShapeshiftForm();
            return form != FORM_NONE &&
                form != FORM_BATTLESTANCE &&
                form != FORM_BERSERKERSTANCE &&
                form != FORM_DEFENSIVESTANCE &&
                form != FORM_SHADOW &&
                form != FORM_STEALTH;
        }

        float m_modMeleeHitChance;
        float m_modRangedHitChance;
        float m_modSpellHitChance;
        int32 m_baseSpellCritChance;

        float m_threatModifier[MAX_SPELL_SCHOOL];
        float m_modAttackSpeedPct[3];

        EventProcessor m_Events;

        SpellSchoolMask m_schoolLockoutMask;
        time_t m_schoolLockoutExpire;

        Tallies& Tallied() { return m_tallies; }
        Tallies const& Tallied() const { return m_tallies; }

        virtual StatSheet& Sheet() = 0;
        virtual StatSheet const& Sheet() const = 0;

        float GetTotalAttackPowerValue(WeaponAttackType attType) const;
        float GetWeaponDamageRange(WeaponAttackType attType , WeaponDamageRange type) const;
        void SetBaseWeaponDamage(WeaponAttackType attType , WeaponDamageRange damageRange, float value) { m_weaponDamage[attType][damageRange] = value; }

        SpellSchools GetWeaponDamageSchool(WeaponAttackType attType, uint8 index = 0) const { return m_weaponDamageInfo.weapon[attType].damage[index].school; }
        void SetWeaponDamageSchool(WeaponAttackType attType, SpellSchools school, uint8 index = 0) { m_weaponDamageInfo.weapon[attType].damage[index].school = school; }

        UnitVisibility GetVisibility() const { return m_Visibility; }
        void SetVisibility(UnitVisibility x);
        void UpdateVisibilityAndView() override;

        bool IsVisibleForOrDetect(Unit const* u, Occupant const* viewPoint, bool detect, bool inVisibleList = false, bool is3dDistance = true) const;
        bool CanDetectInvisibilityOf(Unit const* u) const;

        bool IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const override;

        virtual bool IsVisibleInGridForPlayer(Player* pl) const = 0;
        bool IsInvisibleForAlive() const;

        TrackedAuraTargetMap&       GetTrackedAuraTargets(TrackedAuraType type)       { return m_auras.Tracked(type); }
        TrackedAuraTargetMap const& GetTrackedAuraTargets(TrackedAuraType type) const { return m_auras.Tracked(type); }

        bool CanHaveThreatList(bool ignoreAliveState = false) const;
        void AddThreat(Unit* pVictim, float threat = 0.0f, bool crit = false, SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NONE, SpellEntry const* threatSpell = nullptr);
        float ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NORMAL);
        void DeleteThreatList();
        bool IsSecondChoiceTarget(Unit* pTarget, bool checkThreatArea);
        bool SelectHostileTarget();
        void TauntApply(Unit* pVictim);
        void TauntFadeOut(Unit* taunter);
        void FixateTarget(Unit* pVictim);
        ObjectGuid GetFixateTargetGuid() const { return m_fixateTargetGuid; }
        ThreatManager& GetThreatManager()
        {
            return m_ThreatManager;
        }

        ThreatManager const& GetThreatManager() const { return m_ThreatManager; }
        void AddHatedBy(HostileReference* pHostileReference) { m_HostileRefManager.insertFirst(pHostileReference); };
        void RemoveHatedBy(HostileReference* ) {  }

        HostileRefManager& GetHostileRefManager()
        {
            return m_HostileRefManager;
        }

        Aura* GetAura(uint32 spellId, SpellEffectIndex effindex);
        Aura* GetAura(AuraType type, SpellFamily family, uint64 familyFlag, ObjectGuid casterGuid = 0);
        SpellAuraHolder* GetSpellAuraHolder(uint32 spellid) const;
        SpellAuraHolder* GetSpellAuraHolder(uint32 spellid, ObjectGuid casterGUID) const;

        AuraBook&       Carrying()       { return m_auras; }
        AuraBook const& Carrying() const { return m_auras; }

        SpellAuraHolderMap&       GetSpellAuraHolderMap()       { return m_auras.All(); }
        SpellAuraHolderMap const& GetSpellAuraHolderMap() const { return m_auras.All(); }

        auras::Index::Range GetAurasByType(AuraType type) const { return m_auraIndex.Of(type); }

        std::vector<Aura*> GetAurasByRecency(AuraType type) const { return m_auraIndex.ByRecency(type); }

        void ApplyAuraProcTriggerDamage(Aura* aura, bool apply);

        int32 GetTotalAuraModifier(AuraType auratype) const;
        float GetTotalAuraMultiplier(AuraType auratype) const;
        int32 GetMaxPositiveAuraModifier(AuraType auratype) const;
        int32 GetMaxNegativeAuraModifier(AuraType auratype) const;

        int32 GetTotalAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        float GetTotalAuraMultiplierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        int32 GetMaxPositiveAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        int32 GetMaxNegativeAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;

        int32 GetTotalAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;
        float GetTotalAuraMultiplierByMiscValue(AuraType auratype, int32 misc_value) const;
        int32 GetMaxPositiveAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;
        int32 GetMaxNegativeAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;

        Aura* GetDummyAura(uint32 spell_id) const;

        uint32 m_AuraFlags;

        uint32 GetDisplayId() const { return GetUInt32Value(UNIT_FIELD_DISPLAYID); }
        void SetDisplayId(uint32 modelId);
        uint32 GetNativeDisplayId() const { return GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID); }
        void SetNativeDisplayId(uint32 modelId) { SetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID, modelId); }
        void SetTransform(uint32 spellid) { m_transform = spellid;}
        uint32 GetTransform() const { return m_transform;}

        void UpdateModelData();
        float GetObjectScaleMod() const;

        Conjurations& Conjured() { return m_conjured; }
        Conjurations const& Conjured() const { return m_conjured; }

        uint32 CalculateDamage(WeaponAttackType attType, bool normalized);
        float GetAPMultiplier(WeaponAttackType attType, bool normalized);
        void ModifyAuraState(AuraState flag, bool apply);
        bool HasAuraState(AuraState flag) const { return HasFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1)); }
        Unit* SelectMagnetTarget(Unit* victim, Spell* spell = nullptr, SpellEffectIndex eff = EFFECT_INDEX_0);

        int32 SpellBonusWithCoeffs(Unit* pCaster, SpellEntry const* spellProto, int32 total, int32 benefit, int32 ap_benefit, DamageEffectType damagetype, bool donePart, Spell const* spell = nullptr);
        int32 SpellBaseDamageBonusDone(SpellSchoolMask schoolMask);
        int32 SpellBaseDamageBonusTaken(SpellSchoolMask schoolMask);
        uint32 SpellDamageBonusDone(Unit* pVictim, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
        uint32 SpellDamageBonusTaken(Unit* pCaster, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
        int32 SpellBaseHealingBonusDone(SpellSchoolMask schoolMask);
        int32 SpellBaseHealingBonusTaken(SpellSchoolMask schoolMask);
        uint32 SpellHealingBonusDone(Unit* pVictim, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack = 1, Spell const* spell = nullptr);
        uint32 SpellHealingBonusTaken(Unit* pCaster, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack = 1, Spell const* spell = nullptr);
        uint32 MeleeDamageBonusDone(Unit* pVictim, uint32 damage, WeaponAttackType attType, SpellEntry const* spellProto = nullptr, DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1);
        uint32 MeleeDamageBonusTaken(Unit* pCaster, uint32 pdamage, WeaponAttackType attType, SpellEntry const* spellProto = nullptr, DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1);

        bool   IsSpellBlocked(Unit* pCaster, SpellEntry const* spellProto, WeaponAttackType attackType = BASE_ATTACK);
        bool   IsSpellCrit(Unit* pVictim, SpellEntry const* spellProto, SpellSchoolMask schoolMask, WeaponAttackType attackType = BASE_ATTACK);
        uint32 SpellCriticalDamageBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim);
        uint32 SpellCriticalHealingBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim);

        bool IsTriggeredAtSpellProcEvent(Unit* pVictim, SpellAuraHolder* holder, SpellEntry const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, bool isVictim, SpellProcEventEntry const*& spellProcEvent);

        SpellAuraProcResult HandleDummyAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleHasteAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleProcTriggerSpellAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleProcTriggerDamageAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleOverrideClassScriptAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleModCastingSpeedNotStackAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleReflectSpellsSchoolAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleModPowerCostSchoolAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleMechanicImmuneResistanceAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleModResistanceAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleRemoveByDamageChanceProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleInvisibilityAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleNULLProc(Unit* , uint32 , Aura* , SpellEntry const* , uint32 , uint32 , uint32 )
        {

            return SPELL_AURA_PROC_OK;
        }
        SpellAuraProcResult HandleCantTrigger(Unit* , uint32 , Aura* , SpellEntry const* , uint32 , uint32 , uint32 )
        {

            return SPELL_AURA_PROC_CANT_TRIGGER;
        }

        void SetLastManaUse()
        {
            m_recovery.ManaSpent();
        }
        bool IsUnderLastManaUseEffect() const { return m_recovery.HoldingBack(); }

        void SetContestedPvP(Player* attackedPlayer = nullptr);

        void ApplySpellImmune(uint32 spellId, uint32 op, uint32 type, bool apply);
        void ApplySpellDispelImmunity(const SpellEntry* spellProto, DispelType type, bool apply);
        virtual bool IsImmuneToSpell(SpellEntry const* spellInfo, bool castOnSelf);
        virtual bool IsImmuneToDamage(SpellSchoolMask meleeSchoolMask);
        virtual bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const;

        uint32 CalcArmorReducedDamage(Unit* pVictim, const uint32 damage);
        void CalculateDamageAbsorbAndResist(Unit* pCaster, SpellSchoolMask schoolMask, DamageEffectType damagetype, const uint32 damage, uint32* absorb, uint32* resist, bool canReflect = false);
        void CalculateAbsorbResistBlock(Unit* pCaster, SpellNonMeleeDamage* damageInfo, SpellEntry const* spellProto, WeaponAttackType attType = BASE_ATTACK);

        virtual Pace& Pacing() = 0;
        virtual Pace const& Pacing() const = 0;

        bool IsHover() const { return HasAuraType(SPELL_AURA_HOVER); }

        void _RemoveAllAuraMods();
        void _ApplyAllAuraMods();

        int32 CalculateSpellDamage(Unit const* target, const cast::Recipe& recipe, const cast::Operation& operation, int32 const* basePoints = nullptr);

        float CalculateLevelPenalty(SpellEntry const* spellProto) const;

        void AddFollower(FollowerReference* pRef) { m_FollowingRefManager.insertFirst(pRef); }
        void RemoveFollower(FollowerReference* ) {  }

        MotionMaster* GetMotionMaster()
        {
            return &i_motionMaster;
        }

        bool IsStopped() const { return !(hasUnitState(UNIT_STAT_MOVING)); }
        void StopMoving(bool forceSendStop = false);
        void InterruptMoving(bool forceSendStop = false);

        bool IsImmobilized() const { return hasUnitState(UNIT_STAT_ROOT | UNIT_STAT_STUNNED); }
        void SetImmobilizedState(bool apply, bool stun = false);

        bool IsFleeing() const { return HasUnitFlag(UNIT_FLAG_FLEEING); }
        bool IsConfused() const { return HasUnitFlag(UNIT_FLAG_CONFUSED); }
        bool IsStunned() const { return HasUnitFlag(UNIT_FLAG_STUNNED); }
        bool IsIncapacitated() const { return (IsFleeing() || IsConfused() || IsStunned()); }

        void SetFeared(bool apply, ObjectGuid casterGuid = 0, uint32 spellID = 0, uint32 time = 0);
        void SetConfused(bool apply, ObjectGuid casterGuid = 0, uint32 spellID = 0);
        void SetStunned(bool apply);
        void SetIncapacitatedState(bool apply, uint32 state = 0, ObjectGuid casterGuid = 0, uint32 spellID = 0, uint32 time = 0);

        void SetFeignDeath(bool apply, ObjectGuid casterGuid = 0);

        void AddComboPointHolder(uint32 lowguid) { m_ComboPointHolders.insert(lowguid); }
        void RemoveComboPointHolder(uint32 lowguid) { m_ComboPointHolders.erase(lowguid); }
        void ClearComboPointHolders();

        void SendPetCastFail(uint32 spellid, SpellCastResult msg);
        void SendPetActionFeedback(uint8 msg);
        void SendPetTalk(uint32 pettalk);
        void SendPetAIReaction();

        void PropagateSpeedChange()
        {
            GetMotionMaster()->PropagateSpeedChange();
        }

        void ClearAllReactives();
        void StartReactiveTimer(ReactiveType reactive) { m_reactiveTimer[reactive] = REACTIVE_TIMER_START;}
        void UpdateReactives(uint32 p_time);

        void UpdateAuraForGroup(uint8 slot);

        typedef ::PetAuraSet PetAuraSet;
        void AddPetAura(PetAura const* petSpell);
        void RemovePetAura(PetAura const* petSpell);

        MovementInfo m_movementInfo;
        Movement::MoveSpline* movespline;

        void ScheduleAINotify(uint32 delay);
        bool IsAINotifyScheduled() const { return m_AINotifyScheduled;}
        void _SetAINotifyScheduled(bool on) { m_AINotifyScheduled = on;}
        void OnRelocated();

        virtual bool CanSwim() const = 0;
        virtual bool CanFly() const = 0;

    protected:

        CreatureInfo const* m_creatureInfo = nullptr;

        CreatureSubtype m_subtype = CREATURE_SUBTYPE_GENERIC;

        Vigil m_vigil;

        Station m_station;

        LootClaim m_claim;

        Pickings m_pickings;

        bool m_calledForHelp = false;
        bool m_searchedForHelp = false;

        Disguise m_disguise;

        Repertoire m_repertoire;

        VendorItemCounts m_vendorItemCounts;

        uint32 m_equipmentId = 0;
        uint32 m_originalEntry = 0;
        SpellSchoolMask m_meleeDamageSchoolMask = SPELL_SCHOOL_MASK_NORMAL;

        bool m_noReputationGain = false;
        bool m_aiLocked = false;

        struct WeaponDamageInfo
        {
            struct Weapon
            {
                struct Damage
                {
                    SpellSchools school = SPELL_SCHOOL_NORMAL;
                    float value[2] = { BASE_MINDAMAGE, BASE_MAXDAMAGE };
                };

                uint32 lines = 1;
                Damage damage[MAX_ITEM_PROTO_DAMAGES];
            };

            Weapon weapon[MAX_ATTACK];
        };

        explicit Unit();

        void _UpdateSpells(uint32 time);
        void _UpdateAutoRepeatSpell();
        bool m_AutoRepeatFirstCast;

        uint32 m_attackTimer[MAX_ATTACK];

        AttackerSet m_attackers;
        Unit* m_attacking;

        DeathState m_deathState;

        AuraBook m_auras;

        Immunities m_immune;

        Conjurations m_conjured;
        bool m_isSorted;
        uint32 m_transform;

        auras::Index m_auraIndex;
        Tallies m_tallies;
        float m_weaponDamage[MAX_ATTACK][2];
        WeaponDamageInfo m_weaponDamageInfo;

        std::optional<CharmInfo> m_charmInfo;

        unit::Diminishing m_diminishing;

        MotionMaster i_motionMaster;

        uint32 m_reactiveTimer[MAX_REACTIVE];
        Recovery m_recovery;

        uint32 m_health;
        uint32 m_maxHealth;

        void DisableSpline();

    private:

        void CleanupDeletedAuras();
        void UpdateSplineMovement(uint32 t_diff);

        Pet* _GetPet(ObjectGuid guid) const;

        void JustKilledCreature(Creature* victim, Player* responsiblePlayer);

        uint32 m_state;
        uint32 m_CombatTimer;
        bool   m_dummyCombatState;

        Spell* m_currentSpells[CURRENT_MAX_SPELL];
        uint32 m_castCounter;

        UnitVisibility m_Visibility;
        Position m_last_notified_position;
        bool m_AINotifyScheduled;
        TimeTracker m_movesplineTimer;

        ThreatManager m_ThreatManager;

        HostileRefManager m_HostileRefManager;

        FollowerRefManager m_FollowingRefManager;

        ComboPointHolderSet m_ComboPointHolders;

        Retinue m_retinue;

        ObjectGuid m_fixateTargetGuid = 0;

    private:

        template <typename TR>
            void CastSpell(Unit* Victim, uint32 spell, TR triggered);
        template <typename TR>
            void CastSpell(Unit* Victim, SpellEntry const* spell, TR triggered);
        template <typename TR>
            void CastCustomSpell(Unit* Victim, uint32 spell, int32 const* bp0, int32 const* bp1, int32 const* bp2, TR triggered);
        template <typename SP, typename TR>
            void CastCustomSpell(Unit* Victim, SpellEntry const* spell, int32 const* bp0, int32 const* bp1, int32 const* bp2, TR triggered);
        template <typename TR>
            void CastSpell(float x, float y, float z, uint32 spell, TR triggered);
        template <typename TR>
            void CastSpell(float x, float y, float z, SpellEntry const* spell, TR triggered);
};

template<typename Func>
    void Unit::CallForAllControlledUnits(Func const& func, uint32 controlledMask)
{
    if (controlledMask & CONTROLLED_PET)
    {
        if (Pet* pet = GetPet())
        {
            func(pet);
        }
    }

    if (controlledMask & CONTROLLED_MINIPET)
    {
        if (Pet* mini = GetMiniPet())
        {
            func(mini);
        }
    }

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        GuidSet const& guardians = m_retinue.Guardians();
        for (GuidSet::const_iterator itr = guardians.begin(); itr != guardians.end();)
        {
            if (Pet* guardian = _GetPet(*(itr++)))
            {
                func(guardian);
            }
        }
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
        {
            if (Unit* totem = m_retinue.UnitIn(TotemSlot(i)))
            {
                func(totem);
            }
        }
    }

    if (controlledMask & CONTROLLED_CHARM)
    {
        if (Unit* charm = GetCharm())
        {
            func(charm);
        }
    }
}

template<typename Func>
    bool Unit::CheckAllControlledUnits(Func const& func, uint32 controlledMask) const
{
    if (controlledMask & CONTROLLED_PET)
    {
        if (Pet const* pet = GetPet())
        {
            if (func(pet))
            {
                return true;
            }
        }
    }

    if (controlledMask & CONTROLLED_MINIPET)
    {
        if (Pet* mini = GetMiniPet())
        {
            if (func(mini))
            {
                return true;
            }
        }
    }

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        GuidSet const& guardians = m_retinue.Guardians();
        for (GuidSet::const_iterator itr = guardians.begin(); itr != guardians.end();)
        {
            if (Pet const* guardian = _GetPet(*(itr++)))
            {
                if (func(guardian))
                {
                    return true;
                }
            }
        }
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
        {
            if (Unit const* totem = m_retinue.UnitIn(TotemSlot(i)))
            {
                if (func(totem))
                {
                    return true;
                }
            }
        }
    }

    if (controlledMask & CONTROLLED_CHARM)
    {
        if (Unit const* charm = GetCharm())
        {
            if (func(charm))
            {
                return true;
            }
        }
    }

    return false;
}
