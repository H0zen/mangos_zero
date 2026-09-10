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
#include "Utilities/Errors.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include "Utilities/MathDefines.h"
#include "Creature.h"
#include "LivingWorldAnchorPolicy.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "SQLStorages.h"
#include "SpellMgr.h"
#include "GossipDef.h"
#include "QuestBond.h"
#include "Player.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "Opcodes.h"
#include "Log.h"
#include "LootMgr.h"
#include "TransportMap.h"
#include "CreatureAI.h"
#include "CreatureAISelector.h"
#include "InstanceData.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Spell.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "CreatureLinkingMgr.h"
#include "SpawnRecord.h"
#include "DisableMgr.h"
#include "MovementGenerator.h"

#include "Policies/Singleton.h"
#include "PlayerRegistry.h"
#include "Corpse.h"
#include "Cast/Recipe/RecipeBook.h"

TrainerSpell const* TrainerSpellData::Find(uint32 spell_id) const
{
    TrainerSpellMap::const_iterator itr = spellList.find(spell_id);
    if (itr != spellList.end())
    {
        return &itr->second;
    }

    return nullptr;
}

bool VendorItemData::RemoveItem(uint32 item_id)
{
    for (VendorItemList::iterator i = m_items.begin(); i != m_items.end(); ++i)
    {
        if ((*i)->item == item_id)
        {
            m_items.erase(i);
            return true;
        }
    }
    return false;
}

size_t VendorItemData::FindItemSlot(uint32 item_id) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i]->item == item_id)
        {
            return i;
        }
    }
    return m_items.size();
}

VendorItem const* VendorItemData::FindItem(uint32 item_id) const
{
    for (VendorItemList::const_iterator i = m_items.begin(); i != m_items.end(); ++i)
    {

        if ((*i)->item == item_id)
        {
            return *i;
        }
    }
    return nullptr;
}

bool ForcedDespawnDelayEvent::Execute(uint64 , uint32 )
{
    m_owner.ForcedDespawn();
    return true;
}

void CreatureCreatePos::SelectFinalPoint(Creature* cr)
{

    if (!m_closeObject)
    {
        return;
    }

    if (m_dist == 0.0f)
    {
        m_pos.x = m_closeObject->Where().X();
        m_pos.y = m_closeObject->Where().Y();
        m_pos.z = m_closeObject->Where().Z();
        return;
    }

    if (TransportMap* hull = m_closeObject->GetMap()->AsTransport())
    {
        const float distance2d = m_dist + m_closeObject->Where().Extent() +
                                 cr->Where().Extent();

        if (const auto spot = hull->FreeSpotNear(*m_closeObject, distance2d, m_angle))
        {
            m_pos.x = spot->x;
            m_pos.y = spot->y;
            m_pos.z = spot->z;
            return;
        }
    }

    ClosePointNear(*m_closeObject, m_pos.x, m_pos.y, m_pos.z, cr->Where().Extent(), m_dist, m_angle);
}

bool CreatureCreatePos::PlaceOn(Creature* cr) const
{
    cr->Place().MoveTo(m_pos.x, m_pos.y, m_pos.z, m_pos.o);

    cr->m_movementInfo.ChangePosition(m_pos.x, m_pos.y, m_pos.z, m_pos.o);

    if (!IsPlaceable(*cr))
    {
        sLog.outError("%s not created. Suggested coordinates isn't valid (X: %f Y: %f)", cr->GetGuidStr().c_str(), cr->Where().X(), cr->Where().Y());
        return false;
    }

    return true;
}

Creature::Creature(CreatureSubtype subtype) : Unit(),
    i_AI(nullptr),
    loot(this),
    m_sheet(*this), m_links(*this), m_pace(*this)
{
    m_subtype = subtype;

    m_recovery.NextIn(200);

    Stationed().Anchor(Geometry::Vector3());

    SetWalk(true, true);
}

Creature::~Creature()
{
    CleanupsBeforeDelete();

    m_vendorItemCounts.clear();

    delete i_AI;
    i_AI = nullptr;
}

void Creature::AddToWorld()
{

    if (!IsInWorld() && (GuidHigh(GetObjectGuid()) == HIGHGUID_UNIT))
    {
        GetMap()->GetObjectsStore().insert<Creature>(GetObjectGuid(), (Creature*)this);
    }

    Unit::AddToWorld();

    if (TransportMap* hull = GetMap()->AsTransport())
    {
        hull->EnlistCrew(this);
    }

    if ((GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_ACTIVE) ||
        IsLivingWorldAnchor(GetCreatureInfo(), sMapStore.LookupEntry(GetMapId()),
                            sWorld.getConfig(CONFIG_UINT32_LIVINGWORLD_ANCHOR_MASK)) ||
        (GetLivingWorldDefenderCategory(GetCreatureInfo(), sMapStore.LookupEntry(GetMapId()),
                                        GetDefaultMovementType() == WAYPOINT_MOTION_TYPE)
            & sWorld.getConfig(CONFIG_UINT32_LIVINGWORLD_ANCHOR_MASK)))
    {
        SetActiveObjectState(true);
    }

}

void Creature::RemoveFromWorld()
{

    if (IsInWorld() && GetMap())
    {
        if (TransportMap* hull = GetMap()->AsTransport())
        {
            hull->DelistCrew(this);
        }
    }

    if (IsInWorld() && (GuidHigh(GetObjectGuid()) == HIGHGUID_UNIT))
    {
        GetMap()->GetObjectsStore().erase<Creature>(GetObjectGuid(), (Creature*)nullptr);
    }

    Unit::RemoveFromWorld();
}

void Creature::CleanupsBeforeDelete()
{
    if (Map* on = FindMap())
    {
        if (TransportMap* hull = on->AsTransport())
        {
            hull->DelistCrew(this);
        }
    }

    Unit::CleanupsBeforeDelete();
}

void Creature::RemoveCorpse(bool inPlace)
{
    if (!inPlace)
    {

        if (uint16 poolid = sPoolMgr.IsPartOfAPool<Creature>(GetGUIDLow()))
        {
            sPoolMgr.UpdatePool<Creature>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow());
        }
        if (!IsInWorld())
        {
            return;
        }
    }

    if ((GetDeathState() != CORPSE && !Watch().DeadByDefault()) || (GetDeathState() != ALIVE && Watch().DeadByDefault()))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Removing corpse of %s ", GetGuidStr().c_str());

    Watch().CorpseGoesAt(time(nullptr));
    SetDeathState(DEAD);
    UpdateObjectVisibility();

    Claim().StopRoll();

    loot.clear();

    Watch().KilledAt(0);
    Taking().Opened(false);
    Taking().AssignedTo(0);
    m_claim.StakedBy(nullptr);

    RemoveDynFlag(UNIT_DYNFLAG_TAPPED);

    uint32 respawnDelay = 0;

    if (AI())
    {
        AI()->CorpseRemoved(respawnDelay);
    }

    m_links.Despawned();

    if (InstanceData* mapInstance = GetInstanceData())
    {
        mapInstance->OnCreatureDespawn(this);
    }

    if (respawnDelay)
    {
        Watch().RespawnsAt(time(nullptr) + respawnDelay);
    }

    float x, y, z, o;
    const Geometry::Vector3 home = Spawn().Pos();
    x = home.x;
    y = home.y;
    z = home.z;
    o = Spawn().Facing();
    GetMap()->CreatureRelocation(this, x, y, z, o);

    UnitVisibility currentVis = GetVisibility();
    SetVisibility(VISIBILITY_REMOVE_CORPSE);
    UpdateObjectVisibility();
    SetVisibility(currentVis);
}

bool Creature::InitEntry(uint32 Entry, Team team, CreatureData const* data , GameEventCreatureData const* eventData )
{

    if (eventData && eventData->entry_id)
    {
        Entry = eventData->entry_id;
    }

    CreatureInfo const* normalInfo = ObjectMgr::GetCreatureTemplate(Entry);
    if (!normalInfo)
    {
        sLog.outErrorDb("Creature::UpdateEntry creature entry %u does not exist.", Entry);
        return false;
    }

    CreatureInfo const* cinfo = normalInfo;

    SetEntry(Entry);
    m_creatureInfo = cinfo;

    SetObjectScale(cinfo->Scale);

    SetRace(0);

    SetClass(uint8(cinfo->UnitClass));

    uint32 display_id = ChooseDisplayId(GetCreatureInfo(), data, eventData);
    if (!display_id)
    {
        sLog.outErrorDb("Creature (Entry: %u) has no model defined in table `creature_template`, can't load.", Entry);
        return false;
    }

    CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelRandomGender(display_id);
    if (!minfo)
    {
        sLog.outErrorDb("Creature (Entry: %u) has no model info defined in table `creature_model_info`, can't load.", Entry);
        return false;
    }

    display_id = minfo->modelid;

    SetNativeDisplayId(display_id);

    if (team == ALLIANCE && cinfo->CreatureType == CREATURE_TYPE_TOTEM)
    {
        uint32 modelid_tmp = sObjectMgr.GetCreatureModelOtherTeamModel(display_id);
        display_id = modelid_tmp ? modelid_tmp : display_id;
    }

    SetDisplayId(display_id);

    SetGender(minfo->gender);

    switch (cinfo->UnitClass)
    {
        case CLASS_WARRIOR:
            SetPowerType(POWER_RAGE);
            break;
        case CLASS_PALADIN:
        case CLASS_MAGE:
            SetPowerType(POWER_MANA);
            break;
        case CLASS_ROGUE:
            SetPowerType(POWER_ENERGY);
            break;
        default:
            sLog.outErrorDb("Creature (Entry: %u) has unhandled unit class. Power type will not be set!", Entry);
            break;
    }

    if (eventData && eventData->equipment_id)
    {
        LoadEquipment(eventData->equipment_id);
    }
    else if (!data || data->equipmentId == 0)
    {

        LoadEquipment(cinfo->EquipmentTemplateId);
    }
    else if (data && data->equipmentId != -1)
    {

        LoadEquipment(data->equipmentId);
    }

    SetName(normalInfo->Name);

    SetCastSpeedMod(1.0f);

    Pacing().Reckon(MOVE_WALK, false);
    Pacing().Reckon(MOVE_RUN,  false);

    SetLevitate(cinfo->InhabitType & INHABIT_AIR);

    if (cinfo->InhabitType & INHABIT_WATER &&
        data &&
        !(cinfo->ExtraFlags & CREATURE_FLAG_EXTRA_WALK_IN_WATER) &&
        GetMap()->GetTerrain()->IsSwimmable(data->posX, data->posY, data->posZ, minfo->bounding_radius))
        m_movementInfo.AddMovementFlag(MOVEFLAG_SWIMMING);

    Stationed().Wander(MovementGeneratorType(cinfo->MovementType));

    return true;
}

bool Creature::UpdateEntry(uint32 Entry, Team team, const CreatureData* data , GameEventCreatureData const* eventData , bool preserveHPAndPower )
{
    if (!InitEntry(Entry, team, data, eventData))
    {
        return false;
    }

    SetSheath(SHEATH_STATE_MELEE);

    if (preserveHPAndPower)
    {
        uint32 healthPercent = GetHealthPercent();
        SelectLevel();
        SetHealthPercent(healthPercent);
    }
    else
    {
        SelectLevel();
    }

    if (team == HORDE)
    {
        setFaction(GetCreatureInfo()->FactionHorde);
    }
    else
    {
        setFaction(GetCreatureInfo()->FactionAlliance);
    }

    SetUInt32Value(UNIT_NPC_FLAGS, GetCreatureInfo()->NpcFlags);

    uint32 attackTimer = GetCreatureInfo()->MeleeBaseAttackTime;

    SetAttackTime(BASE_ATTACK, attackTimer);
    SetAttackTime(OFF_ATTACK, attackTimer - attackTimer / 4);
    SetAttackTime(RANGED_ATTACK, GetCreatureInfo()->RangedBaseAttackTime);

    uint32 unitFlags = GetCreatureInfo()->UnitFlags;

    if (HasUnitFlag(UNIT_FLAG_IN_COMBAT))
    {
        unitFlags |= UNIT_FLAG_IN_COMBAT;
    }

    if (m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) && (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_HAVE_NO_SWIM_ANIMATION) == 0)
    {
        unitFlags |= UNIT_FLAG_UNK_15;
    }
    else
    {
        unitFlags &= ~UNIT_FLAG_UNK_15;
    }

    SetUInt32Value(UNIT_FIELD_FLAGS, unitFlags);

    uint32 dynFlags = GetUInt32Value(UNIT_DYNAMIC_FLAGS);
    SetUInt32Value(UNIT_DYNAMIC_FLAGS, dynFlags ? dynFlags : GetCreatureInfo()->DynamicFlags);

    Tallied().Value(UNIT_MOD_ARMOR, BASE_VALUE, float(GetCreatureInfo()->Armor));
    Tallied().Value(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(GetCreatureInfo()->ResistanceHoly));
    Tallied().Value(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(GetCreatureInfo()->ResistanceFire));
    Tallied().Value(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(GetCreatureInfo()->ResistanceNature));
    Tallied().Value(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(GetCreatureInfo()->ResistanceFrost));
    Tallied().Value(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(GetCreatureInfo()->ResistanceShadow));
    Tallied().Value(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(GetCreatureInfo()->ResistanceArcane));

    Tallied().Ready(true);
    Sheet().Everything();

    if (FactionTemplateEntry const* factionTemplate = sFactionTemplateStore.LookupEntry(GetCreatureInfo()->FactionAlliance))
    {
        if (factionTemplate->Flags & FACTION_TEMPLATE_FLAG_PVP)
        {
            SetPvP(true);
        }
        else
        {
            if (!IsRacialLeader())
            {
                SetPvP(false);
            }
        }
    }

    CreatureTemplateSpells const* templateSpells = sCreatureTemplateSpellsStorage.LookupEntry<CreatureTemplateSpells>(GetCreatureInfo()->Entry);
    if (!templateSpells)
    {
        templateSpells = sCreatureTemplateSpellsStorage.LookupEntry<CreatureTemplateSpells>(GetEntry());
    }

    if (templateSpells)
    {
        for (int i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            Knowing().Slot(i, templateSpells->spells[i]);
        }
    }

    if (eventData)
    {
        ApplyGameEventSpells(eventData, true);
    }

    return true;
}

uint32 Creature::ChooseDisplayId(const CreatureInfo* cinfo, const CreatureData* data , GameEventCreatureData const* eventData )
{

    if (eventData && eventData->modelid)
    {
        return eventData->modelid;
    }

    if (data && data->modelid_override)
    {
        return data->modelid_override;
    }

    uint32 display_id = 0;

    if (!cinfo->ModelId[1])
    {
        display_id = cinfo->ModelId[0];
    }
    else if (!cinfo->ModelId[2])
    {
        display_id = cinfo->ModelId[urand(0, 1)];
    }
    else if (!cinfo->ModelId[3])
    {
        display_id = cinfo->ModelId[urand(0, 2)];
    }
    else
    {
        display_id = cinfo->ModelId[urand(0, 3)];
    }

    if (!display_id)
    {
        sLog.outErrorDb("Call customer support, ChooseDisplayId can not select native model for creature entry %u, model from creature entry 1 will be used instead.", cinfo->Entry);

        if (const CreatureInfo* creatureDefault = ObjectMgr::GetCreatureTemplate(1))
        {
            display_id = creatureDefault->ModelId[0];
        }
    }

    return display_id;
}

void Creature::Update(uint32 update_diff, uint32 diff)
{
    switch (m_deathState)
    {
        case JUST_ALIVED:

            sLog.outError("Creature (GUIDLow: %u Entry: %u ) in wrong state: JUST_ALIVED (4)", GetGUIDLow(), GetEntry());
            break;
        case JUST_DIED:

            sLog.outError("Creature (GUIDLow: %u Entry: %u ) in wrong state: JUST_DEAD (1)", GetGUIDLow(), GetEntry());
            break;
        case DEAD:
        {
            if (Watch().RespawnsAt() <= time(nullptr) && m_links.MayRespawn())
            {
                DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Respawning...");
                Watch().RespawnsAt(0);
                Watch().AggroDelay(sWorld.getConfig(CONFIG_UINT32_CREATURE_RESPAWN_AGGRO_DELAY));
                Taking().PocketsPicked(false);
                Taking().BodyTaken(false);
                Taking().Skinned(false);

                RemoveAllAuras();

                if (m_originalEntry != GetEntry())
                {

                    GameEventCreatureData const* eventData = sGameEventMgr.GetCreatureUpdateDataForActiveEvent(GetGUIDLow());
                    UpdateEntry(m_originalEntry, TEAM_NONE, nullptr, eventData);
                }

                CreatureInfo const* cinfo = GetCreatureInfo();

                SelectLevel();
                Sheet().Everything();
                SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
                if (Watch().DeadByDefault())
                {
                    SetDeathState(JUST_DIED);
                    SetHealth(0);
                    i_motionMaster.Clear();
                    clearUnitState(UNIT_STAT_ALL_STATE);
                    LoadCreatureAddon(true);
                }
                else
                {
                    SetDeathState(JUST_ALIVED);
                }

                if (AI())
                {
                    AI()->JustRespawned();
                }

                m_links.Respawned();

                GetMap()->Add(this);
            }
            break;
        }
        case CORPSE:
        {
            Unit::Update(update_diff, diff);

            if (Watch().DeadByDefault())
            {
                break;
            }

            m_claim.TickRoll(update_diff);

            if (Watch().CorpseGoesAt() <= time(nullptr))
            {
                RemoveCorpse();
            }

            break;
        }
        case ALIVE:
        {
            if (GetCharmerGuid())
            {
                Unit* charmer = GetCharmer();
                if (!charmer || (!InReach(*this, *charmer, GetMap()->GetVisibilityDistance()) && (charmer->GetCharmGuid() == GetObjectGuid())))
                {
                    if (charmer)
                    {
                        charmer->Uncharm();
                    }
                    ForcedDespawn();
                    return;
                }
            }
            Watch().StillDazed(update_diff);

            if (Watch().DeadByDefault())
            {
                if (Watch().CorpseGoesAt() <= time(nullptr))
                {
                    RemoveCorpse();
                    break;
                }
            }

            Unit::Update(update_diff, diff);

            if (!IsAlive())
            {
                break;
            }

            if (!IsInEvadeMode())
            {
                if (AI())
                {

                    m_aiLocked = true;
                    AI()->UpdateAI(diff);
                    m_aiLocked = false;
                }
            }

            if (!IsAlive())
            {
                break;
            }
            RegenerateAll(update_diff);
            break;
        }
        default:
            break;
    }
}

void Creature::RegenerateAll(uint32 update_diff)
{
    m_recovery.Run(update_diff);

    if (!m_recovery.Due())
    {
        return;
    }

    if (!IsInCombat() || IsPolymorphed())
    {
        RegenerateHealth();
    }

    RegeneratePower();

    m_recovery.NextIn(REGEN_TIME_FULL);
}

void Creature::RegeneratePower()
{
    if (!IsRegeneratingPower() && !IsPet())
    {
        return;
    }

    Powers powerType = GetPowerType();
    uint32 curValue = GetPower(powerType);
    uint32 maxValue = GetMaxPower(powerType);

    if (curValue >= maxValue)
    {
        return;
    }

    regen::Rates rates;
    rates.mana = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_MANA);
    rates.energy = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_ENERGY);
    rates.focus = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_FOCUS);

    regen::Share const share = regen::PowerTick(powerType, GetStat(STAT_SPIRIT), maxValue,
                                                IsInCombat() || GetCharmerOrOwnerGuid(),
                                                IsUnderLastManaUseEffect(), rates);
    if (!share.any)
    {
        return;
    }

    float addValue = share.amount;

    const auto ModPowerRegenAuras = GetAurasByType(SPELL_AURA_MOD_POWER_REGEN);
    for (auto* aura : ModPowerRegenAuras)
    {
        Modifier const* modifier = aura->GetModifier();
        if (modifier->m_miscvalue == int32(powerType))
        {
            addValue += modifier->m_amount;
        }
    }

    const auto ModPowerRegenPCTAuras = GetAurasByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    for (auto* aura : ModPowerRegenPCTAuras)
    {
        Modifier const* modifier = aura->GetModifier();
        if (modifier->m_miscvalue == int32(powerType))
        {
            addValue *= (modifier->m_amount + 100) / 100.0f;
        }
    }

    ModifyPower(powerType, int32(addValue));
}

void Creature::RegenerateHealth()
{
    if (!IsRegeneratingHealth())
    {
        return;
    }

    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
    {
        return;
    }

    ModifyHealth(regen::HealthTick(GetStat(STAT_SPIRIT), maxValue,
                                   GetCharmerOrOwnerGuid(), GetPower(POWER_MANA) > 0,
                                   sWorld.getConfig(CONFIG_FLOAT_RATE_HEALTH)));
}

void Creature::DoFleeToGetAssistance()
{
    if (!getVictim())
    {
        return;
    }

    float radius = sWorld.getConfig(CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS);
    if (radius > 0)
    {
        Creature* pCreature = nullptr;

        MaNGOS::NearestAssistCreatureInCreatureRangeCheck u_check(this, getVictim(), radius);
        MaNGOS::CreatureLastSearcher<MaNGOS::NearestAssistCreatureInCreatureRangeCheck> searcher(pCreature, u_check);
        Cell::VisitGridObjects(this, searcher, radius);

        SetNoSearchAssistance(true);
        Pacing().Reckon(MOVE_RUN, false);

        if (!pCreature)
        {
            SetFeared(true, getVictim()->GetObjectGuid(), 0 , sWorld.getConfig(CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY));
        }
        else
        {
            SetTargetGuid(0);
            GetMotionMaster()->MoveSeekAssistance(pCreature->Where().X(), pCreature->Where().Y(), pCreature->Where().Z());
        }
    }
}

bool Creature::AIM_Initialize()
{

    if (m_aiLocked)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "AIM_Initialize: failed to init, locked.");
        return false;
    }

    CreatureAI* oldAI = i_AI;
    i_motionMaster.Initialize();
    i_AI = FactorySelector::selectAI(this);
    delete oldAI;
    return true;
}

bool Creature::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, Team team , const CreatureData* data , GameEventCreatureData const* eventData )
{
    SetMap(cPos.GetMap());

    if (!CreateFromProto(guidlow, cinfo, team, data, eventData))
    {
        return false;
    }

    cPos.SelectFinalPoint(this);

    if (!cPos.PlaceOn(this))
    {
        return false;
    }

    if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z())))
    {
        outdoorPvP->HandleCreatureCreate(this);
    }

    if (InstanceData* iData = GetMap()->GetInstanceData())
    {
        iData->OnCreatureCreate(this);
    }

    vigil::Decay decay;
    decay.normal = sWorld.getConfig(CONFIG_UINT32_CORPSE_DECAY_NORMAL);
    decay.rare = sWorld.getConfig(CONFIG_UINT32_CORPSE_DECAY_RARE);
    decay.elite = sWorld.getConfig(CONFIG_UINT32_CORPSE_DECAY_ELITE);
    decay.rareElite = sWorld.getConfig(CONFIG_UINT32_CORPSE_DECAY_RAREELITE);
    decay.worldBoss = sWorld.getConfig(CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS);

    Watch().CorpseDelay(vigil::DecayFor(GetCreatureInfo()->Rank, decay));

    m_links.Enrol(*cPos.GetMap());

    LoadCreatureAddon(false);

    return true;
}

bool Creature::IsTrainerOf(Player* pPlayer, bool msg) const
{
    if (!IsTrainer())
    {
        return false;
    }

    TrainerSpellData const* cSpells = GetTrainerSpells();
    TrainerSpellData const* tSpells = GetTrainerTemplateSpells();

    if ((!cSpells || cSpells->spellList.empty()) && (!tSpells || tSpells->spellList.empty()))
    {
        sLog.outErrorDb("Creature %u (Entry: %u) have UNIT_NPC_FLAG_TRAINER but have empty trainer spell list.",
            GetGUIDLow(), GetEntry());
        return false;
    }

    switch (GetCreatureInfo()->TrainerType)
    {
        case TRAINER_TYPE_CLASS:
            if (pPlayer->getClass() != GetCreatureInfo()->TrainerClass)
            {
                if (msg)
                {
                    pPlayer->PlayerTalkClass->ClearMenus();
                    switch (GetCreatureInfo()->TrainerClass)
                    {
                        case CLASS_DRUID:  pPlayer->PlayerTalkClass->SendGossipMenu(4913, GetObjectGuid()); break;
                        case CLASS_HUNTER: pPlayer->PlayerTalkClass->SendGossipMenu(10090, GetObjectGuid()); break;
                        case CLASS_MAGE:   pPlayer->PlayerTalkClass->SendGossipMenu(328, GetObjectGuid()); break;
                        case CLASS_PALADIN: pPlayer->PlayerTalkClass->SendGossipMenu(1635, GetObjectGuid()); break;
                        case CLASS_PRIEST: pPlayer->PlayerTalkClass->SendGossipMenu(4436, GetObjectGuid()); break;
                        case CLASS_ROGUE:  pPlayer->PlayerTalkClass->SendGossipMenu(4797, GetObjectGuid()); break;
                        case CLASS_SHAMAN: pPlayer->PlayerTalkClass->SendGossipMenu(5003, GetObjectGuid()); break;
                        case CLASS_WARLOCK: pPlayer->PlayerTalkClass->SendGossipMenu(5836, GetObjectGuid()); break;
                        case CLASS_WARRIOR: pPlayer->PlayerTalkClass->SendGossipMenu(4985, GetObjectGuid()); break;
                    }
                }
                return false;
            }
            break;
        case TRAINER_TYPE_PETS:
            if (pPlayer->getClass() != CLASS_HUNTER)
            {
                if (msg)
                {
                    pPlayer->PlayerTalkClass->ClearMenus();
                    pPlayer->PlayerTalkClass->SendGossipMenu(3620, GetObjectGuid());
                }
                return false;
            }
            break;
        case TRAINER_TYPE_MOUNTS:
            if (GetCreatureInfo()->TrainerRace && pPlayer->getRace() != GetCreatureInfo()->TrainerRace)
            {

                if (FactionTemplateEntry const* faction_template = getFactionTemplateEntry())
                {
                    if (pPlayer->GetReputationRank(faction_template->Faction) == REP_EXALTED)
                    {
                        return true;
                    }
                }

                if (msg)
                {
                    pPlayer->PlayerTalkClass->ClearMenus();
                    switch (GetCreatureInfo()->TrainerClass)
                    {
                        case RACE_DWARF:        pPlayer->PlayerTalkClass->SendGossipMenu(5865, GetObjectGuid()); break;
                        case RACE_GNOME:        pPlayer->PlayerTalkClass->SendGossipMenu(4881, GetObjectGuid()); break;
                        case RACE_HUMAN:        pPlayer->PlayerTalkClass->SendGossipMenu(5861, GetObjectGuid()); break;
                        case RACE_NIGHTELF:     pPlayer->PlayerTalkClass->SendGossipMenu(5862, GetObjectGuid()); break;
                        case RACE_ORC:          pPlayer->PlayerTalkClass->SendGossipMenu(5863, GetObjectGuid()); break;
                        case RACE_TAUREN:       pPlayer->PlayerTalkClass->SendGossipMenu(5864, GetObjectGuid()); break;
                        case RACE_TROLL:        pPlayer->PlayerTalkClass->SendGossipMenu(5816, GetObjectGuid()); break;
                        case RACE_UNDEAD:       pPlayer->PlayerTalkClass->SendGossipMenu(624, GetObjectGuid()); break;
                    }
                }
                return false;
            }
            break;
        case TRAINER_TYPE_TRADESKILLS:
            if (GetCreatureInfo()->TrainerSpell && !pPlayer->HasSpell(GetCreatureInfo()->TrainerSpell))
            {
                if (msg)
                {
                    pPlayer->PlayerTalkClass->ClearMenus();
                    pPlayer->PlayerTalkClass->SendGossipMenu(11031, GetObjectGuid());
                }
                return false;
            }
            break;
        default:
            return false;
    }
    return true;
}

bool Creature::CanInteractWithBattleMaster(Player* pPlayer, bool msg) const
{
    if (!IsBattleMaster())
    {
        return false;
    }

    BattleGroundTypeId bgTypeId = sBattleGroundMgr.GetBattleMasterBG(GetEntry());
    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        return false;
    }

    if (!msg)
    {
        return pPlayer->GetBGAccessByLevel(bgTypeId);
    }

    if (!pPlayer->GetBGAccessByLevel(bgTypeId))
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        switch (bgTypeId)
        {
            case BATTLEGROUND_AV:  pPlayer->PlayerTalkClass->SendGossipMenu(7616, GetObjectGuid()); break;
            case BATTLEGROUND_WS:  pPlayer->PlayerTalkClass->SendGossipMenu(7599, GetObjectGuid()); break;
            case BATTLEGROUND_AB:  pPlayer->PlayerTalkClass->SendGossipMenu(7642, GetObjectGuid()); break;
            default: break;
        }
        return false;
    }
    return true;
}

bool Creature::CanTrainAndResetTalentsOf(Player* pPlayer) const
{
    return pPlayer->getLevel() >= 10 &&
        GetCreatureInfo()->TrainerType == TRAINER_TYPE_CLASS &&
        pPlayer->getClass() == GetCreatureInfo()->TrainerClass;
}

void Creature::PrepareBodyLootState()
{
    loot.clear();

    if (!Taking().BodyTaken())
    {

        if (GetCreatureInfo()->MaxLootGold > 0 || GetCreatureInfo()->LootId || (GetCreatureType() != CREATURE_TYPE_CRITTER && (GetCreatureInfo()->SkinningLootId && sWorld.getConfig(CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW))))
        {
            SetDynFlag(UNIT_DYNFLAG_LOOTABLE);
            return;
        }
    }

    Taking().BodyTaken(true);

    if (!Taking().Skinned() && GetCreatureInfo()->SkinningLootId)
    {
        RemoveDynFlag(UNIT_DYNFLAG_LOOTABLE);
        SetUnitFlag(UNIT_FLAG_SKINNABLE);
        return;
    }

    RemoveDynFlag(UNIT_DYNFLAG_LOOTABLE);
    RemoveUnitFlag(UNIT_FLAG_SKINNABLE);
}

bool Creature::IsTappedBy(Player const* player) const
{
    if (player == Claim().Taker())
    {
        return true;
    }

    Group const* playerGroup = player->GetGroup();
    if (!playerGroup || playerGroup != Claim().HoldingGroup())
    {
        return false;
    }

    return true;
}

void Creature::LowerPlayerDamageReq(uint32 unDamage)
{
    uint32 const owed = Taking().DamageOwed();
    Taking().DamageOwed(owed > unDamage ? owed - unDamage : 0);
}

bool Creature::CreateFromProto(uint32 guidlow, CreatureInfo const* cinfo, Team team, const CreatureData* data , GameEventCreatureData const* eventData )
{
    m_originalEntry = cinfo->Entry;

    Object::_Create(guidlow, cinfo->Entry, cinfo->GetHighGuid());

    if (!UpdateEntry(cinfo->Entry, team, data, eventData, false))
    {
        return false;
    }

    return true;
}

bool Creature::LoadFromDB(uint32 guidlow, Map* map)
{
    CreatureData const* data = sObjectMgr.GetCreatureData(guidlow);

    if (!data)
    {
        sLog.outErrorDb("Creature (GUID: %u) not found in table `creature`, can't load. ", guidlow);
        return false;
    }

    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(data->id);
    if (!cinfo)
    {
        sLog.outErrorDb("Creature (Entry: %u) not found in table `creature_template`, can't load. ", data->id);
        return false;
    }

    GameEventCreatureData const* eventData = sGameEventMgr.GetCreatureUpdateDataForActiveEvent(guidlow);

    if (map->GetCreature(cinfo->GetObjectGuid(guidlow)))
    {
        return false;
    }

    CreatureCreatePos pos(map, data->posX, data->posY, data->posZ, data->orientation);

    if (!Create(guidlow, pos, cinfo, TEAM_NONE, data, eventData))
    {
        return false;
    }

    SetSpawn(pos);
    Stationed().Radius(data->spawndist);

    Watch().RespawnDelay(data->spawntimesecs);
    Watch().CorpseDelay(std::min(Watch().RespawnDelay() * 9 / 10, Watch().CorpseDelay()));
    Watch().DeadByDefault(data->is_dead);
    m_deathState = Watch().DeadByDefault() ? DEAD : ALIVE;

    Watch().RespawnsAt(map->GetPersistentState()->GetCreatureRespawnTime(GetGUIDLow()));

    if (Watch().RespawnsAt() > time(nullptr))
    {
        m_deathState = DEAD;
        if (CanFly())
        {
            const auto spawnFloor = GetMap()->GetTerrain()->StaticFloor(data->posX, data->posY, data->posZ);
                float tz = spawnFloor ? *spawnFloor : INVALID_HEIGHT;
            if (data->posZ - tz > 0.1)
            {
                Place().MoveTo(data->posX, data->posY, tz);
            }
        }
    }
    else if (Watch().RespawnsAt())
    {
        Watch().RespawnsAt(0);

        GetMap()->GetPersistentState()->SaveCreatureRespawnTime(GetGUIDLow(), 0);
    }

    uint32 curhealth = data->curhealth;
    if (curhealth)
    {
        curhealth = uint32(curhealth * RatesFor(GetCreatureInfo()->Rank).health);
        if (curhealth < 1)
        {
            curhealth = 1;
        }
    }

    if (sCreatureLinkingMgr.IsSpawnedByLinkedMob(this))
    {
        m_links.WaitsOnAnother();
        if (m_deathState == ALIVE && !m_links.MayRespawn())
        {
            m_deathState = DEAD;

            if (CanFly())
            {
                const auto spawnFloor = GetMap()->GetTerrain()->StaticFloor(data->posX, data->posY, data->posZ);
                float tz = spawnFloor ? *spawnFloor : INVALID_HEIGHT;
                if (data->posZ - tz > 0.1)
                {
                    Place().MoveTo(data->posX, data->posY, tz);
                }
            }
        }
    }

    SetHealth(m_deathState == ALIVE ? curhealth : 0);
    SetPower(POWER_MANA, data->curmana);

    SetMeleeDamageSchool(SpellSchools(GetCreatureInfo()->DamageSchool));

    Stationed().Wander(MovementGeneratorType(data->movementType));

    map->Add(this);

    AIM_Initialize();

    if (IsAlive())
    {
        m_links.Respawned();
    }

    if (IsAlive() && sWorld.getConfig(CONFIG_UINT32_RABBIT_DAY))
    {
        time_t rabbit_day = time_t(sWorld.getConfig(CONFIG_UINT32_RABBIT_DAY));
        std::tm rabbit_day_tm = safe_localtime(rabbit_day);
        std::tm now_tm = safe_localtime(sWorld.GetGameTime());

        if (now_tm.tm_mon == rabbit_day_tm.tm_mon && now_tm.tm_mday == rabbit_day_tm.tm_mday)
        {
            CastSpell(this, 10710 + urand(0, 2), true);
        }
    }

    return true;
}

void Creature::LoadEquipment(uint32 equip_entry, bool force)
{
    if (equip_entry == 0)
    {
        if (force)
        {
            for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
            {
                SetVirtualItem(VirtualItemSlot(i), 0);
            }
            m_equipmentId = 0;
        }
        return;
    }

    if (EquipmentInfo const* einfo = sObjectMgr.GetEquipmentInfo(equip_entry))
    {
        m_equipmentId = equip_entry;
        for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
        {
            SetVirtualItem(VirtualItemSlot(i), einfo->equipentry[i]);
        }
    }
    else if (EquipmentInfoRaw const* einfo = sObjectMgr.GetEquipmentInfoRaw(equip_entry))
    {
        m_equipmentId = equip_entry;
        for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
        {
            SetVirtualItemRaw(VirtualItemSlot(i), einfo->equipmodel[i], einfo->equipinfo[i], einfo->equipslot[i]);
        }
    }
}

bool Creature::OffersQuest(uint32 quest_id) const
{
    return NamesQuest(sObjectMgr.GetCreatureQuestRelationsMapBounds(GetEntry()), quest_id);
}

bool Creature::TakesQuest(uint32 quest_id) const
{
    return NamesQuest(sObjectMgr.GetCreatureQuestInvolvedRelationsMapBounds(GetEntry()), quest_id);
}

float Creature::GetAttackDistance(Unit const* pl) const
{

    const float detection = float(GetTotalAuraModifier(SPELL_AURA_MOD_DETECT_RANGE))
                          + float(pl->GetTotalAuraModifier(SPELL_AURA_MOD_DETECTED_RANGE));

    return stats::NoticeRange(GetLevelForTarget(pl), pl->GetLevelForTarget(this), detection,
                              sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL),
                              sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO));
}

void Creature::SetDeathState(DeathState s)
{
    if ((s == JUST_DIED && !Watch().DeadByDefault()) || (s == JUST_ALIVED && Watch().DeadByDefault()))
    {
        Watch().CorpseGoesAt(time(nullptr) + Watch().CorpseDelay());
        Watch().RespawnsAt(time(nullptr) + Watch().RespawnDelay());

        if (sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY) || IsWorldBoss())
        {
            npcs::SaveRespawnTime(*this);
        }
    }

    Unit::SetDeathState(s);

    if (s == JUST_DIED)
    {
        SetTargetGuid(0);
        SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

        if (HasSearchedAssistance())
        {
            SetNoSearchAssistance(false);
            Pacing().Reckon(MOVE_RUN, false);
        }

        if (CanFly())
        {
            i_motionMaster.MoveFall();
        }

        if (Pet* pet = GetPet())
        {
            pet->Unsummon(PET_SAVE_AS_DELETED, this);
        }

        Unit::SetDeathState(CORPSE);
    }

    if (s == JUST_ALIVED)
    {
        clearUnitState(UNIT_STAT_ALL_STATE);

        Unit::SetDeathState(ALIVE);

        SetHealth(GetMaxHealth());
        Claim().StakedBy(nullptr);
        if (GetTemporaryFactionFlags() & TEMPFACTION_RESTORE_RESPAWN)
        {
            ClearTemporaryFaction();
        }

        SetMeleeDamageSchool(SpellSchools(GetCreatureInfo()->DamageSchool));

        SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
        LoadCreatureAddon(true);
        Taking().DamageOwed(GetHealth() / 2);

        SetUInt32Value(UNIT_NPC_FLAGS, GetCreatureInfo()->NpcFlags);
        RemoveUnitFlag(UNIT_FLAG_SKINNABLE);

        SetWalk(true, true);
        i_motionMaster.Initialize();
    }
}

void Creature::Respawn()
{
    RemoveCorpse();
    if (!IsInWorld())
    {
        return;
    }

    if (IsDespawned())
    {
        if (npcs::Listed(*this))
        {
            GetMap()->GetPersistentState()->SaveCreatureRespawnTime(GetGUIDLow(), 0);
        }
        Watch().RespawnsAt(time(nullptr));
    }
}

void Creature::ForcedDespawn(uint32 timeMSToDespawn)
{
    if (timeMSToDespawn)
    {
        ForcedDespawnDelayEvent* pEvent = new ForcedDespawnDelayEvent(*this);

        m_Events.AddEvent(pEvent, m_Events.CalculateTime(timeMSToDespawn));
        return;
    }

    if (IsDespawned())
    {
        return;
    }

    if (IsAlive())
    {
        SetDeathState(JUST_DIED);
    }

    RemoveCorpse(true);

    SetHealth(0);
}

bool Creature::IsImmuneToSpell(SpellEntry const* spellInfo, bool castOnSelf)
{
    if (!spellInfo)
    {
        return false;
    }

    if (!castOnSelf)
    {
        if (GetCreatureInfo()->MechanicImmuneMask & (1 << (spellInfo->Mechanic - 1)))
        {
            return true;
        }

        if (GetCreatureInfo()->SchoolImmuneMask & (1 << spellInfo->School))
        {
            return true;
        }
    }

    return Unit::IsImmuneToSpell(spellInfo, castOnSelf);
}

bool Creature::IsImmuneToDamage(SpellSchoolMask meleeSchoolMask)
{
    if (GetCreatureInfo()->SchoolImmuneMask & meleeSchoolMask)
    {
        return true;
    }

    return Unit::IsImmuneToDamage(meleeSchoolMask);
}

bool Creature::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const
{
    if (!castOnSelf && GetCreatureInfo()->MechanicImmuneMask & (1 << (spellInfo->EffectMechanic[index] - 1)))
    {
        return true;
    }

    if (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NOT_TAUNTABLE)
    {

        if (spellInfo->Effect[index] == SPELL_EFFECT_APPLY_AURA)
        {
            if (spellInfo->EffectAura[index] == SPELL_AURA_MOD_TAUNT)
            {
                return true;
            }
        }

        else if (spellInfo->Effect[index] == SPELL_EFFECT_ATTACK_ME)
        {
            return true;
        }
    }

    return Unit::IsImmuneToSpellEffect(spellInfo, index, castOnSelf);
}

SpellEntry const* Creature::ReachWithSpellAttack(Unit* pVictim)
{
    if (!pVictim)
    {
        return nullptr;
    }

    for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
    {
        if (!Knowing().Slot(i))
        {
            continue;
        }
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(Knowing().Slot(i));
        if (!spellInfo)
        {
            sLog.outError("WORLD: unknown spell id %i", Knowing().Slot(i));
            continue;
        }

        bool bcontinue = true;
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if ((spellInfo->Effect[j] == SPELL_EFFECT_SCHOOL_DAMAGE)       ||
                (spellInfo->Effect[j] == SPELL_EFFECT_INSTAKILL)            ||
                (spellInfo->Effect[j] == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE) ||
                (spellInfo->Effect[j] == SPELL_EFFECT_HEALTH_LEECH))
            {
                bcontinue = false;
                break;
            }
        }
        if (bcontinue)
        {
            continue;
        }

        if (spellInfo->ManaCost > GetPower(POWER_MANA))
        {
            continue;
        }
        SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(spellInfo->RangeIndex);
        float range = GetSpellMaxRange(srange);
        float minrange = GetSpellMinRange(srange);

        float dist = CombatDistanceBetween(*this, *pVictim, spellInfo->RangeIndex == SPELL_RANGE_IDX_COMBAT);

        if (dist > range || dist < minrange)
        {
            continue;
        }
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE && HasUnitFlag(UNIT_FLAG_SILENCED))
        {
            continue;
        }
        if (IsSchoolLockedOut(GetSpellSchoolMask(spellInfo)))
        {
            continue;
        }
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY && HasUnitFlag(UNIT_FLAG_PACIFIED))
        {
            continue;
        }
        return spellInfo;
    }
    return nullptr;
}

SpellEntry const* Creature::ReachWithSpellCure(Unit* pVictim)
{
    if (!pVictim)
    {
        return nullptr;
    }

    for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
    {
        if (!Knowing().Slot(i))
        {
            continue;
        }
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(Knowing().Slot(i));
        if (!spellInfo)
        {
            sLog.outError("WORLD: unknown spell id %i", Knowing().Slot(i));
            continue;
        }

        if (!spellInfo->HasSpellEffect(SPELL_EFFECT_HEAL))
        {
            continue;
        }

        if (spellInfo->ManaCost > GetPower(POWER_MANA))
        {
            continue;
        }
        SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(spellInfo->RangeIndex);
        float range = GetSpellMaxRange(srange);
        float minrange = GetSpellMinRange(srange);

        float dist = CombatDistanceBetween(*this, *pVictim, spellInfo->RangeIndex == SPELL_RANGE_IDX_COMBAT);

        if (dist > range || dist < minrange)
        {
            continue;
        }
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE && HasUnitFlag(UNIT_FLAG_SILENCED))
        {
            continue;
        }
        if (IsSchoolLockedOut(GetSpellSchoolMask(spellInfo)))
        {
            continue;
        }
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY && HasUnitFlag(UNIT_FLAG_PACIFIED))
        {
            continue;
        }
        return spellInfo;
    }
    return nullptr;
}

bool Creature::IsVisibleInGridForPlayer(Player* pl) const
{

    if (pl->isGameMaster())
    {
        return true;
    }

    if (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_INVISIBLE)
    {
        return false;
    }

    if (pl->IsAlive() || pl->GetDeathTimer() > 0)
    {
        return (IsAlive() || Watch().CorpseGoesAt() > time(nullptr) || (Watch().DeadByDefault() && m_deathState == CORPSE));
    }

    if (IsAlive())
    {
        Corpse* corpse = pl->GetCorpse();
        if (corpse)
        {

            if (InReach(*corpse, *this, (20 + 25)*sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO)))
            {
                return true;
            }
        }
    }

    if (GetCreatureInfo()->CreatureTypeFlags & CREATURE_TYPEFLAGS_GHOST_VISIBLE)
    {
        return true;
    }

    return false;
}

void Creature::SendAIReaction(AiReaction reactionType)
{
    WorldPacket data(SMSG_AI_REACTION, 12);

    data << GetObjectGuid();
    data << uint32(reactionType);

    Deliver(Audience::Around(*this).AndSubject(), &data);

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "WORLD: Sent SMSG_AI_REACTION, type %u.", reactionType);
}

void Creature::CallAssistance()
{

    if (!m_calledForHelp && getVictim() && !IsCharmed())
    {
        SetNoCallAssistance(true);

        if (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_CALL_ASSIST)
        {
            return;
        }

        AI()->SendAIEventAround(AI_EVENT_CALL_ASSISTANCE, getVictim(), sWorld.getConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY), sWorld.getConfig(CONFIG_FLOAT_CREATURE_FAMILY_ASSISTANCE_RADIUS));
    }
}

void Creature::CallForHelp(float fRadius)
{
    if (fRadius <= 0.0f || !getVictim() || IsPet() || IsCharmed())
    {
        return;
    }

    MaNGOS::CallOfHelpCreatureInRangeDo u_do(this, getVictim(), fRadius);
    MaNGOS::CreatureWorker<MaNGOS::CallOfHelpCreatureInRangeDo> worker(this, u_do);
    Cell::VisitGridObjects(this, worker, fRadius);
}

bool Creature::CanAssistTo(const Unit* u, const Unit* enemy, bool checkfaction ) const
{

    if (!IsAlive())
    {
        return false;
    }

    if (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_AGGRO)
    {
        return false;
    }

    if (HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_PASSIVE))
    {
        return false;
    }

    if (enemy && IsInCombat())
    {
        return false;
    }

    if (GetCharmerOrOwnerGuid())
    {
        return false;
    }

    if (checkfaction)
    {
        if (getFaction() != u->getFaction())
        {
            return false;
        }
    }
    else
    {
        if (!IsFriendly(*this, *u))
        {
            return false;
        }
    }

    if (enemy && !IsHostile(*this, *enemy))
    {
        return false;
    }

    return true;
}

void Creature::MovedTo(float x, float y, float z, float o)
{
    GetMap()->CreatureRelocation(this, x, y, z, o);
}

bool Creature::OpenableBy(Player const& who) const
{

    const bool state = IsAlive() == (who.getClass() == CLASS_ROGUE && Taking().PocketsPicked());

    return state && InReach(*this, who, INTERACTION_DISTANCE);
}

bool Creature::CanInitiateAttack()
{
    if (hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED))
    {
        return false;
    }

    if (HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
    {
        return false;
    }

    if (isPassiveToHostile())
    {
        return false;
    }

    if (Watch().AggroDelay() != 0)
    {
        return false;
    }

    return true;
}

bool Creature::IsOutOfThreatArea(Unit* pVictim) const
{
    if (!pVictim)
    {
        return true;
    }

    if (!pVictim->Where().ShareFrame(this->Where()))
    {
        return true;
    }

    if (!pVictim->IsTargetableForAttack())
    {
        return true;
    }

    if (!pVictim->isInAccessablePlaceFor(this))
    {
        return true;
    }

    if (!pVictim->IsVisibleForOrDetect(this, this, false))
    {
        return true;
    }

    if (sMapStore.LookupEntry(GetMapId())->IsDungeon())
    {
        return false;
    }

    float AttackDist = GetAttackDistance(pVictim);
    float ThreatRadius = sWorld.getConfig(CONFIG_FLOAT_THREAT_RADIUS);

    return !pVictim->Where().WithinDist(CombatAnchor(), ThreatRadius > AttackDist ? ThreatRadius : AttackDist);
}

CreatureDataAddon const* Creature::GetCreatureAddon() const
{
    if (CreatureDataAddon const* addon = ObjectMgr::GetCreatureAddon(GetGUIDLow()))
    {
        return addon;
    }

    return ObjectMgr::GetCreatureTemplateAddon(GetCreatureInfo()->Entry);
}

bool Creature::LoadCreatureAddon(bool reload)
{
    CreatureDataAddon const* cainfo = GetCreatureAddon();
    if (!cainfo)
    {
        return false;
    }

    if (cainfo->mount != 0)
    {
        Mount(cainfo->mount);
    }

    if (cainfo->bytes1 != 0)
    {

        SetByteValue(UNIT_FIELD_BYTES_1, 0, uint8(cainfo->bytes1 & 0xFF));
        SetBearing(uint8((cainfo->bytes1 >> 24) & 0xFF));
    }

    SetSheath(SheathState(cainfo->sheath_state));

    if (cainfo->emote != 0)
    {
        SetUInt32Value(UNIT_NPC_EMOTESTATE, cainfo->emote);
    }

    if (cainfo->auras)
    {
        for (uint32 const* cAura = cainfo->auras; *cAura; ++cAura)
        {
            if (HasAura(*cAura))
            {
                if (!reload)
                {
                    sLog.outErrorDb("Creature (GUIDLow: %u Entry: %u) has spell %u in `auras` field, but aura is already applied.", GetGUIDLow(), GetEntry(), *cAura);
                }

                continue;
            }

            CastSpell(this, *cAura, true);
        }
    }
    return true;
}

void Creature::SendZoneUnderAttackMessage(Player* attacker)
{
    sWorld.SendZoneUnderAttackMessage(GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z()), attacker->GetTeam() == ALLIANCE ? HORDE : ALLIANCE);
}

void Creature::SetInCombatWithZone()
{
    if (!CanHaveThreatList())
    {
        sLog.outError("Creature entry %u call SetInCombatWithZone but creature can not have threat list.", GetEntry());
        return;
    }

    Map* pMap = GetMap();

    if (!pMap->IsDungeon())
    {
        sLog.outError("Creature entry %u call SetInCombatWithZone for map (id: %u) that isn't an instance.", GetEntry(), pMap->GetId());
        return;
    }

    Map::PlayerList const& PlList = pMap->GetPlayers();

    if (PlList.isEmpty())
    {
        return;
    }

    for (Map::PlayerList::const_iterator i = PlList.begin(); i != PlList.end(); ++i)
    {
        if (Player* pPlayer = i->getSource())
        {
            if (pPlayer->isGameMaster())
            {
                continue;
            }

            if (pPlayer->IsAlive() && !IsFriendly(*this, *pPlayer))
            {
                pPlayer->SetInCombatWith(this);
                AddThreat(pPlayer);
            }
        }
    }
}

bool Creature::MeetsSelectAttackingRequirement(Unit* pTarget, SpellEntry const* pSpellInfo, uint32 selectFlags) const
{
    if (selectFlags & SELECT_FLAG_PLAYER && !IsPlayer(pTarget))
    {
        return false;
    }

    if (selectFlags & SELECT_FLAG_POWER_MANA && pTarget->GetPowerType() != POWER_MANA)
    {
        return false;
    }
    else if (selectFlags & SELECT_FLAG_POWER_RAGE && pTarget->GetPowerType() != POWER_RAGE)
    {
        return false;
    }
    else if (selectFlags & SELECT_FLAG_POWER_ENERGY && pTarget->GetPowerType() != POWER_ENERGY)
    {
        return false;
    }

    if (selectFlags & SELECT_FLAG_IN_MELEE_RANGE && !InMeleeReach(*this, *pTarget))
    {
        return false;
    }
    if (selectFlags & SELECT_FLAG_NOT_IN_MELEE_RANGE && InMeleeReach(*this, *pTarget))
    {
        return false;
    }

    if (pSpellInfo && selectFlags & SELECT_FLAG_IN_LOS && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, pSpellInfo->ID, pTarget, SPELL_DISABLE_LOS) && !HasLineOfSight(*this, *pTarget))
    {
        return false;
    }

    if (pSpellInfo)
    {
        switch (pSpellInfo->RangeIndex)
        {
            case SPELL_RANGE_IDX_SELF_ONLY: return false;
            case SPELL_RANGE_IDX_ANYWHERE:  return true;
            case SPELL_RANGE_IDX_COMBAT:    return InMeleeReach(*this, *pTarget);
        }

        SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(pSpellInfo->RangeIndex);
        float max_range = GetSpellMaxRange(srange);
        float min_range = GetSpellMinRange(srange);
        float dist = CombatDistanceBetween(*this, *pTarget, false);

        return dist < max_range && dist >= min_range;
    }

    return true;
}

Unit* Creature::SelectAttackingTarget(AttackingTarget target, uint32 position, uint32 uiSpellEntry, uint32 selectFlags) const
{
    return SelectAttackingTarget(target, position, sSpellStore.LookupEntry(uiSpellEntry), selectFlags);
}

Unit* Creature::SelectAttackingTarget(AttackingTarget target, uint32 position, SpellEntry const* pSpellInfo , uint32 selectFlags) const
{
    if (!CanHaveThreatList())
    {
        return nullptr;
    }

    ThreatList const& threatlist = GetThreatManager().getThreatList();
    ThreatList::const_iterator itr = threatlist.begin();
    ThreatList::const_reverse_iterator ritr = threatlist.rbegin();

    if (position >= threatlist.size() || !threatlist.size())
    {
        return nullptr;
    }

    switch (target)
    {
        case ATTACKING_TARGET_RANDOM:
        {
            std::vector<Unit*> suitableUnits;
            suitableUnits.reserve(threatlist.size() - position);
            advance(itr, position);
            for (; itr != threatlist.end(); ++itr)
            {
                if (Unit* pTarget = GetMap()->GetUnit((*itr)->getUnitGuid()))
                {
                    if (!selectFlags || MeetsSelectAttackingRequirement(pTarget, pSpellInfo, selectFlags))
                    {
                        suitableUnits.push_back(pTarget);
                    }
                }
            }

            if (!suitableUnits.empty())
            {
                return suitableUnits[urand(0, suitableUnits.size() - 1)];
            }

            break;
        }
        case ATTACKING_TARGET_TOPAGGRO:
        {
            advance(itr, position);
            for (; itr != threatlist.end(); ++itr)
            {
                if (Unit* pTarget = GetMap()->GetUnit((*itr)->getUnitGuid()))
                {
                    if (!selectFlags || MeetsSelectAttackingRequirement(pTarget, pSpellInfo, selectFlags))
                    {
                        return pTarget;
                    }
                }
            }
            break;
        }
        case ATTACKING_TARGET_BOTTOMAGGRO:
        {
            advance(ritr, position);
            for (; ritr != threatlist.rend(); ++ritr)
            {
                if (Unit* pTarget = GetMap()->GetUnit((*itr)->getUnitGuid()))
                {
                    if (!selectFlags || MeetsSelectAttackingRequirement(pTarget, pSpellInfo, selectFlags))
                    {
                        return pTarget;
                    }
                }
            }
            break;
        }
    }

    return nullptr;
}

bool Creature::IsInEvadeMode() const
{
    return !i_motionMaster.empty() && i_motionMaster.GetCurrentMovementGeneratorType() == HOME_MOTION_TYPE;
}

void Creature::SetSpawn(CreatureCreatePos const& pos)
{
    SetSpawn(Geometry::Vector3(pos.m_pos.x, pos.m_pos.y, pos.m_pos.z), pos.m_pos.o);
}

void Creature::SetSpawn(Geometry::Vector3 const& at, float facing)
{
    Stationed().PlaceInFrameOf(Where(), at, facing);

    MANGOS_ASSERT(MaNGOS::IsValidMapCoord(at.x, at.y, at.z) ||
                  PrintCoordinatesError(at.x, at.y, at.z, "respawn"));
}

void Creature::ResetSpawn()
{
    if (CreatureData const* data = sObjectMgr.GetCreatureData(GetGUIDLow()))
    {
        SetSpawn(Geometry::Vector3(data->posX, data->posY, data->posZ), data->orientation);
    }
}

void Creature::AllLootRemovedFromCorpse()
{
    if (loot.loot_type != LOOT_SKINNING && !IsPet() && GetCreatureInfo()->LootId && Claim().Entitled())
    {
        if (LootTemplates_Skinning.HaveLootFor(GetCreatureInfo()->LootId))
        {

            if (!Taking().Skinned())
            {
                HasUnitFlag(UNIT_FLAG_SKINNABLE);
            }
        }
    }

    time_t now = time(nullptr);
    if (Watch().CorpseGoesAt() <= now)
    {
        return;
    }

    float decayRate = sWorld.getConfig(CONFIG_FLOAT_RATE_CORPSE_DECAY_LOOTED);

    if (loot.loot_type == LOOT_SKINNING)
    {
        Watch().CorpseGoesAt(now);
    }
    else
    {
        Watch().CorpseGoesAt(now + uint32(Watch().CorpseDelay() * decayRate));
    }

    Watch().RespawnsAt(Watch().CorpseGoesAt() + Watch().RespawnDelay());
}

uint32 Creature::GetLevelForTarget(Unit const* target) const
{
    if (!IsWorldBoss())
    {
        return Unit::GetLevelForTarget(target);
    }

    uint32 level = target->getLevel() + sWorld.getConfig(CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF);
    if (level < 1)
    {
        return 1;
    }
    if (level > 255)
    {
        return 255;
    }
    return level;
}

std::string Creature::GetAIName() const
{
    return ObjectMgr::GetCreatureTemplate(GetEntry())->AIName;
}

std::string Creature::GetScriptName() const
{
    return sScriptMgr.GetScriptName(GetScriptId());
}

uint32 Creature::GetScriptId() const
{

    return sScriptMgr.GetBoundScriptId(SCRIPTED_UNIT, -int32(GetGUIDLow())) ? sScriptMgr.GetBoundScriptId(SCRIPTED_UNIT, -int32(GetGUIDLow())) : sScriptMgr.GetBoundScriptId(SCRIPTED_UNIT, GetEntry());
}

const char* Creature::GetNameForLocaleIdx(int32 loc_idx) const
{
    char const* name = GetName();
    sObjectMgr.GetCreatureLocaleStrings(GetEntry(), loc_idx, &name);
    return name;
}

void Creature::SendAreaSpiritHealerQueryOpcode(Player* pl)
{
    uint32 next_resurrect = 0;
    if (Spell* pcurSpell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        next_resurrect = pcurSpell->GetCastedTime();
    }
    WorldPacket data(SMSG_AREA_SPIRIT_HEALER_TIME, 8 + 4);
    data << static_cast<ObjectGuid>(GetObjectGuid());
    data << uint32(next_resurrect);
    pl->SendDirectMessage(&data);
}

void Creature::ApplyGameEventSpells(GameEventCreatureData const* eventData, bool activated)
{
    uint32 cast_spell = activated ? eventData->spell_id_start : eventData->spell_id_end;
    uint32 remove_spell = activated ? eventData->spell_id_end : eventData->spell_id_start;

    if (remove_spell)
    {
        if (SpellEntry const* spellEntry = sSpellStore.LookupEntry(remove_spell))
        {
            if (IsSpellAppliesAura(spellEntry))
            {
                RemoveAuras(remove_spell);
            }
        }
    }

    if (cast_spell)
    {
        CastSpell(this, cast_spell, true);
    }
}

void Creature::FillGuidsListFromThreatList(GuidVector& guids, uint32 maxamount )
{
    if (!CanHaveThreatList())
    {
        return;
    }

    ThreatList const& threats = GetThreatManager().getThreatList();

    maxamount = maxamount > 0 ? std::min(maxamount, uint32(threats.size())) : threats.size();

    guids.reserve(guids.size() + maxamount);

    for (ThreatList::const_iterator itr = threats.begin(); maxamount && itr != threats.end(); ++itr, --maxamount)
    {
        guids.push_back((*itr)->getUnitGuid());
    }
}

void Creature::SetVirtualItem(VirtualItemSlot slot, uint32 item_id)
{
    if (item_id == 0)
    {
        SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + slot, 0);
        SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, 0);
        SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 1, 0);
        return;
    }

    EquipmentInfoItem const* proto = ObjectMgr::GetEquipmentInfoItem(item_id);
    if (!proto)
    {
        sLog.outError("Not listed in 'creature_item_template' item (ID:%u) used as virtual item for %s", item_id, GetGuidStr().c_str());
        return;
    }

    SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + slot, proto->DisplayID);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_CLASS,         proto->Class);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_SUBCLASS,      proto->SubClass);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_MATERIAL,      proto->Material);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_INVENTORYTYPE, proto->InventoryType);

    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 1, VIRTUAL_ITEM_INFO_1_OFFSET_SHEATH,        proto->Sheath);
}

void Creature::SetVirtualItemRaw(VirtualItemSlot slot, uint32 display_id, uint32 info0, uint32 info1)
{
    SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + slot, display_id);
    SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, info0);
    SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 1, info1);
}

SpellCastResult Creature::TryToCast(Unit* pTarget, uint32 uiSpell, uint32 uiCastFlags, uint8 uiChance)
{
    if (IsNonMeleeSpellCasted(false) && !(uiCastFlags & (CF_TRIGGERED | CF_INTERRUPT_PREVIOUS)))
    {
        return SPELL_FAILED_SPELL_IN_PROGRESS;
    }

    const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(uiSpell);

    if (!pSpellInfo)
    {
        sLog.outError("TryToCast: attempt to cast unknown spell %u by creature with entry: %u", uiSpell, GetEntry());
        return SPELL_FAILED_SPELL_UNAVAILABLE;
    }

    return TryToCast(pTarget, pSpellInfo, uiCastFlags, uiChance);
}

SpellCastResult Creature::TryToCast(Unit* pTarget, const SpellEntry* pSpellInfo, uint32 uiCastFlags, uint8 uiChance)
{
    if (!pTarget)
    {
        return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
    }

    if (hasUnitState(UNIT_STAT_STUNNED))
    {
        return SPELL_FAILED_STUNNED;
    }

    if ((uiCastFlags & CF_AURA_NOT_PRESENT) && pTarget->HasAura(pSpellInfo->ID))
    {
        return SPELL_FAILED_MORE_POWERFUL_SPELL_ACTIVE;
    }

    if (GetMotionMaster()->GetCurrentMovementGeneratorType() == TIMED_FLEEING_MOTION_TYPE)
    {
        return SPELL_FAILED_FLEEING;
    }

    if ((uiCastFlags & CF_ONLY_IN_MELEE) && !InMeleeReach(*this, *pTarget))
    {
        return SPELL_FAILED_OUT_OF_RANGE;
    }

    if ((uiCastFlags & CF_NOT_IN_MELEE) && InMeleeReach(*this, *pTarget))
    {
        return SPELL_FAILED_TOO_CLOSE;
    }

    if ((uiCastFlags & CF_TARGET_UNREACHABLE) && (InMeleeReach(*this, *pTarget) || (GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE) || !(hasUnitState(UNIT_STAT_ROOT) || !GetMotionMaster()->GetCurrent()->IsReachable())))
    {
        return SPELL_FAILED_MOVING;
    }

    if (!(uiCastFlags & CF_FORCE_CAST))
    {

        if (!hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
        {

            switch (GetMotionMaster()->GetCurrentMovementGeneratorType())
            {
                case TIMED_FLEEING_MOTION_TYPE:
                    return SPELL_FAILED_FLEEING;
            }
        }

        if (pSpellInfo->AttributesExB == SPELL_ATTR_EX2_FACING_TARGETS_BACK && cast::RecipeOf(*pSpellInfo).Says().needsFacing && pTarget->Where().HasInArc(this->Where(), M_PI_F))
        {
            return SPELL_FAILED_UNIT_NOT_BEHIND;
        }

        if (!IsAreaOfEffectSpell(pSpellInfo))
        {

            if (!IsTargetPowerTypeValid(pSpellInfo, pTarget->GetPowerType()))
            {
                return SPELL_FAILED_UNKNOWN;
            }

            if (pTarget->IsImmuneToDamage(GetSpellSchoolMask(pSpellInfo)))
            {
                return SPELL_FAILED_IMMUNE;
            }
        }

        if ((GetThreatManager().getThreatList().size() == 1) && (IsSpellHaveAura(pSpellInfo, SPELL_AURA_MOD_CHARM) || IsSpellHaveAura(pSpellInfo, SPELL_AURA_MOD_POSSESS)))
        {
            return SPELL_FAILED_UNKNOWN;
        }

        if (!pTarget->IsMounted() && IsDismountSpell(pSpellInfo))
        {
            return SPELL_FAILED_ONLY_MOUNTED;
        }
    }

    if ((uiCastFlags & CF_INTERRUPT_PREVIOUS) && IsNonMeleeSpellCasted(false))
    {
        InterruptNonMeleeSpells(false);
    }

    Spell *spell = new Spell(this, pSpellInfo, uiCastFlags & CF_TRIGGERED);

    SpellCastTargets targets;
    targets.setUnitTarget(pTarget);

    if (pSpellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
    {
        targets.setDestination(pTarget->Where().X(), pTarget->Where().Y(), pTarget->Where().Z());
    }
    if (pSpellInfo->Targets & TARGET_FLAG_SOURCE_LOCATION)
    {
        if (Occupant* caster = spell->GetCastingObject())
        {
            targets.setSource(caster->Where().X(), caster->Where().Y(), caster->Where().Z());
        }
    }

    spell->m_CastItem = nullptr;
    return spell->prepare(&targets, nullptr, uiChance);
}
