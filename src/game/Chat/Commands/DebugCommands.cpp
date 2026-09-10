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

#include <cstdlib>
#include "Utterance.h"
#include "Platform/Define.h"
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include "DBCStores.h"
#include "WorldPacket.h"
#include "Player.h"
#include "OpcodeTable.h"
#include "Chat.h"
#include "Synthetic/SyntheticCrowd.h"
#include "Log.h"
#include "Unit.h"
#include "GossipDef.h"
#include "Language.h"
#include "BattleGround/BattleGroundMgr.h"
#include <fstream>
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "Pet.h"
#include "Map.h"
#include "Fleet.h"
#include "TransportMap.h"
#include "Transports.h"
#include "VesselRoute.h"
#include "CellImpl.h"
#include "Cast/Recipe/RecipeBook.h"

bool ChatHandler::HandleDebugSendSpellFailCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 failnum;
    if (!ExtractUInt32(&args, failnum) || failnum > 255)
    {
        return false;
    }

    uint32 failarg1;
    if (!ExtractOptUInt32(&args, failarg1, 0))
    {
        return false;
    }

    uint32 failarg2;
    if (!ExtractOptUInt32(&args, failarg2, 0))
    {
        return false;
    }

    WorldPacket data(SMSG_CAST_FAILED, 4 + 1 + 1);
    data << uint32(133);
    data << uint8(2);
    data << uint8(failnum);
    if (failarg1 || failarg2)
    {
        data << uint32(failarg1);
    }
    if (failarg2)
    {
        data << uint32(failarg2);
    }

    m_session->SendPacket(&data);

    return true;
}

bool ChatHandler::HandleDebugSendPoiCommand(char* args)
{
    Player* pPlayer = m_session->GetPlayer();
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    uint32 icon;
    if (!ExtractUInt32(&args, icon))
    {
        return false;
    }

    uint32 flags;
    if (!ExtractUInt32(&args, flags))
    {
        return false;
    }

    DETAIL_LOG("Command : POI, NPC = %u, icon = %u flags = %u", target->GetGUIDLow(), icon, flags);
    pPlayer->PlayerTalkClass->SendPointOfInterest(target->Where().X(), target->Where().Y(), Poi_Icon(icon), flags, 30, "Test POI");
    return true;
}

bool ChatHandler::HandleDebugSendEquipErrorCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint8 msg = atoi(args);
    m_session->GetPlayer()->SendEquipError(InventoryResult(msg), nullptr, nullptr);
    return true;
}

bool ChatHandler::HandleDebugSendSellErrorCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint8 msg = atoi(args);
    m_session->GetPlayer()->SendSellError(SellResult(msg), 0, 0, 0);
    return true;
}

bool ChatHandler::HandleDebugSendBuyErrorCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint8 msg = atoi(args);
    m_session->GetPlayer()->SendBuyError(BuyResult(msg), 0, 0, 0);
    return true;
}

bool ChatHandler::HandleDebugRecvOpcodeCommand(char* )
{
    Unit* unit = getSelectedUnit();
    if (!unit || (!IsPlayer(unit)))
    {
        unit = m_session->GetPlayer();
    }

    std::ifstream stream("ropcode.txt");
    if (!stream.is_open())
    {
        return false;
    }

    uint32 opcode = 0;
    if (!(stream >> opcode))
    {
        stream.close();
        return false;
    }

    WorldPacket *data = new WorldPacket(opcode, 10);

    std::string type;
    while (stream >> type)
    {
        if (type.empty())
        {
            break;
        }

        if (type == "uint8")
        {
            uint16 value;
            stream >> value;
            *data << uint8(value);
        }
        else if (type == "uint16")
        {
            uint16 value;
            stream >> value;
            *data << value;
        }
        else if (type == "uint32")
        {
            uint32 value;
            stream >> value;
            *data << value;
        }
        else if (type == "uint64")
        {
            uint64 value;
            stream >> value;
            *data << value;
        }
        else if (type == "float")
        {
            float value;
            stream >> value;
            *data << value;
        }
        else if (type == "string")
        {
            std::string value;
            stream >> value;
            *data << value;
        }
        else if (type == "pguid")
        {
            *data << unit->GetPackGUID();
        }
        else if (type == "guid")
        {
            *data << unit->GetObjectGuid();
        }
        else if (type == "mypguid")
        {
            *data << m_session->GetPlayer()->GetPackGUID();
        }
        else if (type == "myguid")
        {
            *data << m_session->GetPlayer()->GetObjectGuid();
        }
        else if (type == "name")
        {
            *data << unit->GetName();
        }
        else if (type == "myname")
        {
            *data << m_session->GetPlayerName();
        }
        else
        {
            DEBUG_LOG("Sending opcode: unknown type '%s'", type.c_str());
            break;
        }
    }
    stream.close();

    DEBUG_LOG("Queued opcode %u, %s", data->GetOpcode(), LookupOpcodeName(data->GetOpcode()));

    m_session->QueuePacket(data);

    PSendSysMessage(LANG_COMMAND_OPCODEGOT, data->GetOpcode(), unit->GetName());
    return true;
}

bool ChatHandler::HandleDebugSendOpcodeCommand(char* )
{
    Unit* unit = getSelectedUnit();
    if (!unit || (!IsPlayer(unit)))
    {
        unit = m_session->GetPlayer();
    }

    std::ifstream stream("opcode.txt");
    if (!stream.is_open())
    {
        return false;
    }

    uint32 opcode = 0;
    if (!(stream >> opcode))
    {
        stream.close();
        return false;
    }

    WorldPacket data(opcode, 0);

    std::string type;
    while (stream >> type)
    {
        if (type.empty())
        {
            break;
        }

        if (type == "uint8")
        {
            uint16 value;
            stream >> value;
            data << uint8(value);
        }
        else if (type == "uint16")
        {
            uint16 value;
            stream >> value;
            data << value;
        }
        else if (type == "uint32")
        {
            uint32 value;
            stream >> value;
            data << value;
        }
        else if (type == "uint64")
        {
            uint64 value;
            stream >> value;
            data << value;
        }
        else if (type == "float")
        {
            float value;
            stream >> value;
            data << value;
        }
        else if (type == "string")
        {
            std::string value;
            stream >> value;
            data << value;
        }
        else if (type == "pguid")
        {
            data << unit->GetPackGUID();
        }
        else if (type == "guid")
        {
            data << unit->GetObjectGuid();
        }
        else if (type == "mypguid")
        {
            data << m_session->GetPlayer()->GetPackGUID();
        }
        else if (type == "myguid")
        {
            data << m_session->GetPlayer()->GetObjectGuid();
        }
        else if (type == "name")
        {
            data << unit->GetName();
        }
        else if (type == "myname")
        {
            data << m_session->GetPlayerName();
        }
        else
        {
            DEBUG_LOG("Sending opcode: unknown type '%s'", type.c_str());
            break;
        }
    }
    stream.close();

    DEBUG_LOG("Sending opcode %u, %s", data.GetOpcode(), LookupOpcodeName(data.GetOpcode()));

    data.hexlike();
    static_cast<Player*>(unit)->SendDirectMessage(&data);

    PSendSysMessage(LANG_COMMAND_OPCODESENT, data.GetOpcode(), unit->GetName());

    return true;
}

bool ChatHandler::HandleDebugUpdateWorldStateCommand(char* args)
{
    uint32 world;
    if (!ExtractUInt32(&args, world))
    {
        return false;
    }

    uint32 state;
    if (!ExtractUInt32(&args, state))
    {
        return false;
    }

    m_session->GetPlayer()->SendUpdateWorldState(world, state);
    return true;
}

bool ChatHandler::HandleDebugPlayCinematicCommand(char* args)
{

    uint32 dwId;
    if (!ExtractUInt32(&args, dwId))
    {
        return false;
    }

    if (!sCinematicSequencesStore.LookupEntry(dwId))
    {
        PSendSysMessage(LANG_CINEMATIC_NOT_EXIST, dwId);
        SetSentErrorMessage(true);
        return false;
    }

    m_session->GetPlayer()->SendCinematicStart(dwId);
    return true;
}

bool ChatHandler::HandleDebugPlayMovieCommand(char* args)
{
    return true;
}

bool ChatHandler::HandleDebugPlaySoundCommand(char* args)
{

    uint32 dwSoundId;
    if (!ExtractUInt32(&args, dwSoundId))
    {
        return false;
    }

    if (!sSoundEntriesStore.LookupEntry(dwSoundId))
    {
        PSendSysMessage(LANG_SOUND_NOT_EXIST, dwSoundId);
        SetSentErrorMessage(true);
        return false;
    }

    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (m_session->GetPlayer()->GetSelectionGuid())
    {
        PlaySound(*unit, SoundKind::AtObject, dwSoundId, m_session->GetPlayer());
    }
    else
    {
        PlaySound(*unit, SoundKind::Flat, dwSoundId, m_session->GetPlayer());
    }

    PSendSysMessage(LANG_YOU_HEAR_SOUND, dwSoundId);
    return true;
}

bool ChatHandler::HandleDebugSendChannelNotifyCommand(char* args)
{
    const char* name = "test";

    uint32 code;
    if (!ExtractUInt32(&args, code) || code > 255)
    {
        return false;
    }

    WorldPacket data(SMSG_CHANNEL_NOTIFY, (1 + 10));
    data << uint8(code);
    data << name;
    data << uint32(0);
    data << uint32(0);
    m_session->SendPacket(&data);
    return true;
}

bool ChatHandler::HandleDebugSendChatMsgCommand(char* args)
{
    const char* msg = args;

    uint32 type;
    if (!ExtractUInt32(&args, type) || type > 255)
    {
        return false;
    }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, ChatMsg(type), msg, LANG_UNIVERSAL, CHAT_TAG_NONE, m_session->GetPlayer()->GetObjectGuid(), m_session->GetPlayerName());
    m_session->SendPacket(&data);
    return true;
}

bool ChatHandler::HandleDebugSendQuestPartyMsgCommand(char* args)
{
    uint32 msg;
    if (!ExtractUInt32(&args, msg))
    {
        return false;
    }
    if (msg > 0xFF)
    {
        return false;
    }

    m_session->GetPlayer()->SendPushToPartyResponse(m_session->GetPlayer(), uint8(msg));
    return true;
}

bool ChatHandler::HandleDebugGetLootRecipientCommand(char* )
{
    Creature* target = getSelectedCreature();
    if (!target)
    {
        return false;
    }

    if (!target->Claim().IsClaimed())
    {
        SendSysMessage("loot recipient: no loot recipient");
    }
    else if (Player* recipient = target->Claim().Entitled())
    {
        PSendSysMessage("loot recipient: %s with raw data %s from group %u",
            recipient->GetGuidStr().c_str(),
            GuidString(target->Claim().TakerGuid()).c_str(),
            target->Claim().GroupId());
    }
    else
    {
        SendSysMessage("loot recipient: offline ");
    }

    return true;
}

bool ChatHandler::HandleDebugSendQuestInvalidMsgCommand(char* args)
{
    uint32 msg = std::strtoul(args, nullptr, 10);
    m_session->GetPlayer()->SendCanTakeQuestResponse(msg);
    return true;
}

bool ChatHandler::HandleDebugGetItemStateCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    ItemUpdateState state = ITEM_UNCHANGED;
    bool list_queue = false, check_all = false;

    std::string state_str;

    if (strncmp(args, "unchanged", strlen(args)) == 0)
    {
        state = ITEM_UNCHANGED;
        state_str = "unchanged";
    }
    else if (strncmp(args, "changed", strlen(args)) == 0)
    {
        state = ITEM_CHANGED;
        state_str = "changed";
    }
    else if (strncmp(args, "new", strlen(args)) == 0)
    {
        state = ITEM_NEW;
        state_str = "new";
    }
    else if (strncmp(args, "removed", strlen(args)) == 0)
    {
        state = ITEM_REMOVED;
        state_str = "removed";
    }
    else if (strncmp(args, "queue", strlen(args)) == 0)
    {
        list_queue = true;
    }
    else if (strncmp(args, "all", strlen(args)) == 0)
    {
        check_all = true;
    }
    else
    {
        return false;
    }

    Player* player = getSelectedPlayer();
    if (!player)
    {
        player = m_session->GetPlayer();
    }

    if (!list_queue && !check_all)
    {
        state_str = "The player has the following " + state_str + " items: ";
        SendSysMessage(state_str.c_str());
        for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
        {
            if (i >= BUYBACK_SLOT_START && i < BUYBACK_SLOT_END)
            {
                continue;
            }

            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!item)
            {
                continue;
            }
            if (!item->IsBag())
            {
                if (item->GetState() == state)
                {
                    PSendSysMessage("%s bag: 255 slot: %u owner: %s",
                        item->GetGuidStr().c_str(),  item->GetSlot(), GuidString(item->GetOwnerGuid()).c_str());
                }
            }
            else
            {
                Bag* bag = (Bag*)item;
                for (uint8 j = 0; j < bag->GetBagSize(); ++j)
                {
                    Item* item2 = bag->GetItemByPos(j);
                    if (item2 && item2->GetState() == state)
                    {
                        PSendSysMessage("%s bag: %u slot: %u owner: %s",
                            item2->GetGuidStr().c_str(), item2->GetBagSlot(), item2->GetSlot(),
                            GuidString(item2->GetOwnerGuid()).c_str());
                    }
                }
            }
        }
    }

    if (list_queue)
    {
        ItemSaveQueue const& saves = player->ItemSaves();
        for (Item* item : saves.Waiting())
        {
            if (!item)
            {
                continue;
            }

            Bag* container = item->GetContainer();
            uint8 bag_slot = container ? container->GetSlot() : uint8(INVENTORY_SLOT_BAG_0);

            std::string st;
            switch (item->GetState())
            {
                case ITEM_UNCHANGED: st = "unchanged"; break;
                case ITEM_CHANGED: st = "changed"; break;
                case ITEM_NEW: st = "new"; break;
                case ITEM_REMOVED: st = "removed"; break;
            }

            PSendSysMessage("%s bag: %u slot: %u - state: %s",
                item->GetGuidStr().c_str(), bag_slot, item->GetSlot(), st.c_str());
        }
        if (saves.IsEmpty())
        {
            PSendSysMessage("nothing is waiting to be saved");
        }
    }

    if (check_all)
    {
        bool error = false;
        ItemSaveQueue const& saves = player->ItemSaves();
        for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
        {
            if (i >= BUYBACK_SLOT_START && i < BUYBACK_SLOT_END)
            {
                continue;
            }

            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!item)
            {
                continue;
            }

            if (item->GetSlot() != i)
            {
                PSendSysMessage("%s at slot %u has an incorrect slot value: %d",
                    item->GetGuidStr().c_str(), i, item->GetSlot());
                error = true; continue;
            }

            if (item->GetOwnerGuid() != player->GetObjectGuid())
            {
                PSendSysMessage("%s at slot %u owner (%s) and inventory owner (%s) don't match!",
                    item->GetGuidStr().c_str(), item->GetSlot(),
                    GuidString(item->GetOwnerGuid()).c_str(), player->GetGuidStr().c_str());
                error = true; continue;
            }

            if (Bag* container = item->GetContainer())
            {
                PSendSysMessage("%s at slot %u has a container %s from slot %u but shouldnt!",
                    item->GetGuidStr().c_str(), item->GetSlot(),
                    container->GetGuidStr().c_str(), container->GetSlot());
                error = true; continue;
            }

            if (!saves.Holds(item) && item->GetState() != ITEM_UNCHANGED)
            {
                PSendSysMessage("%s at slot %u is not waiting to be saved but should be (state: %d)!",
                    item->GetGuidStr().c_str(), item->GetSlot(), item->GetState());
                error = true; continue;
            }

            if (item->IsBag())
            {
                Bag* bag = (Bag*)item;
                for (uint8 j = 0; j < bag->GetBagSize(); ++j)
                {
                    Item* item2 = bag->GetItemByPos(j);
                    if (!item2)
                    {
                        continue;
                    }

                    if (item2->GetSlot() != j)
                    {
                        PSendSysMessage("%s in bag %u at slot %u has an incorrect slot value: %u",
                            item2->GetGuidStr().c_str(), bag->GetSlot(), j, item2->GetSlot());
                        error = true; continue;
                    }

                    if (item2->GetOwnerGuid() != player->GetObjectGuid())
                    {
                        PSendSysMessage("%s in bag %u at slot %u owner (%s) and inventory owner (%s) don't match!",
                            item2->GetGuidStr().c_str(), bag->GetSlot(), item2->GetSlot(),
                            GuidString(item2->GetOwnerGuid()).c_str(), player->GetGuidStr().c_str());
                        error = true; continue;
                    }

                    Bag* container = item2->GetContainer();
                    if (!container)
                    {
                        PSendSysMessage("%s in bag %u at slot %u has no container!",
                            item2->GetGuidStr().c_str(), bag->GetSlot(), item2->GetSlot());
                        error = true; continue;
                    }

                    if (container != bag)
                    {
                        PSendSysMessage("%s in bag %u at slot %u has a different container %s from slot %u!",
                            item2->GetGuidStr().c_str(), bag->GetSlot(), item2->GetSlot(),
                            container->GetGuidStr().c_str(), container->GetSlot());
                        error = true; continue;
                    }

                    if (!saves.Holds(item2) && item2->GetState() != ITEM_UNCHANGED)
                    {
                        PSendSysMessage("%s in bag %u at slot %u is not waiting to be saved but should be (state: %d)!",
                            item2->GetGuidStr().c_str(), bag->GetSlot(), item2->GetSlot(), item2->GetState());
                        error = true; continue;
                    }
                }
            }
        }

        size_t i = 0;
        for (Item* item : saves.Waiting())
        {
            ++i;
            if (!item)
            {
                continue;
            }

            if (item->GetOwnerGuid() != player->GetObjectGuid())
            {
                PSendSysMessage("queue(%zu): %s has the owner (%s) and inventory owner (%s) don't match!",
                    i, item->GetGuidStr().c_str(),
                    GuidString(item->GetOwnerGuid()).c_str(), player->GetGuidStr().c_str());
                error = true; continue;
            }

            if (item->GetState() == ITEM_REMOVED)
            {
                continue;
            }
            Item* test = player->GetItemByPos(item->GetBagSlot(), item->GetSlot());

            if (test == nullptr)
            {
                PSendSysMessage("queue(%zu): %s has incorrect (bag %u slot %u) values, the player doesn't have an item at that position!",
                    i, item->GetGuidStr().c_str(), item->GetBagSlot(), item->GetSlot());
                error = true; continue;
            }

            if (test != item)
            {
                PSendSysMessage("queue(%zu): %s has incorrect (bag %u slot %u) values, the %s is there instead!",
                    i, item->GetGuidStr().c_str(), item->GetBagSlot(), item->GetSlot(),
                    test->GetGuidStr().c_str());
                error = true; continue;
            }
        }
        if (!error)
        {
            SendSysMessage("All OK!");
        }
    }

    return true;
}

bool ChatHandler::HandleDebugBattlegroundCommand(char* )
{
    sBattleGroundMgr.ToggleTesting();
    return true;
}

bool ChatHandler::HandleDebugSpellCheckCommand(char* )
{
    sLog.outString("Check expected in code spell properties base at table 'spell_check' content...");
    sSpellMgr.CheckUsedSpells("spell_check");
    return true;
}

bool ChatHandler::HandleDebugCrowdSpawnCommand(char* args)
{
    Player* me = m_session ? m_session->GetPlayer() : nullptr;
    if (!me)
    {
        SendSysMessage("This one has to be run from in the world.");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count = 499;
    float radius = 40.f;

    if (char* countStr = ExtractLiteralArg(&args))
    {
        count = uint32(atoi(countStr));
    }
    if (char* radiusStr = ExtractLiteralArg(&args))
    {
        radius = float(atof(radiusStr));
    }

    if (count == 0 || count > 2000)
    {
        SendSysMessage("Ask for between one and two thousand.");
        SetSentErrorMessage(true);
        return false;
    }

    std::string error;
    const uint32 placed = synthetic::SyntheticCrowd::Instance().Spawn(
        count, me->GetMapId(), me->Where().X(), me->Where().Y(), me->Where().Z(),
        radius, error);

    if (placed == 0)
    {
        PSendSysMessage("No crowd: %s", error.empty() ? "unknown reason" : error.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage("Synthetic crowd: %u placed within %.0f yards. Watch the METRICS line.",
                    placed, radius);
    if (!error.empty())
    {
        PSendSysMessage("Stopped early: %s", error.c_str());
    }
    return true;
}

bool ChatHandler::HandleDebugCrowdDespawnCommand(char* )
{
    const uint32 released = synthetic::SyntheticCrowd::Instance().Despawn();
    PSendSysMessage("Synthetic crowd: %u released.", released);
    return true;
}

bool ChatHandler::HandleDebugAnimCommand(char* args)
{
    uint32 emote_id;
    if (!ExtractUInt32(&args, emote_id))
    {
        return false;
    }

    m_session->GetPlayer()->HandleEmoteCommand(emote_id);
    return true;
}

bool ChatHandler::HandleDebugSetAuraStateCommand(char* args)
{
    int32 state;
    if (!ExtractInt32(&args, state))
    {
        return false;
    }

    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!state)
    {

        for (int i = 1; i <= 32; ++i)
        {
            unit->ModifyAuraState(AuraState(i), false);
        }
        return true;
    }

    unit->ModifyAuraState(AuraState(abs(state)), state > 0);
    return true;
}

bool ChatHandler::HandleSetValueHelper(Object* target, uint32 field, char* typeStr, char* valStr)
{
    ObjectGuid guid = target->GetObjectGuid();

    if (field >= target->GetValuesCount() || field <= OBJECT_FIELD_ENTRY)
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, field, GuidString(guid).c_str(), target->GetValuesCount());
        return false;
    }

    uint32 base;
    if (!typeStr)
    {
        base = 10;
    }
    else if (strncmp(typeStr, "int", strlen(typeStr)) == 0)
    {
        base = 10;
    }
    else if (strncmp(typeStr, "hex", strlen(typeStr)) == 0)
    {
        base = 16;
    }
    else if (strncmp(typeStr, "bit", strlen(typeStr)) == 0)
    {
        base = 2;
    }
    else if (strncmp(typeStr, "float", strlen(typeStr)) == 0)
    {
        base = 0;
    }
    else
    {
        return false;
    }

    if (base)
    {
        uint32 iValue;
        if (!ExtractUInt32Base(&valStr, iValue, base))
        {
            return false;
        }

        DEBUG_LOG(GetMangosString(LANG_SET_UINT), GuidString(guid).c_str(), field, iValue);
        target->SetUInt32Value(field , iValue);
        PSendSysMessage(LANG_SET_UINT_FIELD, GuidString(guid).c_str(), field, iValue);
    }
    else
    {
        float fValue;
        if (!ExtractFloat(&valStr, fValue))
        {
            return false;
        }

        DEBUG_LOG(GetMangosString(LANG_SET_FLOAT), GuidString(guid).c_str(), field, fValue);
        target->SetFloatValue(field , fValue);
        PSendSysMessage(LANG_SET_FLOAT_FIELD, GuidString(guid).c_str(), field, fValue);
    }

    return true;
}

bool ChatHandler::HandleDebugSetItemValueCommand(char* args)
{
    uint32 guid;
    if (!ExtractUInt32(&args, guid))
    {
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
    {
        return false;
    }

    char* typeStr = ExtractOptNotLastArg(&args);
    if (!typeStr)
    {
        return false;
    }

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
    {
        return false;
    }

    Item* item = m_session->GetPlayer()->GetItemByGuid(MakeGuid(HIGHGUID_ITEM, guid));
    if (!item)
    {
        return false;
    }

    return HandleSetValueHelper(item, field, typeStr, valStr);
}

bool ChatHandler::HandleDebugSetValueCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
    {
        return false;
    }

    char* typeStr = ExtractOptNotLastArg(&args);
    if (!typeStr)
    {
        return false;
    }

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
    {
        return false;
    }

    return HandleSetValueHelper(target, field, typeStr, valStr);
}

bool ChatHandler::HandleGetValueHelper(Object* target, uint32 field, char* typeStr)
{
    ObjectGuid guid = target->GetObjectGuid();

    if (field >= target->GetValuesCount())
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, field, GuidString(guid).c_str(), target->GetValuesCount());
        return false;
    }

    uint32 base;
    if (!typeStr)
    {
        base = 10;
    }
    else if (strncmp(typeStr, "int", strlen(typeStr)) == 0)
    {
        base = 10;
    }
    else if (strncmp(typeStr, "hex", strlen(typeStr)) == 0)
    {
        base = 16;
    }
    else if (strncmp(typeStr, "bit", strlen(typeStr)) == 0)
    {
        base = 2;
    }
    else if (strncmp(typeStr, "float", strlen(typeStr)) == 0)
    {
        base = 0;
    }
    else
    {
        return false;
    }

    if (base)
    {
        uint32 iValue = target->GetUInt32Value(field);

        switch (base)
        {
            case 2:
            {

                std::string res;
                res.reserve(1 + 32 + 1);
                res = (iValue & (1 << (32 - 1))) ? "0" : " ";
                for (int i = 32; i > 0; --i)
                {
                    res += (iValue & (1 << (i - 1))) ? "1" : "0";
                }
                DEBUG_LOG(GetMangosString(LANG_GET_BITSTR), GuidString(guid).c_str(), field, res.c_str());
                PSendSysMessage(LANG_GET_BITSTR_FIELD, GuidString(guid).c_str(), field, res.c_str());
                break;
            }
            case 16:
                DEBUG_LOG(GetMangosString(LANG_GET_HEX), GuidString(guid).c_str(), field, iValue);
                PSendSysMessage(LANG_GET_HEX_FIELD, GuidString(guid).c_str(), field, iValue);
                break;
            case 10:
            default:
                DEBUG_LOG(GetMangosString(LANG_GET_UINT), GuidString(guid).c_str(), field, iValue);
                PSendSysMessage(LANG_GET_UINT_FIELD, GuidString(guid).c_str(), field, iValue);
        }
    }
    else
    {
        float fValue = target->GetFloatValue(field);
        DEBUG_LOG(GetMangosString(LANG_GET_FLOAT), GuidString(guid).c_str(), field, fValue);
        PSendSysMessage(LANG_GET_FLOAT_FIELD, GuidString(guid).c_str(), field, fValue);
    }

    return true;
}

bool ChatHandler::HandleDebugGetItemValueCommand(char* args)
{
    uint32 guid;
    if (!ExtractUInt32(&args, guid))
    {
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
    {
        return false;
    }

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr && *args)
    {
        return false;
    }

    Item* item = m_session->GetPlayer()->GetItemByGuid(MakeGuid(HIGHGUID_ITEM, guid));
    if (!item)
    {
        return false;
    }

    return HandleGetValueHelper(item, field, typeStr);
}

bool ChatHandler::HandleDebugGetValueCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
    {
        return false;
    }

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr && *args)
    {
        return false;
    }

    return HandleGetValueHelper(target, field, typeStr);
}

bool ChatHandler::HandlerDebugModValueHelper(Object* target, uint32 field, char* typeStr, char* valStr)
{
    ObjectGuid guid = target->GetObjectGuid();

    if (field >= target->GetValuesCount() || field <= OBJECT_FIELD_ENTRY)
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, field, GuidString(guid).c_str(), target->GetValuesCount());
        return false;
    }

    uint32 type;
    if (strncmp(typeStr, "int", strlen(typeStr)) == 0)
    {
        type = 1;
    }
    else if (strncmp(typeStr, "float", strlen(typeStr)) == 0)
    {
        type = 0;
    }
    else if (strncmp(typeStr, "|=", strlen("|=") + 1) == 0)
    {
        type = 2;
    }
    else if (strncmp(typeStr, "&=", strlen("&=") + 1) == 0)
    {
        type = 3;
    }
    else if (strncmp(typeStr, "&=~", strlen("&=~") + 1) == 0)
    {
        type = 4;
    }
    else
    {
        return false;
    }

    if (type)
    {
        uint32 iValue;
        if (!ExtractUInt32Base(&valStr, iValue, type == 1 ? 10 : 16))
        {
            return false;
        }

        uint32 value = target->GetUInt32Value(field);
        const std::string guidText = GuidString(guid);
        const char* guidString = guidText.c_str();

        switch (type)
        {
            default:
            case 1:
                value = uint32(int32(value) + int32(iValue));
                DEBUG_LOG(GetMangosString(LANG_CHANGE_INT32), guidString, field, iValue, value, value);
                PSendSysMessage(LANG_CHANGE_INT32_FIELD, guidString, field, iValue, value, value);
                break;
            case 2:
                value |= iValue;
                DEBUG_LOG(GetMangosString(LANG_CHANGE_HEX), guidString, field, typeStr, iValue, value);
                PSendSysMessage(LANG_CHANGE_HEX_FIELD, guidString, field, typeStr, iValue, value);
                break;
            case 3:
                value &= iValue;
                DEBUG_LOG(GetMangosString(LANG_CHANGE_HEX), guidString, field, typeStr, iValue, value);
                PSendSysMessage(LANG_CHANGE_HEX_FIELD, guidString, field, typeStr, iValue, value);
                break;
            case 4:
                value &= ~iValue;
                DEBUG_LOG(GetMangosString(LANG_CHANGE_HEX), guidString, field, typeStr, iValue, value);
                PSendSysMessage(LANG_CHANGE_HEX_FIELD, guidString, field, typeStr, iValue, value);
                break;
        }

        target->SetUInt32Value(field, value);
    }
    else
    {
        float fValue;
        if (!ExtractFloat(&valStr, fValue))
        {
            return false;
        }

        float value = target->GetFloatValue(field);

        value += fValue;

        DEBUG_LOG(GetMangosString(LANG_CHANGE_FLOAT), GuidString(guid).c_str(), field, fValue, value);
        PSendSysMessage(LANG_CHANGE_FLOAT_FIELD, GuidString(guid).c_str(), field, fValue, value);

        target->SetFloatValue(field, value);
    }

    return true;
}

bool ChatHandler::HandleDebugModItemValueCommand(char* args)
{
    uint32 guid;
    if (!ExtractUInt32(&args, guid))
    {
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
    {
        return false;
    }

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr)
    {
        return false;
    }

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
    {
        return false;
    }

    Item* item = m_session->GetPlayer()->GetItemByGuid(MakeGuid(HIGHGUID_ITEM, guid));
    if (!item)
    {
        return false;
    }

    return HandlerDebugModValueHelper(item, field, typeStr, valStr);
}

bool ChatHandler::HandleDebugModValueCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
    {
        return false;
    }

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr && *args)
    {
        return false;
    }

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
    {
        return false;
    }

    return HandlerDebugModValueHelper(target, field, typeStr, valStr);
}

bool ChatHandler::HandleDebugSpellCoefsCommand(char* args)
{
    uint32 spellid = ExtractSpellIdFromLink(&args);
    if (!spellid)
    {
        return false;
    }

    SpellEntry const* spellEntry = sSpellStore.LookupEntry(spellid);
    if (!spellEntry)
    {
        return false;
    }

    const cast::Recipe* recipe = cast::Recipes().Find(spellid);
    SpellBonusEntry const* bonus = recipe != nullptr ? recipe->Bonus() : nullptr;

    float direct_calc = cast::RecipeOf(*spellEntry).Coefficient(false);
    float dot_calc = cast::RecipeOf(*spellEntry).Coefficient(true);

    bool isDirectHeal = false;
    for (int i = 0; i < 3; ++i)
    {

        if (spellEntry->Effect[i] == SPELL_EFFECT_HEAL || spellEntry->Effect[i] == SPELL_EFFECT_HEAL_MAX_HEALTH ||
            (spellEntry->Effect[i] == SPELL_EFFECT_APPLY_AURA && (spellEntry->EffectAura[i] == SPELL_AURA_SCHOOL_ABSORB || spellEntry->EffectAura[i] == SPELL_AURA_PERIODIC_HEAL)))
        {
            isDirectHeal = true;
            break;
        }
    }

    bool isDotHeal = false;
    for (int i = 0; i < 3; ++i)
    {

        if (spellEntry->Effect[i] == SPELL_EFFECT_APPLY_AURA && spellEntry->EffectAura[i] == SPELL_AURA_PERIODIC_HEAL)
        {
            isDotHeal = true;
            break;
        }
    }

    char const* directHealStr = GetMangosString(LANG_DIRECT_HEAL);
    char const* directDamageStr = GetMangosString(LANG_DIRECT_DAMAGE);
    char const* dotHealStr = GetMangosString(LANG_DOT_HEAL);
    char const* dotDamageStr = GetMangosString(LANG_DOT_DAMAGE);

    PSendSysMessage(LANG_SPELLCOEFS, spellid, isDirectHeal ? directHealStr : directDamageStr,
        direct_calc, direct_calc * 1.88f, bonus ? bonus->direct_damage : 0.0f, bonus ? bonus->ap_bonus : 0.0f);
    PSendSysMessage(LANG_SPELLCOEFS, spellid, isDotHeal ? dotHealStr : dotDamageStr,
        dot_calc, dot_calc * 1.88f, bonus ? bonus->dot_damage : 0.0f, bonus ? bonus->ap_dot_bonus : 0.0f);

    return true;
}

bool ChatHandler::HandleDebugSpellModsCommand(char* args)
{
    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr)
    {
        return false;
    }

    uint16 opcode;
    if (strncmp(typeStr, "flat", strlen(typeStr)) == 0)
    {
        opcode = SMSG_SET_FLAT_SPELL_MODIFIER;
    }
    else if (strncmp(typeStr, "pct", strlen(typeStr)) == 0)
    {
        opcode = SMSG_SET_PCT_SPELL_MODIFIER;
    }
    else
    {
        return false;
    }

    uint32 effidx;
    if (!ExtractUInt32(&args, effidx) || effidx >= 64)
    {
        return false;
    }

    uint32 spellmodop;
    if (!ExtractUInt32(&args, spellmodop) || spellmodop >= MAX_SPELLMOD)
    {
        return false;
    }

    int32 value;
    if (!ExtractInt32(&args, value))
    {
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (HasLowerSecurity(chr))
    {
        return false;
    }

    PSendSysMessage(LANG_YOU_CHANGE_SPELLMODS, opcode == SMSG_SET_FLAT_SPELL_MODIFIER ? "flat" : "pct",
        spellmodop, value, effidx, GetNameLink(chr).c_str());
    if (needReportToTarget(chr))
    {
        ChatHandler(chr).PSendSysMessage(LANG_YOURS_SPELLMODS_CHANGED, GetNameLink().c_str(),
            opcode == SMSG_SET_FLAT_SPELL_MODIFIER ? "flat" : "pct", spellmodop, value, effidx);
    }

    WorldPacket data(opcode, (1 + 1 + 2 + 2));
    data << uint8(effidx);
    data << uint8(spellmodop);
    data << int32(value);
    chr->GetSession()->SendPacket(&data);

    return true;
}

bool ChatHandler::HandleDebugMinionCommand(char* )
{
    Player* master = getSelectedPlayer();
    if (!master)
    {
        master = m_session ? m_session->GetPlayer() : nullptr;
    }

    if (!master)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage("master   %s", DescribeSpatially(master).c_str());
    PSendSysMessage("petguid  %s  transport=%s",
                    GuidString(master->GetPetGuid()).c_str(),
                    master->GetTransport() ? "yes" : "no");

    int owned = 0;
    master->CallForAllControlledUnits(
        [this, &owned](Unit* minion)
        {
            ++owned;
            PSendSysMessage("owned    %s", DescribeSpatially(minion).c_str());
        },
        CONTROLLED_PET | CONTROLLED_MINIPET | CONTROLLED_GUARDIANS | CONTROLLED_TOTEMS |
        CONTROLLED_CHARM);

    if (!owned)
    {
        SendSysMessage("owned    NONE -- the master controls nothing the sweep can find");
    }

    Map* on = master->FindMap();
    if (!on)
    {
        return true;
    }

    DumpPetsOn(on, "onmap");

    if (TransportMap* deck = on->AsTransport())
    {
        if (Transport* vessel = deck->Vessel())
        {
            if (Map* sailed = vessel->GetMap())
            {
                DumpPetsOn(sailed, "ashore");
            }
        }
    }
    else
    {
        for (Transport* vessel : sFleet.On(on->GetId()))
        {
            if (TransportMap* hull = vessel->AsMap())
            {
                DumpPetsOn(hull, "ondeck");
            }
        }
    }

    return true;
}

bool ChatHandler::HandleDebugVesselCommand(char* )
{
    Player* caller = m_session ? m_session->GetPlayer() : nullptr;
    Map* on = caller ? caller->FindMap() : nullptr;
    if (!on)
    {
        return true;
    }

    uint32 mx = 0, my = 0;
    Cell::GridOf(caller->Where().X(), caller->Where().Y(), mx, my);
    PSendSysMessage("caller   map %u  (%.1f %.1f)  grid %u,%u", on->GetId(),
                    caller->Where().X(), caller->Where().Y(), mx, my);

    std::vector<Transport*> vessels;

    if (TransportMap* deck = on->AsTransport())
    {
        PSendSysMessage("aboard   deck map %u, hull radius %.1f", deck->GetId(), deck->HullRadius());
        if (Transport* mine = deck->Vessel())
        {
            vessels.push_back(mine);
        }
    }
    else
    {
        for (Transport* vessel : sFleet.On(on->GetId()))
        {
            vessels.push_back(vessel);
        }
    }

    if (vessels.empty())
    {
        SendSysMessage("vessel   NONE registered for this map");
        return true;
    }

    for (Transport* vessel : vessels)
    {
        uint32 vx = 0, vy = 0;
        Cell::GridOf(vessel->Where().X(), vessel->Where().Y(), vx, vy);

        uint32 const phase = vessel->GetPathProgress();
        VesselLeg const* leg = vessel->m_route.LegAt(phase);
        VesselPose const pose = vessel->m_route.PoseAt(phase);

        PSendSysMessage("vessel   %u '%s' on map %u  (%.1f %.1f)  grid %u,%u%s",
                        vessel->GetEntry(), vessel->GetName(),
                        vessel->GetMapId(), vessel->Where().X(), vessel->Where().Y(),
                        vx, vy,
                        vessel->IsCrossing() ? "  CROSSING" : "");

        PSendSysMessage("  route  phase %u of %u ms, %u legs, %u ms waiting",
                        phase, vessel->m_period, uint32(vessel->m_route.Legs().size()),
                        vessel->m_route.Waiting());

        if (leg)
        {
            PSendSysMessage("  leg    map %u  [%u..%u]  %u nodes  %u runs",
                            leg->mapId, leg->startsAt, leg->endsAt,
                            uint32(leg->nodes.size()), uint32(leg->runs.size()));
        }
        else
        {
            SendSysMessage("  leg    NONE -- the phase falls in no leg");
        }

        if (pose.known)
        {
            uint32 px = 0, py = 0;
            Cell::GridOf(pose.at.x, pose.at.y, px, py);
            PSendSysMessage("  pose   map %u  (%.1f %.1f %.1f)  grid %u,%u%s",
                            pose.mapId, pose.at.x, pose.at.y, pose.at.z, px, py,
                            pose.mapId == vessel->GetMapId() ? "" : "  MAP MISMATCH: pose not applied");
        }
        else
        {
            SendSysMessage("  pose   UNKNOWN -- the world pose is frozen where it last was");
        }

        bool const shares = (vx == mx && vy == my);
        PSendSysMessage("  relay   %s", on->AsTransport()
                        ? "aboard: the whole of the vessel's grid is swept"
                        : (shares ? "SAME GRID: the vessel is swept" : "DIFFERENT GRID: no relay"));
    }

    return true;
}

void ChatHandler::DumpPetsOn(Map* on, char const* label)
{
    for (auto const& entry : on->GetObjectsStore().GetElements<Pet>())
    {
        Pet* pet = entry.second;
        if (!pet)
        {
            continue;
        }

        Unit* owner = pet->GetOwner();
        PSendSysMessage("%-8s %s owner=%s", label, DescribeSpatially(pet).c_str(),
                        owner ? owner->GetGuidStr().c_str() : "(none)");
    }
}
