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

class Mint
{
    public:

        class Counter
        {
            public:
                Counter(char const* name, uint32 ceiling)
                    : m_name(name), m_ceiling(ceiling), m_next(1) {}

                void Set(uint32 val) { m_next.store(val, std::memory_order_relaxed); }
                uint32 NextAfterMaxUsed() const { return m_next.load(std::memory_order_relaxed); }

                uint32 Next();

            private:
                char const* m_name;
                uint32 m_ceiling;
                std::atomic<uint32> m_next;
        };

        Mint();

        Counter& PlayerGuids() { return m_players; }
        Counter& ItemGuids() { return m_items; }
        Counter& CorpseGuids() { return m_corpses; }
        Counter& AuctionIds() { return m_auctions; }
        Counter& GuildIds() { return m_guilds; }
        Counter& GroupIds() { return m_groups; }
        Counter& MailIds() { return m_mails; }
        Counter& PetNumbers() { return m_pets; }

        uint32 FirstTemporaryCreature() const { return m_firstTemporaryCreature; }
        uint32 FirstTemporaryGameObject() const { return m_firstTemporaryGameObject; }
        void FirstTemporaryCreature(uint32 guid) { m_firstTemporaryCreature = guid; }
        void FirstTemporaryGameObject(uint32 guid) { m_firstTemporaryGameObject = guid; }

        uint32 StaticCreatureGuid();
        uint32 StaticGameObjectGuid();

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
