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
#include "Platform/Define.h"
#include <cstring>
#include <string>
#include <ctime>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "PetAnswers.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Spell.h"
#include "CreatureAI.h"
#include "Util.h"
#include "Pet.h"
#include "Cast/Recipe/RecipeBook.h"

void pets::PetAction(Player& who, WorldPacket& recv_data)
{
    ObjectGuid petGuid = 0;
    uint32 data;
    ObjectGuid targetGuid = 0;
    recv_data >> petGuid;
    recv_data >> data;
    recv_data >> targetGuid;

    uint32 spellid = UNIT_ACTION_BUTTON_ACTION(data);
    uint8 flag = UNIT_ACTION_BUTTON_TYPE(data);

    DETAIL_LOG("HandlePetAction: %s flag is %u, spellid is %u, target %s.", GuidString(petGuid).c_str(), uint32(flag), spellid, GuidString(targetGuid).c_str());

    Unit* pet = who.GetMap()->GetUnit(petGuid);
    if (!pet)
    {
        sLog.outError("HandlePetAction: %s not exist.", GuidString(petGuid).c_str());
        return;
    }

    if (who.GetObjectGuid() != pet->GetCharmerOrOwnerGuid())
    {
        sLog.outError("HandlePetAction: %s isn't controlled by %s.", GuidString(petGuid).c_str(), who.GetGuidStr().c_str());
        return;
    }

    if (!pet->IsAlive())
    {
        return;
    }

    if (IsPlayer(pet))
    {

        if (!(flag == ACT_COMMAND && spellid == COMMAND_ATTACK))
        {
            return;
        }
    }
    else if (((Creature*)pet)->IsPet())
    {

        if (((Pet*)pet)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
        {
            return;
        }
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("WorldSession::HandlePetAction: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    switch (flag)
    {
        case ACT_COMMAND:
            switch (spellid)
            {
                case COMMAND_STAY:
                    pet->StopMoving();
                    pet->GetMotionMaster()->Clear(false);
                    pet->GetMotionMaster()->MoveIdle();
                    charmInfo->SetCommandState(COMMAND_STAY);
                    if (pet->getVictim())
                    {
                        pet->AttackStop();
                    }
                    break;
                case COMMAND_FOLLOW:
                    pet->AttackStop();
                    pet->GetMotionMaster()->MoveFollow(&who, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
                    charmInfo->SetCommandState(COMMAND_FOLLOW);
                    break;
                case COMMAND_ATTACK:
                {
                    Unit* TargetUnit = who.GetMap()->GetUnit(targetGuid);
                    if (!TargetUnit)
                    {
                        return;
                    }

                    if (IsFriendly(who, *TargetUnit))
                    {
                        return;
                    }

                    if (!HasLineOfSight(*pet, *TargetUnit))
                    {
                        return;
                    }

                    if (pet->getVictim() != TargetUnit)
                    {
                        if (pet->getVictim())
                        {
                            pet->AttackStop();
                        }

                        if (pet->hasUnitState(UNIT_STAT_CONTROLLED))
                        {
                            pet->Attack(TargetUnit, true);
                            pet->SendPetAIReaction();
                        }
                        else
                        {
                            pet->GetMotionMaster()->Clear();

                            if (((Creature*)pet)->AI())
                            {
                                ((Creature*)pet)->AI()->AttackStart(TargetUnit);
                            }

                            if (((Creature*)pet)->IsPet() && ((Pet*)pet)->getPetType() == SUMMON_PET && pet != TargetUnit && roll_chance_i(10))
                            {
                                pet->SendPetTalk((uint32)PET_TALK_ATTACK);
                            }
                            else
                            {

                                pet->SendPetAIReaction();
                            }
                        }
                    }
                    break;
                }
                case COMMAND_ABANDON:
                    if (((Creature*)pet)->IsPet())
                    {
                        Pet* p = (Pet*)pet;
                        if (p->getPetType() == HUNTER_PET)
                        {
                            p->Unsummon(PET_SAVE_AS_DELETED, &who);
                        }
                        else

                        {
                            p->SetDeathState(CORPSE);
                        }
                    }
                    else
                    {
                        who.Uncharm();
                    }
                    break;
                default:
                    sLog.outError("WORLD: unknown PET flag Action %i and spellid %i.", uint32(flag), spellid);
            }
            break;
        case ACT_REACTION:
            switch (spellid)
            {
                case REACT_PASSIVE:
                    if (pet->getVictim())
                    {
                        pet->AttackStop();
                    }
                    pet->GetMotionMaster()->MoveFollow(&who, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
                case REACT_DEFENSIVE:
                case REACT_AGGRESSIVE:
                    charmInfo->SetReactState(ReactStates(spellid));
                    break;
            }
            break;
        case ACT_DISABLED:
        case ACT_PASSIVE:
        case ACT_ENABLED:
        {
            Unit* unit_target = nullptr;
            if (targetGuid)
            {
                unit_target = who.GetMap()->GetUnit(targetGuid);
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
            if (!spellInfo)
            {
                sLog.outError("WORLD: unknown PET spell id %i", spellid);
                return;
            }

            if (pet->GetCharmInfo() && pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
            {
                return;
            }

            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (spellInfo->ImplicitTargetA[i] == TARGET_ALL_ENEMY_IN_AREA || spellInfo->ImplicitTargetA[i] == TARGET_ALL_ENEMY_IN_AREA_INSTANT || spellInfo->ImplicitTargetA[i] == TARGET_ALL_ENEMY_IN_AREA_CHANNELED)
                {
                    return;
                }
            }

            if (!pet->HasSpell(spellid) || (cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
            {
                return;
            }

            pet->clearUnitState(UNIT_STAT_MOVING);

            Spell* spell = new Spell(pet, spellInfo, false);

            SpellCastResult result = spell->CheckPetCast(unit_target);

            if (result == SPELL_FAILED_UNIT_NOT_INFRONT && !pet->HasAuraType(SPELL_AURA_MOD_POSSESS))
            {
                if (unit_target)
                {
                    pet->SetInFront(unit_target);
                    if (IsPlayer(unit_target))
                    {
                        pet->SendCreateUpdateToPlayer((Player*)unit_target);
                    }
                }
                else if (Unit* unit_target2 = spell->m_targets.getUnitTarget())
                {
                    pet->SetInFront(unit_target2);
                    if (IsPlayer(unit_target2))
                    {
                        pet->SendCreateUpdateToPlayer((Player*)unit_target2);
                    }
                }
                if (Unit* powner = pet->GetCharmerOrOwner())
                {
                    if (IsPlayer(powner))
                    {
                        pet->SendCreateUpdateToPlayer((Player*)powner);
                    }
                }
                result = SPELL_CAST_OK;
            }

            if (result == SPELL_CAST_OK)
            {
                ((Creature*)pet)->AddCreatureSpellCooldown(spellid);
                if (((Creature*)pet)->IsPet())
                {
                    ((Pet*)pet)->CheckLearning(spellid);
                }

                unit_target = spell->m_targets.getUnitTarget();

                if (((Creature*)pet)->IsPet() && (((Pet*)pet)->getPetType() == SUMMON_PET) && (pet != unit_target) && (urand(0, 100) < 10))
                {
                    pet->SendPetTalk((uint32)PET_TALK_SPECIAL_SPELL);
                }
                else
                {
                    pet->SendPetAIReaction();
                }

                if (unit_target && !IsFriendly(who, *unit_target) && !pet->HasAuraType(SPELL_AURA_MOD_POSSESS))
                {

                    if (pet->getVictim() != unit_target)
                    {
                        if (pet->getVictim())
                        {
                            pet->AttackStop();
                        }
                        pet->GetMotionMaster()->Clear();
                        if (((Creature*)pet)->AI())
                        {
                            ((Creature*)pet)->AI()->AttackStart(unit_target);
                        }
                    }
                }

                spell->prepare(&(spell->m_targets));
            }
            else
            {
                if (pet->HasAuraType(SPELL_AURA_MOD_POSSESS))
                {
                    Spell::SendCastResult(&who, spellInfo, result);
                }
                else
                {
                    pet->SendPetCastFail(spellid, result);
                }

                if (!((Creature*)pet)->HasSpellCooldown(spellid))
                {
                    who.SendClearCooldown(spellid, pet);
                }

                spell->finish(false);
                delete spell;
            }
            break;
        }
        default:
            sLog.outError("WORLD: unknown PET flag Action %i and spellid %i.", uint32(flag), spellid);
    }
}

void pets::PetStopAttack(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_PET_STOP_ATTACK");

    ObjectGuid petGuid = 0;
    recv_data >> petGuid;

    Unit* pet = who.GetMap()->GetUnit(petGuid);
    if (!pet)
    {
        sLog.outError("%s doesn't exist.", GuidString(petGuid).c_str());
        return;
    }

    if (who.GetObjectGuid() != pet->GetCharmerOrOwnerGuid())
    {
        sLog.outError("HandlePetStopAttack: %s isn't charm/pet of %s.", GuidString(petGuid).c_str(), who.GetGuidStr().c_str());
        return;
    }

    if (!pet->IsAlive())
    {
        return;
    }

    pet->AttackStop();
}

void pets::PetNameQuery(Player& who, WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetNameQuery. CMSG_PET_NAME_QUERY");

    uint32 petnumber;
    ObjectGuid petguid = 0;

    recv_data >> petnumber;
    recv_data >> petguid;

    who.GetSession()->SendPetNameQuery(petguid, petnumber);
}

void WorldSession::SendPetNameQuery(ObjectGuid petguid, uint32 petnumber)
{
    Creature* pet = _player->GetMap()->GetAnyTypeCreature(petguid);
    if (!pet || !pet->GetCharmInfo() || pet->GetCharmInfo()->GetPetNumber() != petnumber)
    {
        return;
    }

    char const* name = pet->GetName();

    if (!(pet->GetOwnerGuid() != 0 && GuidHigh(pet->GetOwnerGuid()) == HIGHGUID_PLAYER))
    {
        int loc_idx = GetSessionDbLocaleIndex();
        sObjectMgr.GetCreatureLocaleStrings(pet->GetEntry(), loc_idx, &name);
    }

    WorldPacket data(SMSG_PET_NAME_QUERY_RESPONSE, (4 + 4 + strlen(name) + 1));
    data << uint32(petnumber);
    data << name;
    data << uint32(pet->GetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP));

    _player->GetSession()->SendPacket(&data);
}

void pets::PetSetAction(Player& who, WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetSetAction. CMSG_PET_SET_ACTION");

    ObjectGuid petGuid = 0;
    uint8  count;

    recv_data >> petGuid;

    Creature* pet = who.GetMap()->GetAnyTypeCreature(petGuid);

    if (!pet || (pet != who.GetPet() && pet != who.GetCharm()))
    {
        sLog.outError("HandlePetSetAction: Unknown pet or pet owner.");
        return;
    }

    if (pet->IsPet() && ((Pet*)pet)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
    {
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("WorldSession::HandlePetSetAction: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    count = (recv_data.size() == 24) ? 2 : 1;

    uint32 position[2];
    uint32 data[2];
    bool move_command = false;

    for (uint8 i = 0; i < count; ++i)
    {
        recv_data >> position[i];
        recv_data >> data[i];

        uint8 act_state = UNIT_ACTION_BUTTON_TYPE(data[i]);

        if (position[i] >= MAX_UNIT_ACTION_BAR_INDEX)
        {
            return;
        }

        if (act_state == ACT_COMMAND || act_state == ACT_REACTION)
        {
            if (count == 1)
            {
                return;
            }

            move_command = true;
        }
    }

    if (move_command)
    {
        uint8 act_state_0 = UNIT_ACTION_BUTTON_TYPE(data[0]);
        if (act_state_0 == ACT_COMMAND || act_state_0 == ACT_REACTION)
        {
            uint32 spell_id_0 = UNIT_ACTION_BUTTON_ACTION(data[0]);
            UnitActionBarEntry const* actionEntry_1 = charmInfo->GetActionBarEntry(position[1]);
            if (!actionEntry_1 || spell_id_0 != actionEntry_1->GetAction() ||
                act_state_0 != actionEntry_1->GetType())
            {
                return;
            }
        }

        uint8 act_state_1 = UNIT_ACTION_BUTTON_TYPE(data[1]);
        if (act_state_1 == ACT_COMMAND || act_state_1 == ACT_REACTION)
        {
            uint32 spell_id_1 = UNIT_ACTION_BUTTON_ACTION(data[1]);
            UnitActionBarEntry const* actionEntry_0 = charmInfo->GetActionBarEntry(position[0]);
            if (!actionEntry_0 || spell_id_1 != actionEntry_0->GetAction() ||
                act_state_1 != actionEntry_0->GetType())
            {
                return;
            }
        }
    }

    for (uint8 i = 0; i < count; ++i)
    {
        uint32 spell_id = UNIT_ACTION_BUTTON_ACTION(data[i]);
        uint8 act_state = UNIT_ACTION_BUTTON_TYPE(data[i]);

        DETAIL_LOG("Player %s has changed pet spell action. Position: %u, Spell: %u, State: 0x%X", who.GetName(), position[i], spell_id, uint32(act_state));

        if (!((act_state == ACT_ENABLED || act_state == ACT_DISABLED || act_state == ACT_PASSIVE) && spell_id && !pet->HasSpell(spell_id)))
        {

            if (act_state == ACT_ENABLED && spell_id)
            {
                if (pet->IsCharmed())
                {
                    charmInfo->ToggleCreatureAutocast(spell_id, true);
                }
                else
                {
                    ((Pet*)pet)->ToggleAutocast(spell_id, true);
                }
            }

            else if (act_state == ACT_DISABLED && spell_id)
            {
                if (pet->IsCharmed())
                {
                    charmInfo->ToggleCreatureAutocast(spell_id, false);
                }
                else
                {
                    ((Pet*)pet)->ToggleAutocast(spell_id, false);
                }
            }

            charmInfo->SetActionBar(position[i], spell_id, ActiveStates(act_state));
        }
    }
}

void pets::PetRename(Player& who, WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetRename. CMSG_PET_RENAME");

    ObjectGuid petGuid = 0;
    std::string name;

    recv_data >> petGuid;
    recv_data >> name;

    Pet* pet = who.GetMap()->GetPet(petGuid);

    if (!pet || pet->getPetType() != HUNTER_PET ||
        !pet->HasUnitFlag(UNIT_FLAG_RENAME) ||
        pet->GetOwnerGuid() != who.GetObjectGuid() || !pet->GetCharmInfo())
    {
        return;
    }

    PetNameInvalidReason res = ObjectMgr::CheckPetName(name);
    if (res != PET_NAME_SUCCESS)
    {
        who.GetSession()->SendPetNameInvalid(res, name);
        return;
    }

    if (sObjectMgr.IsReservedName(name))
    {
        who.GetSession()->SendPetNameInvalid(PET_NAME_RESERVED, name);
        return;
    }

    pet->SetName(name);

    if (who.GetGroup())
    {
        who.SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);
    }

    pet->RemoveUnitFlag(UNIT_FLAG_RENAME);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.escape_string(name);
    CharacterDatabase.PExecute("UPDATE `character_pet` SET `name` = '%s', `renamed` = '1' WHERE `owner` = '%u' AND `id` = '%u'", name.c_str(), who.GetGUIDLow(), pet->GetCharmInfo()->GetPetNumber());
    CharacterDatabase.CommitTransaction();

    pet->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(nullptr)));
}

void pets::PetAbandon(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    DETAIL_LOG("HandlePetAbandon. CMSG_PET_ABANDON pet guid is %s", GuidString(guid).c_str());

    if (!who.IsInWorld())
    {
        return;
    }

    if (Creature* pet = who.GetMap()->GetAnyTypeCreature(guid))
    {
        if (pet->IsPet())
        {
            if (pet->GetObjectGuid() == who.GetPetGuid())
            {
                pet->ModifyPower(POWER_HAPPINESS, -50000);
            }

            ((Pet*)pet)->Unsummon(PET_SAVE_AS_DELETED, &who);
        }
        else if (pet->GetObjectGuid() == who.GetCharmGuid())
        {
            who.Uncharm();
        }
    }
}

void pets::PetUnlearn(Player& who, WorldPacket& recvPacket)
{
    DETAIL_LOG("CMSG_PET_UNLEARN");

    ObjectGuid guid = 0;
    recvPacket >> guid;

    Pet* pet = who.GetPet();

    if (!pet || guid != pet->GetObjectGuid())
    {
        sLog.outError("HandlePetUnlearnOpcode. %s isn't pet of %s .", GuidString(guid).c_str(), who.GetGuidStr().c_str());
        return;
    }

    if (pet->getPetType() != HUNTER_PET || pet->m_spells.size() <= 1)
    {
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("WorldSession::HandlePetUnlearnOpcode: %s is considered pet-like but doesn't have a charminfo!", pet->GetGuidStr().c_str());
        return;
    }

    uint32 cost = pet->resetTalentsCost();

    if (who.GetMoney() < cost)
    {
        who.SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
        return;
    }

    for (PetSpellMap::iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end();)
    {
        uint32 spell_id = itr->first;
        ++itr;
        pet->unlearnSpell(spell_id, false);
    }

    pet->SetTP(pet->getLevel() * (pet->GetLoyaltyLevel() - 1));

    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (UnitActionBarEntry const* ab = charmInfo->GetActionBarEntry(i))
        {
            if (ab->GetAction() && ab->IsActionBarForSpell())
            {
                charmInfo->SetActionBar(i, 0, ACT_DISABLED);
            }
        }
    }

    pet->LearnPetPassives();

    pet->m_resetTalentsTime = time(nullptr);
    pet->m_resetTalentsCost = cost;
    who.ModifyMoney(-(int32)cost);

    who.PetSpellInitialize();
}

void pets::PetSpellAutocast(Player& who, WorldPacket& recvPacket)
{
    DETAIL_LOG("CMSG_PET_SPELL_AUTOCAST");

    ObjectGuid guid = 0;
    uint32 spellid;
    uint8  state;
    recvPacket >> guid >> spellid >> state;

    Creature* pet = who.GetMap()->GetAnyTypeCreature(guid);
    if (!pet || (guid != who.GetPetGuid() && guid != who.GetCharmGuid()))
    {
        sLog.outError("HandlePetSpellAutocastOpcode. %s isn't pet of %s .", GuidString(guid).c_str(), who.GetGuidStr().c_str());
        return;
    }

    if (!pet->HasSpell(spellid) || cast::Recipes().StartsAs(spellid, cast::Start::Passive))
    {
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("WorldSession::HandlePetSpellAutocastOpcod: %s is considered pet-like but doesn't have a charminfo!", GuidString(guid).c_str());
        return;
    }

    if (pet->IsCharmed())

    {
        pet->GetCharmInfo()->ToggleCreatureAutocast(spellid, state);
    }
    else
    {
        ((Pet*)pet)->ToggleAutocast(spellid, state);
    }

    charmInfo->SetSpellAutocast(spellid, state);
}

void pets::PetCastSpell(Player& who, WorldPacket& recvPacket)
{
    DETAIL_LOG("WORLD: CMSG_PET_CAST_SPELL");

    ObjectGuid guid = 0;
    uint32 spellid;

    recvPacket >> guid >> spellid;

    DEBUG_LOG("WORLD: CMSG_PET_CAST_SPELL, %s, spellid %u", GuidString(guid).c_str(), spellid);

    Creature* pet = who.GetMap()->GetAnyTypeCreature(guid);

    if (!pet || (guid != who.GetPetGuid() && guid != who.GetCharmGuid()))
    {
        sLog.outError("HandlePetCastSpellOpcode: %s isn't pet of %s .", GuidString(guid).c_str(), who.GetGuidStr().c_str());
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown PET spell id %i", spellid);
        return;
    }

    if (pet->GetCharmInfo() && pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
    {
        return;
    }

    if (!pet->HasSpell(spellid) || (cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
    {
        return;
    }

    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(pet);

    pet->clearUnitState(UNIT_STAT_MOVING);

    Spell* spell = new Spell(pet, spellInfo, false);
    spell->m_targets = targets;

    SpellCastResult result = spell->CheckPetCast(nullptr);
    if (result == SPELL_CAST_OK)
    {
        pet->AddCreatureSpellCooldown(spellid);
        if (pet->IsPet())
        {
            ((Pet*)pet)->CheckLearning(spellid);

            if (((Pet*)pet)->getPetType() == SUMMON_PET && (urand(0, 100) < 10))
            {
                pet->SendPetTalk((uint32)PET_TALK_SPECIAL_SPELL);
            }
            else
            {
                pet->SendPetAIReaction();
            }
        }

        spell->prepare(&(spell->m_targets));
    }
    else
    {
        pet->SendPetCastFail(spellid, result);
        if (!pet->HasSpellCooldown(spellid))
        {
            who.SendClearCooldown(spellid, pet);
        }

        spell->finish(false);
        delete spell;
    }
}

void WorldSession::SendPetNameInvalid(uint32 error, const std::string& name)
{
    WorldPacket data(SMSG_PET_NAME_INVALID, 0);
    SendPacket(&data);
}
