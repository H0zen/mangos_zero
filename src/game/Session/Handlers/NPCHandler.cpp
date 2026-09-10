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

#include <cmath>
#include "Platform/Define.h"
#include <string>
#include <algorithm>
#include "Common/ServerDefines.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "NPCAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "GossipDef.h"
#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureRecord.h"
#include "Pet.h"
#include "Guild.h"
#include "Spell.h"
#include "GuildMgr.h"
#include "Chat.h"
#include "World.h"
#include "Item.h"
#include "Corpse.h"

enum StableResultCode
{
    STABLE_ERR_MONEY        = 0x01,
    STABLE_ERR_STABLE       = 0x06,
    STABLE_SUCCESS_STABLE   = 0x08,
    STABLE_SUCCESS_UNSTABLE = 0x09,
    STABLE_SUCCESS_BUY_SLOT = 0x0A,
};

void npcs::TabardVendorActivate(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    Creature* unit = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTabardVendorActivateOpcode - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    who.GetSession()->SendTabardVendorActivate(guid);
}

void WorldSession::SendTabardVendorActivate(ObjectGuid guid)
{
    WorldPacket data(MSG_TABARDVENDOR_ACTIVATE, 8);
    data << static_cast<ObjectGuid>(guid);
    SendPacket(&data);
}

void npcs::BankerActivate(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;

    DEBUG_LOG("WORLD: Received opcode CMSG_BANKER_ACTIVATE");

    recv_data >> guid;

    if (!session.CheckBanker(guid))
    {
        return;
    }

    if (session.GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        session.GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    session.SendShowBank(guid);
}

void WorldSession::SendShowBank(ObjectGuid guid)
{
    WorldPacket data(SMSG_SHOW_BANK, 8);
    data << static_cast<ObjectGuid>(guid);
    SendPacket(&data);
}

void npcs::TrainerList(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;

    recv_data >> guid;

    who.GetSession()->SendTrainerList(guid);
}

void WorldSession::SendTrainerList(ObjectGuid guid)
{
    std::string str = GetMangosString(LANG_NPC_TAINER_HELLO);
    SendTrainerList(guid, str);
}

static void SendTrainerSpellHelper(WorldPacket& data, TrainerSpell const* tSpell, uint32 triggerSpell, TrainerSpellState state, float fDiscountMod, bool can_learn_primary_prof, uint32 reqLevel)
{
    bool primary_prof_first_rank = sSpellMgr.IsPrimaryProfessionFirstRankSpell(triggerSpell);

    SpellChainNode const* chain_node = sSpellMgr.GetSpellChainNode(triggerSpell);

    data << uint32(tSpell->spell);
    data << uint8(state == TRAINER_SPELL_GREEN_DISABLED ? TRAINER_SPELL_GREEN : state);

    switch (tSpell->spell)
    {
        case 33388:
        case 33389:
            data << uint32(floor(AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRAIN_MOUNT_COST)) * fDiscountMod));
            break;
        case 33391:
        case 33392:
            data << uint32(floor(AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRAIN_EPIC_MOUNT_COST)) * fDiscountMod));
            break;
        default:
            data << uint32(floor(tSpell->spellCost * fDiscountMod));
            break;
    }

    data << uint32(primary_prof_first_rank && can_learn_primary_prof ? 1 : 0);

    data << uint32(primary_prof_first_rank ? 1 : 0);
    data << uint8(reqLevel);
    data << uint32(tSpell->reqSkill);
    data << uint32(tSpell->reqSkillValue);
    data << uint32(chain_node ? (chain_node->prev ? chain_node->prev : chain_node->req) : 0);
    data << uint32(chain_node && chain_node->prev ? chain_node->req : 0);
    data << uint32(0);
}

void WorldSession::SendTrainerList(ObjectGuid guid, const std::string& strTitle)
{
    DEBUG_LOG("WORLD: SendTrainerList");

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: SendTrainerList - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (!unit->IsTrainerOf(_player, true))
    {
        return;
    }

    CreatureInfo const* ci = unit->GetCreatureInfo();
    if (!ci)
    {
        return;
    }

    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
    {
        DEBUG_LOG("WORLD: SendTrainerList - Training spells not found for %s", GuidString(guid).c_str());
        return;
    }

    uint32 maxcount = (cSpells ? cSpells->spellList.size() : 0) + (tSpells ? tSpells->spellList.size() : 0);
    uint32 trainer_type = cSpells && cSpells->trainerType ? cSpells->trainerType : (tSpells ? tSpells->trainerType : 0);

    WorldPacket data(SMSG_TRAINER_LIST, 8 + 4 + 4 + maxcount * 38 + strTitle.size() + 1);
    data << static_cast<ObjectGuid>(guid);
    data << uint32(trainer_type);

    size_t count_pos = data.wpos();
    data << uint32(maxcount);

    float fDiscountMod = _player->GetReputationPriceDiscount(unit);
    bool can_learn_primary_prof = GetPlayer()->GetFreePrimaryProfessionPoints() > 0;

    uint32 count = 0;

    if (cSpells)
    {
        for (TrainerSpellMap::const_iterator itr = cSpells->spellList.begin(); itr != cSpells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            uint32 triggerSpell = sSpellStore.LookupEntry(tSpell->spell)->EffectTriggerSpell[0];

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->spell, &reqLevel))
            {
                continue;
            }

            switch (tSpell->spell)
            {
                case 33388:
                case 33389:
                    reqLevel = AccountTypes(sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL));
                    break;
                case 33391:
                case 33392:
                    reqLevel = AccountTypes(sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL));
                    break;
                default:
                    reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);
                    break;
            }

            TrainerSpellState state = _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, triggerSpell, state, fDiscountMod, can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    if (tSpells)
    {
        for (TrainerSpellMap::const_iterator itr = tSpells->spellList.begin(); itr != tSpells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            uint32 triggerSpell = sSpellStore.LookupEntry(tSpell->spell)->EffectTriggerSpell[0];

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->spell, &reqLevel))
            {
                continue;
            }

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);

            TrainerSpellState state = _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, triggerSpell, state, fDiscountMod, can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    data << strTitle;

    data.put<uint32>(count_pos, count);
    SendPacket(&data);
}

void npcs::TrainerBuySpell(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    uint32 spellId = 0;

    recv_data >> guid >> spellId;
    DEBUG_LOG("WORLD: Received opcode CMSG_TRAINER_BUY_SPELL Trainer: %s, learn spell id is: %u", GuidString(guid).c_str(), spellId);

    Creature* unit = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTrainerBuySpellOpcode - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (!unit->IsTrainerOf(&who, true))
    {
        return;
    }

    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
    {
        return;
    }

    TrainerSpell const* trainer_spell = cSpells ? cSpells->Find(spellId) : nullptr;

    if (!trainer_spell && tSpells)
    {
        trainer_spell = tSpells->Find(spellId);
    }

    if (!trainer_spell)
    {
        return;
    }

    uint32 reqLevel = 0;
    if (!who.IsSpellFitByClassAndRace(trainer_spell->spell, &reqLevel))
    {
        return;
    }

    reqLevel = trainer_spell->isProvidedReqLevel ? trainer_spell->reqLevel : std::max(reqLevel, trainer_spell->reqLevel);
    if (who.GetTrainerSpellState(trainer_spell, reqLevel) != TRAINER_SPELL_GREEN)
    {
        return;
    }

    SpellEntry const* proto = sSpellStore.LookupEntry(trainer_spell->spell);

    uint32 nSpellCost = uint32(floor(trainer_spell->spellCost * who.GetReputationPriceDiscount(unit)));

    if (who.GetMoney() < nSpellCost)
    {
        return;
    }

    who.ModifyMoney(-int32(nSpellCost));

    who.GetSession()->SendPlaySpellVisual(guid, 0xB3);

    WorldPacket data(SMSG_PLAY_SPELL_IMPACT, 8 + 4);
    data << who.GetObjectGuid();
    data << uint32(0x016A);
    who.GetSession()->SendPacket(&data);

    data.Initialize(SMSG_TRAINER_BUY_SUCCEEDED, 12);
    data << static_cast<ObjectGuid>(guid);
    data << uint32(spellId);
    who.GetSession()->SendPacket(&data);

    Spell* spell;
    if (proto->SpellVisualID == 222)
    {
        spell = new Spell(&who, proto, false);
    }
    else
    {
        spell = new Spell(unit, proto, false);
    }

    SpellCastTargets targets;
    targets.setUnitTarget(&who);

    spell->prepare(&targets);
}

void npcs::GossipHello(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GOSSIP_HELLO");

    ObjectGuid guid = 0;
    recv_data >> guid;

    Creature* pCreature = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleGossipHelloOpcode - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    pCreature->StopMoving();

    if (pCreature->IsSpiritGuide())
    {
        pCreature->SendAreaSpiritHealerQueryOpcode(&who);
    }

    if (!sScriptMgr.OnGossipHello(&who, pCreature))
    {
        who.PrepareGossipMenu(pCreature, pCreature->GetCreatureInfo()->GossipMenuId);
        who.SendPreparedGossip(pCreature);
    }
}

void npcs::GossipSelectOption(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_GOSSIP_SELECT_OPTION");

    uint32 gossipListId;
    ObjectGuid guid = 0;
    std::string code;

    recv_data >> guid >> gossipListId;

    if (who.PlayerTalkClass->GossipOptionCoded(gossipListId))
    {
        recv_data >> code;
        DEBUG_LOG("Gossip code: %s", code.c_str());
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    uint32 sender = who.PlayerTalkClass->GossipOptionSender(gossipListId);
    uint32 action = who.PlayerTalkClass->GossipOptionAction(gossipListId);

    if ((GuidHigh(guid) == HIGHGUID_UNIT || GuidHigh(guid) == HIGHGUID_PET))
    {
        Creature* pCreature = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);

        if (!pCreature)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", GuidString(guid).c_str());
            return;
        }

        if (!sScriptMgr.OnGossipSelect(&who, pCreature, sender, action, code.empty() ? nullptr : code.c_str()))
        {
            who.OnGossipSelect(pCreature, gossipListId);
        }
    }
    else if ((GuidHigh(guid) == HIGHGUID_GAMEOBJECT))
    {
        GameObject* pGo = who.GetGameObjectIfCanInteractWith(guid);

        if (!pGo)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", GuidString(guid).c_str());
            return;
        }

        if (!sScriptMgr.OnGossipSelect(&who, pGo, sender, action, code.empty() ? nullptr : code.c_str()))
        {
            who.OnGossipSelect(pGo, gossipListId);
        }
    }
    else if ((GuidHigh(guid) == HIGHGUID_ITEM))
    {
        Item* item = who.GetItemByGuid(guid);
        if (!item)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", GuidString(guid).c_str());
            return;
        }

        if (!sScriptMgr.OnGossipSelect(&who, item, sender, action, code.empty() ? nullptr : code.c_str()))
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - item script for %s not found or you can't interact with it.", item->GetProto()->Name1);
            return;
        }

    }
    else if ((guid != 0 && GuidHigh(guid) == HIGHGUID_PLAYER))
    {
        if (who.GetGUIDLow() != guid)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", GuidString(guid).c_str());
            return;
        }

    }
}

void npcs::SpiritHealerActivate(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_SPIRIT_HEALER_ACTIVATE");

    ObjectGuid guid = 0;

    recv_data >> guid;

    Creature* unit = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_SPIRITHEALER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleSpiritHealerActivateOpcode - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    who.GetSession()->SendSpiritResurrect();
}

void WorldSession::SendSpiritResurrect()
{
    _player->ResurrectPlayer(0.5f, true);

    _player->DurabilityLossAll(0.25f, true);

    WorldSafeLocsEntry const* corpseGrave = nullptr;
    Corpse* corpse = _player->GetCorpse();
    if (corpse)
    {
        corpseGrave = sObjectMgr.GetClosestGraveYard(corpse->Where().X(), corpse->Where().Y(), corpse->Where().Z(), corpse->GetMapId(), _player->GetTeam());
    }

    _player->SpawnCorpseBones();

    if (corpseGrave)
    {
        WorldSafeLocsEntry const* ghostGrave = sObjectMgr.GetClosestGraveYard(
            _player->Where().X(), _player->Where().Y(), _player->Where().Z(), _player->GetMapId(), _player->GetTeam());

        if (corpseGrave != ghostGrave)
        {
            _player->TeleportTo(corpseGrave->map_id, corpseGrave->x, corpseGrave->y, corpseGrave->z, _player->Where().Facing());
        }

        else
        {
            _player->GetCamera().UpdateVisibilityForOwner();
            _player->UpdateObjectVisibility();
        }
    }

    else
    {
        _player->GetCamera().UpdateVisibilityForOwner();
        _player->UpdateObjectVisibility();
    }
}

void npcs::BinderActivate(Player& who, WorldPacket& recv_data)
{
    ObjectGuid npcGuid = 0;
    recv_data >> npcGuid;

    if (!who.IsInWorld() || !who.IsAlive())
    {
        return;
    }

    Creature* unit = who.GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_INNKEEPER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleBinderActivateOpcode - %s not found or you can't interact with him.", GuidString(npcGuid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    who.GetSession()->SendBindPoint(unit);
}

void WorldSession::SendBindPoint(Creature* npc)
{

    if (GetPlayer()->GetMap()->Instanceable())
    {
        return;
    }

    npc->CastSpell(_player, 3286, true);

    _player->PlayerTalkClass->CloseGossip();
}

void npcs::ListStabledPets(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv MSG_LIST_STABLED_PETS");
    ObjectGuid npcGUID = 0;

    recv_data >> npcGUID;

    Creature* unit = who.GetNPCIfCanInteractWith(npcGUID, UNIT_NPC_FLAG_STABLEMASTER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleListStabledPetsOpcode - %s not found or you can't interact with him.", GuidString(npcGUID).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    who.GetSession()->SendStablePet(npcGUID);
}

void WorldSession::SendStablePet(ObjectGuid guid)
{
    DEBUG_LOG("WORLD: Recv MSG_LIST_STABLED_PETS Send.");

    WorldPacket data(MSG_LIST_STABLED_PETS, 200);
    data << guid;

    Pet* pet = _player->GetPet();

    size_t wpos = data.wpos();
    data << uint8(0);

    data << uint8(GetPlayer()->GetStableSlots());

    uint8 num = 0;

    if (pet && pet->IsAlive() && pet->getPetType() == HUNTER_PET)
    {
        data << uint32(pet->GetCharmInfo()->GetPetNumber());
        data << uint32(pet->GetEntry());
        data << uint32(pet->getLevel());
        data << pet->GetName();
        data << uint32(pet->GetLoyaltyLevel());
        data << uint8(0x01);
        ++num;
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `owner`, `slot`, `id`, `entry`, `level`, `loyalty`, `name` FROM `character_pet` WHERE `owner` = '%u' AND `slot` >= '%u' AND `slot` <= '%u' ORDER BY `slot`",
        _player->GetGUIDLow(), PET_SAVE_FIRST_STABLE_SLOT, PET_SAVE_LAST_STABLE_SLOT);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            data << uint32(fields[2].GetUInt32());
            data << uint32(fields[3].GetUInt32());
            data << uint32(fields[4].GetUInt32());
            data << fields[6].GetString();
            data << uint32(fields[5].GetUInt32());
            data << uint8(fields[1].GetUInt32() + 1);

            ++num;
        }
        while (result->NextRow());

        delete result;
    }

    data.put<uint8>(wpos, num);
    SendPacket(&data);
}

void WorldSession::SendStableResult(uint8 res)
{
    WorldPacket data(SMSG_STABLE_RESULT, 1);
    data << uint8(res);
    SendPacket(&data);
}

bool WorldSession::CheckStableMaster(ObjectGuid guid)
{

    if (guid == GetPlayer()->GetObjectGuid())
    {

        if (!ChatHandler(GetPlayer()).FindCommand("stable"))
        {
            DEBUG_LOG("%s attempt open stable in cheating way.", GuidString(guid).c_str());
            return false;
        }
    }

    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_STABLEMASTER))
        {
            DEBUG_LOG("Stablemaster %s not found or you can't interact with him.", GuidString(guid).c_str());
            return false;
        }
    }

    return true;
}

void npcs::StablePet(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_STABLE_PET");
    ObjectGuid npcGUID = 0;

    recv_data >> npcGUID;

    if (!who.IsAlive())
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (!who.GetSession()->CheckStableMaster(npcGUID))
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    Pet* pet = who.GetPet();

    if (!pet || !pet->IsAlive() || pet->getPetType() != HUNTER_PET)
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    uint32 free_slot = 1;

    QueryResult* result = CharacterDatabase.PQuery("SELECT `owner`,`slot`,`id` FROM `character_pet` WHERE `owner` = '%u' AND `slot` >= '%u' AND `slot` <= '%u' ORDER BY `slot`",
        who.GetGUIDLow(), PET_SAVE_FIRST_STABLE_SLOT, PET_SAVE_LAST_STABLE_SLOT);
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 slot = fields[1].GetUInt32();

            if (slot != free_slot)
            {
                break;
            }

            ++free_slot;
        }
        while (result->NextRow());

        delete result;
    }

    if (free_slot > 0 && free_slot <= who.GetStableSlots())
    {
        pet->Unsummon(PetSaveMode(free_slot), &who);
        who.GetSession()->SendStableResult(STABLE_SUCCESS_STABLE);
    }
    else
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
    }
}

void npcs::UnstablePet(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_UNSTABLE_PET.");
    ObjectGuid npcGUID = 0;
    uint32 petnumber;

    recv_data >> npcGUID >> petnumber;

    if (!who.GetSession()->CheckStableMaster(npcGUID))
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    uint32 creature_id = 0;

    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `entry` FROM `character_pet` WHERE `owner` = '%u' AND `id` = '%u' AND `slot` >='%u' AND `slot` <= '%u'",
            who.GetGUIDLow(), petnumber, PET_SAVE_FIRST_STABLE_SLOT, PET_SAVE_LAST_STABLE_SLOT);
        if (result)
        {
            Field* fields = result->Fetch();
            creature_id = fields[0].GetUInt32();
            delete result;
        }
    }

    if (!creature_id)
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(creature_id);
    if (!creatureInfo || !CreatureRecord(*creatureInfo).IsTameable())
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    Pet* pet = who.GetPet();
    if (pet && pet->IsAlive())
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (pet)
    {
        pet->Unsummon(PET_SAVE_AS_DELETED, &who);
    }

    Pet* newpet = new Pet(HUNTER_PET);
    if (!newpet->LoadPetFromDB(&who, creature_id, petnumber))
    {
        delete newpet;
        newpet = nullptr;
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    who.GetSession()->SendStableResult(STABLE_SUCCESS_UNSTABLE);
}

void npcs::BuyStableSlot(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_BUY_STABLE_SLOT.");
    ObjectGuid npcGUID = 0;

    recv_data >> npcGUID;

    if (!who.GetSession()->CheckStableMaster(npcGUID))
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (who.GetStableSlots() < MAX_PET_STABLES)
    {
        StableSlotPricesEntry const* SlotPrice = sStableSlotPricesStore.LookupEntry(who.GetStableSlots() + 1);
        if (who.GetMoney() >= SlotPrice->Price)
        {
            who.SetStableSlots(who.GetStableSlots() + 1);
            who.ModifyMoney(-int32(SlotPrice->Price));
            who.GetSession()->SendStableResult(STABLE_SUCCESS_BUY_SLOT);
        }
        else
        {
            who.GetSession()->SendStableResult(STABLE_ERR_MONEY);
        }
    }
    else
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
    }
}

void npcs::StableRevivePet(Player& who, WorldPacket& )
{
    DEBUG_LOG("HandleStableRevivePet: Not implemented");
}

void npcs::StableSwapPet(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_STABLE_SWAP_PET.");
    ObjectGuid npcGUID = 0;
    uint32 pet_number;

    recv_data >> npcGUID >> pet_number;

    if (!who.GetSession()->CheckStableMaster(npcGUID))
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    Pet* pet = who.GetPet();

    if (!pet || pet->getPetType() != HUNTER_PET)
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `slot`,`entry` FROM `character_pet` WHERE `owner` = '%u' AND `id` = '%u'",
        who.GetGUIDLow(), pet_number);
    if (!result)
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    Field* fields = result->Fetch();

    uint32 slot        = fields[0].GetUInt32();
    uint32 creature_id = fields[1].GetUInt32();
    delete result;

    if (!creature_id)
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(creature_id);
    if (!creatureInfo || !CreatureRecord(*creatureInfo).IsTameable())
    {
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    pet->Unsummon(pet->IsAlive() ? PetSaveMode(slot) : PET_SAVE_AS_DELETED, &who);

    Pet* newpet = new Pet;
    if (!newpet->LoadPetFromDB(&who, creature_id, pet_number))
    {
        delete newpet;
        who.GetSession()->SendStableResult(STABLE_ERR_STABLE);
    }
    else
    {
        who.GetSession()->SendStableResult(STABLE_SUCCESS_UNSTABLE);
    }
}

void npcs::RepairItem(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_REPAIR_ITEM");

    ObjectGuid npcGuid = 0;
    ObjectGuid itemGuid = 0;

    recv_data >> npcGuid >> itemGuid;

    Creature* unit = who.GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_REPAIR);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleRepairItemOpcode - %s not found or you can't interact with him.", GuidString(npcGuid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    float discountMod = who.GetReputationPriceDiscount(unit);

    uint32 TotalCost = 0;
    if (itemGuid)
    {
        DEBUG_LOG("ITEM: %s repair of %s", GuidString(npcGuid).c_str(), GuidString(itemGuid).c_str());

        Item* item = who.GetItemByGuid(itemGuid);

        if (item)
        {
            TotalCost = who.DurabilityRepair(item->GetPos(), true, discountMod);
        }
    }
    else
    {
        DEBUG_LOG("ITEM: %s repair all items", GuidString(npcGuid).c_str());

        TotalCost = who.DurabilityRepairAll(true, discountMod);
    }
}
