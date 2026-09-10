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

#include <random>
#include <utility>
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
#include <vector>
#include <list>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "Kinds.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Geometry/Vector3.h"
#include "Cast/Recipe/RecipeBook.h"

void Spell::EffectDispelMechanic(const cast::Operation& operation)
{
    if (!unitTarget)
    {
        return;
    }

    uint32 mechanic = operation.miscValue;

    Unit::SpellAuraHolderMap& Auras = unitTarget->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::iterator iter = Auras.begin(), next; iter != Auras.end(); iter = next)
    {
        next = iter;
        ++next;
        SpellEntry const* spell = iter->second->GetSpellProto();
        if (iter->second->HasMechanic(mechanic))
        {
            unitTarget->RemoveAuras(spell->ID);
            if (Auras.empty())
            {
                break;
            }
            else
            {
                next = Auras.begin();
            }
        }
    }
}

void Spell::EffectSummonDeadPet(const cast::Operation& )
{
    Player* _player = static_cast<Player*>(m_caster);

    if (!_player || damage < 0)
    {
        return;
    }

    Pet* pet = _player->GetPet();

    bool hadPet = true;

    if (pet)
    {
        if (pet->IsAlive())
        {
            return;
        }
    }
    else
    {
        Pet* newPet = new Pet;
        if (!newPet->LoadPetFromDB(_player))
        {
            delete newPet;
            return;
        }
        hadPet = false;
    }

    pet = _player->GetPet();
    if (!pet || pet->IsAlive())
    {
        return;
    }

    if (hadPet)
    {
        float px, py, pz;
        ClosePointNear(*_player, px, py, pz, pet->Where().Extent(), _player->Where().Extent());
        pet->NearTeleportTo(px, py, pz, PET_FOLLOW_ANGLE);
    }

    pet->SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
    pet->RemoveUnitFlag(UNIT_FLAG_SKINNABLE);
    pet->SetDeathState(ALIVE);
    pet->clearUnitState(UNIT_STAT_ALL_STATE);
    pet->SetHealth(uint32(pet->GetMaxHealth() * (float(damage) / 100)));

    pet->AIM_Initialize();

    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
}

void Spell::EffectDestroyAllTotems(const cast::Operation& )
{
    for (int slot = 0;  slot < MAX_TOTEM_SLOT; ++slot)
    {
        if (Totem* totem = m_caster->Retainers().TotemIn(TotemSlot(slot)))
        {
            totem->UnSummon();
        }
    }
}

void Spell::EffectDurabilityDamage(const cast::Operation& operation)
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    int32 slot = operation.miscValue;

    if (slot < 0)
    {
        ((Player*)unitTarget)->DurabilityPointsLossAll(damage, (slot < -1));
        return;
    }

    if (slot >= INVENTORY_SLOT_BAG_END)
    {
        return;
    }

    if (Item* item = ((Player*)unitTarget)->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
    {
        ((Player*)unitTarget)->DurabilityPointsLoss(item, damage);
    }
}

void Spell::EffectDurabilityDamagePCT(const cast::Operation& operation)
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    int32 slot = operation.miscValue;

    if (slot < 0)
    {
        ((Player*)unitTarget)->DurabilityLossAll(double(damage) / 100.0f, (slot < -1));
        return;
    }

    if (slot >= INVENTORY_SLOT_BAG_END)
    {
        return;
    }

    if (damage <= 0)
    {
        return;
    }

    if (Item* item = ((Player*)unitTarget)->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
    {
        ((Player*)unitTarget)->DurabilityLoss(item, double(damage) / 100.0f);
    }
}

void Spell::EffectModifyThreatPercent(const cast::Operation& )
{
    if (!unitTarget)
    {
        return;
    }

    unitTarget->GetThreatManager().modifyThreatPercent(m_caster, damage);
}

void Spell::EffectTransmitted(const cast::Operation& operation)
{
    uint32 name_id = operation.miscValue;

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(name_id);

    if (!goinfo)
    {
        sLog.outErrorDb("Gameobject (Entry: %u) not exist and not created at spell (ID: %u) cast", name_id, m_spellInfo->ID);
        return;
    }

    float fx, fy, fz;

    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        m_targets.getDestination(fx, fy, fz);
    }

    else if (operation.radiusIndex && m_spellInfo->Speed == 0)
    {
        float dis = GetSpellRadius(sSpellRadiusStore.LookupEntry(operation.radiusIndex));
        ClosePointNear(*m_caster, fx, fy, fz, DEFAULT_WORLD_OBJECT_SIZE, dis);
    }
    else
    {
        float min_dis = Recipe().Takes().rangeMin;
        float max_dis = Recipe().Takes().rangeMax;
        float dis = rand_norm_f() * (max_dis - min_dis) + min_dis;

        if (goinfo->type == GAMEOBJECT_TYPE_FISHINGNODE)
        {

            float max_angle = (max_dis - min_dis) / (max_dis + m_caster->Where().Extent());
            float angle_offset = max_angle * (rand_norm_f() - 0.5f);
            const Geometry::Vector3 near_ = PointNear(*m_caster, dis + m_caster->Where().Extent(), m_caster->Where().Facing() + angle_offset);
            fx = near_.x;
            fy = near_.y;

            GridMapLiquidData liqData;
            if (!m_caster->GetMap()->GetTerrain()->IsInWater(fx, fy, m_caster->Where().Z() + 1.f, &liqData))
            {
                SendCastResult(SPELL_FAILED_NOT_FISHABLE);
                SendChannelUpdate(0);
                return;
            }

            fz = liqData.level;

            if (!HasLineOfSight(*m_caster, Geometry::Vector3(fx, fy, fz)))
            {
                SendCastResult(SPELL_FAILED_LINE_OF_SIGHT);
                SendChannelUpdate(0);
                return;
            }
        }
        else
        {
            ClosePointNear(*m_caster, fx, fy, fz, DEFAULT_WORLD_OBJECT_SIZE, dis);
        }
    }

    Map* cMap = m_caster->GetMap();

    if (goinfo->type == GAMEOBJECT_TYPE_SUMMONING_RITUAL)
    {
        fx = m_caster->Where().X();
        fy = m_caster->Where().Y();
        fz = m_caster->Where().Z();
    }

    GameObject* pGameObj = new GameObject;

    if (!pGameObj->Create(cMap->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), name_id, cMap,
        fx, fy, fz, m_caster->Where().Facing()))
    {
        delete pGameObj;
        return;
    }

    int32 duration = Recipe().DurationMs();

    switch (goinfo->type)
    {
        case GAMEOBJECT_TYPE_FISHINGNODE:
        {
            m_caster->SetChannelObjectGuid(pGameObj->GetObjectGuid());
            m_caster->Conjured().AddObject(pGameObj);

            int32 lastSec = 0;
            switch (urand(0, 3))
            {
                case 0: lastSec =  3; break;
                case 1: lastSec =  7; break;
                case 2: lastSec = 13; break;
                case 3: lastSec = 17; break;
            }

            duration = duration - lastSec * IN_MILLISECONDS + FISHING_BOBBER_READY_TIME * IN_MILLISECONDS;
            break;
        }
        case GAMEOBJECT_TYPE_SUMMONING_RITUAL:
        {
            if (IsPlayer(m_caster))
            {
                pGameObj->Behaves<RitualBehaviour>()->Tally().UsedBy(m_caster->GetObjectGuid());
                m_caster->Conjured().AddObject(pGameObj);
            }
            break;
        }
        case GAMEOBJECT_TYPE_SPELLCASTER:
        {
            m_caster->Conjured().AddObject(pGameObj);
            break;
        }
        case GAMEOBJECT_TYPE_FISHINGHOLE:
        case GAMEOBJECT_TYPE_CHEST:
        default:
            break;
    }

    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);

    pGameObj->SetOwnerGuid(m_caster->GetObjectGuid());

    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel());
    pGameObj->SetSpellId(m_spellInfo->ID);

    DEBUG_LOG("AddObject at SpellEfects.cpp EffectTransmitted");

    cMap->Add(pGameObj);

    pGameObj->SummonLinkedTrapIfAny();

    if (IsCreature(m_caster) && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->JustSummoned(pGameObj);
    }
    if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned(pGameObj);
    }
}

void Spell::EffectSkill(const cast::Operation& )
{
    DEBUG_LOG("WORLD: SkillEFFECT");
}

void Spell::EffectSpiritHeal(const cast::Operation& )
{

    if (!unitTarget || unitTarget->IsAlive())
    {
        return;
    }
    if (!IsPlayer(unitTarget))
    {
        return;
    }
    if (!unitTarget->IsInWorld())
    {
        return;
    }
    if (m_spellInfo->ID == 22012 && !unitTarget->HasAura(2584))
    {
        return;
    }

    ((Player*)unitTarget)->ResurrectPlayer(1.0f);
    ((Player*)unitTarget)->SpawnCorpseBones();
}

void Spell::EffectSkinPlayerCorpse(const cast::Operation& )
{
    DEBUG_LOG("Effect: SkinPlayerCorpse");
    if ((!IsPlayer(m_caster)) || (!IsPlayer(unitTarget)) || (unitTarget->IsAlive()))
    {
        return;
    }

    ((Player*)unitTarget)->RemovedInsignia((Player*)m_caster);
}

void Spell::EffectBind(const cast::Operation& )
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    Player* player = (Player*)unitTarget;

    uint32 area_id;
    Geometry::Placement loc;
    loc = Geometry::Placement::Somewhere(player->GetMapId(), Geometry::Vector3(player->Where().X(), player->Where().Y(), player->Where().Z()), player->Where().Facing());
    area_id = player->GetTerrain()->GetAreaId(player->Where().X(), player->Where().Y(), player->Where().Z());

    player->SetHomebindToLocation(loc, area_id);

    WorldPacket data(SMSG_BINDPOINTUPDATE, (4 + 4 + 4 + 4 + 4));
    data << float(loc.X());
    data << float(loc.Y());
    data << float(loc.Z());
    data << uint32(loc.MapId());
    data << uint32(area_id);
    player->SendDirectMessage(&data);

    DEBUG_LOG("New Home Position X is %f", loc.X());
    DEBUG_LOG("New Home Position Y is %f", loc.Y());
    DEBUG_LOG("New Home Position Z is %f", loc.Z());
    DEBUG_LOG("New Home MapId is %u", loc.MapId());
    DEBUG_LOG("New Home AreaId is %u", area_id);

    data.Initialize(SMSG_PLAYERBOUND, 8 + 4);
    data << m_caster->GetObjectGuid();
    data << uint32(area_id);
    player->SendDirectMessage(&data);
}
