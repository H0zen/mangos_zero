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

#include "Kinds.h"

#include <cmath>
#include "Utterance.h"
#include "Summoning.h"
#include "Utilities/Errors.h"
#include <sstream>
#include "Utilities/MathDefines.h"
#include "GameObject.h"
#include "QuestDef.h"
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
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "GameObjectModel.h"
#include "CreatureAISelector.h"
#include "SQLStorages.h"
#include "GameObjectAI.h"
#include "Geometry/Quat.h"
#include "AnimatedTraps.h"
#include "Cast/Recipe/RecipeBook.h"

enum
{
    GO_DIRE_MAUL_FIXED_TRAP = 179512,
    NPC_SLIPKIK_GUARD = 14323
};

GameObjectBehaviour::Casting DoorBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    It().UseDoorOrButton();

    if (!scriptSaidYes)
    {
        It().GetMap()->Scripts().Start(DBS_ON_GO_USE, It().GetGUIDLow(), cast.caster, &It());
    }
    return Casting();
}

GameObjectBehaviour::Casting ButtonBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    It().UseDoorOrButton();

    It().TriggerLinkedGameObject(user);

    if (!scriptSaidYes)
    {
        It().GetMap()->Scripts().Start(DBS_ON_GO_USE, It().GetGUIDLow(), cast.caster, &It());
    }

    return Casting();
}

GameObjectBehaviour::Casting QuestGiverBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    if (!sScriptMgr.OnGossipHello(player, &It()))
    {
        player->PrepareGossipMenu(&It(), It().GetGOInfo()->questgiver.gossipID);
        player->SendPreparedGossip(&It());
    }

    return Casting();
}

GameObjectBehaviour::Casting ChestBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (!IsPlayer(user))
    {
        return Casting();
    }

    It().TriggerLinkedGameObject(user);

    if (It().GetGOInfo()->chest.eventId)
    {
        DEBUG_LOG("Chest ScriptStart id %u for %s (opened by %s)", It().GetGOInfo()->chest.eventId, It().GetGuidStr().c_str(), user->GetGuidStr().c_str());
        StartEvents_Event(It().GetMap(), It().GetGOInfo()->chest.eventId, user, &It());
    }

    return Casting();
}

GameObjectBehaviour::Casting GenericBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (scriptSaidYes)
    {
        return Casting();
    }

    It().SetLootState(GO_JUST_DEACTIVATED);
    return Casting();
}

GameObjectBehaviour::Casting TrapBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (scriptSaidYes)
    {
        return Casting();
    }

    Unit* owner = It().GetOwner();
    Unit* caster = owner ? owner : user;

    GameObjectInfo const* goInfo = It().GetGOInfo();
    float radius = float(goInfo->trap.radius);
    bool IsBattleGroundTrap = !radius && goInfo->trap.cooldown == 3 && It().Clock().Moment() == 0;

    if (goInfo->trap.spellId)
    {
        caster->CastSpell(user, goInfo->trap.spellId, true, nullptr, nullptr, It().GetObjectGuid());
    }

    It().UsableAt(time(nullptr) + (goInfo->trap.cooldown ? goInfo->trap.cooldown : uint32(4)));

    if (goInfo->trap.charges > 0)
    {
        m_tally.Used();
    }

    if (IsBattleGroundTrap &&IsPlayer(user))
    {

        if (BattleGround* bg = static_cast<Player*>(user)->Battle().Ground())
        {
            bg->HandleTriggerBuff(It().GetObjectGuid());
        }
    }

    if (sAnimatedTraps.NeedTelling(It().GetDisplayId()))
    {
        It().SendGameObjectCustomAnim();
    }

    if (!scriptSaidYes &&IsCreature(user))
    {
        sScriptMgr.OnGameObjectUse(user, &It());
    }

    return Casting();
}

GameObjectBehaviour::Casting ChairBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    GameObjectInfo const* info = It().GetGOInfo();
    if (!info)
    {
        return Casting();
    }

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    if (info->chair.slots > 0)
    {
        float lowestDist = DEFAULT_VISIBILITY_DISTANCE;

        float x_lowest = It().Where().X();
        float y_lowest = It().Where().Y();

        float orthogonalOrientation = It().Where().Facing() + M_PI_F * 0.5f;

        for (uint32 i = 0; i < info->chair.slots; ++i)
        {

            float relativeDistance = (info->size * i) - (info->size * (info->chair.slots - 1) / 2.0f);

            float x_i = It().Where().X() + relativeDistance * cos(orthogonalOrientation);
            float y_i = It().Where().Y() + relativeDistance * sin(orthogonalOrientation);

            float thisDistance = player->Where().DistanceTo(Geometry::Vector2(x_i, y_i));

            if (thisDistance <= lowestDist)
            {
                lowestDist = thisDistance;
                x_lowest = x_i;
                y_lowest = y_i;
            }
        }
        player->TeleportTo(It().GetMapId(), x_lowest, y_lowest, It().Where().Z(), It().Where().Facing(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
    }
    else
    {

        player->TeleportTo(It().GetMapId(), It().Where().X(), It().Where().Y(), It().Where().Z(), It().Where().Facing(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
    }
    player->SetStandState(UNIT_STAND_STATE_SIT_LOW_CHAIR + info->chair.height);
    return Casting();
}

GameObjectBehaviour::Casting SpellFocusBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    It().TriggerLinkedGameObject(user);

    return Casting();
}

GameObjectBehaviour::Casting GooberBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (IsPlayer(user))
    {
        Player* player = static_cast<Player*>(user);
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
        {
            outdoorPvP->HandleGameObjectUse(player, &It());
        }
    }

    GameObjectInfo const* info = It().GetGOInfo();

    It().TriggerLinkedGameObject(user);

    It().SetGoFlag(GO_FLAG_IN_USE);
    It().SetLootState(GO_ACTIVATED);

    if (info->goober.customAnim)
    {
        It().SendGameObjectCustomAnim();
    }
    else
    {
        It().SetGoState(GO_STATE_ACTIVE);
    }

    It().ClosesAt(time(nullptr) + info->GetAutoCloseTime());

    if (IsPlayer(user))
    {
        Player* player = static_cast<Player*>(user);

        if (info->goober.pageId)
        {
            WorldPacket data(SMSG_GAMEOBJECT_PAGETEXT, 8);
            data << static_cast<ObjectGuid>(It().GetObjectGuid());
            player->GetSession()->SendPacket(&data);
        }
        else if (info->goober.gossipID)
        {
            if (!sScriptMgr.OnGossipHello(player, &It()))
            {
                player->PrepareGossipMenu(&It(), info->goober.gossipID);
                player->SendPreparedGossip(&It());
            }
        }

        if (info->goober.eventId)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Goober ScriptStart id %u for %s (Used by %s).", info->goober.eventId, It().GetGuidStr().c_str(), player->GetGuidStr().c_str());
            StartEvents_Event(It().GetMap(), info->goober.eventId, player, &It());
        }

        if (info->goober.questId && sObjectMgr.GetQuestTemplate(info->goober.questId))
        {

            if (player->GetQuestStatus(info->goober.questId) != QUEST_STATUS_INCOMPLETE)
            {
                return cast;
            }
        }

        player->RewardPlayerAndGroupAtCast(&It());
    }

    if (!scriptSaidYes)
    {
        It().GetMap()->Scripts().Start(DBS_ON_GO_USE, It().GetGUIDLow(), cast.caster, &It());
    }
    else
    {
        return Casting();
    }

    cast.spellId = info->goober.spellId;

    return cast;
}

GameObjectBehaviour::Casting CameraBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    GameObjectInfo const* info = It().GetGOInfo();
    if (!info)
    {
        return Casting();
    }

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    if (info->camera.cinematicId)
    {
        player->SendCinematicStart(info->camera.cinematicId);
    }

    if (info->camera.eventID)
    {
        StartEvents_Event(It().GetMap(), info->camera.eventID, player, &It());
    }

    return Casting();
}

GameObjectBehaviour::Casting FishingNodeBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    if (player->GetObjectGuid() != It().GetOwnerGuid())
    {
        return Casting();
    }

    switch (It().getLootState())
    {
        case GO_READY:
        {

            uint32 zone, subzone;
            It().GetTerrain()->GetZoneAndAreaId(zone, subzone, It().Where().X(), It().Where().Y(), It().Where().Z());

            int32 zone_skill = sObjectMgr.GetFishingBaseSkillLevel(subzone);
            if (!zone_skill)
            {
                zone_skill = sObjectMgr.GetFishingBaseSkillLevel(zone);
            }

            if (!zone_skill)
            {
                sLog.outErrorDb("Fishable areaId %u are not properly defined in `skill_fishing_base_level`.", subzone);
            }

            int32 skill = player->GetSkillValue(SKILL_FISHING);
            int32 chance = skill - zone_skill + 5;
            int32 roll = irand(1, 100);

            DEBUG_LOG("Fishing check (skill: %i zone min skill: %i chance %i roll: %i", skill, zone_skill, chance, roll);

            bool success = skill >= zone_skill && chance >= roll;
            GameObject* fishingHole = nullptr;

            if (!success)
            {
                if (!sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL))
                {

                    fishingHole = It().LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
                    if (fishingHole)
                    {
                        success = true;
                    }
                }
            }

            else

            {
                fishingHole = It().LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
            }

            if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING))
            {
                player->UpdateFishingSkill();
            }

            if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING))
            {

                player->Conjured().RemoveObject(&It(), false);
                It().SetOwnerGuid(player->GetObjectGuid());

                if (fishingHole)
                {
                    fishingHole->Use(player);
                    It().SetLootState(GO_JUST_DEACTIVATED);
                }
                else
                {
                    player->SendLoot(It().GetObjectGuid(), success ? LOOT_FISHING : LOOT_FISHING_FAIL);
                }
            }
            else
            {

                It().SetLootState(GO_JUST_DEACTIVATED);

                WorldPacket data(SMSG_FISH_ESCAPED, 0);
                player->GetSession()->SendPacket(&data);
            }
            break;
        }
        case GO_JUST_DEACTIVATED:
            break;
        default:
        {
            It().SetLootState(GO_JUST_DEACTIVATED);

            WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
            player->GetSession()->SendPacket(&data);
            break;
        }
    }

    player->FinishSpell(CURRENT_CHANNELED_SPELL);
    return Casting();
}

GameObjectBehaviour::Casting RitualBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    Unit* owner = It().GetOwner();

    GameObjectInfo const* info = It().GetGOInfo();

    if (owner)
    {
        if (!IsPlayer(owner))
        {
            return Casting();
        }

        if (player == static_cast<Player*>(owner) || (info->summoningRitual.castersGrouped && !player->IsInSameRaidWith(static_cast<Player*>(owner))))
        {
            return Casting();
        }

        if (!owner->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            return Casting();
        }

        cast.caster = owner;
    }
    else
    {
        ObjectGuid firstUser = m_tally.First();
        if (firstUser && player->GetObjectGuid() != firstUser && info->summoningRitual.castersGrouped)
        {
            if (Group* group = player->GetGroup())
            {
                if (!group->IsMember(firstUser))
                {
                    return Casting();
                }
            }
            else
            {
                return Casting();
            }
        }

        cast.caster = player;
    }

    m_tally.UsedBy(player->GetObjectGuid());

    if (info->summoningRitual.animSpell)
    {
        player->CastSpell(player, info->summoningRitual.animSpell, true);

        cast.triggered = true;
    }

    if (m_tally.Distinct() < info->summoningRitual.reqParticipants)
    {
        return Casting();
    }

    if (!It().GetOwnerGuid())
    {
        if (Player* opener = It().GetMap()->GetPlayer(m_tally.First()))
        {
            cast.caster = opener;
        }
    }

    cast.spellId = info->summoningRitual.spellId;

    cast.triggered = true;

    if (owner)
    {
        owner->FinishSpell(CURRENT_CHANNELED_SPELL);
    }

    if (!info->summoningRitual.ritualPersistent)
    {
        It().SetLootState(GO_JUST_DEACTIVATED);
    }

    else
    {
        It().ClearAllUsesData();
    }

    return cast;
}

GameObjectBehaviour::Casting SpellCasterBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    It().SetAllGoFlags(GO_FLAG_LOCKED);

    GameObjectInfo const* info = It().GetGOInfo();
    if (!info)
    {
        return Casting();
    }

    if (info->spellcaster.partyOnly)
    {
        Unit* caster = It().GetOwner();
        if (!caster || !IsPlayer(caster))
        {
            return Casting();
        }

        if (!IsPlayer(user) || !static_cast<Player*>(user)->IsInSameRaidWith(static_cast<Player*>(caster)))
        {
            return Casting();
        }
    }

    cast.spellId = info->spellcaster.spellId;

    m_tally.Used();
    return cast;
}

GameObjectBehaviour::Casting FlagStandBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    if (player->CanUseBattleGroundObject())
    {

        BattleGround* bg = player->Battle().Ground();
        if (!bg)
        {
            return Casting();
        }

        bg->EventPlayerClickedOnFlag(player, &It());
        return Casting();
    }
    return cast;
}

GameObjectBehaviour::Casting FishingHoleBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    player->SendLoot(It().GetObjectGuid(), LOOT_FISHINGHOLE);
    return Casting();
}

GameObjectBehaviour::Casting FlagDropBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (!IsPlayer(user))
    {
        return Casting();
    }

    Player* player = static_cast<Player*>(user);

    if (player->CanUseBattleGroundObject())
    {

        BattleGround* bg = player->Battle().Ground();
        if (!bg)
        {
            return Casting();
        }

        bg->EventPlayerClickedOnFlag(player, &It());

        It().Delete();
    }
    return cast;
}

void TrapBehaviour::Arming()
{
    Unit* owner = It().GetOwner();
    if (owner && owner->IsInCombat())
    {
        It().UsableAt(time(nullptr) + Data().trap.startDelay);
    }

    It().SetLootState(GO_READY);
}

void FishingNodeBehaviour::Arming()
{
    if (time(nullptr) <= It().Clock().Moment() - FISHING_BOBBER_READY_TIME)
    {
        return;
    }

    Unit* caster = It().GetOwner();
    if (caster &&IsPlayer(caster))
    {
        It().SetGoState(GO_STATE_ACTIVE);
        It().SendForcedObjectUpdate();
        It().SendGameObjectCustomAnim();
    }

    It().SetLootState(GO_READY);
}

void ChestBehaviour::Arming()
{

    It().SetLootState(GO_READY);
}

GameObjectBehaviour::Tick FishingNodeBehaviour::TimedOut()
{
    Unit* caster = It().GetOwner();
    if (caster &&IsPlayer(caster))
    {
        caster->FinishSpell(CURRENT_CHANNELED_SPELL);

        WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
        static_cast<Player*>(caster)->GetSession()->SendPacket(&data);
    }

    It().SetLootState(GO_JUST_DEACTIVATED);
    return Tick::Stop;
}

GameObjectBehaviour::Tick DoorBehaviour::TimedOut()
{
    if (It().GetGoState() != GO_STATE_READY)
    {
        It().ResetDoorOrButton();
    }

    return Tick::Carry;
}

GameObjectBehaviour::Tick ButtonBehaviour::TimedOut()
{
    if (It().GetGoState() != GO_STATE_READY)
    {
        It().ResetDoorOrButton();
    }

    return Tick::Carry;
}

GameObjectBehaviour::Tick TrapBehaviour::Standing()
{
    if (It().UsableAt() >= time(nullptr))
    {
        return Tick::Stop;
    }

    float radius = float(Data().trap.radius);
    if (!radius)
    {

        if (Data().trap.cooldown != 3)
        {
            return Tick::Stop;
        }

        if (It().Clock().Moment() > 0)
        {
            return Tick::Rest;
        }

        radius = float(Data().trap.cooldown);
    }

    SpellEntry const* se = sSpellStore.LookupEntry(Data().trap.spellId);

    if (IsAreaOfEffectSpell(se))
    {
        MaNGOS::AllSpecificUnitsInGameObjectRangeDo unit_do(&It(), radius, cast::RecipeOf(*se).IsPositive());
        MaNGOS::UnitWorker<MaNGOS::AllSpecificUnitsInGameObjectRangeDo> worker(unit_do);
        Cell::VisitAllObjects(&It(), worker, radius);

        return Tick::Carry;
    }

    Unit* targetUnit = nullptr;
    MaNGOS::AnySpecificUnitInGameObjectRangeCheck u_check(&It(), radius, cast::RecipeOf(*se).IsPositive());
    MaNGOS::UnitSearcher<MaNGOS::AnySpecificUnitInGameObjectRangeCheck> checker(targetUnit, u_check);
    Cell::VisitAllObjects(&It(), checker, radius);

    if (targetUnit)
    {

        if (It().GetEntry() != GO_DIRE_MAUL_FIXED_TRAP || targetUnit->GetEntry() == NPC_SLIPKIK_GUARD)
        {
            It().Use(targetUnit);
        }
    }

    return Tick::Carry;
}

void DoorBehaviour::InUse(uint32 )
{
    if (It().ClosesAt() != 0 && It().ClosesAt() <= time(nullptr))
    {
        It().ResetDoorOrButton();
    }
}

void ButtonBehaviour::InUse(uint32 )
{
    if (It().ClosesAt() != 0 && It().ClosesAt() <= time(nullptr))
    {
        It().ResetDoorOrButton();
    }
}

void ChestBehaviour::InUse(uint32 )
{

    if (!It().loot.empty())
    {
        m_lock.EmptyAt(time(nullptr) + CHEST_LINGER);
    }
    else if (m_lock.IsEmptyingDue(time(nullptr)))
    {
        It().SetLootState(GO_JUST_DEACTIVATED);
    }
}

void GooberBehaviour::InUse(uint32 )
{
    if (It().ClosesAt() > time(nullptr))
    {
        return;
    }

    It().RemoveGoFlag(GO_FLAG_IN_USE);
    It().SetLootState(GO_JUST_DEACTIVATED);
    It().ClosesAt(0);
}

GameObjectBehaviour::Tick GooberBehaviour::Spent()
{
    if (uint32 spellId = Data().goober.spellId)
    {
        for (auto const& guid : m_tally.Everyone())
        {
            if (Player* owner = It().GetMap()->GetPlayer(guid))
            {
                owner->CastSpell(owner, spellId, false, nullptr, nullptr, It().GetObjectGuid());
            }
        }

        It().ClearAllUsesData();
    }

    It().SetGoState(GO_STATE_READY);

    return Tick::Carry;
}

GameObjectBehaviour::Tick ChestBehaviour::Spent()
{
    uint32 const trapEntry = Data().GetLinkedGameObjectEntry();

    if (trapEntry == 144064)
    {
        float const range = 0.5f;
        GameObject* visualGO = nullptr;

        MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(It(), 177683, range);
        MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(visualGO, go_check);

        Cell::VisitGridObjects(&It(), checker, range);

        if (visualGO)
        {
            visualGO->SetLootState(GO_JUST_DEACTIVATED);
        }
    }

    if (!trapEntry)
    {
        return Tick::Carry;
    }

    GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(trapEntry);
    if (!trapInfo || trapInfo->type != GAMEOBJECT_TYPE_TRAP)
    {
        return Tick::Carry;
    }

    float const range = 0.5f;
    GameObject* trapGO = nullptr;

    MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(It(), trapEntry, range);
    MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(trapGO, go_check);

    Cell::VisitGridObjects(&It(), checker, range);

    if (trapGO)
    {
        trapGO->SetLootState(GO_JUST_DEACTIVATED);
    }

    return Tick::Carry;
}

void ChestBehaviour::Respawning()
{
    It().RollIfMineralVein();
}
