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
#include "MapManager.h"
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

// The one trap in the game that fires at a single creature and ignores everyone
// else. It is in Dire Maul, and what it is aimed at is Slip'kik's guard.
enum
{
    GO_DIRE_MAUL_FIXED_TRAP = 179512,
    NPC_SLIPKIK_GUARD = 14323
};

/**
 * @brief A door swings and swings back on its own.
 */
GameObjectBehaviour::Casting DoorBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    // doors never really despawn, only reset to default state/flags
    It().UseDoorOrButton();

    // activate script
    if (!scriptSaidYes)
    {
        It().GetMap()->ScriptsStart(DBS_ON_GO_USE, It().GetGUIDLow(), cast.caster, &It());
    }
    return Casting();
}
/**
 * @brief A button does its work through whatever is linked to it.
 */
GameObjectBehaviour::Casting ButtonBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    // buttons never really despawn, only reset to default state/flags
    It().UseDoorOrButton();

    It().TriggerLinkedGameObject(user);

    // activate script
    if (!scriptSaidYes)
    {
        It().GetMap()->ScriptsStart(DBS_ON_GO_USE, It().GetGUIDLow(), cast.caster, &It());
    }

    return Casting();
}
/**
 * @brief A quest giver opens its dialogue.
 */
GameObjectBehaviour::Casting QuestGiverBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    if (!sScriptMgr.OnGossipHello(player, &It()))
    {
        player->PrepareGossipMenu(&It(), It().GetGOInfo()->questgiver.gossipID);
        player->SendPreparedGossip(&It());
    }

    return Casting();
}
/**
 * @brief A chest hands over its loot, and may spring what is linked to it.
 */
GameObjectBehaviour::Casting ChestBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    It().TriggerLinkedGameObject(user);

    // TODO: possible must be moved to loot release (in different from linked triggering)
    if (It().GetGOInfo()->chest.eventId)
    {
        DEBUG_LOG("Chest ScriptStart id %u for %s (opened by %s)", It().GetGOInfo()->chest.eventId, It().GetGuidStr().c_str(), user->GetGuidStr().c_str());
        StartEvents_Event(It().GetMap(), It().GetGOInfo()->chest.eventId, user, &It());
    }

    return Casting();
}
/**
 * @brief A generic object is spent by being touched.
 */
GameObjectBehaviour::Casting GenericBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (scriptSaidYes)
    {
        return Casting();
    }

    // No known way to exclude some - only different approach is to select despawnable GOs by Entry
    It().SetLootState(GO_JUST_DEACTIVATED);
    return Casting();
}
/**
 * @brief A trap fires at whoever set it off.
 */
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

    // FIXME: when GO casting will be implemented trap must cast spell to target
    if (goInfo->trap.spellId)
    {
        caster->CastSpell(user, goInfo->trap.spellId, true, nullptr, nullptr, It().GetObjectGuid());
    }
    // use template cooldown if provided
    It().UsableAt(time(nullptr) + (goInfo->trap.cooldown ? goInfo->trap.cooldown : uint32(4)));

    // count charges
    if (goInfo->trap.charges > 0)
    {
        It().Users().Used();
    }

    if (IsBattleGroundTrap && user->GetTypeId() == TYPEID_PLAYER)
    {
        // BattleGround gameobjects case
        if (BattleGround* bg = ((Player*)user)->GetBattleGround())
        {
            bg->HandleTriggerBuff(It().GetObjectGuid());
        }
    }

    // TODO: all traps can be activated, also those without spell.
    // Some may have have animation and/or are expected to despawn.

    // A few models will stand there doing nothing unless the animation is sent.
    if (sAnimatedTraps.NeedTelling(It().GetDisplayId()))
    {
        It().SendGameObjectCustomAnim();
    }

    if (!scriptSaidYes && user->GetTypeId() == TYPEID_UNIT)
    {
        sScriptMgr.OnGameObjectUse(user, &It());
    }

    // TODO: Despawning of traps? (Also related to code in ::Update)
    return Casting();
}
/**
 * @brief A chair seats the player at its nearest free slot.
 */
GameObjectBehaviour::Casting ChairBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    GameObjectInfo const* info = It().GetGOInfo();
    if (!info)
    {
        return Casting();
    }

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    // a chair may have n slots. we have to calculate their positions and teleport the player to the nearest one

    // check if the db is sane
    if (info->chair.slots > 0)
    {
        float lowestDist = DEFAULT_VISIBILITY_DISTANCE;

        float x_lowest = It().Where().X();
        float y_lowest = It().Where().Y();

        // the object orientation + 1/2 pi
        // every slot will be on that straight line
        float orthogonalOrientation = It().Where().Facing() + M_PI_F * 0.5f;
        // find nearest slot
        for (uint32 i = 0; i < info->chair.slots; ++i)
        {
            // the distance between this slot and the center of the go - imagine a 1D space
            float relativeDistance = (info->size * i) - (info->size * (info->chair.slots - 1) / 2.0f);

            float x_i = It().Where().X() + relativeDistance * cos(orthogonalOrientation);
            float y_i = It().Where().Y() + relativeDistance * sin(orthogonalOrientation);

            // calculate the distance between the player and this slot
            float thisDistance = player->Where().DistanceTo(Geometry::Vector2(x_i, y_i));

            /* debug code. It will spawn a npc on each slot to visualize them.
            Creature* helper = SummonCreature(*player, 14496, x_i, y_i, It().Where().Z(), It().Where().Facing(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 10000);
            std::ostringstream output;
            output << i << ": thisDist: " << thisDistance;
            Utter(*helper, CHAT_TYPE_SAY, output.str().c_str());
            */

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
        // fallback, will always work
        player->TeleportTo(It().GetMapId(), It().Where().X(), It().Where().Y(), It().Where().Z(), It().Where().Facing(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
    }
    player->SetStandState(UNIT_STAND_STATE_SIT_LOW_CHAIR + info->chair.height);
    return Casting();
}
/**
 * @brief A spell focus only springs what is linked to it.
 */
GameObjectBehaviour::Casting SpellFocusBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    It().TriggerLinkedGameObject(user);

    // some may be activated in addition? Conditions for this? (ex: entry 181616)
    return Casting();
}
/**
 * @brief A goober does something to whoever touched it, then shuts.
 */
GameObjectBehaviour::Casting GooberBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    // Handle OutdoorPvP use cases
    // Note: this may be also handled by DB spell scripts in the future, when the world state manager is implemented
    if (user->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = (Player*)user;
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
        {
            outdoorPvP->HandleGameObjectUse(player, &It());
        }
    }

    GameObjectInfo const* info = It().GetGOInfo();

    It().TriggerLinkedGameObject(user);

    It().SetGoFlag(GO_FLAG_IN_USE);
    It().SetLootState(GO_ACTIVATED);

    // this appear to be ok, however others exist in addition to this that should have custom (ex: 190510, 188692, 187389)
    if (info->goober.customAnim)
    {
        It().SendGameObjectCustomAnim();
    }
    else
    {
        It().SetGoState(GO_STATE_ACTIVE);
    }

    It().ClosesAt(time(nullptr) + info->GetAutoCloseTime());

    if (user->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = (Player*)user;

        if (info->goober.pageId)                    // show page...
        {
            WorldPacket data(SMSG_GAMEOBJECT_PAGETEXT, 8);
            data << ObjectGuid(It().GetObjectGuid());
            player->GetSession()->SendPacket(&data);
        }
        else if (info->goober.gossipID)             // ...or gossip, if page does not exist
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

        // possible quest objective for active quests
        if (info->goober.questId && sObjectMgr.GetQuestTemplate(info->goober.questId))
        {
            // Quest require to be active for GO using
            if (player->GetQuestStatus(info->goober.questId) != QUEST_STATUS_INCOMPLETE)
            {
                return cast;
            }
        }

        player->RewardPlayerAndGroupAtCast(&It());
    }

    // activate script
    if (!scriptSaidYes)
    {
        It().GetMap()->ScriptsStart(DBS_ON_GO_USE, It().GetGUIDLow(), cast.caster, &It());
    }
    else
    {
        return Casting();
    }

    // cast this spell later if provided
    cast.spellId = info->goober.spellId;

    return cast;
}
/**
 * @brief A camera plays its cinematic to the player.
 */
GameObjectBehaviour::Casting CameraBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    GameObjectInfo const* info = It().GetGOInfo();
    if (!info)
    {
        return Casting();
    }

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

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
/**
 * @brief A bobber is pulled, and the catch is decided here.
 */
GameObjectBehaviour::Casting FishingNodeBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    if (player->GetObjectGuid() != It().GetOwnerGuid())
    {
        return Casting();
    }

    switch (It().getLootState())
    {
        case GO_READY:                              // ready for loot
        {
            // 1) skill must be >= base_zone_skill
            // 2) if skill == base_zone_skill => 5% chance
            // 3) chance is linear dependence from (base_zone_skill-skill)

            uint32 zone, subzone;
            It().GetTerrain()->GetZoneAndAreaId(zone, subzone, It().Where().X(), It().Where().Y(), It().Where().Z());

            int32 zone_skill = sObjectMgr.GetFishingBaseSkillLevel(subzone);
            if (!zone_skill)
            {
                zone_skill = sObjectMgr.GetFishingBaseSkillLevel(zone);
            }

            // provide error, no fishable zone or area should be 0
            if (!zone_skill)
            {
                sLog.outErrorDb("Fishable areaId %u are not properly defined in `skill_fishing_base_level`.", subzone);
            }

            int32 skill = player->GetSkillValue(SKILL_FISHING);
            int32 chance = skill - zone_skill + 5;
            int32 roll = irand(1, 100);

            DEBUG_LOG("Fishing check (skill: %i zone min skill: %i chance %i roll: %i", skill, zone_skill, chance, roll);

            // normal chance
            bool success = skill >= zone_skill && chance >= roll;
            GameObject* fishingHole = nullptr;

            // overwrite fail in case fishhole if allowed (after 3.3.0)
            if (!success)
            {
                if (!sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL))
                {
                    // TODO: find reasonable value for fishing hole search
                    fishingHole = It().LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
                    if (fishingHole)
                    {
                        success = true;
                    }
                }
            }
            // just search fishhole for success case
            else
                // TODO: find reasonable value for fishing hole search
            {
                fishingHole = It().LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
            }

            if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING))
            {
                player->UpdateFishingSkill();
            }

            // fish catch or fail and junk allowed (after 3.1.0)
            if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING))
            {
                // prevent removing GO at spell cancel
                player->RemoveGameObject(&It(), false);
                It().SetOwnerGuid(player->GetObjectGuid());

                if (fishingHole)                    // will set at success only
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
                // fish escaped, can be deleted now
                It().SetLootState(GO_JUST_DEACTIVATED);

                WorldPacket data(SMSG_FISH_ESCAPED, 0);
                player->GetSession()->SendPacket(&data);
            }
            break;
        }
        case GO_JUST_DEACTIVATED:                   // nothing to do, will be deleted at next update
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
/**
 * @brief A ritual completes once enough casters have joined it.
 */
GameObjectBehaviour::Casting RitualBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    Unit* owner = It().GetOwner();

    GameObjectInfo const* info = It().GetGOInfo();

    if (owner)
    {
        if (owner->GetTypeId() != TYPEID_PLAYER)
        {
            return Casting();
        }

        // accept only use by player from same group as owner, excluding owner itself (unique use already added in spell effect)
        if (player == (Player*)owner || (info->summoningRitual.castersGrouped && !player->IsInSameRaidWith(((Player*)owner))))
        {
            return Casting();
        }

        // expect owner to already be channeling, so if not...
        if (!owner->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            return Casting();
        }

        // in case summoning ritual caster is GO creator
        cast.caster = owner;
    }
    else
    {
        ObjectGuid const& firstUser = It().Users().First();
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

    It().Users().UsedBy(player->GetObjectGuid());

    if (info->summoningRitual.animSpell)
    {
        player->CastSpell(player, info->summoningRitual.animSpell, true);

        // for this case, summoningRitual.spellId is always triggered
        cast.triggered = true;
    }

    // full amount unique participants including original summoner, need more
    if (It().Users().Distinct() < info->summoningRitual.reqParticipants)
    {
        return Casting();
    }

    // owner is first user for non-wild GO objects, if it offline value already set to current user
    if (!It().GetOwnerGuid())
    {
        if (Player* opener = It().GetMap()->GetPlayer(It().Users().First()))
        {
            cast.caster = opener;
        }
    }

    cast.spellId = info->summoningRitual.spellId;

    // spell have reagent and mana cost but it not expected use its
    // it triggered spell in fact casted at currently channeled GO
    cast.triggered = true;

    // finish owners spell
    if (owner)
    {
        owner->FinishSpell(CURRENT_CHANNELED_SPELL);
    }

    // can be deleted now, if
    if (!info->summoningRitual.ritualPersistent)
    {
        It().SetLootState(GO_JUST_DEACTIVATED);
    }
    // reset ritual for this GO
    else
    {
        It().ClearAllUsesData();
    }

    // go to end function to spell casting
    return cast;
}
/**
 * @brief A spell caster spends a charge to cast at whoever used it.
 */
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
        if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        {
            return Casting();
        }

        if (user->GetTypeId() != TYPEID_PLAYER || !((Player*)user)->IsInSameRaidWith((Player*)caster))
        {
            return Casting();
        }
    }

    cast.spellId = info->spellcaster.spellId;

    It().Users().Used();
    return cast;
}
/**
 * @brief A flag stand hands its flag to the player.
 */
GameObjectBehaviour::Casting FlagStandBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    if (player->CanUseBattleGroundObject())
    {
        // in battleground check
        BattleGround* bg = player->GetBattleGround();
        if (!bg)
        {
            return Casting();
        }
        // The battleground knows its own flags: it looks this one up in the event
        // table it was spawned from, and does nothing at all with an object that is
        // not in it.
        bg->EventPlayerClickedOnFlag(player, &It());
        return Casting();                                     // we don't need to delete flag ... it is despawned!
    }
    return cast;
}
/**
 * @brief A fishing hole gives up one of its catches.
 */
GameObjectBehaviour::Casting FishingHoleBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    player->SendLoot(It().GetObjectGuid(), LOOT_FISHINGHOLE);
    return Casting();
}
/**
 * @brief A dropped flag is picked up or returned.
 */
GameObjectBehaviour::Casting FlagDropBehaviour::UsedBy(Unit* user, bool scriptSaidYes)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    if (player->CanUseBattleGroundObject())
    {
        // in battleground check
        BattleGround* bg = player->GetBattleGround();
        if (!bg)
        {
            return Casting();
        }
        // Asked the same way a flag on its stand is: the battleground looks the
        // object up in its own event table and does nothing with one that is not in
        // it, so which battleground and which flag are both its business.
        bg->EventPlayerClickedOnFlag(player, &It());

        // A flag that has been picked up off the ground is gone from the ground,
        // whatever the battleground made of it.
        It().Delete();
    }
    return cast;
}


/* ****************************** Being made ready ************************* */

/**
 * @brief A trap is armed the tick after it is placed, not when it is placed.
 *
 * Its arming delay is only served against an owner who is already fighting, and
 * who that owner is cannot be asked until the object is in the world.
 */
void TrapBehaviour::Arming()
{
    Unit* owner = It().GetOwner();
    if (owner && owner->IsInCombat())
    {
        It().UsableAt(time(nullptr) + Data().trap.startDelay);
    }

    It().SetLootState(GO_READY);
}

/**
 * @brief A bobber sits under the water until it is time for the fish to take it.
 */
void FishingNodeBehaviour::Arming()
{
    if (time(nullptr) <= It().Clock().Moment() - FISHING_BOBBER_READY_TIME)
    {
        return;
    }

    // The splash is what the caster is watching for; it is what says the bite may
    // now be caught.
    Unit* caster = It().GetOwner();
    if (caster && caster->GetTypeId() == TYPEID_PLAYER)
    {
        It().SetGoState(GO_STATE_ACTIVE);
        It().SendForcedObjectUpdate();
        It().SendGameObjectCustomAnim();
    }

    It().SetLootState(GO_READY);
}

/**
 * @brief A chest is ready the moment it is looked at.
 */
void ChestBehaviour::Arming()
{
    // A chest the data says refills is simply always ready. Nothing ever starts
    // the countdown, so refilling is not implemented and the restock time in the
    // template goes unread.
    //
    // <<TODO: implement it, or say in the schema that the column is not read. A
    // chest with chestRestockTime set is meant to fill again after that long
    // rather than despawn.
    It().SetLootState(GO_READY);
}

/* ****************************** The clock running out ******************** */

/**
 * @brief Nobody caught anything, and the cast ends.
 */
GameObjectBehaviour::Tick FishingNodeBehaviour::TimedOut()
{
    Unit* caster = It().GetOwner();
    if (caster && caster->GetTypeId() == TYPEID_PLAYER)
    {
        caster->FinishSpell(CURRENT_CHANNELED_SPELL);

        WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
        ((Player*)caster)->GetSession()->SendPacket(&data);
    }

    It().SetLootState(GO_JUST_DEACTIVATED);
    return Tick::Stop;
}

/**
 * @brief A door left open closes again when its time is up.
 *
 * And then it is put back like anything else, which is why this carries on: a
 * battleground door is a permanent spawn and has to come back.
 */
GameObjectBehaviour::Tick DoorBehaviour::TimedOut()
{
    if (It().GetGoState() != GO_STATE_READY)
    {
        It().ResetDoorOrButton();
    }

    return Tick::Carry;
}

/// The flags of Arathi Basin are buttons, and they close the same way.
GameObjectBehaviour::Tick ButtonBehaviour::TimedOut()
{
    if (It().GetGoState() != GO_STATE_READY)
    {
        It().ResetDoorOrButton();
    }

    return Tick::Carry;
}

/* ****************************** Standing there *************************** */

/**
 * @brief A trap watches the ground around it for somebody to step on.
 */
GameObjectBehaviour::Tick TrapBehaviour::Standing()
{
    if (It().UsableAt() >= time(nullptr))
    {
        return Tick::Stop;                                  // still arming
    }

    // FIXME: this is activation radius (in different casting radius that must be selected from spell data)
    // TODO: move activated state code (cast itself) to GO_ACTIVATED, in this place only check activating and set state
    float radius = float(Data().trap.radius);
    if (!radius)
    {
        // No radius of its own: it goes off only when something else trips it,
        // unless it is one of the battleground traps, which say so by carrying a
        // cooldown of three and mean it as a radius.
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
        MaNGOS::AllSpecificUnitsInGameObjectRangeDo unit_do(&It(), radius, IsPositiveSpell(se));
        MaNGOS::UnitWorker<MaNGOS::AllSpecificUnitsInGameObjectRangeDo> worker(unit_do);
        Cell::VisitAllObjects(&It(), worker, radius);

        return Tick::Carry;
    }

    Unit* targetUnit = nullptr;
    MaNGOS::AnySpecificUnitInGameObjectRangeCheck u_check(&It(), radius, IsPositiveSpell(se));
    MaNGOS::UnitSearcher<MaNGOS::AnySpecificUnitInGameObjectRangeCheck> checker(targetUnit, u_check);
    Cell::VisitAllObjects(&It(), checker, radius);

    if (targetUnit)
    {
        // prevent use if GO entry is "Fixed Trap" and target is not SLIKIK
        if (It().GetEntry() != GO_DIRE_MAUL_FIXED_TRAP || targetUnit->GetEntry() == NPC_SLIPKIK_GUARD)
        {
            It().Use(targetUnit);
        }
    }

    return Tick::Carry;
}

/* ****************************** Being used ******************************* */

/// A door shuts itself at the moment it was told to.
void DoorBehaviour::InUse(uint32 /*elapsed*/)
{
    if (It().ClosesAt() != 0 && It().ClosesAt() <= time(nullptr))
    {
        It().ResetDoorOrButton();
    }
}

/// And so does a button.
void ButtonBehaviour::InUse(uint32 /*elapsed*/)
{
    if (It().ClosesAt() != 0 && It().ClosesAt() <= time(nullptr))
    {
        It().ResetDoorOrButton();
    }
}

/**
 * @brief An emptied chest lingers a moment before it goes.
 *
 * As long as anything is left in it the moment keeps being pushed back, so the
 * countdown only really starts when the last item is taken.
 */
void ChestBehaviour::InUse(uint32 /*elapsed*/)
{
    // TODO : Missing Loot::Update() method found in CMangos
    if (!It().loot.empty())
    {
        It().AsChest().EmptyAt(time(nullptr) + CHEST_LINGER);
    }
    else if (It().AsChest().IsEmptyingDue(time(nullptr)))
    {
        It().SetLootState(GO_JUST_DEACTIVATED);
    }
}

/// A goober is held in use until the moment its template names, and then released.
void GooberBehaviour::InUse(uint32 /*elapsed*/)
{
    if (It().ClosesAt() > time(nullptr))
    {
        return;
    }

    It().RemoveGoFlag(GO_FLAG_IN_USE);
    It().SetLootState(GO_JUST_DEACTIVATED);
    It().ClosesAt(0);
}

/// The bar moves on its own clock while anybody is standing in the circle.
void CapturePointBehaviour::InUse(uint32 elapsed)
{
    if (It().AsCapturePoint().IsTickDue(elapsed))
    {
        It().TickCapturePoint();
    }
}

/* ****************************** Finished with **************************** */

/**
 * @brief What a goober was clicked for is cast on everyone who clicked it.
 */
GameObjectBehaviour::Tick GooberBehaviour::Spent()
{
    if (uint32 spellId = Data().goober.spellId)
    {
        for (auto const& guid : It().Users().Everyone())
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

/**
 * @brief A capture point is never spent, only locked and reopened.
 *
 * It goes straight back to ready rather than through the tail, because the tail
 * despawns anything showing full progress -- which, for a tower being taken, is
 * every tower at the moment it changes hands.
 */
GameObjectBehaviour::Tick CapturePointBehaviour::Spent()
{
    // The bar is not drawn for a locked point, so nobody is left standing in it.
    for (auto const& guid : It().AsCapturePoint().Standing())
    {
        if (Player* owner = It().GetMap()->GetPlayer(guid))
        {
            owner->SendUpdateWorldState(Data().capturePoint.worldState1, WORLD_STATE_REMOVE);
        }
    }

    It().AsCapturePoint().Desert();
    It().SetLootState(GO_READY);

    return Tick::Stop;
}

/**
 * @brief An opened chest springs whatever was linked to it.
 */
GameObjectBehaviour::Tick ChestBehaviour::Spent()
{
    uint32 const trapEntry = Data().GetLinkedGameObjectEntry();

    // <<TODO: the one hardcoded entry left in this file, and the shape is wrong to
    // move as it stands. The key is the chest's linkedTrapId, which here names
    // another chest rather than a trap and is being used as a marker; what it does
    // with it is despawn a separate visual object standing on the same spot. Decide
    // whether that is one chest's patch or the case of a general rule -- a visual
    // that belongs to an object and goes when it goes -- before giving it a table.
    if (trapEntry == 144064) // Special case for Gordunni Cobalt Visual
    {
        float const range = 0.5f;
        GameObject* visualGO = nullptr;

        MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(It(), 177683, range); //177683 Visual Entry
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

/* ****************************** Coming back ****************************** */

/// A vein is rolled afresh every time it comes back, so the ore it holds can change.
void ChestBehaviour::Respawning()
{
    It().RollIfMineralVein();
}
