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

#include "Loot/Spoilable.h"
#include <cmath>
#include "Platform/Define.h"
#include <vector>
#include "WorldPacket.h"
#include "Log.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "ObjectGuid.h"
#include "WorldSession.h"
#include "LootAnswers.h"
#include "SpoilsHolder.h"
#include "LootMgr.h"
#include "Occupant.h"
#include "GameObject.h"
#include "Kinds.h"
#include "Group.h"
#include "World.h"
#include "Corpse.h"

void spoils::AutostoreItem(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_AUTOSTORE_LOOT_ITEM");
    Player*  player =   &who;
    ObjectGuid lguid = player->GetLootGuid();
    uint8    lootSlot;

    recv_data >> lootSlot;

    Spoilable* holder = spoils::Holder(who, lguid);
    if (!holder)
    {
        sLog.outError("%s is unsupported for looting.", GuidString(lguid).c_str());
        return;
    }

    Loot* loot = holder->SpoilsFor(who);
    if (!loot)
    {
        player->SendLootRelease(lguid);
        return;
    }

    Item* pItem = (GuidHigh(lguid) == HIGHGUID_ITEM) ? static_cast<Item*>(holder) : nullptr;

    QuestItem* qitem = nullptr;
    QuestItem* ffaitem = nullptr;
    QuestItem* conditem = nullptr;

    LootItem* item = loot->LootItemInSlot(lootSlot, player, &qitem, &ffaitem, &conditem);

    if (!item)
    {
        player->SendEquipError(EQUIP_ERR_ALREADY_LOOTED, nullptr, nullptr);
        return;
    }

    Group * group = player->GetGroup();

    if (group)
    {
        Occupant * pObject = player->GetMap()->GetOccupant(lguid);

        switch (group->GetLootMethod())
        {
            case GROUP_LOOT:
            case NEED_BEFORE_GREED:
            {
                if ((!item->is_underthreshold && !group->IsRollDoneForItem(pObject, item)) || (item->winner && item->winner != player->GetObjectGuid()))
                {
                    player->SendEquipError(EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW, nullptr, nullptr, item->itemid);
                    return;
                }
                break;
            }
            case MASTER_LOOT:
            {
                if ((item->winner && item->winner != player->GetObjectGuid()) || (!item->winner && !item->is_underthreshold && !item->freeforall))
                {
                    player->SendEquipError(EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW, nullptr, nullptr, item->itemid);
                    return;
                }
                break;
            }
            default:
                break;
        }
    }

    if (!qitem && item->is_blocked)
    {
        player->SendLootRelease(lguid);
        return;
    }

    if (pItem)
    {
        pItem->SetLootState(ITEM_LOOT_CHANGED);
    }

    ItemPosCountVec dest;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item->itemid, item->count);
    if (msg == EQUIP_ERR_OK)
    {
        Item* newitem = player->StoreNewItem(dest, item->itemid, true, item->randomPropertyId);

        if (qitem)
        {
            qitem->is_looted = true;

            if (item->freeforall || loot->GetPlayerQuestItems().size() == 1)
            {
                player->SendNotifyLootItemRemoved(lootSlot);
            }
            else
            {
                loot->NotifyQuestItemRemoved(qitem->index);
            }
        }
        else
        {
            if (ffaitem)
            {

                ffaitem->is_looted = true;
                player->SendNotifyLootItemRemoved(lootSlot);
            }
            else
            {

                if (conditem)
                {
                    conditem->is_looted = true;
                }
                loot->NotifyItemRemoved(lootSlot);
            }
        }

        if (!item->freeforall)
        {
            item->is_looted = true;
        }

        --loot->unlootedCount;

        player->SendNewItem(newitem, uint32(item->count), false, false, true);

    }
    else
    {
        player->SendEquipError(msg, nullptr, nullptr, item->itemid);
    }
}

void spoils::Money(Player& who, WorldPacket& )
{
    DEBUG_LOG("WORLD: CMSG_LOOT_MONEY");

    Player* player = &who;
    ObjectGuid guid = player->GetLootGuid();
    if (!guid)
    {
        return;
    }

    Spoilable* holder = spoils::Holder(who, guid);
    Loot* pLoot = holder ? holder->SpoilsFor(who) : nullptr;

    if (!pLoot)
    {
        return;
    }

    Item* pItem = (GuidHigh(guid) == HIGHGUID_ITEM) ? static_cast<Item*>(holder) : nullptr;

    if ((GuidHigh(guid) == HIGHGUID_CORPSE))
    {
        pLoot->NotifyMoneyRemoved();
        player->ModifyMoney(pLoot->gold);
        pLoot->gold = 0;
        return;
    }

    if (pLoot)
    {
        pLoot->NotifyMoneyRemoved();

        if (!(GuidHigh(guid) == HIGHGUID_ITEM) && player->GetGroup())
        {

            if (player->getClass() == CLASS_ROGUE && who.GetMap()->GetCreature(guid)->Taking().PocketsPicked())
            {
                player->ModifyMoney(pLoot->gold);
            }
            else
            {
                Group* group = player->GetGroup();

                std::vector<Player*> playersNear;
                for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    Player* playerGroup = itr->getSource();
                    if (!playerGroup)
                    {
                        continue;
                    }
                    if (InReach(*player, *playerGroup, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
                    {
                        playersNear.push_back(playerGroup);
                    }
                }

                uint32 money_per_player = uint32((pLoot->gold) / (playersNear.size()));

                for (std::vector<Player*>::const_iterator i = playersNear.begin(); i != playersNear.end(); ++i)
                {
                    (*i)->ModifyMoney(money_per_player);

                    WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4);
                    data << uint32(money_per_player);

                    (*i)->GetSession()->SendPacket(&data);
                }
            }
        }
        else
        {
            player->ModifyMoney(pLoot->gold);
        }

        pLoot->gold = 0;

        if (pItem)
        {
            pItem->SetLootState(ITEM_LOOT_CHANGED);
        }
    }
}

void spoils::Open(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_LOOT");

    ObjectGuid guid = 0;
    recv_data >> guid;

    if (!who.IsAlive())
    {
        return;
    }

    who.SendLoot(guid, LOOT_CORPSE);
}

void spoils::Release(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_LOOT_RELEASE");

    recv_data.read_skip<uint64>();

    if (ObjectGuid lootGuid = session.GetPlayer()->GetLootGuid())
    {
        session.DoLootRelease(lootGuid);
    }
}

void WorldSession::DoLootRelease(ObjectGuid lguid)
{
    Player*  player = GetPlayer();
    Loot*    loot;

    player->SetLootGuid(0);
    player->SendLootRelease(lguid);

    player->RemoveUnitFlag(UNIT_FLAG_LOOTING);

    if (!player->IsInWorld())
    {
        return;
    }

    switch (GuidHigh(lguid))
    {
        case HIGHGUID_GAMEOBJECT:
        {
            GameObject* go = GetPlayer()->GetMap()->GetGameObject(lguid);

            if (!go || ((go->GetOwnerGuid() != _player->GetObjectGuid() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) && !InReach(*go, *_player, INTERACTION_DISTANCE)))
            {
                return;
            }

            loot = &go->loot;

            if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR)
            {

                go->UseDoorOrButton();
            }
            else if (loot->isLooted() || go->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
            {

                if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST)
                {
                    uint32 go_min = go->GetGOInfo()->chest.minSuccessOpens;
                    uint32 go_max = go->GetGOInfo()->chest.maxSuccessOpens;

                    if (go_min != 0 && go_max > go_min)
                    {
                        float amount_rate = sWorld.getConfig(CONFIG_FLOAT_RATE_MINING_AMOUNT);
                        float min_amount = go_min * amount_rate;
                        float max_amount = go_max * amount_rate;

                        UserTally& tally = go->Behaves<ChestBehaviour>()->Tally();
                        tally.Used();
                        float uses = float(tally.Uses());

                        if (uses < max_amount)
                        {
                            if (uses >= min_amount)
                            {
                                float chance_rate = sWorld.getConfig(CONFIG_FLOAT_RATE_MINING_NEXT);

                                int32 ReqValue = 175;
                                LockEntry const* lockInfo = sLockStore.LookupEntry(go->GetGOInfo()->chest.lockId);
                                if (lockInfo)
                                {
                                    ReqValue = lockInfo->Skill[0];
                                }
                                float skill = float(player->GetSkillValue(SKILL_MINING)) / (ReqValue + 25);
                                double chance = pow(0.8 * chance_rate, 4 * (1 / double(max_amount)) * double(uses));
                                if (roll_chance_f(float(100.0f * chance + skill)))
                                {
                                    go->SetLootState(GO_READY);
                                }
                                else
                                {
                                    go->SetLootState(GO_JUST_DEACTIVATED);
                                }
                            }
                            else
                            {
                                go->SetLootState(GO_READY);
                            }
                        }
                        else
                        {
                            go->SetLootState(GO_JUST_DEACTIVATED);
                        }
                    }
                    else
                    {
                        go->SetLootState(GO_JUST_DEACTIVATED);
                    }
                }
                else if (go->GetGoType() == GAMEOBJECT_TYPE_FISHINGHOLE)
                {

                    UserTally& tally = go->Behaves<FishingHoleBehaviour>()->Tally();
                    tally.Used();
                    if (tally.Uses() >= urand(go->GetGOInfo()->fishinghole.minSuccessOpens, go->GetGOInfo()->fishinghole.maxSuccessOpens))
                    {
                        go->SetLootState(GO_JUST_DEACTIVATED);
                    }
                    else
                    {
                        go->SetLootState(GO_READY);
                    }
                }
                else
                {
                    go->SetLootState(GO_JUST_DEACTIVATED);
                }

                loot->clear();
            }
            else

            {
                go->SetLootState(GO_ACTIVATED);
            }

            go->SetGoState(GO_STATE_READY);

            break;
        }

        case HIGHGUID_CORPSE:
        {

            Corpse* corpse = _player->GetMap()->GetCorpse(lguid);

            if (!corpse || !InReach(*corpse, *_player, INTERACTION_DISTANCE))
            {
                return;
            }

            loot = &corpse->loot;

            if (loot->isLooted())
            {
                loot->clear();
                corpse->RemoveCorpseDynFlag(CORPSE_DYNFLAG_LOOTABLE);
            }
            break;
        }
        case HIGHGUID_ITEM:
        {
            Item* pItem = player->GetItemByGuid(lguid);
            if (!pItem)
            {
                return;
            }

            switch (pItem->loot.loot_type)
            {

                case LOOT_DISENCHANTING:
                {
                    if (!pItem->loot.isLooted())
                    {
                        player->AutoStoreLoot(pItem->loot);
                    }
                    pItem->loot.clear();
                    pItem->SetLootState(ITEM_LOOT_REMOVED);
                    player->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
                    break;
                }

                default:
                {

                    if (pItem->loot.isLooted())
                    {
                        pItem->SetLootState(ITEM_LOOT_REMOVED);
                        player->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
                    }
                    break;
                }
            }
            return;
        }
        case HIGHGUID_UNIT:
        {

            Creature* pCreature = GetPlayer()->GetMap()->GetCreature(lguid);

            bool ok_loot = (pCreature &&
                pCreature->IsAlive() ==
                (player->getClass() == CLASS_ROGUE && pCreature->Taking().PocketsPicked()));
            if (!ok_loot || !InReach(*pCreature, *_player, INTERACTION_DISTANCE))
            {
                return;
            }

            loot = &pCreature->loot;

            if (!loot->isLooted())
            {
                Group const* group = pCreature->Claim().HoldingGroup();
                if (group && !pCreature->Taking().Opened())
                {

                    switch (group->GetLootMethod())
                    {
                        case FREE_FOR_ALL:
                        case NEED_BEFORE_GREED:
                        case ROUND_ROBIN:
                        case GROUP_LOOT:
                        {
                            pCreature->Taking().Opened(true);
                            break;
                        }
                        case MASTER_LOOT:
                        {
                            pCreature->Taking().Opened((group->GetLooterGuid() == player->GetObjectGuid()));
                            break;
                        }

                    }
                    pCreature->ResendField(UNIT_DYNAMIC_FLAGS);
                }
            }

            if (loot->isLooted() && !pCreature->IsAlive())
            {

                pCreature->PrepareBodyLootState();
                pCreature->AllLootRemovedFromCorpse();
            }
            break;
        }
        default:
        {
            sLog.outError("%s is unsupported for looting.", GuidString(lguid).c_str());
            return;
        }
    }

    loot->RemoveLooter(player->GetObjectGuid());
}

void spoils::MasterGive(Player& who, WorldPacket& recv_data)
{
    uint8 slotid;
    ObjectGuid lootguid = 0;
    ObjectGuid target_playerguid = 0;

    recv_data >> lootguid >> slotid >> target_playerguid;

    if (!who.GetGroup() || who.GetGroup()->GetLooterGuid() != who.GetObjectGuid())
    {
        who.SendLootRelease(who.GetLootGuid());
        return;
    }

    Player* target = sPlayerRegistry.Find(target_playerguid);
    if (!target)
    {
        return;
    }

    DEBUG_LOG("WorldSession::HandleLootMasterGiveOpcode (CMSG_LOOT_MASTER_GIVE, 0x02A3) Target = %s [%s].", GuidString(target_playerguid).c_str(), target->GetName());

    if (who.GetLootGuid() != lootguid)
    {
        return;
    }

    if (!(GuidHigh(lootguid) == HIGHGUID_UNIT) && !(GuidHigh(lootguid) == HIGHGUID_GAMEOBJECT))
    {
        return;
    }

    Spoilable* holder = spoils::Holder(who, lootguid);
    Loot* pLoot = holder ? holder->Spoils() : nullptr;

    if (!pLoot)
    {
        return;
    }

    if (slotid > pLoot->items.size())
    {
        DEBUG_LOG("AutoLootItem: Player %s might be using a hack! (slot %d, size %zu)", who.GetName(), slotid, pLoot->items.size());
        return;
    }

    LootItem& item = pLoot->items[slotid];

    ItemPosCountVec dest;
    InventoryResult msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemid, item.count);
    if (msg != EQUIP_ERR_OK)
    {

        item.winner = target->GetObjectGuid();
        target->SendEquipError(msg, nullptr, nullptr, item.itemid);

        pLoot->NotifyItemRemoved(slotid);

        return;
    }

    Item* newitem = target->StoreNewItem(dest, item.itemid, true, item.randomPropertyId);
    target->SendNewItem(newitem, uint32(item.count), false, false, true);

    item.count = 0;
    item.is_looted = true;

    pLoot->NotifyItemRemoved(slotid);
    --pLoot->unlootedCount;
}
