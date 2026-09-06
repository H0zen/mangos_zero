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

// Trade slots
enum TradeSlots
{
    TRADE_SLOT_COUNT            = 7, // Total trade slots
    TRADE_SLOT_TRADED_COUNT     = 6, // Traded slots count
    TRADE_SLOT_NONTRADED        = 6  // Non-traded slots count
};

/**
 * One side of a trade window.
 *
 * Each of the two men has his own, and each names the other, so a question about
 * what is on the table is asked of the man whose side it is. The other side is
 * reached through him rather than held twice.
 *
 * Seven slots are shown but only six are traded. The seventh holds an item the
 * spell in the window will be cast upon -- an enchant a crafter is applying to
 * something the other man is holding -- and it stays with its owner.
 *
 * Items are held by guid rather than by pointer, because between putting one on
 * the table and the trade going through he may destroy it, sell it, or post it
 * away, and a stale pointer would outlive it.
 *
 * Accepting is a state, not an act: either man may take his acceptance back, and
 * anything either of them changes clears both.
 */
class TradeData
{
    public: // Constructors
        TradeData(Player* player, Player* trader)
            : m_player(player),  m_trader(trader), m_accepted(false), m_acceptProccess(false),
            m_money(0), m_spell(0) {}

    public: // Access functions

        // Get the trader
        Player* GetTrader() const
        {
            return m_trader;
        }

        // Get the trade data of the trader
        TradeData* GetTraderData() const;

        // Get the item in the specified trade slot
        Item* GetItem(TradeSlots slot) const;

        // Check if the trade has the specified item
        bool HasItem(ObjectGuid item_guid) const;

        // Get the spell applied to the trade
        uint32 GetSpell() const
        {
            return m_spell;
        }

        // Get the item used to cast the spell
        Item* GetSpellCastItem() const;

        // Check if there is a spell cast item
        bool HasSpellCastItem() const
        {
            return !m_spellCastItem.IsEmpty();
        }

        // Get the money placed in the trade
        uint32 GetMoney() const
        {
            return m_money;
        }

        // Check if the trade is accepted
        bool IsAccepted() const
        {
            return m_accepted;
        }

        // Check if the trade is in the accept process
        bool IsInAcceptProcess() const
        {
            return m_acceptProccess;
        }

    public: // Access functions

        // Set the item in the specified trade slot
        void SetItem(TradeSlots slot, Item* item);

        // Set the spell applied to the trade
        void SetSpell(uint32 spell_id, Item* castItem = nullptr);

        // Set the money placed in the trade
        void SetMoney(uint32 money);

        // Set the accepted state of the trade
        void SetAccepted(bool state, bool crosssend = false);

        // Set the accept process state of the trade
        void SetInAcceptProcess(bool state)
        {
            m_acceptProccess = state;
        }

    private: // Internal functions

        // Update the trade data
        void Update(bool for_trader = true);

    private: // Fields

        Player* m_player; // Player who owns this TradeData
        Player* m_trader; // Player who trades with m_player

        bool m_accepted; // Indicates if m_player has accepted the trade
        bool m_acceptProccess; // Indicates if the accept process is ongoing

        uint32 m_money; // Money placed in the trade

        uint32 m_spell; // Spell applied to the non-traded slot item
        ObjectGuid m_spellCastItem; // Item used to cast the spell

        ObjectGuid m_items[TRADE_SLOT_COUNT]; // Items traded from m_player's side, including non-traded slot
};
