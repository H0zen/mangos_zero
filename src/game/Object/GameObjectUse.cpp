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

/**
 * @brief Someone has used this object.
 *
 * What that comes to depends entirely on what kind of object it is, and each kind
 * answers for itself below. All that is common is what happens before -- the use
 * cooldown the template may impose, and the scripts that get a say -- and what may
 * happen after, which is one spell, cast at whoever used it.
 *
 * @param user The unit using the object.
 */
void GameObject::Use(Unit* user)
{
    // user must be provided
    MANGOS_ASSERT(user || PrintEntryError("GameObject::Use (without user)"));

    // traps and goobers are the only ones the template gives a use cooldown to
    if (uint32 cooldown = GetGOInfo()->GetCooldown())
    {
        if (m_usableAt > sWorld.GetGameTime())
        {
            return;
        }

        m_usableAt = sWorld.GetGameTime() + cooldown;
    }

    bool const scriptReturnValue = user->GetTypeId() == TYPEID_PLAYER && sScriptMgr.OnGameObjectUse((Player*)user, this);

    if (!scriptReturnValue)
    {
        GetMap()->ScriptsStart(DBS_ON_GOT_USE, GetEntry(), user, this);
    }

    Casting cast;

    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:             cast = UsedAsDoor(user, scriptReturnValue); break;  // 0
        case GAMEOBJECT_TYPE_BUTTON:           cast = UsedAsButton(user, scriptReturnValue); break;  // 1
        case GAMEOBJECT_TYPE_QUESTGIVER:       cast = UsedAsQuestGiver(user, scriptReturnValue); break;  // 2
        case GAMEOBJECT_TYPE_CHEST:            cast = UsedAsChest(user, scriptReturnValue); break;  // 3
        case GAMEOBJECT_TYPE_GENERIC:          cast = UsedAsGeneric(user, scriptReturnValue); break;  // 5
        case GAMEOBJECT_TYPE_TRAP:             cast = UsedAsTrap(user, scriptReturnValue); break;  // 6
        case GAMEOBJECT_TYPE_CHAIR:            cast = UsedAsChair(user, scriptReturnValue); break;  // 7
        case GAMEOBJECT_TYPE_SPELL_FOCUS:      cast = UsedAsSpellFocus(user, scriptReturnValue); break;  // 8
        case GAMEOBJECT_TYPE_GOOBER:           cast = UsedAsGoober(user, scriptReturnValue); break;  // 10
        case GAMEOBJECT_TYPE_CAMERA:           cast = UsedAsCamera(user, scriptReturnValue); break;  // 13
        case GAMEOBJECT_TYPE_FISHINGNODE:      cast = UsedAsFishingNode(user, scriptReturnValue); break;  // 17
        case GAMEOBJECT_TYPE_SUMMONING_RITUAL: cast = UsedAsRitual(user, scriptReturnValue); break;  // 18
        case GAMEOBJECT_TYPE_SPELLCASTER:      cast = UsedAsSpellCaster(user, scriptReturnValue); break;  // 22
        case GAMEOBJECT_TYPE_FLAGSTAND:        cast = UsedAsFlagStand(user, scriptReturnValue); break;  // 24
        case GAMEOBJECT_TYPE_FISHINGHOLE:      cast = UsedAsFishingHole(user, scriptReturnValue); break;  // 25
        case GAMEOBJECT_TYPE_FLAGDROP:         cast = UsedAsDroppedFlag(user, scriptReturnValue); break;  // 26
        default:
            sLog.outError("GameObject::Use unhandled GameObject type %u (entry %u).", GetGoType(), GetEntry());
            return;
    }

    if (!cast.spellId)
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(cast.spellId);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u at use action for gameobject (Entry: %u GoType: %u )", cast.spellId, GetEntry(), GetGoType());
        return;
    }

    Spell* spell = new Spell(cast.caster, spellInfo, cast.triggered, GetObjectGuid());

    // spell target is user of GO
    SpellCastTargets targets;
    targets.setUnitTarget(user);

    spell->prepare(&targets);
}

/**
 * @brief A door swings and swings back on its own.
 */
GameObject::Casting GameObject::UsedAsDoor(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    // doors never really despawn, only reset to default state/flags
    UseDoorOrButton();

    // activate script
    if (!scriptReturnValue)
    {
        GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), cast.caster, this);
    }
    return Casting();
}

/**
 * @brief A button does its work through whatever is linked to it.
 */
GameObject::Casting GameObject::UsedAsButton(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    // buttons never really despawn, only reset to default state/flags
    UseDoorOrButton();

    TriggerLinkedGameObject(user);

    // activate script
    if (!scriptReturnValue)
    {
        GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), cast.caster, this);
    }

    return Casting();
}

/**
 * @brief A quest giver opens its dialogue.
 */
GameObject::Casting GameObject::UsedAsQuestGiver(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    if (!sScriptMgr.OnGossipHello(player, this))
    {
        player->PrepareGossipMenu(this, GetGOInfo()->questgiver.gossipID);
        player->SendPreparedGossip(this);
    }

    return Casting();
}

/**
 * @brief A chest hands over its loot, and may spring what is linked to it.
 */
GameObject::Casting GameObject::UsedAsChest(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    TriggerLinkedGameObject(user);

    // TODO: possible must be moved to loot release (in different from linked triggering)
    if (GetGOInfo()->chest.eventId)
    {
        DEBUG_LOG("Chest ScriptStart id %u for %s (opened by %s)", GetGOInfo()->chest.eventId, GetGuidStr().c_str(), user->GetGuidStr().c_str());
        StartEvents_Event(GetMap(), GetGOInfo()->chest.eventId, user, this);
    }

    return Casting();
}

/**
 * @brief A generic object is spent by being touched.
 */
GameObject::Casting GameObject::UsedAsGeneric(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    if (scriptReturnValue)
    {
        return Casting();
    }

    // No known way to exclude some - only different approach is to select despawnable GOs by Entry
    SetLootState(GO_JUST_DEACTIVATED);
    return Casting();
}

/**
 * @brief A trap fires at whoever set it off.
 */
GameObject::Casting GameObject::UsedAsTrap(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    if (scriptReturnValue)
    {
        return Casting();
    }

    Unit* owner = GetOwner();
    Unit* caster = owner ? owner : user;

    GameObjectInfo const* goInfo = GetGOInfo();
    float radius = float(goInfo->trap.radius);
    bool IsBattleGroundTrap = !radius && goInfo->trap.cooldown == 3 && m_spawn.Moment() == 0;

    // FIXME: when GO casting will be implemented trap must cast spell to target
    if (goInfo->trap.spellId)
    {
        caster->CastSpell(user, goInfo->trap.spellId, true, nullptr, nullptr, GetObjectGuid());
    }
    // use template cooldown if provided
    m_usableAt = time(nullptr) + (goInfo->trap.cooldown ? goInfo->trap.cooldown : uint32(4));

    // count charges
    if (goInfo->trap.charges > 0)
    {
        m_users.Used();
    }

    if (IsBattleGroundTrap && user->GetTypeId() == TYPEID_PLAYER)
    {
        // BattleGround gameobjects case
        if (BattleGround* bg = ((Player*)user)->GetBattleGround())
        {
            bg->HandleTriggerBuff(GetObjectGuid());
        }
    }

    // TODO: all traps can be activated, also those without spell.
    // Some may have have animation and/or are expected to despawn.

    // A few models will stand there doing nothing unless the animation is sent.
    if (sAnimatedTraps.NeedTelling(GetDisplayId()))
    {
        SendGameObjectCustomAnim();
    }

    if (!scriptReturnValue && user->GetTypeId() == TYPEID_UNIT)
    {
        sScriptMgr.OnGameObjectUse(user, this);
    }

    // TODO: Despawning of traps? (Also related to code in ::Update)
    return Casting();
}

/**
 * @brief A chair seats the player at its nearest free slot.
 */
GameObject::Casting GameObject::UsedAsChair(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    GameObjectInfo const* info = GetGOInfo();
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

        float x_lowest = Where().X();
        float y_lowest = Where().Y();

        // the object orientation + 1/2 pi
        // every slot will be on that straight line
        float orthogonalOrientation = Where().Facing() + M_PI_F * 0.5f;
        // find nearest slot
        for (uint32 i = 0; i < info->chair.slots; ++i)
        {
            // the distance between this slot and the center of the go - imagine a 1D space
            float relativeDistance = (info->size * i) - (info->size * (info->chair.slots - 1) / 2.0f);

            float x_i = Where().X() + relativeDistance * cos(orthogonalOrientation);
            float y_i = Where().Y() + relativeDistance * sin(orthogonalOrientation);

            // calculate the distance between the player and this slot
            float thisDistance = player->Where().DistanceTo(Geometry::Vector2(x_i, y_i));

            /* debug code. It will spawn a npc on each slot to visualize them.
            Creature* helper = SummonCreature(*player, 14496, x_i, y_i, Where().Z(), Where().Facing(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 10000);
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
        player->TeleportTo(GetMapId(), x_lowest, y_lowest, Where().Z(), Where().Facing(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
    }
    else
    {
        // fallback, will always work
        player->TeleportTo(GetMapId(), Where().X(), Where().Y(), Where().Z(), Where().Facing(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
    }
    player->SetStandState(UNIT_STAND_STATE_SIT_LOW_CHAIR + info->chair.height);
    return Casting();
}

/**
 * @brief A spell focus only springs what is linked to it.
 */
GameObject::Casting GameObject::UsedAsSpellFocus(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    TriggerLinkedGameObject(user);

    // some may be activated in addition? Conditions for this? (ex: entry 181616)
    return Casting();
}

/**
 * @brief A goober does something to whoever touched it, then shuts.
 */
GameObject::Casting GameObject::UsedAsGoober(Unit* user, bool scriptReturnValue)
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
            outdoorPvP->HandleGameObjectUse(player, this);
        }
    }

    GameObjectInfo const* info = GetGOInfo();

    TriggerLinkedGameObject(user);

    SetGoFlag(GO_FLAG_IN_USE);
    SetLootState(GO_ACTIVATED);

    // this appear to be ok, however others exist in addition to this that should have custom (ex: 190510, 188692, 187389)
    if (info->goober.customAnim)
    {
        SendGameObjectCustomAnim();
    }
    else
    {
        SetGoState(GO_STATE_ACTIVE);
    }

    m_closesAt = time(nullptr) + info->GetAutoCloseTime();

    if (user->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = (Player*)user;

        if (info->goober.pageId)                    // show page...
        {
            WorldPacket data(SMSG_GAMEOBJECT_PAGETEXT, 8);
            data << ObjectGuid(GetObjectGuid());
            player->GetSession()->SendPacket(&data);
        }
        else if (info->goober.gossipID)             // ...or gossip, if page does not exist
        {
            if (!sScriptMgr.OnGossipHello(player, this))
            {
                player->PrepareGossipMenu(this, info->goober.gossipID);
                player->SendPreparedGossip(this);
            }
        }

        if (info->goober.eventId)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Goober ScriptStart id %u for %s (Used by %s).", info->goober.eventId, GetGuidStr().c_str(), player->GetGuidStr().c_str());
            StartEvents_Event(GetMap(), info->goober.eventId, player, this);
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

        player->RewardPlayerAndGroupAtCast(this);
    }

    // activate script
    if (!scriptReturnValue)
    {
        GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), cast.caster, this);
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
GameObject::Casting GameObject::UsedAsCamera(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    GameObjectInfo const* info = GetGOInfo();
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
        StartEvents_Event(GetMap(), info->camera.eventID, player, this);
    }

    return Casting();
}

/**
 * @brief A bobber is pulled, and the catch is decided here.
 */
GameObject::Casting GameObject::UsedAsFishingNode(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    if (player->GetObjectGuid() != GetOwnerGuid())
    {
        return Casting();
    }

    switch (getLootState())
    {
        case GO_READY:                              // ready for loot
        {
            // 1) skill must be >= base_zone_skill
            // 2) if skill == base_zone_skill => 5% chance
            // 3) chance is linear dependence from (base_zone_skill-skill)

            uint32 zone, subzone;
            GetTerrain()->GetZoneAndAreaId(zone, subzone, Where().X(), Where().Y(), Where().Z());

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
                    fishingHole = LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
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
                fishingHole = LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
            }

            if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING))
            {
                player->UpdateFishingSkill();
            }

            // fish catch or fail and junk allowed (after 3.1.0)
            if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING))
            {
                // prevent removing GO at spell cancel
                player->RemoveGameObject(this, false);
                SetOwnerGuid(player->GetObjectGuid());

                if (fishingHole)                    // will set at success only
                {
                    fishingHole->Use(player);
                    SetLootState(GO_JUST_DEACTIVATED);
                }
                else
                {
                    player->SendLoot(GetObjectGuid(), success ? LOOT_FISHING : LOOT_FISHING_FAIL);
                }
            }
            else
            {
                // fish escaped, can be deleted now
                SetLootState(GO_JUST_DEACTIVATED);

                WorldPacket data(SMSG_FISH_ESCAPED, 0);
                player->GetSession()->SendPacket(&data);
            }
            break;
        }
        case GO_JUST_DEACTIVATED:                   // nothing to do, will be deleted at next update
            break;
        default:
        {
            SetLootState(GO_JUST_DEACTIVATED);

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
GameObject::Casting GameObject::UsedAsRitual(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    Unit* owner = GetOwner();

    GameObjectInfo const* info = GetGOInfo();

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
        ObjectGuid const& firstUser = m_users.First();
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

    m_users.UsedBy(player->GetObjectGuid());

    if (info->summoningRitual.animSpell)
    {
        player->CastSpell(player, info->summoningRitual.animSpell, true);

        // for this case, summoningRitual.spellId is always triggered
        cast.triggered = true;
    }

    // full amount unique participants including original summoner, need more
    if (m_users.Distinct() < info->summoningRitual.reqParticipants)
    {
        return Casting();
    }

    // owner is first user for non-wild GO objects, if it offline value already set to current user
    if (!GetOwnerGuid())
    {
        if (Player* opener = GetMap()->GetPlayer(m_users.First()))
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
        SetLootState(GO_JUST_DEACTIVATED);
    }
    // reset ritual for this GO
    else
    {
        ClearAllUsesData();
    }

    // go to end function to spell casting
    return cast;
}

/**
 * @brief A spell caster spends a charge to cast at whoever used it.
 */
GameObject::Casting GameObject::UsedAsSpellCaster(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    SetAllGoFlags(GO_FLAG_LOCKED);

    GameObjectInfo const* info = GetGOInfo();
    if (!info)
    {
        return Casting();
    }

    if (info->spellcaster.partyOnly)
    {
        Unit* caster = GetOwner();
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

    m_users.Used();
    return cast;
}

/**
 * @brief A flag stand hands its flag to the player.
 */
GameObject::Casting GameObject::UsedAsFlagStand(Unit* user, bool scriptReturnValue)
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
        bg->EventPlayerClickedOnFlag(player, this);
        return Casting();                                     // we don't need to delete flag ... it is despawned!
    }
    return cast;
}

/**
 * @brief A fishing hole gives up one of its catches.
 */
GameObject::Casting GameObject::UsedAsFishingHole(Unit* user, bool scriptReturnValue)
{
    Casting cast;
    cast.caster = user;

    if (user->GetTypeId() != TYPEID_PLAYER)
    {
        return Casting();
    }

    Player* player = (Player*)user;

    player->SendLoot(GetObjectGuid(), LOOT_FISHINGHOLE);
    return Casting();
}

/**
 * @brief A dropped flag is picked up or returned.
 */
GameObject::Casting GameObject::UsedAsDroppedFlag(Unit* user, bool scriptReturnValue)
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
        bg->EventPlayerClickedOnFlag(player, this);

        // A flag that has been picked up off the ground is gone from the ground,
        // whatever the battleground made of it.
        Delete();
    }
    return cast;
}
