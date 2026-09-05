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

#include "Reaction.h"
#include "Utterance.h"
#include "GameObject.h"
#include "Geometry/Quat.h"
#include "MineralVein.h"
#include "QuestDef.h"
#include "QuestBond.h"
#include "Kinds.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "InstanceData.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "GameObjectModel.h"
#include "CreatureAISelector.h"
#include "SQLStorages.h"
#include "GameObjectAI.h"
#include <memory>
#include "PlayerRegistry.h"
#include "ObjectLookup.h"



/**
 * @brief Creates a game object instance with default runtime state.
 */
GameObject::GameObject() : Occupant(),
    loot(this),
    m_model(nullptr),
    m_goInfo(nullptr),
    m_AI_locked(false)
{
    m_objectType |= TYPEMASK_GAMEOBJECT;
    m_objectTypeId = TYPEID_GAMEOBJECT;
    m_updateFlag = (UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);

    m_spawn.ComesBackAfter(25);
    m_lootState = GO_READY;
    m_spellId = 0;
    m_usableAt = 0;
    m_closesAt = 0;
}

/**
 * @brief Destroys the game object and its collision model.
 */
GameObject::~GameObject()
{
    delete m_model;
}

/**
 * @brief Adds the game object and its model to the world.
 */
void GameObject::AddToWorld()
{

    ///- Register the gameobject for guid lookup
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().insert<GameObject>(GetObjectGuid(), this);
    }

    if (m_model)
    {
        GetMap()->InsertGameObjectModel(*m_model);
    }

    Object::AddToWorld();

    // After Object::AddToWorld so that for initial state the GO is added to the world (and hence handled correctly)
    UpdateCollisionState();


}

/**
 * @brief Removes the game object and its model from the world.
 */
void GameObject::RemoveFromWorld()
{
    ///- Remove the gameobject from the accessor
    if (IsInWorld())
    {

        // Notify the outdoor pvp script
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z())))
        {
            outdoorPvP->HandleGameObjectRemove(this);
        }

        // Remove GO from owner
        if (ObjectGuid owner_guid = GetOwnerGuid())
        {
            if (Unit* owner = ObjectLookup::GetUnit(*this, owner_guid))
            {
                owner->RemoveGameObject(this, false);
            }
            else
            {
                sLog.outError("Delete %s with SpellId %u LinkedGO %u that lost references to owner %s GO list. Crash possible later.",
                    GetGuidStr().c_str(), m_spellId, GetGOInfo()->GetLinkedGameObjectEntry(), owner_guid.GetString().c_str());
            }
        }

        if (m_model && GetMap()->ContainsGameObjectModel(*m_model))
        {
            GetMap()->RemoveGameObjectModel(*m_model);
        }

        GetMap()->GetObjectsStore().erase<GameObject>(GetObjectGuid(), nullptr);
    }

    Object::RemoveFromWorld();
}

/**
 * @brief Performs cleanup before deleting the game object.
 */
void GameObject::CleanupsBeforeDelete()
{
    Occupant::CleanupsBeforeDelete();
}

/**
 * @brief Creates a game object from template and placement data.
 *
 * @param guidlow The low GUID to assign.
 * @param name_id The gameobject entry id.
 * @param map The target map.
 * @param x The x coordinate.
 * @param y The y coordinate.
 * @param z The z coordinate.
 * @param ang The facing angle.
 * @param r0 Quaternion x component.
 * @param r1 Quaternion y component.
 * @param r2 Quaternion z component.
 * @param r3 Quaternion w component.
 * @param animprogress The initial animation progress.
 * @param go_state The initial gameobject state.
 * @return true if creation succeeded; otherwise, false.
 */
bool GameObject::Create(uint32 guidlow, uint32 name_id, Map* map,float x, float y, float z, float ang,
    float r0, float r1, float r2, float r3, uint32 animprogress, GOState go_state)
{
    if (!map)
    {
        return false;
    }

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(name_id);
    if (!goinfo)
    {
        sLog.outErrorDb("Gameobject (GUID: %u) not created: Entry %u does not exist in `gameobject_template`", guidlow, name_id);
        return false;
    }

    if (goinfo->type >= MAX_GAMEOBJECT_TYPE)
    {
        sLog.outErrorDb("Gameobject (GUID: %u) not created: Entry %u has invalid type %u in `gameobject_template`. It may crash client if created.", guidlow, name_id, goinfo->type);
        return false;
    }

    Object::_Create(guidlow, goinfo->id, HIGHGUID_GAMEOBJECT);

    // A lift carries its phase in the create block, the way a vessel does: without it
    // the client animates the platform from its own uptime and no two of them agree.
    if (goinfo->type == GAMEOBJECT_TYPE_TRANSPORT)
    {
        m_updateFlag |= UPDATEFLAG_TRANSPORT;
    }

    // let's make sure we don't send the client invalid quaternion
    if (r0 == 0.0f && r1 == 0.0f && r2 == 0.0f)
    {
        r2 = sin(ang/2);
        r3 = cos(ang/2);
    }

    Geometry::Quat q(r0, r1, r2, r3);
    q.unitize();

    float o = Geometry::YawOf(q);
    Place().MoveTo(x, y, z, o);
    SetMap(map);

    if (!IsPlaceable(*this))
    {
        sLog.outError("Gameobject (GUID: %u Entry: %u ) not created. Suggested coordinates are invalid (X: %f Y: %f)", guidlow, name_id, x, y);
        return false;
    }

    SetQuaternion(q);
    SetGOInfo(goinfo);
    SetObjectScale(m_goInfo->size);
    SetGoPosition(x, y, z);
    SetGoFacing(o);
    SetUInt32Value(GAMEOBJECT_FACTION, m_goInfo->faction);
    SetAllGoFlags(m_goInfo->flags);
    SetEntry(m_goInfo->id);
    SetDisplayId(m_goInfo->displayId);
    SetGoState(go_state);
    SetGoType(GameobjectTypes(m_goInfo->type));

    SetGoAnimProgress(animprogress);

    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_TRAP:
        case GAMEOBJECT_TYPE_FISHINGNODE:
            m_lootState = GO_NOT_READY;                     // Initialize Traps and Fishingnode delayed in ::Update
            break;
        case GAMEOBJECT_TYPE_CHEST:
            RollIfMineralVein();
            break;
        default:
            break;
    }


    // Notify the battleground or outdoor pvp script
    if (map->IsBattleGround())
    {
        static_cast<BattleGroundMap*>(map)->GetBG()->HandleGameObjectCreate(this);
    }
    else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z())))
    {
        outdoorPvP->HandleGameObjectCreate(this);
    }

    // Notify the map's instance data.
    // Only works if you create the object in it, not if it is moves to that map.
    // Normally non-players do not teleport to other maps.
    if (InstanceData* iData = map->GetInstanceData())
    {
        iData->OnObjectCreate(this);
    }

    return true;
}


/**
 * @brief Refreshes the game object spawn state on the map.
 */
void GameObject::Refresh()
{
    // not refresh despawned not casted GO (despawned casted GO destroyed in all cases anyway)
    if (m_spawn.Moment() > 0 && m_spawn.IsPermanent())
    {
        return;
    }

    if (isSpawned())
    {
        GetMap()->Add(this);
    }
}

/**
 * @brief Despawns or schedules removal of the game object.
 */
void GameObject::Delete()
{
    SendDespawnAnimation(*this);

    SetGoState(GO_STATE_READY);
    SetAllGoFlags(GetGOInfo()->flags);

    if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(GetGUIDLow()))
    {
        sPoolMgr.UpdatePool<GameObject>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow());
    }
    else
    {
        AddObjectToRemoveList();
    }
}

/**
 * @brief Saves the loaded game object back to the database.
 */
void GameObject::SaveToDB()
{
    // this should only be used when the gameobject has already been loaded
    // preferably after adding to map, because mapid may not be valid otherwise
    GameObjectData const* data = sObjectMgr.GetGOData(GetGUIDLow());
    if (!data)
    {
        sLog.outError("GameObject::SaveToDB failed, can not get gameobject data!");
        return;
    }

    SaveToDB(GetMapId());
}

/**
 * @brief Saves the game object spawn data to the database for a map.
 *
 * @param mapid The map id to persist.
 */
void GameObject::SaveToDB(uint32 mapid)
{
    const GameObjectInfo* goI = GetGOInfo();

    if (!goI)
    {
        return;
    }

    // update in loaded data (changing data only in this place)
    GameObjectData& data = sObjectMgr.NewGOData(GetGUIDLow());

    // data->guid = guid don't must be update at save
    data.id = GetEntry();
    data.mapid = mapid;
    data.posX = GetGoPositionX();
    data.posY = GetGoPositionY();
    data.posZ = GetGoPositionZ();
    data.orientation = GetFloatValue(GAMEOBJECT_FACING);
    data.rotation0 = GetFloatValue(GAMEOBJECT_ROTATION + 0);
    data.rotation1 = GetFloatValue(GAMEOBJECT_ROTATION + 1);
    data.rotation2 = GetFloatValue(GAMEOBJECT_ROTATION + 2);
    data.rotation3 = GetFloatValue(GAMEOBJECT_ROTATION + 3);
    data.spawntimesecs = m_spawn.AsSpawnTimeSecs();
    data.animprogress = GetGoAnimProgress();
    data.go_state = GetGoState();

    // updated in DB
    std::ostringstream ss;
    ss << "INSERT INTO `gameobject` VALUES ( "
       << GetGUIDLow() << ", "
       << GetEntry() << ", "
       << mapid << ", "
       << GetGoPositionX() << ", "
       << GetGoPositionY() << ", "
       << GetGoPositionZ() << ", "
       << GetFloatValue(GAMEOBJECT_FACING) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 1) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 2) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 3) << ", "
       << m_spawn.AsSpawnTimeSecs() << ", "
       << uint32(GetGoAnimProgress()) << ", "
       << uint32(GetGoState()) << ")";

    WorldDatabase.BeginTransaction();
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("%s", ss.str().c_str());
    WorldDatabase.CommitTransaction();
}

/**
 * @brief Loads a game object from static database spawn data.
 *
 * @param guid The database GUID.
 * @param map The destination map.
 * @return true if loading succeeded; otherwise, false.
 */
bool GameObject::LoadFromDB(uint32 guid, Map* map)
{
    GameObjectData const* data = sObjectMgr.GetGOData(guid);

    if (!data)
    {
        sLog.outErrorDb("Gameobject (GUID: %u) not found in table `gameobject`, can't load. ", guid);
        return false;
    }

    uint32 entry = data->id;
    // uint32 map_id = data->mapid;                         // already used before call
    float x = data->posX;
    float y = data->posY;
    float z = data->posZ;
    float ang = data->orientation;

    float rotation0 = data->rotation0;
    float rotation1 = data->rotation1;
    float rotation2 = data->rotation2;
    float rotation3 = data->rotation3;

    uint32 animprogress = data->animprogress;
    GOState go_state = data->go_state;

    if (!Create(guid, entry, map, x, y, z, ang, rotation0, rotation1, rotation2, rotation3, animprogress, go_state))
    {
        return false;
    }

    if (!GetGOInfo()->GetDespawnPossibility() && !GetGOInfo()->IsDespawnAtAction() && data->spawntimesecs >= 0)
    {
        SetGoFlag(GO_FLAG_NODESPAWN);
        m_spawn.Never();
    }
    else
    {
        m_spawn.FromSpawnTimeSecs(data->spawntimesecs);

        if (m_spawn.IsPermanent())
        {
            m_spawn.ChangesAt(map->GetPersistentState()->GetGORespawnTime(GetGUIDLow()));

            // ready to respawn
            if (m_spawn.Moment() && m_spawn.Moment() <= time(nullptr))
            {
                m_spawn.ChangesAt(0);
                map->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), 0);
            }
        }
    }

    AIM_Initialize();

    return true;
}

struct GameObjectRespawnDeleteWorker
{
    explicit GameObjectRespawnDeleteWorker(uint32 guid) : i_guid(guid) {}

    void operator()(MapPersistentState* state)
    {
        state->SaveGORespawnTime(i_guid, 0);
    }

    uint32 i_guid;
};

/**
 * @brief Deletes the static database spawn record for this game object.
 */
void GameObject::DeleteFromDB()
{
    if (!HasStaticDBSpawnData())
    {
        DEBUG_LOG("Trying to delete not saved gameobject!");
        return;
    }

    GameObjectRespawnDeleteWorker worker(GetGUIDLow());
    sMapPersistentStateMgr.DoForAllStatesWithMapId(GetMapId(), worker);

    sObjectMgr.DeleteGOData(GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `game_event_gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject_battleground` WHERE `guid` = '%u'", GetGUIDLow());
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

/**
 * @brief Checks whether the game object starts the specified quest.
 *
 * @param quest_id The quest identifier.
 * @return true if the quest is related to this game object; otherwise, false.
 */
bool GameObject::OffersQuest(uint32 quest_id) const
{
    return NamesQuest(sObjectMgr.GetGOQuestRelationsMapBounds(GetEntry()), quest_id);
}

/**
 * @brief Checks whether the game object is involved in the specified quest.
 *
 * @param quest_id The quest identifier.
 * @return true if the quest is an involved relation for this game object; otherwise, false.
 */
bool GameObject::TakesQuest(uint32 quest_id) const
{
    return NamesQuest(sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(GetEntry()), quest_id);
}

/**
 * @brief Checks whether the game object behaves as a transport.
 *
 * @return true if the game object is a transport type; otherwise, false.
 */
uint32 GameObject::LiftPhase() const
{
    uint32 const period = IsLift() ? LiftPath::Of(GetEntry()).Period() : 0;

    // Wall clock, not uptime: a lift keyed off the time since boot would start its
    // loop from the beginning at every restart.
    return period != 0 ? uint32(GameTime::GetAbsoluteTimeMS() % period) : 0;
}

bool GameObject::IsMovingPlatform() const
{
    // The client draws these two from its own animation data, so where the server thinks
    // they are is not where the player sees them. They must never be culled by distance
    // and must never be told they went out of range.
    GameObjectInfo const* gInfo = GetGOInfo();
    if (!gInfo)
    {
        return false;
    }
    return gInfo->type == GAMEOBJECT_TYPE_TRANSPORT || gInfo->type == GAMEOBJECT_TYPE_MO_TRANSPORT;
}

/**
 * @brief Gets the unit that owns this game object.
 *
 * @return The owning unit, or null if none exists.
 */
Unit* GameObject::GetOwner() const
{
    return ObjectLookup::GetUnit(*this, GetOwnerGuid());
}

/**
 * @brief Saves the current respawn time to persistent state if needed.
 */
void GameObject::SaveRespawnTime()
{
    if (m_spawn.Moment() > time(nullptr) && m_spawn.IsPermanent())
    {
        GetMap()->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), m_spawn.Moment());
    }
}

/**
 * @brief Checks whether the game object is visible for a player in the current state.
 *
 * @param u The observing player.
 * @param viewPoint The viewpoint used for distance checks.
 * @param inVisibleList true when evaluating an already-visible object.
 * @return true if the object should be visible; otherwise, false.
 */
bool GameObject::IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const
{
    // Not in world
    if (!IsInWorld() || !u->IsInWorld())
    {
        return false;
    }

    // a platform the client is animating stays visible however far the server puts it
    if (IsMovingPlatform() && CanBeSeen(*this, *u))
    {
        return true;
    }

    float visibleDistance = GetMap()->GetVisibilityDistance() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f);

    // A game master sees what is there, at the map's own range and whatever state it
    // is in. Everything below is what the world hides from everyone else.
    if (!u->isGameMaster())
    {
        if (!isSpawned())
        {
            return false;
        }

        if (GetGOInfo()->IsServerOnly())
        {
            return false;
        }

        if (IsTrapHidingFrom(u))
        {
            visibleDistance = TrapNoticedWithin(WatchedBy(u), float(GetGOInfo()->trap.radius));
            if (visibleDistance < 0.0f)
            {
                return false;
            }
        }
    }

    return SeenWithin(*this, *viewPoint, visibleDistance, false);
}

/**
 * @brief Whether this is a trap that is trying not to be noticed by this player.
 *
 * Hiding is not something a trap does at large: a trap laid by his own side is not
 * hiding from him, and one whose data says nothing about stealth is not hiding from
 * anybody.
 */
bool GameObject::IsTrapHidingFrom(Player const* watcher) const
{
    if (GetGoType() != GAMEOBJECT_TYPE_TRAP)
    {
        return false;
    }

    if (!GetGOInfo()->trap.stealthed && !GetGOInfo()->trap.stealthAffected)
    {
        return false;
    }

    Unit* owner = GetOwner();

    // Laid by nobody, or laid against him.
    return !owner || IsHostile(*watcher, *owner);
}

/**
 * @brief What this player brings to noticing a hidden trap.
 */
TrapWatcher GameObject::WatchedBy(Player const* watcher) const
{
    TrapWatcher brought;
    brought.isRogue = watcher->getClass() == CLASS_ROGUE;
    brought.invisibilityDetection =
        watcher->GetMaxPositiveAuraModifierByMiscValue(SPELL_AURA_MOD_INVISIBILITY_DETECTION, 8);
    brought.stealthDetect = watcher->GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_DETECT);

    if (Unit* owner = GetOwner())
    {
        brought.hasOwner = true;
        brought.ownerLevel = owner->getLevel();
        brought.levelGap = int32(watcher->GetLevelForTarget(owner)) -
                           int32(owner->GetLevelForTarget(watcher));
    }

    return brought;
}

/**
 * @brief Forces a respawn for a default-spawned game object.
 */
void GameObject::Respawn()
{
    if (m_spawn.IsPermanent() && m_spawn.Moment() > 0)
    {
        m_spawn.ChangesAt(time(nullptr));
        GetMap()->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), 0);
    }
}

/**
 * @brief Forget everyone who has used it.
 *
 * What that means depends on what the kind was keeping: a count of uses, a list
 * of who has been taught, or nothing at all.
 */
void GameObject::ClearAllUsesData()
{
    if (auto* chest = Behaves<ChestBehaviour>())
    {
        chest->Lock().ForgetLearners();
    }

    if (auto* counted = Behaves<CountingBehaviour>())
    {
        counted->Tally().Forget();
    }
}

/**
 * @brief Fixes the template to this object, and with it what kind of thing it is.
 *
 * The kind is a column of the template, so this is the one moment it is read: the
 * behaviour it names is made here and answers for the object from then on.
 */
void GameObject::SetGOInfo(GameObjectInfo const* pg)
{
    m_goInfo = pg;
    m_behaviour = BehaviourOf(*this);
}

/**
 * @brief Whether a questgiver still has business with this player.
 *
 * Either it holds a quest the player could pick up now, or the player is
 * carrying one it takes back and has not been paid for.
 */
bool GameObject::HasQuestBusinessWith(Player* seeker) const
{
    auto const onOffer = sObjectMgr.GetGOQuestRelationsMapBounds(GetEntry());
    for (auto itr = onOffer.first; itr != onOffer.second; ++itr)
    {
        if (seeker->CanTakeQuest(sObjectMgr.GetQuestTemplate(itr->second), false))
        {
            return true;
        }
    }

    auto const toHandIn = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(GetEntry());
    for (auto itr = toHandIn.first; itr != toHandIn.second; ++itr)
    {
        if (IsHandInPending(seeker->GetQuestStatus(itr->second), seeker->GetQuestRewardStatus(itr->second)))
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Whether what this chest holds includes a quest item this player wants.
 */
bool GameObject::HoldsQuestLootFor(Player* seeker) const
{
    if (!LootTemplates_Gameobject.HaveQuestLootForPlayer(GetGOInfo()->GetLootId(), seeker))
    {
        return false;
    }

    // A battleground may hold its own objects back from one side: an Alterac
    // Valley mine counts only for the team that holds it.
    if (BattleGround* bg = seeker->GetBattleGround())
    {
        return bg->AllowsQuestObject(GetEntry(), seeker->GetTeam());
    }

    return true;
}

/**
 * @brief Checks whether this game object should activate for a player's quests.
 *
 * @param seeker The player looking at the object.
 * @return true if the object should be quest-active; otherwise, false.
 */
bool GameObject::ActivateToQuest(Player* seeker) const
{
    // An objective in its own right: the player was told to go and click this.
    if (seeker->HasQuestForGO(GetEntry()))
    {
        return true;
    }

    // The rest reads a quest the template names, and the world data lists an
    // entry here only when it has one. An unlisted entry has nothing to light up
    // for.
    if (!sObjectMgr.IsGameObjectForQuests(GetEntry()))
    {
        return false;
    }

    if (GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
    {
        return HasQuestBusinessWith(seeker);
    }

    if (seeker->GetQuestStatus(GetGOInfo()->GetQuestId()) == QUEST_STATUS_INCOMPLETE)
    {
        return true;
    }

    return GetGoType() == GAMEOBJECT_TYPE_CHEST && HoldsQuestLootFor(seeker);
}

/**
 * @brief Summons the linked trap associated with this game object, if any.
 */
void GameObject::SummonLinkedTrapIfAny()
{
    uint32 linkedEntry = GetGOInfo()->GetLinkedGameObjectEntry();
    if (!linkedEntry)
    {
        return;
    }

    GameObject* linkedGO = new GameObject;
    if (!linkedGO->Create(GetMap()->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), linkedEntry, GetMap(),
        Where().X(), Where().Y(), Where().Z(), Where().Facing(), 0.0f, 0.0f, 0.0f, 0.0f, GO_ANIMPROGRESS_DEFAULT, GO_STATE_READY))
    {
        delete linkedGO;
        return;
    }

    linkedGO->SetRespawnTime(GetRespawnDelay());
    linkedGO->SetSpellId(GetSpellId());

    if (GetOwnerGuid())
    {
        linkedGO->SetOwnerGuid(GetOwnerGuid());
        linkedGO->SetUInt32Value(GAMEOBJECT_LEVEL, GetUInt32Value(GAMEOBJECT_LEVEL));
    }

    linkedGO->AIM_Initialize();
    GetMap()->Add(linkedGO);
}

/**
 * @brief Triggers the linked trap game object against a target.
 *
 * @param target The unit activating the trap.
 */
void GameObject::TriggerLinkedGameObject(Unit* target)
{
    uint32 trapEntry = GetGOInfo()->GetLinkedGameObjectEntry();

    if (!trapEntry)
    {
        return;
    }

    GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(trapEntry);
    if (!trapInfo || trapInfo->type != GAMEOBJECT_TYPE_TRAP)
    {
        return;
    }

    SpellEntry const* trapSpell = sSpellStore.LookupEntry(trapInfo->trap.spellId);

    // The range to search for linked trap is weird. We set 0.5 as default. Most (all?)
    // traps are probably expected to be pretty much at the same location as the used GO,
    // so it appears that using range from spell is obsolete.
    float range = 0.5f;

    if (trapSpell)                                          // checked at load already
    {
        range = GetSpellMaxRange(sSpellRangeStore.LookupEntry(trapSpell->RangeIndex));
    }

    // search nearest linked GO
    GameObject* trapGO = nullptr;

    {
        // search closest with base of used GO, using max range of trap spell as search radius (why? See above)
        MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*this, trapEntry, range);
        MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(trapGO, go_check);

        Cell::VisitGridObjects(this, checker, range);
    }

    // found correct GO
    if (trapGO)
    {
        trapGO->Use(target);
    }
}

/**
 * @brief Finds a nearby fishing hole around this game object.
 *
 * @param range The search radius.
 * @return The nearest fishing hole, or null if none was found.
 */
GameObject* GameObject::LookupFishingHoleAround(float range)
{
    GameObject* ok = nullptr;

    MaNGOS::NearestGameObjectFishingHoleCheck u_check(*this, range);
    MaNGOS::GameObjectSearcher<MaNGOS::NearestGameObjectFishingHoleCheck> checker(ok, u_check);
    Cell::VisitGridObjects(this, checker, range);

    return ok;
}

/**
 * @brief Checks whether collision is currently enabled for the game object.
 *
 * @return true if the model should be collidable; otherwise, false.
 */
bool GameObject::IsCollisionEnabled() const
{
    if (!isSpawned())
    {
        return false;
    }

    // TODO: Possible that this function must consider multiple checks
    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:
            return GetGoState() != GO_STATE_ACTIVE && GetGoState() != GO_STATE_ACTIVE_ALTERNATIVE;

        default:
            return true;
    }
}

/**
 * @brief Resets a door or button back to its default state.
 */
void GameObject::ResetDoorOrButton()
{
    if (m_lootState == GO_READY || m_lootState == GO_JUST_DEACTIVATED)
    {
        return;
    }

    SwitchDoorOrButton(false);
    SetLootState(GO_JUST_DEACTIVATED);
    m_closesAt = 0;
}

/**
 * @brief Activates a door or button and schedules restoration.
 *
 * @param time_to_restore The delay before reset.
 * @param alternative true to use the alternative active state.
 */
void GameObject::UseDoorOrButton(uint32 time_to_restore, bool alternative /* = false */)
{
    if (m_lootState != GO_READY)
    {
        return;
    }

    if (!time_to_restore)
    {
        time_to_restore = GetGOInfo()->GetAutoCloseTime();
    }

    SwitchDoorOrButton(true, alternative);
    SetLootState(GO_ACTIVATED);

    // a door with nothing to close it stays as the last one through it left it
    m_closesAt = time_to_restore ? time(nullptr) + time_to_restore : 0;
}

/**
 * @brief Switches a door or button between active and ready states.
 *
 * @param activate true to activate; false to deactivate.
 * @param alternative true to use the alternative active state.
 */
void GameObject::SwitchDoorOrButton(bool activate, bool alternative /* = false */)
{
    if (activate)
    {
        SetGoFlag(GO_FLAG_IN_USE);
    }
    else
    {
        RemoveGoFlag(GO_FLAG_IN_USE);
    }

    if (GetGoState() == GO_STATE_READY)                     // if closed -> open
    {
        SetGoState(alternative ? GO_STATE_ACTIVE_ALTERNATIVE : GO_STATE_ACTIVE);
    }
    else                                                    // if open -> close
    {
        SetGoState(GO_STATE_READY);
    }
}


// overwrite Occupant function for proper name localization

/**
 * @brief Gets the localized name for a locale index.
 *
 * @param loc_idx The locale index.
 * @return The localized name, or the default name if unavailable.
 */
const char* GameObject::GetNameForLocaleIdx(int32 loc_idx) const
{
    if (loc_idx >= 0)
    {
        GameObjectLocale const* cl = sObjectMgr.GetGameObjectLocale(GetEntry());
        if (cl)
        {
            if (cl->Name.size() > (size_t)loc_idx && !cl->Name[loc_idx].empty())
            {
                return cl->Name[loc_idx].c_str();
            }
        }
    }

    return GetName();
}

/**
 * @brief Stores the object's rotation quaternion and updates the model.
 *
 * @param q The quaternion to apply.
 */
void GameObject::SetQuaternion(Geometry::Quat const& q)
{
    SetFloatValue(GAMEOBJECT_ROTATION + 0, q.x);
    SetFloatValue(GAMEOBJECT_ROTATION + 1, q.y);
    SetFloatValue(GAMEOBJECT_ROTATION + 2, q.z);
    SetFloatValue(GAMEOBJECT_ROTATION + 3, q.w);

    // The pose is the object's to set and the index's only job is to re-file the
    // body under whatever tiles its new world box covers.
    if (m_model && FindMap())
    {
        GetMap()->RefreshGameObjectModel(*m_model);
    }
}

/**
 * @brief Reads the object's current rotation quaternion.
 *
 * @param q Receives the quaternion components.
 */
void GameObject::GetQuaternion(Geometry::Quat& q) const
{
    q.x = GetFloatValue(GAMEOBJECT_ROTATION + 0);
    q.y = GetFloatValue(GAMEOBJECT_ROTATION + 1);
    q.z = GetFloatValue(GAMEOBJECT_ROTATION + 2);
    q.w = GetFloatValue(GAMEOBJECT_ROTATION + 3);
}

/**
 * @brief Rolls alternate mineral vein variants for chest-type nodes.
 */
void GameObject::RollIfMineralVein()
{
    // What makes a chest a vein is that it gives up a random number of ores rather than
    // one lot of loot, and that is in the template it was placed with.
    GameObjectInfo const* placed = GetGOInfo();
    if (!placed || placed->chest.minSuccessOpens == 0
        || placed->chest.maxSuccessOpens <= placed->chest.minSuccessOpens)
    {
        return;
    }

    uint32 const zone = GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z());

    uint32 const came = sMineralVeins.SpawnedAs(
        GetEntry(), zone,
        urand(0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_DARKIRON),
        urand(0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_LOWER),
        urand(0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE));

    if (came == GetEntry())
    {
        return;
    }

    GameObjectInfo const* instead = ObjectMgr::GetGameObjectInfo(came);
    if (!instead)
    {
        return;
    }

    m_goInfo = instead;
    SetUInt32Value(GAMEOBJECT_DISPLAYID, instead->displayId);
    Object::_ReCreate(came);
}

/**
 * @brief Sets the loot state and refreshes collision state.
 *
 * @param state The new loot state.
 */
void GameObject::SetLootState(LootState state)
{
    m_lootState = state;
    UpdateCollisionState();
}

/**
 * @brief Sets the gameobject state and refreshes collision state.
 *
 * @param state The new gameobject state.
 */
void GameObject::SetGoState(GOState state)
{
    SetUInt32Value(GAMEOBJECT_STATE, state);
    UpdateCollisionState();
}

/**
 * @brief Sets the display id and refreshes the collision model.
 *
 * @param modelId The display model id.
 */
void GameObject::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(GAMEOBJECT_DISPLAYID, modelId);
    UpdateModel();
}

/**
 * @brief Updates model collision enablement based on current state.
 */
void GameObject::UpdateCollisionState() const
{
    if (!m_model || !IsInWorld())
    {
        return;
    }

    m_model->SetCollidable(IsCollisionEnabled());
}

/**
 * @brief Rebuilds the collision model for the current display.
 */
void GameObject::UpdateModel()
{
    if (m_model && IsInWorld() && GetMap()->ContainsGameObjectModel(*m_model))
    {
        GetMap()->RemoveGameObjectModel(*m_model);
    }
    delete m_model;

    m_model = GameObjectModel::Create(this);
    if (m_model)
    {
        GetMap()->InsertGameObjectModel(*m_model);
    }
}

/**
 * @brief Gets the object bounding radius used for visibility and interaction.
 *
 * @return The default game object radius.
 */
float GameObject::ComputeBoundingRadius() const
{
    // 1.12.1 GameObjectDisplayInfo.dbc not have any info related to size
    return DEFAULT_WORLD_OBJECT_SIZE;
}

struct AddGameObjectToRemoveListInMapsWorker
{
    AddGameObjectToRemoveListInMapsWorker(ObjectGuid guid) : i_guid(guid) {}

    void operator()(Map* map)
    {
        if (GameObject* pGameobject = map->GetGameObject(i_guid))
        {
            pGameobject->AddObjectToRemoveList();
        }
    }

    ObjectGuid i_guid;
};

/**
 * @brief Adds matching spawned instances to remove lists across loaded maps.
 *
 * @param db_guid The database GUID.
 * @param data The static spawn data.
 */
void GameObject::AddToRemoveListInMaps(uint32 db_guid, GameObjectData const* data)
{
    AddGameObjectToRemoveListInMapsWorker worker(ObjectGuid(HIGHGUID_GAMEOBJECT, data->id, db_guid));
    sMapMgr.DoForAllMapsWithMapId(data->mapid, worker);
}

struct SpawnGameObjectInMapsWorker
{
    SpawnGameObjectInMapsWorker(uint32 guid, GameObjectData const* data)
        : i_guid(guid), i_data(data) {}

    void operator()(Map* map)
    {
        // Spawn if necessary (loaded grids only)
        if (map->IsCellLoaded(i_data->posX, i_data->posY))
        {
            GameObject* pGameobject = new GameObject;
            // DEBUG_LOG("Spawning gameobject %u", *itr);
            if (!pGameobject->LoadFromDB(i_guid, map))
            {
                delete pGameobject;
            }
            else
            {
                if (pGameobject->isSpawnedByDefault())
                {
                    map->Add(pGameobject);
                }
            }
        }
    }

    uint32 i_guid;
    GameObjectData const* i_data;
};

/**
 * @brief Spawns this database game object across eligible loaded maps.
 *
 * @param db_guid The database GUID.
 * @param data The static spawn data.
 */
void GameObject::SpawnInMaps(uint32 db_guid, GameObjectData const* data)
{
    SpawnGameObjectInMapsWorker worker(db_guid, data);
    sMapMgr.DoForAllMapsWithMapId(data->mapid, worker);
}

/**
 * @brief Checks whether this object has static database spawn data.
 *
 * @return true if the object has a saved DB spawn; otherwise, false.
 */
bool GameObject::HasStaticDBSpawnData() const
{
    return sObjectMgr.GetGOData(GetGUIDLow()) != nullptr;
}



/**
 * @brief Gets the bound script id for this game object.
 *
 * @return The script identifier.
 */
uint32 GameObject::GetScriptId()
{
    return sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, -int32(GetGUIDLow())) ? sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, -int32(GetGUIDLow())) : sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, GetEntry());
}

/**
 * @brief Gets the interaction distance for this game object type.
 *
 * @return The maximum interaction distance.
 */
float GameObject::GetInteractionDistance() const
{
    float maxdist = INTERACTION_DISTANCE;
    switch (GetGoType())
    {
        // TODO: find out how the client calculates the maximal usage distance to spellless working
        // gameobjects like mailboxes - 10.0 is a just an abitrary chosen number
        case GAMEOBJECT_TYPE_MAILBOX:
            maxdist = 10.0f;
            break;
        case GAMEOBJECT_TYPE_FISHINGHOLE:
        case GAMEOBJECT_TYPE_FISHINGNODE:
            maxdist = 20.0f + CONTACT_DISTANCE;     // max spell range
            break;
        default:
            break;
    }
    return maxdist;
}

/**
 * @brief Sends a custom animation packet for this game object.
 *
 * @param animId The animation identifier.
 */
void GameObject::SendGameObjectCustomAnim(uint32 animId /*= 0*/)
{
    WorldPacket data(SMSG_GAMEOBJECT_CUSTOM_ANIM, 8 + 4);
    data << GetObjectGuid();
    data << uint32(animId);
    Broadcast(*this, &data, true);
}

/**
 * @brief Sends a reset-state packet for this game object.
 */
void GameObject::SendGameObjectReset()
{
    WorldPacket data(SMSG_GAMEOBJECT_RESET_STATE, 8);
    data << GetObjectGuid();
    Broadcast(*this, &data, true);
}

/**
 * @brief Initializes the scripted AI instance for the game object.
 *
 * @return true if initialization succeeded; otherwise, false.
 */
bool  GameObject::AIM_Initialize()
{

    // make sure nothing can change the AI during AI update
    if (m_AI_locked)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "AIM_Initialize: failed to init, locked.");
        return false;
    }

    m_AI.reset(sScriptMgr.GetGameObjectAI(this));

    return true;
}
