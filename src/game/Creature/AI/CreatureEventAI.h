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

#include <unordered_map>
#include "Platform/Define.h"
#include <vector>
#include <list>
#include "Creature.h"
#include "CreatureAI.h"
#include "Unit.h"

class Player;
class Occupant;

#define EVENT_UPDATE_TIME               500
#define MAX_ACTIONS                     3
#define MAX_PHASE                       32

enum EventAI_Type
{
    EVENT_T_TIMER_IN_COMBAT         = 0,
    EVENT_T_TIMER_OOC               = 1,
    EVENT_T_HP                      = 2,
    EVENT_T_MANA                    = 3,
    EVENT_T_AGGRO                   = 4,
    EVENT_T_KILL                    = 5,
    EVENT_T_DEATH                   = 6,
    EVENT_T_EVADE                   = 7,
    EVENT_T_SPELLHIT                = 8,
    EVENT_T_RANGE                   = 9,
    EVENT_T_OOC_LOS                 = 10,
    EVENT_T_SPAWNED                 = 11,
    EVENT_T_TARGET_HP               = 12,
    EVENT_T_TARGET_CASTING          = 13,
    EVENT_T_FRIENDLY_HP             = 14,
    EVENT_T_FRIENDLY_IS_CC          = 15,
    EVENT_T_FRIENDLY_MISSING_BUFF   = 16,
    EVENT_T_SUMMONED_UNIT           = 17,
    EVENT_T_TARGET_MANA             = 18,
    EVENT_T_QUEST_ACCEPT            = 19,
    EVENT_T_QUEST_COMPLETE          = 20,
    EVENT_T_REACHED_HOME            = 21,
    EVENT_T_RECEIVE_EMOTE           = 22,
    EVENT_T_AURA                    = 23,
    EVENT_T_TARGET_AURA             = 24,
    EVENT_T_SUMMONED_JUST_DIED      = 25,
    EVENT_T_SUMMONED_JUST_DESPAWN   = 26,
    EVENT_T_MISSING_AURA            = 27,
    EVENT_T_TARGET_MISSING_AURA     = 28,
    EVENT_T_TIMER_GENERIC           = 29,
    EVENT_T_RECEIVE_AI_EVENT        = 30,
    EVENT_T_REACHED_WAYPOINT        = 31,
    EVENT_T_ENERGY                  = 32,
    EVENT_T_END,
};

enum EventAI_ActionType
{
    ACTION_T_NONE                       = 0,
    ACTION_T_TEXT                       = 1,
    ACTION_T_SET_FACTION                = 2,
    ACTION_T_MORPH_TO_ENTRY_OR_MODEL    = 3,
    ACTION_T_SOUND                      = 4,
    ACTION_T_EMOTE                      = 5,
    ACTION_T_RANDOM_SAY                 = 6,
    ACTION_T_RANDOM_YELL                = 7,
    ACTION_T_RANDOM_TEXTEMOTE           = 8,
    ACTION_T_RANDOM_SOUND               = 9,
    ACTION_T_RANDOM_EMOTE               = 10,
    ACTION_T_CAST                       = 11,
    ACTION_T_SUMMON                     = 12,
    ACTION_T_THREAT_SINGLE_PCT          = 13,
    ACTION_T_THREAT_ALL_PCT             = 14,
    ACTION_T_QUEST_EVENT                = 15,
    ACTION_T_CAST_EVENT                 = 16,
    ACTION_T_SET_UNIT_FIELD             = 17,
    ACTION_T_SET_UNIT_FLAG              = 18,
    ACTION_T_REMOVE_UNIT_FLAG           = 19,
    ACTION_T_AUTO_ATTACK                = 20,
    ACTION_T_COMBAT_MOVEMENT            = 21,
    ACTION_T_SET_PHASE                  = 22,
    ACTION_T_INC_PHASE                  = 23,
    ACTION_T_EVADE                      = 24,
    ACTION_T_FLEE_FOR_ASSIST            = 25,
    ACTION_T_QUEST_EVENT_ALL            = 26,
    ACTION_T_CAST_EVENT_ALL             = 27,
    ACTION_T_REMOVEAURASFROMSPELL       = 28,
    ACTION_T_RANGED_MOVEMENT            = 29,
    ACTION_T_RANDOM_PHASE               = 30,
    ACTION_T_RANDOM_PHASE_RANGE         = 31,
    ACTION_T_SUMMON_ID                  = 32,
    ACTION_T_KILLED_MONSTER             = 33,
    ACTION_T_SET_INST_DATA              = 34,
    ACTION_T_SET_INST_DATA64            = 35,
    ACTION_T_UPDATE_TEMPLATE            = 36,
    ACTION_T_DIE                        = 37,
    ACTION_T_ZONE_COMBAT_PULSE          = 38,
    ACTION_T_CALL_FOR_HELP              = 39,
    ACTION_T_SET_SHEATH                 = 40,
    ACTION_T_FORCE_DESPAWN              = 41,
    ACTION_T_SET_INVINCIBILITY_HP_LEVEL = 42,
    ACTION_T_MOUNT_TO_ENTRY_OR_MODEL    = 43,
    ACTION_T_CHANCED_TEXT               = 44,
    ACTION_T_THROW_AI_EVENT             = 45,
    ACTION_T_SET_THROW_MASK             = 46,
    ACTION_T_SET_STAND_STATE            = 47,
    ACTION_T_CHANGE_MOVEMENT            = 48,
    ACTION_T_SUMMON_UNIQUE              = 49,
    ACTION_T_EMOTE_TARGET               = 50,

    ACTION_T_END
};

enum Target
{

    TARGET_T_SELF                           = 0,

    TARGET_T_HOSTILE                        = 1,
    TARGET_T_HOSTILE_SECOND_AGGRO           = 2,
    TARGET_T_HOSTILE_LAST_AGGRO             = 3,
    TARGET_T_HOSTILE_RANDOM                 = 4,
    TARGET_T_HOSTILE_RANDOM_NOT_TOP         = 5,

    TARGET_T_ACTION_INVOKER                 = 6,
    TARGET_T_ACTION_INVOKER_OWNER           = 7,
    TARGET_T_EVENT_SENDER                   = 10,

    TARGET_T_HOSTILE_RANDOM_PLAYER          = 8,
    TARGET_T_HOSTILE_RANDOM_NOT_TOP_PLAYER  = 9,

    TARGET_T_END
};

enum EventFlags
{
    EFLAG_REPEATABLE            = 0x01,
    EFLAG_NORMAL                = 0x02,
    EFLAG_HEROIC                = 0x04,
    EFLAG_RESERVED_3            = 0x08,
    EFLAG_RESERVED_4            = 0x10,
    EFLAG_RANDOM_ACTION         = 0x20,
    EFLAG_RESERVED_6            = 0x40,
    EFLAG_DEBUG_ONLY            = 0x80,

};

enum SpawnedEventMode
{
    SPAWNED_EVENT_ALWAY = 0,
    SPAWNED_EVENT_MAP   = 1,
    SPAWNED_EVENT_ZONE  = 2
};

struct CreatureEventAI_Action
{
    EventAI_ActionType type: 16;
    union
    {

        struct
        {
            int32 TextId[3];
        } text;

        struct
        {
            uint32 factionId;
            uint32 factionFlags;
        } set_faction;

        struct
        {
            uint32 creatureId;
            uint32 modelId;
        } morph;

        struct
        {
            uint32 soundId;
        } sound;

        struct
        {
            uint32 emoteId;
        } emote;

        struct
        {
            int32 soundId1;
            int32 soundId2;
            int32 soundId3;
        } random_sound;

        struct
        {
            int32 emoteId1;
            int32 emoteId2;
            int32 emoteId3;
        } random_emote;

        struct
        {
            uint32 spellId;
            uint32 target;
            uint32 castFlags;
        } cast;

        struct
        {
            uint32 creatureId;
            uint32 target;
            uint32 duration;
        } summon;

        struct
        {
            int32 percent;
            uint32 target;
        } threat_single_pct;

        struct
        {
            int32 percent;
        } threat_all_pct;

        struct
        {
            uint32 questId;
            uint32 target;
        } quest_event;

        struct
        {
            uint32 creatureId;
            uint32 spellId;
            uint32 target;
        } cast_event;

        struct
        {
            uint32 field;
            uint32 value;
            uint32 target;
        } set_unit_field;

        struct
        {
            uint32 value;
            uint32 target;
        } unit_flag;

        struct
        {
            uint32 state;
        } auto_attack;

        struct
        {
            uint32 state;
            uint32 melee;
        } combat_movement;

        struct
        {
            uint32 phase;
        } set_phase;

        struct
        {
            int32 step;
        } set_inc_phase;

        struct
        {
            uint32 questId;
        } quest_event_all;

        struct
        {
            uint32 creatureId;
            uint32 spellId;
        } cast_event_all;

        struct
        {
            uint32 target;
            uint32 spellId;
        } remove_aura;

        struct
        {
            uint32 distance;
            int32  angle;
        } ranged_movement;

        struct
        {
            uint32 phase1;
            uint32 phase2;
            uint32 phase3;
        } random_phase;

        struct
        {
            uint32 phaseMin;
            uint32 phaseMax;
        } random_phase_range;

        struct
        {
            uint32 creatureId;
            uint32 target;
            uint32 spawnId;
        } summon_id;

        struct
        {
            uint32 creatureId;
            uint32 target;
        } killed_monster;

        struct
        {
            uint32 field;
            uint32 value;
        } set_inst_data;

        struct
        {
            uint32 field;
            uint32 target;
        } set_inst_data64;

        struct
        {
            uint32 creatureId;
            uint32 team;
        } update_template;

        struct
        {
            uint32 radius;
        } call_for_help;

        struct
        {
            uint32 sheath;
        } set_sheath;

        struct
        {
            uint32 msDelay;
        } forced_despawn;

        struct
        {
            uint32 hp_level;
            uint32 is_percent;
        } invincibility_hp_level;

        struct
        {
            uint32 creatureId;
            uint32 modelId;
        } mount;

        struct
        {
            uint32 chance;
            int32 TextId[2];
        } chanced_text;

        struct
        {
            uint32 eventType;
            uint32 radius;
            uint32 unused;
        } throwEvent;

        struct
        {
            uint32 eventTypeMask;
            uint32 unused1;
            uint32 unused2;
        } setThrowMask;

        struct
        {
            uint32 standState;
            uint32 unused1;
            uint32 unused2;
        } setStandState;

        struct
        {
            uint32 movementType;
            uint32 wanderDistance;
            uint32 unused1;
        } changeMovement;

        struct
        {
            uint32 creatureId;
            uint32 target;
            uint32 spawnId;
        } summon_unique;

        struct
        {
            uint32 emoteId;
            uint32 targetGuid;
        } emoteTarget;

        struct
        {
            uint32 param1;
            uint32 param2;
            uint32 param3;
        } raw;
    };
};

struct CreatureEventAI_Event
{
    uint32 event_id;

    uint32 creature_id;

    uint32 event_inverse_phase_mask;

    EventAI_Type event_type : 16;
    uint8 event_chance : 8;
    uint8 event_flags  : 8;

    union
    {

        struct
        {
            uint32 initialMin;
            uint32 initialMax;
            uint32 repeatMin;
            uint32 repeatMax;
        } timer;

        struct
        {
            uint32 percentMax;
            uint32 percentMin;
            uint32 repeatMin;
            uint32 repeatMax;
        } percent_range;

        struct
        {
            uint32 repeatMin;
            uint32 repeatMax;
        } kill;

        struct
        {
            uint32 spellId;
            uint32 schoolMask;
            uint32 repeatMin;
            uint32 repeatMax;
        } spell_hit;

        struct
        {
            uint32 minDist;
            uint32 maxDist;
            uint32 repeatMin;
            uint32 repeatMax;
        } range;

        struct
        {
            uint32 noHostile;
            uint32 maxRange;
            uint32 repeatMin;
            uint32 repeatMax;
        } ooc_los;

        struct
        {
            uint32 condition;
            uint32 conditionValue1;
        } spawned;

        struct
        {
            uint32 repeatMin;
            uint32 repeatMax;
        } target_casting;

        struct
        {
            uint32 hpDeficit;
            uint32 radius;
            uint32 repeatMin;
            uint32 repeatMax;
        } friendly_hp;

        struct
        {
            uint32 dispelType;
            uint32 radius;
            uint32 repeatMin;
            uint32 repeatMax;
        } friendly_is_cc;

        struct
        {
            uint32 spellId;
            uint32 radius;
            uint32 repeatMin;
            uint32 repeatMax;
        } friendly_buff;

        struct
        {
            uint32 creatureId;
            uint32 repeatMin;
            uint32 repeatMax;
        } summoned;

        struct
        {
            uint32 questId;
        } quest;

        struct
        {
            uint32 emoteId;
            uint32 condition;
            uint32 conditionValue1;
            uint32 conditionValue2;
        } receive_emote;

        struct
        {
            uint32 spellId;
            uint32 amount;
            uint32 repeatMin;
            uint32 repeatMax;
        } buffed;

        struct
        {
            uint32 eventType;
            uint32 senderEntry;
            uint32 unused1;
            uint32 unused2;
        } receiveAIEvent;

        struct
        {
            int positionX;
            int positionY;
            int positionZ;
            int distance;
        } reached_waypoint;

        struct
        {
            uint32 param1;
            uint32 param2;
            uint32 param3;
            uint32 param4;
        } raw;
    };

    CreatureEventAI_Action action[MAX_ACTIONS];
};

#define AIEVENT_DEFAULT_THROW_RADIUS    30.0f

typedef std::vector<CreatureEventAI_Event> CreatureEventAI_Event_Vec;
typedef std::unordered_map<uint32, CreatureEventAI_Event_Vec > CreatureEventAI_Event_Map;

struct CreatureEventAI_Summon
{
    uint32 id;

    float position_x;
    float position_y;
    float position_z;
    float orientation;
    uint32 SpawnTimeSecs;
};

typedef std::unordered_map<uint32, CreatureEventAI_Summon> CreatureEventAI_Summon_Map;

struct CreatureEventAIHolder
{
    CreatureEventAIHolder(CreatureEventAI_Event p) : Event(p), Time(0), Enabled(true) {}

    CreatureEventAI_Event Event;
    uint32 Time;
    bool Enabled;

    bool UpdateRepeatTimer(Creature* creature, uint32 repeatMin, uint32 repeatMax);
};

class CreatureEventAI : public CreatureAI
{
    public:
        explicit CreatureEventAI(Creature* c);
        ~CreatureEventAI()
        {
            m_CreatureEventAIList.clear();
        }

        void GetAIInformation(ChatHandler& reader) override;

        void JustRespawned() override;
        void Reset() override;
        void JustReachedHome() override;
        void EnterCombat(Unit* enemy) override;
        void EnterEvadeMode() override;
        void JustDied(Unit* killer) override;
        void KilledUnit(Unit* victim) override;
        void JustSummoned(Creature* pUnit) override;
        void AttackStart(Unit* who) override;
        void MoveInLineOfSight(Unit* who) override;
        void SpellHit(Unit* pUnit, const SpellEntry* pSpell) override;
        void OnSpellCastChange(const SpellEntry* pSpell, SpellCastResult reason) override;
        CanCastResult DoCastSpellIfCan(Unit* pTarget, uint32 uiSpell, uint32 uiCastFlags = 0, ObjectGuid OriginalCasterGuid = 0) override;
        void DamageTaken(Unit* done_by, uint32& damage) override;
        void HealedBy(Unit* healer, uint32& healedAmount) override;
        void UpdateAI(const uint32 diff) override;
        bool IsVisible(Unit*) const override;
        void ReceiveEmote(Player* pPlayer, uint32 text_emote) override;
        void SummonedCreatureJustDied(Creature* unit) override;
        void SummonedCreatureDespawn(Creature* unit) override;
        void ReceiveAIEvent(AIEventType eventType, Creature* pSender, Unit* pInvoker, uint32 miscValue) override;

        static int Permissible(const Creature*);

        bool ProcessEvent(CreatureEventAIHolder& pHolder, Unit* pActionInvoker = nullptr, Creature* pAIEventSender = nullptr);
        void ProcessAction(CreatureEventAI_Action const& action, uint32 rnd, uint32 EventId, Unit* pActionInvoker, Creature* pAIEventSender);
        inline uint32 GetRandActionParam(uint32 rnd, uint32 param1, uint32 param2, uint32 param3);
        inline int32 GetRandActionParam(uint32 rnd, int32 param1, int32 param2, int32 param3);

        inline Unit* GetTargetByType(uint32 Target, Unit* pActionInvoker, Creature* pAIEventSender, bool& isError, uint32 forSpellId = 0, uint32 selectFlags = 0);

        bool SpawnedEventConditionsCheck(CreatureEventAI_Event const& event);

        Unit* DoSelectLowestHpFriendly(float range, uint32 MinHPDiff);
        void DoFindFriendlyMissingBuff(std::list<Creature*>& _list, float range, uint32 spellid);
        void DoFindFriendlyCC(std::list<Creature*>& _list, float range);

    protected:
        uint32 m_EventUpdateTime;
        uint32 m_EventDiff;

        typedef std::vector<CreatureEventAIHolder> CreatureEventAIList;
        CreatureEventAIList m_CreatureEventAIList;

        uint8  m_Phase;
        bool   m_MeleeEnabled;
        bool   m_HasOOCLoSEvent;
        uint32 m_InvinceabilityHpLevel;
        uint32 m_currSpell;

        uint32 m_throwAIEventMask;

        uint32 m_throwAIEventStep;
};
