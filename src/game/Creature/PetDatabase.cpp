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

#include <sstream>
#include <cstdlib>
#include "Pet.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "CreatureRecord.h"
#include "SpellMgr.h"
#include "Formulas.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "Unit.h"
#include "Util.h"
#include "Transports.h"
#include "Movement/Spline/MoveSpline.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Cast/Recipe/RecipeBook.h"

bool Pet::LoadPetFromDB(Player* owner, uint32 petentry, uint32 petnumber, bool current)
{
    m_loading = true;

    uint32 ownerid = owner->GetGUIDLow();

    QueryResult* result;

    if (petnumber)
    {

        result = CharacterDatabase.PQuery("SELECT `id`, `entry`, `owner`, `modelid`, `level`, `exp`, `Reactstate`, `loyaltypoints`, `loyalty`, `trainpoint`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `curhappiness`, `abdata`, `TeachSpelldata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType` "
            "FROM `character_pet` WHERE `owner` = '%u' AND `id` = '%u'",
            ownerid, petnumber);
    }
    else if (current)
    {

        result = CharacterDatabase.PQuery("SELECT `id`, `entry`, `owner`, `modelid`, `level`, `exp`, `Reactstate`, `loyaltypoints`, `loyalty`, `trainpoint`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `curhappiness`, `abdata`, `TeachSpelldata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType` "
            "FROM `character_pet` WHERE `owner` = '%u' AND `slot` = '%u'",
            ownerid, PET_SAVE_AS_CURRENT);
    }
    else if (petentry)
    {

        result = CharacterDatabase.PQuery("SELECT `id`, `entry`, `owner`, `modelid`, `level`, `exp`, `Reactstate`, `loyaltypoints`, `loyalty`, `trainpoint`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `curhappiness`, `abdata`, `TeachSpelldata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType` "
            "FROM `character_pet` WHERE `owner` = '%u' AND `entry` = '%u' AND (`slot` = '%u' OR `slot` > '%u') ",
            ownerid, petentry, PET_SAVE_AS_CURRENT, PET_SAVE_LAST_STABLE_SLOT);
    }
    else
    {

        result = CharacterDatabase.PQuery("SELECT `id`, `entry`, `owner`, `modelid`, `level`, `exp`, `Reactstate`, `loyaltypoints`, `loyalty`, `trainpoint`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `curhappiness`, `abdata`, `TeachSpelldata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType` "
            "FROM `character_pet` WHERE `owner` = '%u' AND (`slot` = '%u' OR `slot` > '%u') ",
            ownerid, PET_SAVE_AS_CURRENT, PET_SAVE_LAST_STABLE_SLOT);
    }

    if (!result)
    {
        return false;
    }

    Field* fields = result->Fetch();

    petentry = fields[1].GetUInt32();
    if (!petentry)
    {
        delete result;
        return false;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        sLog.outError("Pet entry %u does not exist but used at pet load (owner: %s).", petentry, owner->GetGuidStr().c_str());
        delete result;
        return false;
    }

    uint32 summon_spell_id = fields[21].GetUInt32();
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(summon_spell_id);

    bool is_temporary_summoned = spellInfo && cast::RecipeOf(*spellInfo).DurationMs() > 0;

    if (current && is_temporary_summoned)
    {
        delete result;
        return false;
    }

    PetType pet_type = PetType(fields[22].GetUInt8());
    if (pet_type == HUNTER_PET)
    {
        if (!CreatureRecord(*creatureInfo).IsTameable())
        {
            delete result;
            return false;
        }
    }

    uint32 pet_number = fields[0].GetUInt32();

    if (current && owner->IsPetNeedBeTemporaryUnsummoned())
    {
        owner->SetTemporaryUnsummonedPetNumber(pet_number);
        delete result;
        return false;
    }

    Map* map = owner->GetMap();

    CreatureCreatePos pos(owner, owner->Where().Facing(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    uint32 guid = pos.GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);
    if (!Create(guid, pos, creatureInfo, pet_number))
    {
        delete result;
        return false;
    }

    setPetType(pet_type);
    setFaction(owner->getFaction());
    SetUInt32Value(UNIT_CREATED_BY_SPELL, summon_spell_id);

    CreatureInfo const* cinfo = GetCreatureInfo();
    if (cinfo->CreatureType == CREATURE_TYPE_CRITTER)
    {
        pos.GetMap()->Add((Creature*)this);
        AIM_Initialize();
        delete result;
        return true;
    }

    m_charmInfo->SetPetNumber(pet_number, IsPermanentPetFor(owner));

    SetOwnerGuid(owner->GetObjectGuid());
    SetDisplayId(fields[3].GetUInt32());
    SetNativeDisplayId(fields[3].GetUInt32());
    uint32 petlevel = fields[4].GetUInt32();
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    SetName(fields[11].GetString());

    switch (getPetType())
    {
        case SUMMON_PET:
            petlevel = owner->getLevel();
            break;
        case HUNTER_PET:

            SetLoyaltyByte(fields[8].GetUInt32());

            SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED | UNIT_FLAG_ABANDON);

            if (!fields[12].GetBool())
            {
                SetUnitFlag(UNIT_FLAG_RENAME);
            }

            SetTP(fields[9].GetInt32());
            SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
            SetPower(POWER_HAPPINESS, fields[15].GetUInt32());
            SetPowerType(POWER_FOCUS);
            break;
        default:
            sLog.outError("Pet have incorrect type (%u) for pet loading.", getPetType());
    }

    if (owner->IsPvP())
    {
        SetPvP(true);
    }

    Tallied().Ready(true);
    InitStatsForLevel(petlevel);
    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(nullptr)));
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, fields[5].GetUInt32());
    SetCreatorGuid(owner->GetObjectGuid());

    m_charmInfo->SetReactState(ReactStates(fields[6].GetUInt8()));
    m_loyaltyPoints = fields[7].GetInt32();

    uint32 savedhealth = fields[13].GetUInt32();
    uint32 savedpower = fields[14].GetUInt32();

    if (fields[10].GetUInt32() != 0)
    {
        CharacterDatabase.BeginTransaction();

        static SqlStatementID id_1;
        static SqlStatementID id_2;

        SqlStatement stmt = CharacterDatabase.CreateStatement(id_1, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `slot` = ? AND `id` <> ?");
        stmt.PExecute(uint32(PET_SAVE_NOT_IN_SLOT), ownerid, uint32(PET_SAVE_AS_CURRENT), m_charmInfo->GetPetNumber());

        stmt = CharacterDatabase.CreateStatement(id_2, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `id` = ?");
        stmt.PExecute(uint32(PET_SAVE_AS_CURRENT), ownerid, m_charmInfo->GetPetNumber());

        CharacterDatabase.CommitTransaction();
    }

    if (!is_temporary_summoned)
    {
        m_charmInfo->LoadPetActionBar(fields[16].GetCppString());

        Tokens tokens = StrSplit(fields[17].GetString(), " ");
        Tokens::const_iterator iter;
        int index;
        for (iter = tokens.begin(), index = 0; index < 4; ++iter, ++index)
        {
            uint32 tmp = std::strtoul((*iter).c_str(), nullptr, 10);

            ++iter;

            if (tmp)
            {
                AddTeachSpell(tmp, std::strtoul((*iter).c_str(), nullptr, 10));
            }
            else
            {
                break;
            }
        }
    }

    uint32 timediff = uint32(time(nullptr) - fields[18].GetUInt64());

    delete result;

    _LoadAuras(timediff);

    if (is_temporary_summoned)
    {

        InitPetCreateSpells();
    }
    else
    {
        LearnPetPassives();
        CastPetAuras(current);
        CastOwnerTalentAuras();
    }

    Powers powerType = GetPowerType();

    if (getPetType() == SUMMON_PET && !current)
    {
        SetHealth(GetMaxHealth());
        SetPower(powerType, GetMaxPower(powerType));
    }
    else
    {
        SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
        SetPower(powerType, savedpower > GetMaxPower(powerType) ? GetMaxPower(powerType) : savedpower);

        if (getPetType() == HUNTER_PET && savedhealth == 0)
        {
            SetDeathState(JUST_DIED);
        }

    }

    Player* p_owner =IsPlayer(owner) ? (Player*)owner : nullptr;

    map->Add((Creature*)this);
    AIM_Initialize();

    _LoadSpells();
    CleanupActionBar();

    _LoadSpellCooldowns();

    owner->SetPet(this);
    DEBUG_LOG("New Pet has guid %u", GetGUIDLow());

    if (p_owner)
    {
        p_owner->PetSpellInitialize();
        if (p_owner->GetGroup())
        {
            p_owner->SetGroupUpdateFlag(GROUP_UPDATE_PET);
        }
    }

    m_loading = false;

    SynchronizeLevelWithOwner();
    return true;
}

void Pet::SavePetToDB(PetSaveMode mode)
{
    if (!GetEntry())
    {
        return;
    }

    if (!isControlled())
    {
        return;
    }

    if (!(GetOwnerGuid() != 0 && GuidHigh(GetOwnerGuid()) == HIGHGUID_PLAYER))
    {
        return;
    }

    Player* pOwner = (Player*)GetOwner();
    if (!pOwner)
    {
        return;
    }

    if (mode >= PET_SAVE_AS_CURRENT)
    {

        if (mode == PET_SAVE_REAGENTS)
        {
            mode = PET_SAVE_NOT_IN_SLOT;
        }

        else if (mode == PET_SAVE_AS_CURRENT && pOwner->GetTemporaryUnsummonedPetNumber() &&
            pOwner->GetTemporaryUnsummonedPetNumber() != m_charmInfo->GetPetNumber())
        {

            if (getPetType() == HUNTER_PET)
            {
                return;
            }

            mode = PET_SAVE_NOT_IN_SLOT;
        }

        uint32 curhealth = GetHealth();

        if (getPetType() != HUNTER_PET)
        {
            if (curhealth < 1)
            {
                curhealth = 1;
            }
        }

        uint32 curpower = GetPower(GetPowerType());

        if (mode != PET_SAVE_AS_CURRENT)
        {
            RemoveAllAuras();
        }

        CharacterDatabase.BeginTransaction();
        _SaveSpells();
        _SaveSpellCooldowns();
        _SaveAuras();

        uint32 ownerLow = GuidCounter(GetOwnerGuid());

        static SqlStatementID delPet ;
        static SqlStatementID insPet ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM `character_pet` WHERE `owner` = ? AND `id` = ?");
        stmt.PExecute(ownerLow, m_charmInfo->GetPetNumber());

        if (mode <= PET_SAVE_LAST_STABLE_SLOT)
        {
            static SqlStatementID updPet ;

            stmt = CharacterDatabase.CreateStatement(updPet, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `slot` = ?");
            stmt.PExecute(uint32(PET_SAVE_NOT_IN_SLOT), ownerLow, uint32(mode));
        }

        if (getPetType() == HUNTER_PET && (mode == PET_SAVE_AS_CURRENT || mode > PET_SAVE_LAST_STABLE_SLOT))
        {
            static SqlStatementID del ;

            stmt = CharacterDatabase.CreateStatement(del, "DELETE FROM `character_pet` WHERE `owner` = ? AND (`slot` = ? OR `slot` > ?)");
            stmt.PExecute(ownerLow, uint32(PET_SAVE_AS_CURRENT), uint32(PET_SAVE_LAST_STABLE_SLOT));
        }

        SqlStatement savePet = CharacterDatabase.CreateStatement(insPet, "INSERT INTO `character_pet` "
            "(`id`,`entry`,`owner`,`modelid`,`level`,`exp`,`Reactstate`,`loyaltypoints`,`loyalty`,`trainpoint`,`slot`,`name`,`renamed`,`curhealth`,`curmana`,`curhappiness`,`abdata`,`TeachSpelldata`,`savetime`,`resettalents_cost`,`resettalents_time`,`CreatedBySpell`,`PetType`) "
            "VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        savePet.addUInt32(m_charmInfo->GetPetNumber());
        savePet.addUInt32(GetEntry());
        savePet.addUInt32(ownerLow);
        savePet.addUInt32(GetNativeDisplayId());
        savePet.addUInt32(getLevel());
        savePet.addUInt32(GetUInt32Value(UNIT_FIELD_PETEXPERIENCE));
        savePet.addUInt32(uint32(m_charmInfo->GetReactState()));
        savePet.addInt32(m_loyaltyPoints);
        savePet.addUInt32(GetLoyaltyLevel());
        savePet.addInt32(m_TrainingPoints);
        savePet.addUInt32(uint32(mode));
        savePet.addString(m_name.c_str());
        savePet.addUInt32(uint32(HasUnitFlag(UNIT_FLAG_RENAME) ? 0 : 1));
        savePet.addUInt32((curhealth));
        savePet.addUInt32(curpower);
        savePet.addUInt32(GetPower(POWER_HAPPINESS));

        std::ostringstream ss;
        for (uint32 i = ACTION_BAR_INDEX_START; i < ACTION_BAR_INDEX_END; ++i)
        {
            ss << uint32(m_charmInfo->GetActionBarEntry(i)->GetType()) << " "
               << uint32(m_charmInfo->GetActionBarEntry(i)->GetAction()) << " ";
        };
        savePet.addString(ss);

        {
            int i = 0;
            for (TeachSpellMap::const_iterator itr = m_teachspells.begin(); i < 4 && itr != m_teachspells.end(); ++i, ++itr)
            {
                ss << itr->first << " " << itr->second << " ";
            }
            for (; i < 4; ++i)
            {
                ss << uint32(0) << " " << uint32(0) << " ";
            }
        }
        savePet.addString(ss);

        savePet.addUInt64(uint64(time(nullptr)));
        savePet.addUInt32(uint32(m_resetTalentsCost));
        savePet.addUInt64(uint64(m_resetTalentsTime));
        savePet.addUInt32(GetUInt32Value(UNIT_CREATED_BY_SPELL));
        savePet.addUInt32(uint32(getPetType()));

        savePet.Execute();
        CharacterDatabase.CommitTransaction();
    }
    else
    {
        RemoveAllAuras(AURA_REMOVE_BY_DELETE);
        DeleteFromDB(m_charmInfo->GetPetNumber());
    }
}

void Pet::DeleteFromDB(uint32 guidlow, bool separate_transaction)
{
    if (separate_transaction)
    {
        CharacterDatabase.BeginTransaction();
    }

    static SqlStatementID delPet ;
    static SqlStatementID delAuras ;
    static SqlStatementID delSpells ;
    static SqlStatementID delSpellCD ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM `character_pet` WHERE `id` = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM `pet_aura` WHERE `guid` = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM `pet_spell` WHERE `guid` = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM `pet_spell_cooldown` WHERE `guid` = ?");
    stmt.PExecute(guidlow);

    if (separate_transaction)
    {
        CharacterDatabase.CommitTransaction();
    }
}
