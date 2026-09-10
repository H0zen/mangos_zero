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

#pragma once

#include "Loot/Spoilable.h"
#include "Platform/Define.h"
#include <string>
#include "Object.h"
#include "LootMgr.h"
#include "ItemPrototype.h"

struct SpellEntry;
class Bag;
class Field;
class QueryResult;
class Unit;

struct ItemSetEffect
{
    uint32 setid;
    uint32 item_count;
    SpellEntry const* spells[8];
};

enum InventoryResult
{
    EQUIP_ERR_OK                                 = 0,
    EQUIP_ERR_CANT_EQUIP_LEVEL_I                 = 1,
    EQUIP_ERR_CANT_EQUIP_SKILL                   = 2,
    EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT             = 3,
    EQUIP_ERR_BAG_FULL                           = 4,
    EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG        = 5,
    EQUIP_ERR_CANT_TRADE_EQUIP_BAGS              = 6,
    EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE              = 7,
    EQUIP_ERR_NO_REQUIRED_PROFICIENCY            = 8,
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE        = 9,
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM        = 10,
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM2       = 11,
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE2       = 12,
    EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED          = 13,
    EQUIP_ERR_CANT_DUAL_WIELD                    = 14,
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG            = 15,
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2           = 16,
    EQUIP_ERR_CANT_CARRY_MORE_OF_THIS            = 17,
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE3       = 18,
    EQUIP_ERR_ITEM_CANT_STACK                    = 19,
    EQUIP_ERR_ITEM_CANT_BE_EQUIPPED              = 20,
    EQUIP_ERR_ITEMS_CANT_BE_SWAPPED              = 21,
    EQUIP_ERR_SLOT_IS_EMPTY                      = 22,
    EQUIP_ERR_ITEM_NOT_FOUND                     = 23,
    EQUIP_ERR_CANT_DROP_SOULBOUND                = 24,
    EQUIP_ERR_OUT_OF_RANGE                       = 25,
    EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT     = 26,
    EQUIP_ERR_COULDNT_SPLIT_ITEMS                = 27,
    EQUIP_ERR_MISSING_REAGENT                    = 28,
    EQUIP_ERR_NOT_ENOUGH_MONEY                   = 29,
    EQUIP_ERR_NOT_A_BAG                          = 30,
    EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS        = 31,
    EQUIP_ERR_DONT_OWN_THAT_ITEM                 = 32,
    EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER             = 33,
    EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT        = 34,
    EQUIP_ERR_TOO_FAR_AWAY_FROM_BANK             = 35,
    EQUIP_ERR_ITEM_LOCKED                        = 36,
    EQUIP_ERR_YOU_ARE_STUNNED                    = 37,
    EQUIP_ERR_YOU_ARE_DEAD                       = 38,
    EQUIP_ERR_CANT_DO_RIGHT_NOW                  = 39,
    EQUIP_ERR_INT_BAG_ERROR                      = 40,
    EQUIP_ERR_CAN_EQUIP_ONLY1_BOLT               = 41,
    EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH          = 42,
    EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED          = 43,
    EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED           = 44,
    EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED            = 45,
    EQUIP_ERR_BOUND_CANT_BE_WRAPPED              = 46,
    EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED             = 47,
    EQUIP_ERR_BAGS_CANT_BE_WRAPPED               = 48,
    EQUIP_ERR_ALREADY_LOOTED                     = 49,
    EQUIP_ERR_INVENTORY_FULL                     = 50,
    EQUIP_ERR_BANK_FULL                          = 51,
    EQUIP_ERR_ITEM_IS_CURRENTLY_SOLD_OUT         = 52,
    EQUIP_ERR_BAG_FULL3                          = 53,
    EQUIP_ERR_ITEM_NOT_FOUND2                    = 54,
    EQUIP_ERR_ITEM_CANT_STACK2                   = 55,
    EQUIP_ERR_BAG_FULL4                          = 56,
    EQUIP_ERR_ITEM_SOLD_OUT                      = 57,
    EQUIP_ERR_OBJECT_IS_BUSY                     = 58,
    EQUIP_ERR_NONE                               = 59,
    EQUIP_ERR_NOT_IN_COMBAT                      = 60,
    EQUIP_ERR_NOT_WHILE_DISARMED                 = 61,
    EQUIP_ERR_BAG_FULL6                          = 62,
    EQUIP_ERR_CANT_EQUIP_RANK                    = 63,
    EQUIP_ERR_CANT_EQUIP_REPUTATION              = 64,
    EQUIP_ERR_TOO_MANY_SPECIAL_BAGS              = 65,
    EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW            = 66,

};

enum BuyResult
{
    BUY_ERR_CANT_FIND_ITEM                      = 0,
    BUY_ERR_ITEM_ALREADY_SOLD                   = 1,
    BUY_ERR_NOT_ENOUGHT_MONEY                   = 2,
    BUY_ERR_SELLER_DONT_LIKE_YOU                = 4,
    BUY_ERR_DISTANCE_TOO_FAR                    = 5,
    BUY_ERR_ITEM_SOLD_OUT                       = 7,
    BUY_ERR_CANT_CARRY_MORE                     = 8,
    BUY_ERR_RANK_REQUIRE                        = 11,
    BUY_ERR_REPUTATION_REQUIRE                  = 12
};

enum SellResult
{
    SELL_ERR_CANT_FIND_ITEM                      = 1,
    SELL_ERR_CANT_SELL_ITEM                      = 2,
    SELL_ERR_CANT_FIND_VENDOR                    = 3,
    SELL_ERR_YOU_DONT_OWN_THAT_ITEM              = 4,
    SELL_ERR_UNK                                 = 5,
    SELL_ERR_ONLY_EMPTY_BAG                      = 6
};

enum EnchantmentSlot
{
    PERM_ENCHANTMENT_SLOT           = 0,
    TEMP_ENCHANTMENT_SLOT           = 1,
    MAX_INSPECTED_ENCHANTMENT_SLOT  = 2,

    PROP_ENCHANTMENT_SLOT_0     = 3,
    PROP_ENCHANTMENT_SLOT_1     = 4,
    PROP_ENCHANTMENT_SLOT_2     = 5,
    PROP_ENCHANTMENT_SLOT_3     = 6,
    MAX_ENCHANTMENT_SLOT        = 7
};

#define MAX_VISIBLE_ITEM_OFFSET       12

enum EnchantmentOffset
{
    ENCHANTMENT_ID_OFFSET       = 0,
    ENCHANTMENT_DURATION_OFFSET = 1,
    ENCHANTMENT_CHARGES_OFFSET  = 2
};

#define MAX_ENCHANTMENT_OFFSET    3

enum EnchantmentSlotMask
{
    ENCHANTMENT_CAN_SOULBOUND  = 0x01,
    ENCHANTMENT_UNK1           = 0x02,
    ENCHANTMENT_UNK2           = 0x04,
    ENCHANTMENT_UNK3           = 0x08
};

enum ItemUpdateState
{
    ITEM_UNCHANGED                               = 0,
    ITEM_CHANGED                                 = 1,
    ITEM_NEW                                     = 2,
    ITEM_REMOVED                                 = 3
};

enum ItemLootUpdateState
{
    ITEM_LOOT_NONE                                = 0,
    ITEM_LOOT_TEMPORARY                           = 1,
    ITEM_LOOT_UNCHANGED                           = 2,
    ITEM_LOOT_CHANGED                             = 3,
    ITEM_LOOT_NEW                                 = 4,
    ITEM_LOOT_REMOVED                             = 5
};

enum ItemDynFlags
{
    ITEM_DYNFLAG_BINDED                       = 0x00000001,
    ITEM_DYNFLAG_UNK1                         = 0x00000002,
    ITEM_DYNFLAG_UNLOCKED                     = 0x00000004,
    ITEM_DYNFLAG_WRAPPED                      = 0x00000008,
    ITEM_DYNFLAG_UNK4                         = 0x00000010,
    ITEM_DYNFLAG_UNK5                         = 0x00000020,
    ITEM_DYNFLAG_UNK6                         = 0x00000040,
    ITEM_DYNFLAG_UNK7                         = 0x00000080,
    ITEM_DYNFLAG_UNK8                         = 0x00000100,
    ITEM_DYNFLAG_READABLE                     = 0x00000200,
    ITEM_DYNFLAG_UNK10                        = 0x00000400,
    ITEM_DYNFLAG_UNK11                        = 0x00000800,
    ITEM_DYNFLAG_UNK12                        = 0x00001000,
    ITEM_DYNFLAG_UNK13                        = 0x00002000,
    ITEM_DYNFLAG_UNK14                        = 0x00004000,
    ITEM_DYNFLAG_UNK15                        = 0x00008000,
    ITEM_DYNFLAG_UNK16                        = 0x00010000,
    ITEM_DYNFLAG_UNK17                        = 0x00020000,
};

enum ItemRequiredTargetType
{
    ITEM_TARGET_TYPE_CREATURE   = 1,
    ITEM_TARGET_TYPE_DEAD       = 2
};

#define MAX_ITEM_REQ_TARGET_TYPE 2

struct ItemRequiredTarget
{
    ItemRequiredTarget(ItemRequiredTargetType uiType, uint32 uiTargetEntry) : m_uiType(uiType), m_uiTargetEntry(uiTargetEntry) {}
    ItemRequiredTargetType m_uiType;
    uint32 m_uiTargetEntry;

    bool IsFitToRequirements(Unit* pUnitTarget) const;
};

bool ItemCanGoIntoBag(ItemPrototype const* proto, ItemPrototype const* pBagProto);

class Item : public Object, public Spoilable
{
    public:
        static Item* CreateItem(uint32 item, uint32 count, Player const* player = nullptr, uint32 randomPropertyId = 0);
        Item* CloneItem(uint32 count, Player const* player = nullptr) const;

        Item();
        ~Item();

        virtual bool Create(uint32 guidlow, uint32 itemid, Player const* owner);

        ItemPrototype const* GetProto() const;

        ObjectGuid const& GetOwnerGuid() const { return GetGuidValue(ITEM_FIELD_OWNER); }
        void SetOwnerGuid(ObjectGuid guid) { SetGuidValue(ITEM_FIELD_OWNER, guid); }
        Player* GetOwner()const;

        void SetBinding(bool val) { ApplyItemFlag(ITEM_DYNFLAG_BINDED, val); }
        bool IsSoulBound() const { return HasItemFlag(ITEM_DYNFLAG_BINDED); }

        bool HasItemFlag(uint32 flag) const { return HasFlag(ITEM_FIELD_FLAGS, flag); }
        void SetItemFlag(uint32 flag) { SetFlag(ITEM_FIELD_FLAGS, flag); }
        void RemoveItemFlag(uint32 flag) { RemoveFlag(ITEM_FIELD_FLAGS, flag); }
        void ApplyItemFlag(uint32 flag, bool apply) { ApplyModFlag(ITEM_FIELD_FLAGS, flag, apply); }
        uint32 GetItemFlags() const { return GetUInt32Value(ITEM_FIELD_FLAGS); }
        void SetAllItemFlags(uint32 flags) { SetUInt32Value(ITEM_FIELD_FLAGS, flags); }

        ObjectGuid const& GetCreatorGuid() const { return GetGuidValue(ITEM_FIELD_CREATOR); }
        void SetCreatorGuid(ObjectGuid const& guid) { SetGuidValue(ITEM_FIELD_CREATOR, guid); }
        ObjectGuid const& GetGiftCreatorGuid() const { return GetGuidValue(ITEM_FIELD_GIFTCREATOR); }
        void SetGiftCreatorGuid(ObjectGuid const& guid) { SetGuidValue(ITEM_FIELD_GIFTCREATOR, guid); }
        bool IsBindedNotWith(Player const* player) const;
        bool IsBoundByEnchant() const;
        virtual void SaveToDB();
        virtual bool LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid = 0);
        virtual void DeleteFromDB();
        void DeleteFromInventoryDB();
        void LoadLootFromDB(Field* fields);

        Bag* ToBag()
        {
            if (IsBag())
            {
                return reinterpret_cast<Bag*>(this);
            }
            else
            {
                return nullptr;
            }
        }
        const Bag* ToBag() const { if (IsBag()) return reinterpret_cast<const Bag*>(this); else return nullptr; }

        bool IsLocked() const { return !HasItemFlag(ITEM_DYNFLAG_UNLOCKED); }
        bool IsBag() const { return GetProto()->InventoryType == INVTYPE_BAG; }
        bool IsNotEmptyBag() const;
        bool IsBroken() const { return GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && GetUInt32Value(ITEM_FIELD_DURABILITY) == 0; }
        bool CanBeTraded() const;
        void SetInTrade(bool b = true) { mb_in_trade = b; }
        bool IsInTrade() const { return mb_in_trade; }

        bool IsFitToSpellRequirements(SpellEntry const* spellInfo) const;
        bool IsTargetValidForItemUse(Unit* pUnitTarget);
        bool IsLimitedToAnotherMapOrZone(uint32 cur_mapId, uint32 cur_zoneId) const;

        uint32 GetCount() const { return GetUInt32Value(ITEM_FIELD_STACK_COUNT); }
        void SetCount(uint32 value) { SetUInt32Value(ITEM_FIELD_STACK_COUNT, value); }
        uint32 GetMaxStackCount() const { return GetProto()->GetMaxStackSize(); }
        InventoryResult CanBeMergedPartlyWith(ItemPrototype const* proto) const;

        uint8 GetSlot() const {return m_slot;}
        Bag* GetContainer()
        {
            return m_container;
        }

        uint8 GetBagSlot() const;
        void SetSlot(uint8 slot) {m_slot = slot;}
        uint16 GetPos() const { return uint16(GetBagSlot()) << 8 | GetSlot(); }
        void SetContainer(Bag* container) { m_container = container; }

        bool IsInBag() const { return m_container != nullptr; }
        bool IsEquipped() const;

        uint32 GetSkill();
        uint32 GetSpell();

        int32 GetItemRandomPropertyId() const { return GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID); }
        uint32 GetItemSuffixFactor() const { return GetUInt32Value(ITEM_FIELD_PROPERTY_SEED); }
        void SetItemRandomProperties(int32 randomPropId);
        static int32 GenerateItemRandomPropertyId(uint32 item_id);
        void SetEnchantment(EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges);
        void SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration);
        void SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges);
        void ClearEnchantment(EnchantmentSlot slot);
        uint32 GetEnchantmentId(EnchantmentSlot slot)       const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_ID_OFFSET);}
        uint32 GetEnchantmentDuration(EnchantmentSlot slot) const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET);}
        uint32 GetEnchantmentCharges(EnchantmentSlot slot)  const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET);}

        std::string const& GetText() const { return m_text; }
        void SetText(std::string const& text) { m_text = text; }

        void SendTimeUpdate(Player* owner);

        bool SpendDuration(uint32 elapsed, Player* holder);

        int32 GetSpellCharges(uint8 index = 0) const { return GetInt32Value(ITEM_FIELD_SPELL_CHARGES + index); }
        void SetSpellCharges(uint8 index, int32 value) { SetInt32Value(ITEM_FIELD_SPELL_CHARGES + index, value); }

        Loot loot;

        Loot* Spoils() override { return &loot; }

        bool OpenableBy(Player const& who) const override { return HasGeneratedLoot(); }
        bool FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission) override;

        void SetLootState(ItemLootUpdateState state);
        bool HasGeneratedLoot() const { return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_REMOVED; }
        bool HasTemporaryLoot() const { return m_lootState == ITEM_LOOT_TEMPORARY; }

        bool HasSavedLoot() const { return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_NEW && m_lootState != ITEM_LOOT_TEMPORARY; }

        ItemUpdateState GetState() const { return uState; }
        void SetState(ItemUpdateState state, Player* forplayer = nullptr);
        void FSetState(ItemUpdateState state)
        {
            uState = state;
        }

        bool IsPotion() const { return GetProto()->IsPotion(); }
        bool IsConjuredConsumable() const { return GetProto()->IsConjuredConsumable(); }
        bool IsWeaponVellum() const { return GetProto()->IsWeaponVellum(); }
        bool IsArmorVellum() const { return GetProto()->IsArmorVellum(); }
        uint32 GetScriptId() const;

        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType& update_players) override;
    private:
        std::string m_text;
        uint8 m_slot;
        Bag* m_container;
        ItemUpdateState uState;
        bool mb_in_trade;
        ItemLootUpdateState m_lootState;
};
