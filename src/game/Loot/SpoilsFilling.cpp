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

        if (how != LOOT_FISHINGHOLE
            && ((how != LOOT_FISHING && how != LOOT_FISHING_FAIL) || what.GetOwnerGuid() != who.GetObjectGuid())
            && !InReach(what, who, INTERACTION_DISTANCE))
        {
            return false;
        }

        GameObjectInfo const* goInfo = what.GetGOInfo();
        Loot* loot = &what.loot;

        if (!what.Claim().Entitled())
        {
            what.Claim().StakedBy(&who);
        }

        if (what.getLootState() == GO_READY && what.isSpawned())
        {
            uint32 lootid = goInfo->GetLootId();

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

                case LOOT_FISHING_FAIL:
                    loot->FillLoot(0, LootTemplates_Fishing, &who, true);
                    break;
                case LOOT_FISHING:
                {
                    uint32 zone, subzone;
                    what.GetTerrain()->GetZoneAndAreaId(zone, subzone, what.Where().X(), what.Where().Y(), what.Where().Z());

                    if (!loot->FillLoot(subzone, LootTemplates_Fishing, &who, true, (subzone != zone)) && subzone != zone)

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

        if (!(how == LOOT_CORPSE || how == LOOT_INSIGNIA) || what.GetType() != CORPSE_BONES)
        {
            return false;
        }

        Loot* loot = &what.loot;

        if (!what.lootForBody)
        {
            what.lootForBody = true;

            const uint32 pLevel = loot->gold;
            loot->clear();

            if (who.Battle().Ground()->GetTypeID() == BATTLEGROUND_AV)
            {
                loot->FillLoot(0, LootTemplates_Creature, &who, false);
            }

            loot->gold = uint32(urand(50, 150) * 0.016f * pow(float(pLevel) / 5.76f, 2.5f)
                                * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
        }

        permission = what.lootRecipient == &who ? OWNER_PERMISSION : NONE_PERMISSION;

        return true;
    }

    bool FillBody(Creature& what, Player& who, LootType& how, PermissionTypes& permission)
    {

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

                const uint32 a = urand(0, what.getLevel() / 2);
                const uint32 b = urand(0, who.getLevel() / 2);
                loot->gold = uint32(10 * (a + b) * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
                permission = OWNER_PERMISSION;
            }

            return true;
        }

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

        if (what.Taking().Skinned())
        {
            how = LOOT_SKINNING;
        }

        if (how == LOOT_SKINNING)
        {
            if (!what.Taking().Skinned())
            {
                what.Taking().Skinned(true);
                loot->clear();
                loot->FillLoot(creatureInfo->SkinningLootId, LootTemplates_Skinning, &who, false);

                if (!loot->empty())
                {
                    what.SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                }

                permission = OWNER_PERMISSION;

                if (InstanceData* mapInstance = what.GetInstanceData())
                {
                    mapInstance->OnCreatureLooted(&what, LOOT_SKINNING);
                }
            }

            return true;
        }

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
