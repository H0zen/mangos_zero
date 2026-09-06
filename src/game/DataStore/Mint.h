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

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "Policies/Singleton.h"

#include <atomic>

/**
 * @brief Where the numbers a running world hands out are struck.
 *
 * SEPARATE FROM THE DATA STORE, and that is the whole point. `ARCH.md` §21 says a
 * DataStore is loaded at boot, is immutable afterwards, and is therefore read from
 * any map thread with no lock at all. Every counter here breaks all three: it is
 * written while the world runs, and it is written from PHASE B -- a pet summoned in
 * `SpellEffects`, an item created in `Item`, a corpse in `PlayerDeath` -- which runs
 * one thread per map. Kept alongside the templates, these ten counters made the
 * store's "no lock needed" true of the tables and false of the object.
 *
 * So they live here, and every one of them is atomic.
 */
class Mint
{
    public:
        /**
         * @brief One counter, handed out to whoever asks next.
         *
         * `fetch_add` is the whole mechanism: the value a caller gets is the one
         * nobody else can get, whichever thread asks and however many ask at once.
         */
        class Counter
        {
            public:
                Counter(char const* name, uint32 ceiling)
                    : m_name(name), m_ceiling(ceiling), m_next(1) {}

                void Set(uint32 val) { m_next.store(val, std::memory_order_relaxed); }
                uint32 NextAfterMaxUsed() const { return m_next.load(std::memory_order_relaxed); }

                /// The next number. Nought when the range is spent, and the server
                /// is told to stop -- a spent range must never wrap into a live guid.
                uint32 Next();

            private:
                char const* m_name;
                uint32 m_ceiling;
                std::atomic<uint32> m_next;
        };

        Mint();

        /// The numbers a running world strikes.
        Counter& PlayerGuids() { return m_players; }
        Counter& ItemGuids() { return m_items; }
        Counter& CorpseGuids() { return m_corpses; }
        Counter& AuctionIds() { return m_auctions; }
        Counter& GuildIds() { return m_guilds; }
        Counter& GroupIds() { return m_groups; }
        Counter& MailIds() { return m_mails; }
        Counter& PetNumbers() { return m_pets; }

        /// Where the world database's own spawn guids stop and generated ones begin.
        /// A static spawn added by a command must land below the line.
        uint32 FirstTemporaryCreature() const { return m_firstTemporaryCreature; }
        uint32 FirstTemporaryGameObject() const { return m_firstTemporaryGameObject; }
        void FirstTemporaryCreature(uint32 guid) { m_firstTemporaryCreature = guid; }
        void FirstTemporaryGameObject(uint32 guid) { m_firstTemporaryGameObject = guid; }

        /// A static spawn guid, or nought when the space below the line is spent.
        uint32 StaticCreatureGuid();
        uint32 StaticGameObjectGuid();

        /// The reserved ranges themselves, set once at boot.
        Counter& StaticCreatureGuids() { return m_staticCreatures; }
        Counter& StaticGameObjectGuids() { return m_staticGameObjects; }

    private:
        Counter m_players;
        Counter m_items;
        Counter m_corpses;
        Counter m_auctions;
        Counter m_guilds;
        Counter m_groups;
        Counter m_mails;
        Counter m_pets;

        Counter m_staticCreatures;
        Counter m_staticGameObjects;

        uint32 m_firstTemporaryCreature = 1;
        uint32 m_firstTemporaryGameObject = 1;
};

#define sMint MaNGOS::Singleton<Mint>::Instance()
