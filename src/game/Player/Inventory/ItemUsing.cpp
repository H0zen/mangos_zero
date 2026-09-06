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

#include "Player.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "DBCStores.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SQLStorages.h"
#include "DisableMgr.h"

InventoryResult Player::CanEquipNewItem(uint8 slot, uint16& dest, uint32 item, bool swap) const
{
    dest = 0;
    Item* pItem = Item::CreateItem(item, 1, this);
    if (pItem)
    {
        InventoryResult result = CanEquipItem(slot, dest, pItem, swap);
        delete pItem;
        return result;
    }

    return EQUIP_ERR_ITEM_NOT_FOUND;
}

/**
 * @brief Checks whether an item can be equipped and resolves its destination slot.
 *
 * @param slot The preferred equipment slot.
 * @param dest Output packed destination slot.
 * @param pItem The item to equip.
 * @param swap True to allow replacing an existing item.
 * @param direct_action True if this is an immediate player action.
 * @return The inventory result for the equip attempt.
 */
InventoryResult Player::CanEquipItem(uint8 slot, uint16& dest, Item* pItem, bool swap, bool direct_action) const
{
    dest = 0;
    if (pItem)
    {
        DEBUG_LOG("STORAGE: CanEquipItem slot = %u, item = %u, count = %u", slot, pItem->GetEntry(), pItem->GetCount());
        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            // item used
            if (pItem->HasTemporaryLoot())
            {
                return EQUIP_ERR_ALREADY_LOOTED;
            }

            if (pItem->IsBindedNotWith(this))
            {
                return EQUIP_ERR_DONT_OWN_THAT_ITEM;
            }

            // check count of items (skip for auto move for same player from bank)
            InventoryResult res = CanTakeMoreSimilarItems(pItem);
            if (res != EQUIP_ERR_OK)
            {
                return res;
            }

            // check this only in game
            if (direct_action)
            {
                // May be here should be more stronger checks; STUNNED checked
                // ROOT, CONFUSED, DISTRACTED, FLEEING this needs to be checked.
                if (hasUnitState(UNIT_STAT_STUNNED))
                {
                    return EQUIP_ERR_YOU_ARE_STUNNED;
                }

                // do not allow equipping gear except weapons, offhands, projectiles, relics in
                // - combat
                if (!pProto->CanChangeEquipStateInCombat())
                {
                    if (IsInCombat())
                    {
                        return EQUIP_ERR_NOT_IN_COMBAT;
                    }
                }

                // prevent equip item in process logout
                if (GetSession()->isLogingOut())
                {
                    return EQUIP_ERR_YOU_ARE_STUNNED;
                }

                if (IsInCombat() && pProto->Class == ITEM_CLASS_WEAPON && Arms().ChangeTimer() != 0)
                {
                    return EQUIP_ERR_CANT_DO_RIGHT_NOW; // maybe exist better err
                }

                if (IsNonMeleeSpellCasted(false))
                {
                    return EQUIP_ERR_CANT_DO_RIGHT_NOW;
                }

                // prevent equip item in Spirit of Redemption (Aura: 27827)
                if (HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
                {
                    return EQUIP_ERR_CANT_DO_RIGHT_NOW;
                }
            }

            uint8 eslot = FindEquipSlot(pProto, slot, swap);
            if (eslot == NULL_SLOT)
            {
                return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
            }

            InventoryResult msg = CanUseItem(pItem , direct_action);
            if (msg != EQUIP_ERR_OK)
            {
                return msg;
            }
            if (!swap && GetItemByPos(INVENTORY_SLOT_BAG_0, eslot))
            {
                return EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE;
            }

            // if swap ignore item (equipped also)
            if (InventoryResult res2 = CanEquipUniqueItem(pItem, swap ? eslot : uint8(NULL_SLOT)))
            {
                return res2;
            }

            // check unique-equipped special item classes
            if (pProto->Class == ITEM_CLASS_QUIVER)
            {
                for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
                {
                    if (Item* pBag = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                    {
                        if (pBag != pItem)
                        {
                            if (ItemPrototype const* pBagProto = pBag->GetProto())
                            {
                                if (pBagProto->Class == pProto->Class && (!swap || pBag->GetSlot() != eslot))
                                {
                                    return (pBagProto->SubClass == ITEM_SUBCLASS_AMMO_POUCH)
                                        ? EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH
                                        : EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER;
                                }
                            }
                        }
                    }
                }
            }

            uint32 type = pProto->InventoryType;

            if (eslot == EQUIPMENT_SLOT_OFFHAND)
            {
                if (type == INVTYPE_WEAPON || type == INVTYPE_WEAPONOFFHAND)
                {
                    if (!Arms().CanDualWield())
                    {
                        return EQUIP_ERR_CANT_DUAL_WIELD;
                    }
                }
                else if (type == INVTYPE_2HWEAPON)
                {
                    return EQUIP_ERR_CANT_DUAL_WIELD;
                }

                if (IsTwoHandUsed())
                {
                    return EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED;
                }
            }

            // equip two-hand weapon case (with possible unequip 2 items)
            if (type == INVTYPE_2HWEAPON)
            {
                if (eslot != EQUIPMENT_SLOT_MAINHAND)
                {
                    return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
                }

                // offhand item must can be stored in inventory for offhand item and it also must be unequipped
                Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                ItemPosCountVec off_dest;
                if (offItem && (!direct_action ||
                    CanUnequipItem(uint16(INVENTORY_SLOT_BAG_0) << 8 | EQUIPMENT_SLOT_OFFHAND, false) !=  EQUIP_ERR_OK ||
                    CanStoreItem(NULL_BAG, NULL_SLOT, off_dest, offItem, false) !=  EQUIP_ERR_OK))
                {
                    return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_INVENTORY_FULL;
                }
            }
            dest = ((INVENTORY_SLOT_BAG_0 << 8) | eslot);
            return EQUIP_ERR_OK;
        }
    }

    return !swap ? EQUIP_ERR_ITEM_NOT_FOUND : EQUIP_ERR_ITEMS_CANT_BE_SWAPPED;
}

InventoryResult Player::CanUseItem(Item* pItem, bool direct_action) const
{
    if (pItem)
    {
        DEBUG_LOG("STORAGE: CanUseItem item = %u", pItem->GetEntry());

        if (!IsAlive() && direct_action)
        {
            return EQUIP_ERR_YOU_ARE_DEAD;
        }

        // if (isStunned())
        //    return EQUIP_ERR_YOU_ARE_STUNNED;

        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            if (pItem->IsBindedNotWith(this))
            {
                return EQUIP_ERR_DONT_OWN_THAT_ITEM;
            }

            InventoryResult msg = CanUseItem(pProto, direct_action);
            if (msg != EQUIP_ERR_OK)
            {
                return msg;
            }

            if (pItem->GetSkill() != 0)
            {
                if (GetSkillValue(pItem->GetSkill()) == 0)
                {
                    return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
                }
            }

            if (pProto->RequiredReputationFaction && uint32(GetReputationRank(pProto->RequiredReputationFaction)) < pProto->RequiredReputationRank)
            {
                return EQUIP_ERR_CANT_EQUIP_REPUTATION;
            }

            return EQUIP_ERR_OK;
        }
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

namespace
{
    /// Classic mount item ids whose level requirement comes from the
    /// MinTrainMountLevel / MinTrainEpicMountLevel configuration entries rather
    /// than from the prototype's own RequiredLevel. Frozen 1.12 tables.
    bool IsRegularMount(uint32 id)
    {
        switch (id)
        {
            case 1132: case 2411: case 2414: case 5655: case 5656: case 5665:
            case 5668: case 5864: case 5872: case 5873: case 8563: case 8588:
            case 8591: case 8592: case 8595: case 8629: case 8631: case 8632:
            case 12325: case 12326: case 12327: case 13321: case 13322:
            case 13331: case 13332: case 13333: case 15277: case 15290:
            case 18241: case 18242: case 18243: case 18244: case 18245:
            case 18246: case 18247: case 18248:
                return true;
            default:
                return false;
        }
    }

    bool IsEpicMount(uint32 id)
    {
        switch (id)
        {
            case 12302: case 12303: case 12330: case 12351: case 12353:
            case 12354: case 13086: case 13326: case 13327: case 13328:
            case 13329: case 13334: case 13335: case 18766: case 18767:
            case 18768: case 18772: case 18773: case 18774: case 18776:
            case 18777: case 18778: case 18785: case 18786: case 18787:
            case 18788: case 18789: case 18790: case 18791: case 18793:
            case 18794: case 18795: case 18796: case 18797: case 18798:
            case 18902:
                return true;
            default:
                return false;
        }
    }
}

/**
 * @brief Checks whether an item prototype is usable by the player.
 *
 * The prototype form deliberately checks neither weapon proficiency nor
 * reputation; both live in the Item* overload, because only a concrete item
 * carries them.
 *
 * @param pProto The item prototype to validate.
 * @param direct_action True if the check is for an immediate player action.
 * @return The inventory result for the use check.
 */
InventoryResult Player::CanUseItem(ItemPrototype const* pProto, bool direct_action) const
{
    // Used by group, function NeedBeforeGreed, to know if a prototype can be used by a player

    if (!pProto)
    {
        return EQUIP_ERR_ITEM_NOT_FOUND;
    }

    if ((pProto->AllowableClass & getClassMask()) == 0 ||
        (pProto->AllowableRace & getRaceMask()) == 0)
    {
        return EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;
    }

    if (pProto->RequiredSkill != 0)
    {
        const uint16 have = GetSkillValue(pProto->RequiredSkill);
        if (have == 0)
        {
            return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
        }
        if (have < pProto->RequiredSkillRank)
        {
            return EQUIP_ERR_CANT_EQUIP_SKILL;
        }
    }

    if (pProto->RequiredSpell != 0 && !HasSpell(pProto->RequiredSpell))
    {
        return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
    }

    // The honor requirement gates a deliberate player action only, not the
    // loot-roll usability probe this overload also serves.
    if (direct_action && pProto->RequiredHonorRank != 0 &&
        uint32(GetHonorHighestRankInfo().rank) < pProto->RequiredHonorRank)
    {
        return EQUIP_ERR_CANT_EQUIP_RANK;
    }

    uint32 requiredLevel = pProto->RequiredLevel;
    if (IsRegularMount(pProto->ItemId))
    {
        requiredLevel = sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL);
    }
    else if (IsEpicMount(pProto->ItemId))
    {
        requiredLevel = sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL);
    }

    if (getLevel() < requiredLevel)
    {
        return EQUIP_ERR_CANT_EQUIP_LEVEL_I;
    }

    return EQUIP_ERR_OK;
}

/**
 * @brief Checks whether a specific ammo item can be equipped as ammunition.
 *
 * @param item The ammo item entry.
 * @return The inventory result for the ammo check.
 */
InventoryResult Player::CanUseAmmo(uint32 item) const
{
    DEBUG_LOG("STORAGE: CanUseAmmo item = %u", item);
    if (!IsAlive())
    {
        return EQUIP_ERR_YOU_ARE_DEAD;
    }
    // if ( isStunned() )
    //    return EQUIP_ERR_YOU_ARE_STUNNED;
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto)
    {
        if (pProto->InventoryType != INVTYPE_AMMO)
        {
            return EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE;
        }

        InventoryResult msg = CanUseItem(pProto);
        if (msg != EQUIP_ERR_OK)
        {
            return msg;
        }

        return EQUIP_ERR_OK;
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

/**
 * @brief Sets the player's active ammo item and refreshes ranged bonuses.
 *
 * @param item The ammo item entry to equip.
 */
void Player::SetAmmo(uint32 item)
{
    if (!item)
    {
        return;
    }

    // already set
    if (GetUInt32Value(PLAYER_AMMO_ID) == item)
    {
        return;
    }

    // check ammo
    if (item)
    {
        InventoryResult msg = CanUseAmmo(item);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, item);
            return;
        }
    }

    SetUInt32Value(PLAYER_AMMO_ID, item);

    _ApplyAmmoBonuses();
}

/**
 * @brief Clears the player's active ammo and removes ranged ammo bonuses.
 */
void Player::RemoveAmmo()
{
    SetUInt32Value(PLAYER_AMMO_ID, 0);

    Arms().Ammo(0.0f, 0.0f);

    if (Tallied().Ready())
    {
        Sheet().Swing(RANGED_ATTACK);
    }
}
