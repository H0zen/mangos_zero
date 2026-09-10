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

#include <string>
#include "Chat.h"
#include "SpawnRecord.h"
#include "Language.h"
#include "World.h"
#include "Mint.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "TargetedMovementGenerator.h"
#include "TemporarySummon.h"
#include "WaypointManager.h"
#include "PathFinder.h"
#include "Totem.h"
#include "ObjectLookup.h"

#ifdef _DEBUG_VMAPS
#endif

bool ChatHandler::HandleComeToMeCommand(char* )
{
    Creature* caster = getSelectedCreature();

    if (!caster)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* pl = m_session->GetPlayer();

    caster->GetMotionMaster()->MovePoint(0, pl->Where().X(), pl->Where().Y(), pl->Where().Z());
    return true;
}

bool ChatHandler::HandleRespawnCommand(char* )
{
    Player* pl = m_session->GetPlayer();

    Unit* target = getSelectedUnit();
    if (pl->GetSelectionGuid() && target)
    {
        if (!IsCreature(target))
        {
            SendSysMessage(LANG_SELECT_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }

        if (target->IsDead())
        {
            ((Creature*)target)->Respawn();
        }
        return true;
    }

    MaNGOS::RespawnDo u_do;
    MaNGOS::OccupantWorker<MaNGOS::RespawnDo> worker(u_do);
    Cell::VisitGridObjects(pl, worker, pl->GetMap()->GetVisibilityDistance());
    return true;
}

bool ChatHandler::HandleModifyFactionCommand(char* args)
{
    Creature* chr = getSelectedCreature();
    if (!chr)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!*args)
    {
        if (chr)
        {
            uint32 factionid = chr->getFaction();
            uint32 flag = chr->GetUInt32Value(UNIT_FIELD_FLAGS);
            uint32 npcflag = chr->GetUInt32Value(UNIT_NPC_FLAGS);
            uint32 dyflag = chr->GetUInt32Value(UNIT_DYNAMIC_FLAGS);
            PSendSysMessage(LANG_CURRENT_FACTION, chr->GetGUIDLow(), factionid, flag, npcflag, dyflag);
        }
        return true;
    }

    if (!chr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 factionid;
    if (!ExtractUint32KeyFromLink(&args, "Hfaction", factionid))
    {
        return false;
    }

    if (!sFactionTemplateStore.LookupEntry(factionid))
    {
        PSendSysMessage(LANG_WRONG_FACTION, factionid);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 flag;
    if (!ExtractOptUInt32(&args, flag, chr->GetUInt32Value(UNIT_FIELD_FLAGS)))
    {
        return false;
    }

    uint32 npcflag;
    if (!ExtractOptUInt32(&args, npcflag, chr->GetUInt32Value(UNIT_NPC_FLAGS)))
    {
        return false;
    }

    uint32  dyflag;
    if (!ExtractOptUInt32(&args, dyflag, chr->GetUInt32Value(UNIT_DYNAMIC_FLAGS)))
    {
        return false;
    }

    PSendSysMessage(LANG_YOU_CHANGE_FACTION, chr->GetGUIDLow(), factionid, flag, npcflag, dyflag);

    chr->setFaction(factionid);
    chr->SetUInt32Value(UNIT_FIELD_FLAGS, flag);
    chr->SetUInt32Value(UNIT_NPC_FLAGS, npcflag);
    chr->SetUInt32Value(UNIT_DYNAMIC_FLAGS, dyflag);

    return true;
}

bool ChatHandler::HandleNpcAddCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature_entry", id))
    {
        return false;
    }

    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, id);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = m_session->GetPlayer();
    CreatureCreatePos pos(chr, chr->Where().Facing());
    Map* map = chr->GetMap();

    Creature* pCreature = new Creature;

    uint32 lowguid = sMint.StaticCreatureGuid();
    if (!lowguid)
    {
        SendSysMessage(LANG_NO_FREE_STATIC_GUID_FOR_SPAWN);
        SetSentErrorMessage(true);
        return false;
    }

    if (!pCreature->Create(lowguid, pos, cinfo))
    {
        delete pCreature;
        return false;
    }

    npcs::SaveOn(*pCreature, map->GetId());

    uint32 db_guid = pCreature->GetGUIDLow();

    pCreature->LoadFromDB(db_guid, map);

    return true;
}

bool ChatHandler::HandleNpcAddVendorItemCommand(char* args)
{
    uint32 itemId;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", itemId))
    {
        SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 maxcount;
    if (!ExtractOptUInt32(&args, maxcount, 0))
    {
        return false;
    }

    uint32 incrtime;
    if (!ExtractOptUInt32(&args, incrtime, 0))
    {
        return false;
    }

    Creature* vendor = getSelectedCreature();

    uint32 vendor_entry = vendor ? vendor->GetEntry() : 0;

    if (!sObjectMgr.IsVendorItemValid(false, "npc_vendor", vendor_entry, itemId, maxcount, incrtime, 0, m_session->GetPlayer()))
    {
        SetSentErrorMessage(true);
        return false;
    }

    sObjectMgr.AddVendorItem(vendor_entry, itemId, maxcount, incrtime);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);

    PSendSysMessage(LANG_ITEM_ADDED_TO_LIST, itemId, pProto->Name1, maxcount, incrtime);
    return true;
}

bool ChatHandler::HandleNpcDelVendorItemCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Creature* vendor = getSelectedCreature();
    if (!vendor || !vendor->IsVendor())
    {
        SendSysMessage(LANG_COMMAND_VENDORSELECTION);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 itemId;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", itemId))
    {
        SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sObjectMgr.RemoveVendorItem(vendor->GetEntry(), itemId))
    {
        PSendSysMessage(LANG_ITEM_NOT_IN_LIST, itemId);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);

    PSendSysMessage(LANG_ITEM_DELETED_FROM_LIST, itemId, pProto->Name1);
    return true;
}

bool ChatHandler::HandleNpcAIInfoCommand(char* )
{
    Creature* pTarget = getSelectedCreature();

    if (!pTarget)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_NPC_AI_HEADER, pTarget->GetEntry());

    std::string strScript = pTarget->GetScriptName();
    std::string strAI = pTarget->GetAIName();
    CreatureAI* pAI = pTarget->AI();
    char const* cstrAIClass = pAI ? typeid(*pAI).name() : " - ";

    PSendSysMessage(LANG_NPC_AI_NAMES,
        strAI.empty() ? " - " : strAI.c_str(),
        cstrAIClass ? cstrAIClass : " - ",
        strScript.empty() ? " - " : strScript.c_str());
    PSendSysMessage("Motion Type: %u", pTarget->GetMotionMaster()->GetCurrentMovementGeneratorType());
    PSendSysMessage("Casting Spell: %s", pTarget->IsNonMeleeSpellCasted(true) ? "yes" : "no");

    if (pTarget->AI())
    {
        pTarget->AI()->GetAIInformation(*this);
    }

    return true;
}

bool ChatHandler::HandleNpcChangeLevelCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint8 lvl = (uint8)atoi(args);
    if (lvl < 1 || lvl > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) + 3)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (pCreature->IsPet())
    {
        ((Pet*)pCreature)->GivePetLevel(lvl);
    }
    else
    {
        pCreature->SetMaxHealth(100 + 30 * lvl);
        pCreature->SetHealth(100 + 30 * lvl);
        pCreature->SetLevel(lvl);

        if (npcs::Listed(*pCreature))
        {
            npcs::Save(*pCreature);
        }
    }

    return true;
}

bool ChatHandler::HandleNpcFlagCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 npcFlags = (uint32)atoi(args);

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->SetUInt32Value(UNIT_NPC_FLAGS, npcFlags);

    WorldDatabase.PExecuteLog("UPDATE `creature_template` SET `NpcFlags` = '%u' WHERE `entry` = '%u'", npcFlags, pCreature->GetEntry());

    SendSysMessage(LANG_VALUE_SAVED_REJOIN);

    return true;
}

bool ChatHandler::HandleNpcDeleteCommand(char* args)
{
    Creature* unit = nullptr;

    if (*args)
    {

        uint32 lowguid;
        if (!ExtractUint32KeyFromLink(&args, "Hcreature", lowguid))
        {
            return false;
        }

        if (!lowguid)
        {
            return false;
        }

        if (CreatureData const* data = sObjectMgr.GetCreatureData(lowguid))
        {
            unit = m_session->GetPlayer()->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
        }
    }
    else
    {
        unit = getSelectedCreature();
    }

    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    switch (unit->GetSubtype())
    {
        case CREATURE_SUBTYPE_GENERIC:
        {
            unit->CombatStop();
            if (CreatureData const* data = sObjectMgr.GetCreatureData(unit->GetGUIDLow()))
            {
                npcs::RemoveFromMaps(unit->GetGUIDLow(), data);
                npcs::Forget(unit->GetGUIDLow(), data);
            }
            else
            {
                unit->AddObjectToRemoveList();
            }
            break;
        }
        case CREATURE_SUBTYPE_PET:
            ((Pet*)unit)->Unsummon(PET_SAVE_AS_CURRENT);
            break;
        case CREATURE_SUBTYPE_TOTEM:
            ((Totem*)unit)->UnSummon();
            break;
        case CREATURE_SUBTYPE_TEMPORARY_SUMMON:
            ((TemporarySummon*)unit)->UnSummon();
            break;
        default:
            return false;
    }

    SendSysMessage(LANG_COMMAND_DELCREATMESSAGE);

    return true;
}

bool ChatHandler::HandleNpcMoveCommand(char* args)
{
    uint32 lowguid = 0;
    Player* player = m_session->GetPlayer();

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {

        if (!ExtractUint32KeyFromLink(&args, "Hcreature", lowguid))
        {
            return false;
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        if (player->GetMapId() != data->mapid)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        pCreature = player->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
    }
    else
    {
        lowguid = pCreature->GetGUIDLow();
    }

    float x = player->Where().X();
    float y = player->Where().Y();
    float z = player->Where().Z();
    float o = player->Where().Facing();

    if (pCreature)
    {
        if (CreatureData const* data = sObjectMgr.GetCreatureData(pCreature->GetGUIDLow()))
        {
            const_cast<CreatureData*>(data)->posX = x;
            const_cast<CreatureData*>(data)->posY = y;
            const_cast<CreatureData*>(data)->posZ = z;
            const_cast<CreatureData*>(data)->orientation = o;
        }
        pCreature->GetMap()->CreatureRelocation(pCreature, x, y, z, o);
        pCreature->GetMotionMaster()->Initialize();
        if (pCreature->IsAlive())
        {
            pCreature->SetDeathState(JUST_DIED);
            pCreature->Respawn();
        }
    }

    WorldDatabase.PExecuteLog("UPDATE `creature` SET `position_x` = '%f', `position_y` = '%f', `position_z` = '%f', `orientation` = '%f' WHERE `guid` = '%u'", x, y, z, o, lowguid);
    PSendSysMessage(LANG_COMMAND_CREATUREMOVED);
    return true;
}

bool ChatHandler::HandleNpcSetMoveTypeCommand(char* args)
{

    uint32 lowguid;
    Creature* pCreature;
    if (!ExtractUInt32(&args, lowguid))
    {
        pCreature = getSelectedCreature();
        if (!pCreature || !npcs::Listed(*pCreature))
        {
            return false;
        }
        lowguid = pCreature->GetGUIDLow();
    }
    else
    {
        CreatureData const* data = sObjectMgr.GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Player* player = m_session->GetPlayer();

        if (player->GetMapId() != data->mapid)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        pCreature = player->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
    }

    MovementGeneratorType move_type;
    char* type_str = ExtractLiteralArg(&args);
    if (!type_str)
    {
        return false;
    }

    if (strncmp(type_str, "stay", strlen(type_str)) == 0)
    {
        move_type = IDLE_MOTION_TYPE;
    }
    else if (strncmp(type_str, "random", strlen(type_str)) == 0)
    {
        move_type = RANDOM_MOTION_TYPE;
    }
    else if (strncmp(type_str, "way", strlen(type_str)) == 0)
    {
        move_type = WAYPOINT_MOTION_TYPE;
    }
    else
    {
        return false;
    }

    bool doNotDelete = ExtractLiteralArg(&args, "NODEL") != nullptr;
    if (!doNotDelete && *args)
    {
        return false;
    }

    if (!doNotDelete)
    {
        sWaypointMgr.DeletePath(lowguid);
    }

    if (pCreature)
    {
        pCreature->SetDefaultMovementType(move_type);
        pCreature->GetMotionMaster()->Initialize();
        if (pCreature->IsAlive())
        {
            pCreature->SetDeathState(JUST_DIED);
            pCreature->Respawn();
        }
        npcs::Save(*pCreature);
    }

    if (doNotDelete)
    {
        PSendSysMessage(LANG_MOVE_TYPE_SET_NODEL, type_str);
    }
    else
    {
        PSendSysMessage(LANG_MOVE_TYPE_SET, type_str);
    }

    return true;
}

bool ChatHandler::HandleNpcSetModelCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 displayId = (uint32)atoi(args);

    Creature* pCreature = getSelectedCreature();

    if (!pCreature || pCreature->IsPet())
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sCreatureDisplayInfoStore.LookupEntry(displayId))
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->SetDisplayId(displayId);
    pCreature->SetNativeDisplayId(displayId);

    if (npcs::Listed(*pCreature))
    {
        npcs::Save(*pCreature);
    }

    return true;
}

bool ChatHandler::HandleNpcFactionIdCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 factionId = (uint32)atoi(args);

    if (!sFactionTemplateStore.LookupEntry(factionId))
    {
        PSendSysMessage(LANG_WRONG_FACTION, factionId);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->setFaction(factionId);

    if (CreatureInfo const* cinfo = pCreature->GetCreatureInfo())
    {
        const_cast<CreatureInfo*>(cinfo)->FactionAlliance = factionId;
        const_cast<CreatureInfo*>(cinfo)->FactionHorde = factionId;
    }

    WorldDatabase.PExecuteLog("UPDATE `creature_template` SET `FactionAlliance` = '%u', `FactionHorde` = '%u' WHERE `entry` = '%u'", factionId, factionId, pCreature->GetEntry());

    return true;
}

bool ChatHandler::HandleNpcSpawnDistCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    float option = (float)atof(args);
    if (option < 0.0f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return false;
    }

    MovementGeneratorType mtype = IDLE_MOTION_TYPE;
    if (option > 0.0f)
    {
        mtype = RANDOM_MOTION_TYPE;
    }

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        return false;
    }

    pCreature->SetRespawnRadius((float)option);
    pCreature->SetDefaultMovementType(mtype);
    pCreature->GetMotionMaster()->Initialize();
    if (pCreature->IsAlive())
    {
        pCreature->SetDeathState(JUST_DIED);
        pCreature->Respawn();
    }

    WorldDatabase.PExecuteLog("UPDATE `creature` SET `spawndist`=%f, `MovementType`=%i WHERE `guid`=%u", option, mtype, pCreature->GetGUIDLow());
    PSendSysMessage(LANG_COMMAND_SPAWNDIST, option);
    return true;
}

bool ChatHandler::HandleNpcSpawnTimeCommand(char* args)
{
    uint32 stime;
    if (!ExtractUInt32(&args, stime))
    {
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 u_guidlow = pCreature->GetGUIDLow();

    WorldDatabase.PExecuteLog("UPDATE `creature` SET `spawntimesecs`=%i WHERE `guid`=%u", stime, u_guidlow);
    pCreature->SetRespawnDelay(stime);
    PSendSysMessage(LANG_COMMAND_SPAWNTIME, stime);

    return true;
}

bool ChatHandler::HandleNpcFollowCommand(char* )
{
    Player* player = m_session->GetPlayer();
    Creature* creature = getSelectedCreature();

    if (!creature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    creature->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    PSendSysMessage(LANG_CREATURE_FOLLOW_YOU_NOW, creature->GetName());
    return true;
}

bool ChatHandler::HandleNpcUnFollowCommand(char* )
{
    Player* player = m_session->GetPlayer();
    Creature* creature = getSelectedCreature();

    if (!creature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    MotionMaster* creatureMotion = creature->GetMotionMaster();
    if (creatureMotion->empty() ||
        creatureMotion->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    {
        PSendSysMessage(LANG_CREATURE_NOT_FOLLOW_YOU, creature->GetName());
        SetSentErrorMessage(true);
        return false;
    }

    FollowMovementGenerator const* mgen = static_cast<FollowMovementGenerator const*>(creatureMotion->top());

    if (mgen->GetTarget() != player)
    {
        PSendSysMessage(LANG_CREATURE_NOT_FOLLOW_YOU, creature->GetName());
        SetSentErrorMessage(true);
        return false;
    }

    creatureMotion->MovementExpired(true);

    PSendSysMessage(LANG_CREATURE_NOT_FOLLOW_YOU_NOW, creature->GetName());
    return true;
}

bool ChatHandler::HandleNpcTameCommand(char* )
{
    Creature* creatureTarget = getSelectedCreature();

    if (!creatureTarget || creatureTarget->IsPet())
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();

    if (player->GetPetGuid())
    {
        SendSysMessage(LANG_YOU_ALREADY_HAVE_PET);
        SetSentErrorMessage(true);
        return false;
    }

    player->CastSpell(creatureTarget, 13481, true);
    return true;
}

bool ChatHandler::HandleNpcSetDeathStateCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    if (!pCreature || !npcs::Listed(*pCreature))
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        pCreature->SetDeadByDefault(true);
    }
    else
    {
        pCreature->SetDeadByDefault(false);
    }

    npcs::Save(*pCreature);
    pCreature->Respawn();

    return true;
}

bool ChatHandler::HandleNpcAllowMovementCommand(char* )
{
    if (sWorld.getAllowMovement())
    {
        sWorld.SetAllowMovement(false);
        SendSysMessage(LANG_CREATURE_MOVE_DISABLED);
    }
    else
    {
        sWorld.SetAllowMovement(true);
        SendSysMessage(LANG_CREATURE_MOVE_ENABLED);
    }
    return true;
}

bool ChatHandler::HandleNpcChangeEntryCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 newEntryNum = atoi(args);
    if (!newEntryNum)
    {
        return false;
    }

    Unit* unit = getSelectedUnit();
    if (!unit || !IsCreature(unit))
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }
    Creature* creature = (Creature*)unit;
    if (creature->UpdateEntry(newEntryNum))
    {
        SendSysMessage(LANG_DONE);
    }
    else
    {
        SendSysMessage(LANG_ERROR);
    }
    return true;
}

namespace
{

    void PrintNpcWatchUnitDetails(ChatHandler& handler, char const* label,
                                  Creature const* watched, Unit const* unit)
    {
        float x = unit->Where().X();
        float y = unit->Where().Y();
        float z = unit->Where().Z();
        float o = unit->Where().Facing();

        GridPair gridPair = MaNGOS::ComputeGridPair(x, y);
        CellPair cellPair = MaNGOS::ComputeCellPair(x, y);
        bool inWorld = unit->IsInWorld();
        bool gridLoaded = inWorld && unit->GetMap()->IsLoaded(x, y);
        bool cellLoaded = inWorld && unit->GetMap()->IsCellLoaded(x, y);

        handler.PSendSysMessage("  %s=%s", label, unit->GetGuidStr().c_str());

        if (IsCreature(unit))
        {
            handler.PSendSysMessage("    creature entry=%u name=\"%s\"",
                                    unit->GetEntry(), unit->GetName());
        }

        handler.PSendSysMessage("    in-world=%s active-object=%s",
                                inWorld ? "yes" : "no",
                                unit->IsActiveObject() ? "yes" : "no");
        handler.PSendSysMessage("    map=%u instance=%u",
                                unit->GetMapId(), unit->GetInstanceId());
        handler.PSendSysMessage("    pos x=%.3f y=%.3f z=%.3f o=%.3f",
                                x, y, z, o);
        handler.PSendSysMessage("    grid[%u,%u] cell[%u,%u] grid-loaded=%s cell-loaded=%s%s",
                                gridPair.x_coord, gridPair.y_coord,
                                cellPair.x_coord, cellPair.y_coord,
                                gridLoaded ? "yes" : "no",
                                cellLoaded ? "yes" : "no",
                                inWorld ? "" : " (not in world)");

        if (CanBeSeen(*unit, *watched))
        {
            handler.PSendSysMessage("    distance=%.3f",
                                    watched->Where().DistanceTo(unit->Where()));
        }
        else
        {
            handler.SendSysMessage("    distance=unavailable (different map or not in world)");
        }
    }

    Unit* GetNpcWatchMapStoreTarget(Creature const* watched,
                                    ObjectGuid const& guid)
    {
        if (!(GuidHigh(guid) == HIGHGUID_UNIT || GuidHigh(guid) == HIGHGUID_PET))
        {
            return nullptr;
        }

        return watched->GetMap()->GetAnyTypeCreature(guid);
    }

    void PrintNpcWatchCreatureDetails(ChatHandler& handler, Creature* target)
    {
        float x = target->Where().X();
        float y = target->Where().Y();
        float z = target->Where().Z();
        float o = target->Where().Facing();

        GridPair gridPair = MaNGOS::ComputeGridPair(x, y);
        CellPair cellPair = MaNGOS::ComputeCellPair(x, y);
        bool gridLoaded = target->GetMap()->IsLoaded(x, y);
        bool cellLoaded = target->GetMap()->IsCellLoaded(x, y);

        handler.PSendSysMessage("[LivingWorld] watch %s \"%s\"",
                                target->GetGuidStr().c_str(), target->GetName());
        handler.PSendSysMessage("  map=%u instance=%u", target->GetMapId(), target->GetInstanceId());
        handler.PSendSysMessage("  pos x=%.3f y=%.3f z=%.3f o=%.3f", x, y, z, o);
        handler.PSendSysMessage("  grid[%u,%u] cell[%u,%u] grid-loaded=%s cell-loaded=%s",
                                gridPair.x_coord, gridPair.y_coord, cellPair.x_coord, cellPair.y_coord,
                                gridLoaded ? "yes" : "no",
                                cellLoaded ? "yes" : "no");
        handler.PSendSysMessage("  in-world=%s active-object=%s",
                                target->IsInWorld() ? "yes" : "no",
                                target->IsActiveObject() ? "yes" : "no");
        handler.PSendSysMessage("  movement-generator-type=%u in-combat=%s combat-timer=%u",
                                uint32(target->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                                target->IsInCombat() ? "yes" : "no",
                                target->GetCombatTimer());

        Unit* victim = target->getVictim();
        if (victim)
        {
            PrintNpcWatchUnitDetails(handler, "victim", target, victim);
        }
        else
        {
            handler.SendSysMessage("  victim=none");
        }

        ObjectGuid const& watchTargetGuid = target->GetTargetGuid();
        if (!(watchTargetGuid == 0))
        {
            if (victim && watchTargetGuid == victim->GetObjectGuid())
            {
                handler.PSendSysMessage("  target=%s (same as victim; details above)",
                                        GuidString(watchTargetGuid).c_str());
            }
            else if (Unit* watchTarget =
                         GetNpcWatchMapStoreTarget(target, watchTargetGuid))
            {
                PrintNpcWatchUnitDetails(handler, "target", target, watchTarget);
            }
            else
            {
                handler.PSendSysMessage("  target=%s", GuidString(watchTargetGuid).c_str());
                handler.SendSysMessage("    target-details=details unavailable from safe current-map context");
            }
        }
        else
        {
            handler.SendSysMessage("  target=none");
        }
    }
}

bool ChatHandler::HandleNpcWatchCommand(char* args)
{
    if (char* watchArg = ExtractLiteralArg(&args))
    {
        if (strcmp(watchArg, "last") == 0)
        {
            if (args && *args)
            {
                SendSysMessage("Usage: .npc watch last");
                SetSentErrorMessage(true);
                return false;
            }

            ObjectGuid const& lastGuid = m_session->GetNpcWatchLastGuid();
            if ((lastGuid == 0))
            {
                SendSysMessage("[LivingWorld] watch last: no last watched creature for this session. Select a creature and run .npc watch first.");
                return true;
            }

            Creature* target = m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(lastGuid);
            if (!target)
            {
                PSendSysMessage("[LivingWorld] watch last %s: not resident in safe current-map context",
                                GuidString(lastGuid).c_str());
                SendSysMessage("  details unavailable from safe current-map context");
                return true;
            }

            PrintNpcWatchCreatureDetails(*this, target);
            return true;
        }

        SendSysMessage("Usage: .npc watch [last]");
        SetSentErrorMessage(true);
        return false;
    }

    Creature* target = getSelectedCreature();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    m_session->SetNpcWatchLastGuid(target->GetObjectGuid());
    PrintNpcWatchCreatureDetails(*this, target);
    return true;
}

bool ChatHandler::HandleNpcInfoCommand(char* )
{
    Creature* target = getSelectedCreature();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 faction = target->getFaction();
    uint32 npcflags = target->GetUInt32Value(UNIT_NPC_FLAGS);
    uint32 displayid = target->GetDisplayId();
    uint32 nativeid = target->GetNativeDisplayId();
    uint32 Entry = target->GetEntry();
    CreatureInfo const* cInfo = target->GetCreatureInfo();

    time_t curRespawnDelay = target->GetRespawnTimeEx() - time(nullptr);
    if (curRespawnDelay < 0)
    {
        curRespawnDelay = 0;
    }
    std::string curRespawnDelayStr = secsToTimeString(curRespawnDelay, TimeFormat::ShortText);
    std::string defRespawnDelayStr = secsToTimeString(target->GetRespawnDelay(), TimeFormat::ShortText);

    PSendSysMessage(LANG_NPCINFO_CHAR, target->GetGuidStr().c_str(), faction, npcflags, Entry, displayid, nativeid);
    PSendSysMessage(LANG_NPCINFO_LEVEL, target->getLevel());
    PSendSysMessage(LANG_NPCINFO_HEALTH, target->GetCreateHealth(), target->GetMaxHealth(), target->GetHealth());
    PSendSysMessage(LANG_NPCINFO_FLAGS, target->GetUInt32Value(UNIT_FIELD_FLAGS), target->GetUInt32Value(UNIT_DYNAMIC_FLAGS), target->getFaction());
    PSendSysMessage(LANG_COMMAND_RAWPAWNTIMES, defRespawnDelayStr.c_str(), curRespawnDelayStr.c_str());
    PSendSysMessage(LANG_NPCINFO_LOOT, cInfo->LootId, cInfo->PickpocketLootId, cInfo->SkinningLootId);
    PSendSysMessage(LANG_NPCINFO_DUNGEON_ID, target->GetInstanceId());
    PSendSysMessage(LANG_NPCINFO_POSITION, float(target->Where().X()), float(target->Where().Y()), float(target->Where().Z()));

    if ((npcflags & UNIT_NPC_FLAG_VENDOR))
    {
        SendSysMessage(LANG_NPCINFO_VENDOR);
    }
    if ((npcflags & UNIT_NPC_FLAG_TRAINER))
    {
        SendSysMessage(LANG_NPCINFO_TRAINER);
    }

    ShowNpcOrGoSpawnInformation<Creature>(target->GetGUIDLow());
    return true;
}

bool ChatHandler::HandleNpcPlayEmoteCommand(char* args)
{
    uint32 emote = atoi(args);

    Creature* target = getSelectedCreature();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    target->HandleEmote(emote);

    return true;
}
