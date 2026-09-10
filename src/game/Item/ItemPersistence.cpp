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
#include <string>
#include "Item.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "Database/DatabaseEnv.h"
#include "ItemEnchantmentMgr.h"
#include "SQLStorages.h"

bool Item::Create(uint32 guidlow, uint32 itemid, Player const* owner)
{
    Object::_Create(guidlow, 0, HIGHGUID_ITEM);

    SetEntry(itemid);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(ITEM_FIELD_OWNER, owner ? owner->GetObjectGuid() : 0);
    SetGuidValue(ITEM_FIELD_CONTAINED, 0);

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);
    if (!itemProto)
    {
        return false;
    }

    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);
    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        SetSpellCharges(i, itemProto->Spells[i].SpellCharges);
    }

    SetUInt32Value(ITEM_FIELD_DURATION, itemProto->Duration);

    return true;
}

bool Item::IsNotEmptyBag() const
{
    if (Bag const* bag = ToBag())
    {
        return !bag->IsEmpty();
    }
    return false;
}

void Item::SaveToDB()
{
    uint32 guid = GetGUIDLow();
    switch (uState)
    {
        case ITEM_NEW:
        {
            static SqlStatementID delItem ;
            static SqlStatementID insItem ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(delItem, "DELETE FROM `item_instance` WHERE `guid` = ?");
            stmt.PExecute(guid);

            stmt = CharacterDatabase.CreateStatement(insItem, "INSERT INTO `item_instance` (`guid`,`owner_guid`,`data`,`text`) VALUES (?, ?, ?, ?)");
            stmt.PExecute(guid, GuidCounter(GetOwnerGuid()), SaveFields(0, GetValuesCount()).c_str(), m_text.c_str());
        } break;
        case ITEM_CHANGED:
        {
            std::string text = m_text;
            CharacterDatabase.escape_string(text);
            static SqlStatementID updInstance ;
            static SqlStatementID updGifts ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(updInstance, "UPDATE `item_instance` SET `data` = ?, `owner_guid` = ?, `text` = ? WHERE `guid` = ?");

            stmt.PExecute(SaveFields(0, GetValuesCount()).c_str(), GuidCounter(GetOwnerGuid()), m_text.c_str(), guid);

            if (HasItemFlag(ITEM_DYNFLAG_WRAPPED))
            {
                stmt = CharacterDatabase.CreateStatement(updGifts, "UPDATE `character_gifts` SET `guid` = ? WHERE `item_guid` = ?");
                stmt.PExecute(GuidCounter(GetOwnerGuid()), GetGUIDLow());
            }
        } break;
        case ITEM_REMOVED:
        {
            static SqlStatementID delInst ;
            static SqlStatementID delGifts ;
            static SqlStatementID delLoot ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(delInst, "DELETE FROM `item_instance` WHERE `guid` = ?");
            stmt.PExecute(guid);

            if (HasItemFlag(ITEM_DYNFLAG_WRAPPED))
            {
                stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
                stmt.PExecute(GetGUIDLow());
            }

            if (HasSavedLoot())
            {
                stmt = CharacterDatabase.CreateStatement(delLoot, "DELETE FROM `item_loot` WHERE `guid` = ?");
                stmt.PExecute(GetGUIDLow());
            }

            delete this;
            return;
        }
        case ITEM_UNCHANGED:
            return;
    }

    if (m_lootState == ITEM_LOOT_CHANGED || m_lootState == ITEM_LOOT_REMOVED)
    {
        static SqlStatementID delLoot ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delLoot, "DELETE FROM `item_loot` WHERE `guid` = ?");
        stmt.PExecute(GetGUIDLow());
    }

    if (m_lootState == ITEM_LOOT_NEW || m_lootState == ITEM_LOOT_CHANGED)
    {
        if (Player* owner = GetOwner())
        {
            static SqlStatementID saveGold ;
            static SqlStatementID saveLoot ;

            if (loot.gold)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(saveGold, "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`property`) VALUES (?, ?, 0, ?, 0)");
                stmt.PExecute(GetGUIDLow(), owner->GetGUIDLow(), loot.gold);
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(saveLoot, "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`property`) VALUES (?, ?, ?, ?, ?)");

            for (size_t i = 0; i < loot.GetMaxSlotInLootFor(owner); ++i)
            {
                QuestItem* qitem = nullptr;

                LootItem* item = loot.LootItemInSlot(i, owner, &qitem);
                if (!item)
                {
                    continue;
                }

                if (!qitem && item->is_blocked)
                {
                    continue;
                }

                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(owner->GetGUIDLow());
                stmt.addUInt32(item->itemid);
                stmt.addUInt8(item->count);
                stmt.addInt32(item->randomPropertyId);

                stmt.Execute();
            }
        }
    }

    if (m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_TEMPORARY)
    {
        SetLootState(ITEM_LOOT_UNCHANGED);
    }

    SetState(ITEM_UNCHANGED);
}

bool Item::LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid)
{

    Object::_Create(guidLow, 0, HIGHGUID_ITEM);

    if (!LoadFields(fields[0].GetString(), 0, GetValuesCount()))
    {
        sLog.outError("Item #%d have broken data in `data` field. Can't be loaded.", guidLow);
        return false;
    }

    SetText(fields[1].GetCppString());

    bool need_save = false;

    ObjectGuid new_item_guid = MakeGuid(HIGHGUID_ITEM, guidLow);
    if (GetGuidValue(OBJECT_FIELD_GUID) != new_item_guid)
    {
        SetGuidValue(OBJECT_FIELD_GUID, new_item_guid);
        need_save = true;
    }

    ItemPrototype const* proto = GetProto();
    if (!proto)
    {
        return false;
    }

    if (proto->MaxDurability != GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
    {
        SetUInt32Value(ITEM_FIELD_MAXDURABILITY, proto->MaxDurability);
        if (GetUInt32Value(ITEM_FIELD_DURABILITY) > proto->MaxDurability)
        {
            SetUInt32Value(ITEM_FIELD_DURABILITY, proto->MaxDurability);
        }

        need_save = true;
    }

    if (IsSoulBound() && proto->Bonding == NO_BIND)
    {
        ApplyItemFlag(ITEM_DYNFLAG_BINDED, false);
        need_save = true;
    }

    if ((proto->Duration == 0) != (GetUInt32Value(ITEM_FIELD_DURATION) == 0))
    {
        SetUInt32Value(ITEM_FIELD_DURATION, proto->Duration);
        need_save = true;
    }

    if (ownerGuid && GetOwnerGuid() != ownerGuid)
    {
        SetOwnerGuid(ownerGuid);
        need_save = true;
    }

    if (HasItemFlag(ITEM_DYNFLAG_WRAPPED))
    {

        if (!(proto->Flags & ITEM_FLAG_WRAPPER) || GetMaxStackCount() > 1)
        {
            RemoveItemFlag(ITEM_DYNFLAG_WRAPPED);
            need_save = true;

            static SqlStatementID delGifts ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
            stmt.PExecute(GetGUIDLow());
        }
    }

    if (need_save)
    {
        static SqlStatementID updItem ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(updItem, "UPDATE `item_instance` SET `data` = ?, `owner_guid` = ? WHERE `guid` = ?");

        stmt.addString(SaveFields(0, GetValuesCount()));
        stmt.addUInt32(GuidCounter(GetOwnerGuid()));
        stmt.addUInt32(guidLow);
        stmt.Execute();
    }

    return true;
}

void Item::LoadLootFromDB(Field* fields)
{
    uint32 item_id     = fields[1].GetUInt32();
    uint32 item_amount = fields[2].GetUInt32();
    int32  item_propid = fields[3].GetInt32();

    if (item_id == 0)
    {
        loot.gold = item_amount;
        SetLootState(ITEM_LOOT_UNCHANGED);
        return;
    }

    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

    if (!proto)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_loot` WHERE `guid` = '%u' AND `itemid` = '%u'", GetGUIDLow(), item_id);
        sLog.outError("Item::LoadLootFromDB: %s has an unknown item (id: #%u) in item_loot, deleted.", GuidString(GetOwnerGuid()).c_str(), item_id);
        return;
    }

    loot.items.push_back(LootItem(item_id, item_amount, item_propid));
    ++loot.unlootedCount;

    SetLootState(ITEM_LOOT_UNCHANGED);
}

void Item::DeleteFromDB()
{
    static SqlStatementID delItem ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delItem, "DELETE FROM `item_instance` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());
}

void Item::DeleteFromInventoryDB()
{
    static SqlStatementID delInv ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delInv, "DELETE FROM `character_inventory` WHERE `item` = ?");
    stmt.PExecute(GetGUIDLow());
}
