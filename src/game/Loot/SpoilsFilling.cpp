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

/**
 * @file SpoilsFilling.cpp
 * @brief What goes into a pile, asked of the thing that holds it.
 *
 * The four kinds that can be looted, side by side, because they are variations of one
 * question and reading them together is the only way to see that. Each is written as a free
 * function taking both the thing and the player: with no implicit `this`, every call has to
 * name which of the two it is asking, and a name the two share -- GetMap, getLevel,
 * GetObjectGuid -- cannot silently bind to the wrong one.
 */

#include "BattleGround/BattleGround.h"
#include "Corpse.h"
#include "Creature.h"
#include "GameObject.h"
#include "Group.h"
#include "InstanceData.h"
#include "Item.h"
#include "Log.h"
#include "LootMgr.h"
#include "Player.h"
#include "Reaction.h"
#include "World.h"

#include <cmath>

namespace
{
    bool FillChest(GameObject& what, Player& who, LootType& how, PermissionTypes& permission)
    {
        // Not measured for a bobber of his own, nor for a fishing hole, which is reached
        // at a rod's length.
        if (how != LOOT_FISHINGHOLE
            && ((how != LOOT_FISHING && how != LOOT_FISHING_FAIL) || what.GetOwnerGuid() != who.GetObjectGuid())
            && !InReach(what, who, INTERACTION_DISTANCE))
        {
            return false;
        }

        GameObjectInfo const* goInfo = what.GetGOInfo();
        Loot* loot = &what.loot;

        // Whoever opens it first stakes the claim for himself and his group.
        if (!what.Claim().Entitled())
        {
            what.Claim().StakedBy(&who);
        }

        // generate loot only if ready for open and spawned in world and not already looted once.
        if (what.getLootState() == GO_READY && what.isSpawned())
        {
            uint32 lootid = goInfo->GetLootId();

            // A battleground may keep its own objects for the side that holds the ground
            // they stand on.
            if (BattleGround* bg = who.Battle().Ground())
            {
                if (!bg->AllowsQuestObject(what.GetEntry(), who.GetTeam()))
                {
                    return false;
                }
            }

            loot->clear();
            switch (how)
            {
                // Entry 0 in fishing loot template used for store junk fish loot at fishing fail it junk allowed by config option
                // this is overwrite fishinghole loot for example
                case LOOT_FISHING_FAIL:
                    loot->FillLoot(0, LootTemplates_Fishing, &who, true);
                    break;
                case LOOT_FISHING:
                {
                    uint32 zone, subzone;
                    what.GetTerrain()->GetZoneAndAreaId(zone, subzone, what.Where().X(), what.Where().Y(), what.Where().Z());
                    // if subzone loot exist use it
                    if (!loot->FillLoot(subzone, LootTemplates_Fishing, &who, true, (subzone != zone)) && subzone != zone)
                        // else use zone loot (if zone diff. from subzone, must exist in like case)
                    {
                        loot->FillLoot(zone, LootTemplates_Fishing, &who, true);
                    }
                    break;
                }
                default:
                    if (!lootid)
                    {
                        break;
                    }
                    DEBUG_LOG("       send normal GO loot");

                    loot->FillLoot(lootid, LootTemplates_Gameobject, &who, false);
                    loot->generateMoneyLoot(goInfo->MinMoneyLoot, goInfo->MaxMoneyLoot);

                    if (what.GetGoType() == GAMEOBJECT_TYPE_CHEST && goInfo->chest.groupLootRules)
                    {
                        if (Group* group = what.Claim().HoldingGroup())
                        {
                            switch (group->GetLootMethod())
                            {
                                case GROUP_LOOT:
                                    // GroupLoot delete items over threshold (threshold even not implemented), and roll them. Items with quality<threshold, round robin
                                    group->GroupLoot(&what, loot);
                                    permission = ALL_PERMISSION;
                                    break;
                                case NEED_BEFORE_GREED:
                                    group->NeedBeforeGreed(&what, loot);
                                    permission = ALL_PERMISSION;
                                    break;
                                case MASTER_LOOT:
                                    group->MasterLoot(&what, loot);
                                    permission = MASTER_PERMISSION;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;
            }

            what.SetLootState(GO_ACTIVATED);
        }

        if (what.getLootState() == GO_ACTIVATED && what.GetGoType() == GAMEOBJECT_TYPE_CHEST
            && what.GetGOInfo()->chest.groupLootRules)
        {
            if (Group* group = what.Claim().HoldingGroup())
            {
                if (group == who.GetGroup())
                {
                    permission = group->GetLootMethod() == MASTER_LOOT ? MASTER_PERMISSION : ALL_PERMISSION;
                }
            }
        }

        what.SetGoState(GO_STATE_ACTIVE);
        who.SetUnitFlag(UNIT_FLAG_LOOTING);

        return true;
    }

    bool FillLockbox(Item& what, Player& who, LootType& how, PermissionTypes& permission)
    {
        permission = OWNER_PERMISSION;

        Loot* loot = &what.loot;

        if (!what.HasGeneratedLoot())
        {
            loot->clear();
            ItemPrototype const* itemProto = what.GetProto();

            switch (how)
            {
                case LOOT_DISENCHANTING:
                    loot->FillLoot(itemProto->DisenchantID, LootTemplates_Disenchant, &who, true);
                    what.SetLootState(ITEM_LOOT_TEMPORARY);
                    break;
                default:
                    loot->FillLoot(what.GetEntry(), LootTemplates_Item, &who, true, itemProto->MaxMoneyLoot == 0);
                    loot->generateMoneyLoot(itemProto->MinMoneyLoot, itemProto->MaxMoneyLoot);
                    what.SetLootState(ITEM_LOOT_CHANGED);
                    break;
            }
        }

        return true;
    }

    bool FillBones(Corpse& what, Player& who, LootType& how, PermissionTypes& permission)
    {
        // Only a set of bones in a battleground carries anything, and only an insignia.
        if (!(how == LOOT_CORPSE || how == LOOT_INSIGNIA) || what.GetType() != CORPSE_BONES)
        {
            return false;
        }

        Loot* loot = &what.loot;

        if (!what.lootForBody)
        {
            what.lootForBody = true;

            // The gold field carries the level of the player who fell, which is what the
            // coin on his bones is worked out from.
            const uint32 pLevel = loot->gold;
            loot->clear();

            if (who.Battle().Ground()->GetTypeID() == BATTLEGROUND_AV)
            {
                loot->FillLoot(0, LootTemplates_Creature, &who, false);
            }

            // It may need a better formula
            // Now it works like this: lvl10: ~6copper, lvl70: ~9silver
            loot->gold = uint32(urand(50, 150) * 0.016f * pow(float(pLevel) / 5.76f, 2.5f)
                                * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
        }

        permission = what.lootRecipient == &who ? OWNER_PERMISSION : NONE_PERMISSION;

        return true;
    }

    bool FillBody(Creature& what, Player& who, LootType& how, PermissionTypes& permission)
    {
        // must be in range and creature must be alive for pickpocket and must be dead for another loot
        if (what.IsAlive() != (how == LOOT_PICKPOCKETING) || !InReach(what, who, INTERACTION_DISTANCE))
        {
            return false;
        }

        if (how == LOOT_PICKPOCKETING && IsFriendly(who, what))
        {
            return false;
        }

        Loot* loot = &what.loot;
        CreatureInfo const* creatureInfo = what.GetCreatureInfo();

        if (how == LOOT_PICKPOCKETING)
        {
            if (!what.Taking().PocketsPicked())
            {
                what.Taking().PocketsPicked(true);
                loot->clear();

                if (uint32 lootid = creatureInfo->PickpocketLootId)
                {
                    loot->FillLoot(lootid, LootTemplates_Pickpocketing, &who, false);
                }

                // Generate extra money for pick pocket loot
                const uint32 a = urand(0, what.getLevel() / 2);
                const uint32 b = urand(0, who.getLevel() / 2);
                loot->gold = uint32(10 * (a + b) * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
                permission = OWNER_PERMISSION;
            }

            return true;
        }

        // the player whose group may loot the corpse
        Player* recipient = what.Claim().Entitled();
        if (!recipient)
        {
            what.TappedBy(&who);
            recipient = &who;
        }

        if (what.Taking().PocketsPicked())
        {
            what.Taking().PocketsPicked(false);
            loot->clear();
        }

        if (!what.Taking().BodyTaken())
        {
            what.Taking().BodyTaken(true);
            loot->clear();

            if (uint32 lootid = creatureInfo->LootId)
            {
                loot->FillLoot(lootid, LootTemplates_Creature, recipient, false);
            }

            loot->generateMoneyLoot(creatureInfo->MinLootGold, creatureInfo->MaxLootGold);

            if (Group* group = what.Claim().HoldingGroup())
            {
                group->UpdateLooterGuid(&what, true);

                switch (group->GetLootMethod())
                {
                    case GROUP_LOOT:
                        // GroupLoot delete items over threshold (threshold even not implemented), and roll them. Items with quality<threshold, round robin
                        group->GroupLoot(&what, loot);
                        break;
                    case NEED_BEFORE_GREED:
                        group->NeedBeforeGreed(&what, loot);
                        break;
                    case MASTER_LOOT:
                        group->MasterLoot(&what, loot);
                        break;
                    default:
                        break;
                }
            }
        }

        // if loot for skin is true loot type must be skinning, so loot_type skinning needs to be set
        // in case of reopening the loot window, after bags full or loot not taken
        if (what.Taking().Skinned())
        {
            how = LOOT_SKINNING;
        }

        // possible only if what.Taking().BodyTaken() && loot->empty() at spell cast check
        if (how == LOOT_SKINNING)
        {
            if (!what.Taking().Skinned())
            {
                what.Taking().Skinned(true);
                loot->clear();
                loot->FillLoot(creatureInfo->SkinningLootId, LootTemplates_Skinning, &who, false);

                // let reopen skinning loot if will closed.
                if (!loot->empty())
                {
                    what.SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                }

                permission = OWNER_PERMISSION;

                // Inform Instance Data, may be scripts related to OnSkinning like The Beast in UBRS
                if (InstanceData* mapInstance = what.GetInstanceData())
                {
                    mapInstance->OnCreatureLooted(&what, LOOT_SKINNING);
                }
            }

            return true;
        }

        // set group rights only for loot_type != LOOT_SKINNING
        if (Group* group = what.Claim().HoldingGroup())
        {
            if (group != who.GetGroup())
            {
                permission = NONE_PERMISSION;
            }
            else
            {
                switch (group->GetLootMethod())
                {
                    case FREE_FOR_ALL:
                    case ROUND_ROBIN:
                        permission = ALL_PERMISSION;
                        break;
                    case MASTER_LOOT:
                        permission = MASTER_PERMISSION;
                        break;
                    case GROUP_LOOT:
                    case NEED_BEFORE_GREED:
                        permission = GROUP_PERMISSION;
                        break;
                }
            }
        }
        else
        {
            permission = recipient == &who ? OWNER_PERMISSION : NONE_PERMISSION;
        }

        return true;
    }
}

bool GameObject::FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission)
{
    return FillChest(*this, who, how, permission);
}

bool Item::FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission)
{
    return FillLockbox(*this, who, how, permission);
}

bool Corpse::FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission)
{
    return FillBones(*this, who, how, permission);
}

bool Creature::FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission)
{
    return FillBody(*this, who, how, permission);
}
