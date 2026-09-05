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

#include "Destination.h"

#include "Item.h"
#include "Player.h"

bool Destination::Reachable() const
{
    return Inventory::IsCarried(m_place) || Inventory::IsBanked(m_place) || Inventory::IsWorn(m_place);
}

InventoryResult Destination::Weigh(Item* item, bool swap)
{
    uint8 const bag = Inventory::Container(m_place);
    uint8 const slot = Inventory::Slot(m_place);

    m_spread.clear();
    m_worn = 0;

    if (Inventory::IsCarried(m_place))
    {
        return m_who.CanStoreItem(bag, slot, m_spread, item, swap);
    }

    if (Inventory::IsBanked(m_place))
    {
        return m_who.CanBankItem(bag, slot, m_spread, item, swap);
    }

    if (!Inventory::IsWorn(m_place))
    {
        return EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;
    }

    InventoryResult const fits = m_who.CanEquipItem(slot, m_worn, item, swap);
    if (fits != EQUIP_ERR_OK || !swap)
    {
        return fits;
    }

    // Something is on it already and has to be able to come off before anything
    // can go on.
    return m_who.CanUnequipItem(m_worn, true);
}

void Destination::Carry(Item* item)
{
    if (Inventory::IsCarried(m_place))
    {
        m_who.StoreItem(m_spread, item, true);
    }
    else if (Inventory::IsBanked(m_place))
    {
        m_who.BankItem(m_spread, item, true);
    }
    else if (Inventory::IsWorn(m_place))
    {
        m_who.EquipItem(m_worn, item, true);
    }
}
