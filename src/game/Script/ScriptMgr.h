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
#include <string>
#include <vector>
#include <map>
#include <set>
#include "ObjectGuid.h"
#include "DBCEnums.h"

#include <atomic>
#include <mutex>
#include <shared_mutex>

struct AreaTriggerEntry;
struct SpellEntry;
class Aura;
class Creature;
class CreatureAI;
class GameObject;
class GameObjectAI;
class InstanceData;
class Item;
class Map;
class Object;
class Player;
class Quest;
class SpellCastTargets;
class Unit;
class Occupant;

enum DBScriptType
{
    DBS_INTERNAL              = -1,
    DBS_ON_QUEST_START        = 0,
    DBS_ON_QUEST_END          = 1,
    DBS_ON_GOSSIP             = 2,
    DBS_ON_CREATURE_MOVEMENT  = 3,
    DBS_ON_CREATURE_DEATH     = 4,
    DBS_ON_SPELL              = 5,
    DBS_ON_GO_USE             = 6,
    DBS_ON_GOT_USE            = 7,
    DBS_ON_EVENT              = 8,
    DBS_ON_CREATURE_SPELL     = 9,
    DBS_END                   = 10,
};
#define DBS_START DBS_ON_QUEST_START

enum ScriptedObjectType
{
    SCRIPTED_UNIT           = 0,
    SCRIPTED_GAMEOBJECT     = 1,
    SCRIPTED_ITEM           = 2,
    SCRIPTED_AREATRIGGER    = 3,
    SCRIPTED_SPELL          = 4,
    SCRIPTED_AURASPELL      = 5,
    SCRIPTED_MAPEVENT       = 6,
    SCRIPTED_MAP            = 7,
    SCRIPTED_BATTLEGROUND   = 8,
    SCRIPTED_PVP_ZONE       = 9,
    SCRIPTED_INSTANCE       = 10,
    SCRIPTED_CONDITION      = 11,
    SCRIPTED_ACHIEVEMENT    = 12,
    SCRIPTED_MAX_TYPE
};

enum ScriptImplementation
{
    SCRIPT_FROM_DATABASE    = 0,
    SCRIPT_FROM_CORE        = 1,
    SCRIPT_FROM_ELUNA       = 2,
};

enum DBScriptCommand
{
    SCRIPT_COMMAND_TALK                     = 0,

    SCRIPT_COMMAND_EMOTE                    = 1,

    SCRIPT_COMMAND_FIELD_SET                = 2,
    SCRIPT_COMMAND_MOVE_TO                  = 3,

    SCRIPT_COMMAND_FLAG_SET                 = 4,
    SCRIPT_COMMAND_FLAG_REMOVE              = 5,
    SCRIPT_COMMAND_TELEPORT_TO              = 6,
    SCRIPT_COMMAND_QUEST_EXPLORED           = 7,
    SCRIPT_COMMAND_KILL_CREDIT              = 8,
    SCRIPT_COMMAND_RESPAWN_GO               = 9,
    SCRIPT_COMMAND_TEMP_SUMMON_CREATURE     = 10,

    SCRIPT_COMMAND_OPEN_DOOR                = 11,
    SCRIPT_COMMAND_CLOSE_DOOR               = 12,
    SCRIPT_COMMAND_ACTIVATE_OBJECT          = 13,
    SCRIPT_COMMAND_REMOVE_AURA              = 14,
    SCRIPT_COMMAND_CAST_SPELL               = 15,

    SCRIPT_COMMAND_PLAY_SOUND               = 16,
    SCRIPT_COMMAND_CREATE_ITEM              = 17,
    SCRIPT_COMMAND_DESPAWN_SELF             = 18,
    SCRIPT_COMMAND_PLAY_MOVIE               = 19,
    SCRIPT_COMMAND_MOVEMENT                 = 20,

    SCRIPT_COMMAND_SET_ACTIVEOBJECT         = 21,

    SCRIPT_COMMAND_SET_FACTION              = 22,

    SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL  = 23,

    SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL  = 24,

    SCRIPT_COMMAND_SET_RUN                  = 25,

    SCRIPT_COMMAND_ATTACK_START             = 26,
    SCRIPT_COMMAND_GO_LOCK_STATE            = 27,

    SCRIPT_COMMAND_STAND_STATE              = 28,

    SCRIPT_COMMAND_MODIFY_NPC_FLAGS         = 29,

    SCRIPT_COMMAND_SEND_TAXI_PATH           = 30,
    SCRIPT_COMMAND_TERMINATE_SCRIPT         = 31,

    SCRIPT_COMMAND_PAUSE_WAYPOINTS          = 32,

    SCRIPT_COMMAND_JOIN_LFG                 = 33,
    SCRIPT_COMMAND_TERMINATE_COND           = 34,

    SCRIPT_COMMAND_SEND_AI_EVENT_AROUND     = 35,

    SCRIPT_COMMAND_TURN_TO                  = 36,
    SCRIPT_COMMAND_MOVE_DYNAMIC             = 37,

    SCRIPT_COMMAND_SEND_MAIL                = 38,

    SCRIPT_COMMAND_CHANGE_ENTRY             = 39,

    SCRIPT_COMMAND_DESPAWN_GO               = 40,
    SCRIPT_COMMAND_RESPAWN                  = 41,
    SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS      = 42,

    SCRIPT_COMMAND_RESET_GO                 = 43,
    SCRIPT_COMMAND_UPDATE_TEMPLATE          = 44,

    SCRIPT_COMMAND_XP_USER                  = 53,
    SCRIPT_COMMAND_SET_FLY                  = 59,

};

#define MAX_TEXT_ID 4

enum ScriptInfoDataFlags
{

    SCRIPT_FLAG_BUDDY_AS_TARGET             = 0x01,
    SCRIPT_FLAG_REVERSE_DIRECTION           = 0x02,
    SCRIPT_FLAG_SOURCE_TARGETS_SELF         = 0x04,
    SCRIPT_FLAG_COMMAND_ADDITIONAL          = 0x08,
    SCRIPT_FLAG_BUDDY_BY_GUID               = 0x10,
    SCRIPT_FLAG_BUDDY_IS_PET                = 0x20,
    SCRIPT_FLAG_BUDDY_IS_DESPAWNED          = 0x40,
};
#define MAX_SCRIPT_FLAG_VALID               (2 * SCRIPT_FLAG_BUDDY_IS_DESPAWNED - 1)

struct ScriptInfo
{
    uint32 id;
    uint32 delay;
    uint32 command;

    union
    {

        struct
        {
            uint32 emoteId;
            uint32 unused1;
        } emote;

        struct
        {
            uint32 fieldId;
            uint32 fieldValue;
        } setField;

        struct
        {
            uint32 unused1;
            uint32 travelSpeed;
        } moveTo;

        struct
        {
            uint32 fieldId;
            uint32 fieldValue;
        } setFlag;

        struct
        {
            uint32 fieldId;
            uint32 fieldValue;
        } removeFlag;

        struct
        {
            uint32 mapId;
            uint32 empty;
        } teleportTo;

        struct
        {
            uint32 questId;
            uint32 distance;
        } questExplored;

        struct
        {
            uint32 creatureEntry;
            uint32 isGroupCredit;
        } killCredit;

        struct
        {
            uint32 goGuid;
            uint32 despawnDelay;
        } respawnGo;

        struct
        {
            uint32 creatureEntry;
            uint32 despawnDelay;
        } summonCreature;

        struct
        {
            uint32 goGuid;
            uint32 resetDelay;
        } changeDoor;

        struct
        {
            uint32 empty1;
            uint32 empty2;
        } activateObject;

        struct
        {
            uint32 spellId;
            uint32 empty;
        } removeAura;

        struct
        {
            uint32 spellId;
            uint32 empty;
        } castSpell;

        struct
        {
            uint32 soundId;
            uint32 flags;
        } playSound;

        struct
        {
            uint32 itemEntry;
            uint32 amount;
        } createItem;

        struct
        {
            uint32 despawnDelay;
            uint32 empty;
        } despawn;

        struct
        {
            uint32 movieId;
            uint32 empty;
        } playMovie;

        struct
        {
            uint32 movementType;
            uint32 wanderDistance;
        } movement;

        struct
        {
            uint32 activate;
            uint32 empty;
        } activeObject;

        struct
        {
            uint32 factionId;
            uint32 flags;
        } faction;

        struct
        {
            uint32 creatureOrModelEntry;
            uint32 empty1;
        } morph;

        struct
        {
            uint32 creatureOrModelEntry;
            uint32 empty1;
        } mount;

        struct
        {
            uint32 run;
            uint32 empty;
        } run;

        struct
        {
            uint32 lockState;
            uint32 empty;
        } goLockState;

        struct
        {
            uint32 stand_state;
            uint32 unused1;
        } standState;

        struct
        {
            uint32 flag;
            uint32 change_flag;
        } npcFlag;

        struct
        {
            uint32 taxiPathId;
            uint32 empty;
        } sendTaxiPath;

        struct
        {
            uint32 npcEntry;
            uint32 searchDist;

        } terminateScript;

        struct
        {
            uint32 doPause;
            uint32 empty;
        } pauseWaypoint;

        struct
        {
            uint32 areaId;
        } joinLfg;

        struct
        {
            uint32 conditionId;
            uint32 failQuest;
        } terminateCond;

        struct
        {
            uint32 eventType;
            uint32 radius;
        } sendAIEvent;

        struct
        {
            uint32 targetId;
            uint32 empty1;
        } turnTo;

        struct
        {
            uint32 maxDist;
            uint32 minDist;
        } moveDynamic;

        struct
        {
            uint32 mailTemplateId;
            uint32 altSender;
        } sendMail;

        struct
        {
            uint32 creatureEntry;
            uint32 empty1;
        } changeEntry;

        struct
        {
            uint32 goGuid;
            uint32 respawnTime;
        } despawnGo;

        struct
        {
            uint32 resetDefault;
            uint32 empty;
        } setEquipment;

        struct
        {
            uint32 entry;
            uint32 faction;
        } updateTemplate;

        struct
        {
            uint32 flags;
            uint32 empty;
        } xpDisabled;

        struct
        {
            uint32 enable;
            uint32 empty;
        } fly;

        struct
        {
            uint32 data[2];
        } raw;
    };

    uint32 buddyEntry;
    uint32 searchRadiusOrGuid;
    uint8 data_flags;

    int32 textId[MAX_TEXT_ID];

    float x;
    float y;
    float z;
    float o;

    uint32 GetGOGuid() const
    {
        switch (command)
        {
            case SCRIPT_COMMAND_RESPAWN_GO:
                return respawnGo.goGuid;
            case SCRIPT_COMMAND_DESPAWN_GO:
                return despawnGo.goGuid;
            case SCRIPT_COMMAND_OPEN_DOOR:
            case SCRIPT_COMMAND_CLOSE_DOOR:
                return changeDoor.goGuid;
            default:
                return 0;
        }
    }

    bool IsCreatureBuddy() const
    {
        switch (command)
        {
            case SCRIPT_COMMAND_RESPAWN_GO:
            case SCRIPT_COMMAND_OPEN_DOOR:
            case SCRIPT_COMMAND_CLOSE_DOOR:
            case SCRIPT_COMMAND_ACTIVATE_OBJECT:
            case SCRIPT_COMMAND_GO_LOCK_STATE:
            case SCRIPT_COMMAND_DESPAWN_GO:
            case SCRIPT_COMMAND_RESET_GO:
                return false;
            default:
                return true;
        }
    }

    bool HasAdditionalScriptFlag() const
    {
        switch (command)
        {
            case SCRIPT_COMMAND_MOVE_TO:
            case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:
            case SCRIPT_COMMAND_CAST_SPELL:
            case SCRIPT_COMMAND_PLAY_SOUND:
            case SCRIPT_COMMAND_MOVEMENT:
            case SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL:
            case SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL:
            case SCRIPT_COMMAND_TERMINATE_SCRIPT:
            case SCRIPT_COMMAND_TERMINATE_COND:
            case SCRIPT_COMMAND_TURN_TO:
            case SCRIPT_COMMAND_MOVE_DYNAMIC:
            case SCRIPT_COMMAND_SET_FLY:
                return true;
            default:
                return false;
        }
    }
};

typedef std::vector < ScriptInfo > ScriptChain;
typedef std::map < uint32 , ScriptChain > ScriptChainMap;
typedef std::vector < ScriptChainMap > DBScripts;

class ScriptAction
{
    public:
        ScriptAction(DBScriptType _type, Map* _map, ObjectGuid _sourceGuid, ObjectGuid _targetGuid, ObjectGuid _ownerGuid, ScriptInfo const* _script)
            : m_type(_type), m_map(_map), m_sourceGuid(_sourceGuid), m_targetGuid(_targetGuid), m_ownerGuid(_ownerGuid), m_script(_script)
        {}

        bool HandleScriptStep();

        DBScriptType GetType() const
        {
            return m_type;
        }
        uint32 GetId() const
        {
            return m_script->id;
        }
        ObjectGuid GetSourceGuid() const
        {
            return m_sourceGuid;
        }
        ObjectGuid GetTargetGuid() const
        {
            return m_targetGuid;
        }
        ObjectGuid GetOwnerGuid() const
        {
            return m_ownerGuid;
        }

        bool IsSameScript(DBScriptType type, uint32 id, ObjectGuid sourceGuid, ObjectGuid targetGuid, ObjectGuid ownerGuid) const
        {
            return type == m_type && id == GetId() &&
                (sourceGuid == m_sourceGuid || !sourceGuid) &&
                (targetGuid == m_targetGuid || !targetGuid) &&
                (ownerGuid == m_ownerGuid || !ownerGuid);
        }

    private:
        DBScriptType m_type;
        Map* m_map;
        ObjectGuid m_sourceGuid = 0;
        ObjectGuid m_targetGuid = 0;
        ObjectGuid m_ownerGuid = 0;
        ScriptInfo const* m_script;

        bool GetScriptCommandObject(const ObjectGuid guid, bool includeItem, Object*& resultObject);
        bool GetScriptProcessTargets(Occupant* pOrigSource, Occupant* pOrigTarget, Occupant*& pFinalSource, Occupant*& pFinalTarget);
        bool LogIfNotCreature(Occupant* pOccupant);
        bool LogIfNotUnit(Occupant* pOccupant);
        bool LogIfNotGameObject(Occupant* pOccupant);
        bool LogIfNotPlayer(Occupant* pOccupant);
        Player* GetPlayerTargetOrSourceAndLog(Occupant* pSource, Occupant* pTarget);
};

enum ScriptLoadResult
{
    SCRIPT_LOAD_OK,
    SCRIPT_LOAD_ERR_NOT_FOUND,
    SCRIPT_LOAD_ERR_WRONG_API,
    SCRIPT_LOAD_ERR_OUTDATED,
};

class ScriptMgr
{
    public:
        ScriptMgr();
        ~ScriptMgr();

        std::string GenerateNameToId(ScriptedObjectType sot, uint32 id);

        void LoadDbScripts(DBScriptType type);
        void LoadDbScriptStrings();

        void LoadScriptNames();
        void LoadScriptBinding();
        void LoadAreaTriggerScripts();
        void LoadEventIdScripts();
        void LoadSpellIdScripts();

        uint32 GetAreaTriggerScriptId(uint32 triggerId) const;
        uint32 GetEventIdScriptId(uint32 eventId) const;

        bool ReloadScriptBinding();

        ScriptChainMap const* GetScriptChainMap(DBScriptType type);

        const char* GetScriptName(uint32 id) const
        {
            return id < m_scriptNames.size() ? m_scriptNames[id].c_str() : "";
        }

        uint32 GetScriptId(const char* name) const;

        uint32 GetScriptIdsCount() const
        {
            return m_scriptNames.size();
        }

        uint32 GetBoundScriptId(ScriptedObjectType entity, int32 entry);

        ScriptLoadResult LoadScriptLibrary(const char* libName);
        void UnloadScriptLibrary();
        bool IsScriptLibraryLoaded() const
        {
#ifdef ENABLE_SD3
            return true;
#else
            return false;
#endif
        }

        uint32 IncreaseScheduledScriptsCount()
        {
            return (uint32)++m_scheduledScripts;
        }
        uint32 DecreaseScheduledScriptCount()
        {
            return (uint32)--m_scheduledScripts;
        }
        uint32 DecreaseScheduledScriptCount(size_t count)
        {
            return (uint32)(m_scheduledScripts -= count);
        }
        bool IsScriptScheduled() const
        {
            return m_scheduledScripts > 0;
        }
        static bool CanSpellEffectStartDBScript(SpellEntry const* spellinfo, SpellEffectIndex effIdx);

        CreatureAI* GetCreatureAI(Creature* pCreature);

        GameObjectAI* GetGameObjectAI(GameObject* pGo);

        InstanceData* CreateInstanceData(Map* pMap);

        char const* GetScriptLibraryVersion() const;
        bool OnGossipHello(Player* pPlayer, Creature* pCreature);
        bool OnGossipHello(Player* pPlayer, GameObject* pGameObject);
        bool OnGossipHello(Player* pPlayer, Item* pItem);
        bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code);
        bool OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code);
        bool OnGossipSelect(Player* pPlayer, Item* pItem, uint32 sender, uint32 action, const char* code);
        bool OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest);
        bool OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest);
        bool OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest);
        bool OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 reward);
        bool OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 reward);
        uint32 GetDialogStatus(Player* pPlayer, Creature* pCreature);
        uint32 GetDialogStatus(Player* pPlayer, GameObject* pGameObject);
        bool OnGameObjectUse(Player* pPlayer, GameObject* pGameObject);
        bool OnGameObjectUse(Unit* pUnit, GameObject* pGameObject);
        bool OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets);
        bool OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry);
        bool OnNpcSpellClick(Player* pPlayer, Creature* pClickedCreature, uint32 spellId);
        bool OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart);
        bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid);
        bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid);
        bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid);
        bool OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid);
        bool OnAuraDummy(Aura const* pAura, bool apply);

    private:
        void CollectPossibleEventIds(std::set<uint32>& eventIds);
        void LoadScripts(DBScriptType type);
        void CheckScriptTexts(std::set<int32>& ids);

        typedef std::vector<std::string> ScriptNameMap;
        typedef std::unordered_map<int32, uint32> EntryToScriptIdMap;

        EntryToScriptIdMap m_scriptBind[SCRIPTED_MAX_TYPE];

        ScriptNameMap      m_scriptNames;
        DBScripts          m_dbScripts;
#ifdef _DEBUG

        std::shared_mutex m_bindMutex;
#endif

        std::atomic<long> m_scheduledScripts;
        char __cache_guard[1024];
        std::mutex m_lock;
};

bool StartEvents_Event(Map* map, uint32 id, Object* source, Object* target, bool isStart = true, Unit* forwardToPvp = nullptr);

#define sScriptMgr MaNGOS::Singleton<ScriptMgr>::Instance()

uint32 GetScriptId(const char* name);

char const* GetScriptName(uint32 id);

uint32 GetScriptIdsCount();

uint32 GetAreaTriggerScriptId(uint32 triggerId);

uint32 GetEventIdScriptId(uint32 eventId);

void SetExternalWaypointTable(char const* tableName);

bool AddWaypointFromExternal(uint32 entry, int32 pathId, uint32 pointId, float x, float y, float z, float o, uint32 waittime);
