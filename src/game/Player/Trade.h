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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "ObjectGuid.h"
#include "Platform/Define.h"

class Item;
class Player;

enum TradeSlots
{
    TRADE_SLOT_COUNT            = 7,
    TRADE_SLOT_TRADED_COUNT     = 6,
    TRADE_SLOT_NONTRADED        = 6
};

class TradeData
{
    public:
        TradeData(Player* player, Player* trader)
            : m_player(player),  m_trader(trader), m_accepted(false), m_acceptProccess(false),
            m_money(0), m_spell(0) {}

    public:

        Player* GetTrader() const
        {
            return m_trader;
        }

        TradeData* GetTraderData() const;

        Item* GetItem(TradeSlots slot) const;

        bool HasItem(ObjectGuid item_guid) const;

        uint32 GetSpell() const
        {
            return m_spell;
        }

        Item* GetSpellCastItem() const;

        bool HasSpellCastItem() const
        {
            return !(m_spellCastItem == 0);
        }

        uint32 GetMoney() const
        {
            return m_money;
        }

        bool IsAccepted() const
        {
            return m_accepted;
        }

        bool IsInAcceptProcess() const
        {
            return m_acceptProccess;
        }

    public:

        void SetItem(TradeSlots slot, Item* item);

        void SetSpell(uint32 spell_id, Item* castItem = nullptr);

        void SetMoney(uint32 money);

        void SetAccepted(bool state, bool crosssend = false);

        void SetInAcceptProcess(bool state)
        {
            m_acceptProccess = state;
        }

    private:

        void Update(bool for_trader = true);

    private:

        Player* m_player;
        Player* m_trader;

        bool m_accepted;
        bool m_acceptProccess;

        uint32 m_money;

        uint32 m_spell;
        ObjectGuid m_spellCastItem = 0;

        ObjectGuid m_items[TRADE_SLOT_COUNT] = {};
};
