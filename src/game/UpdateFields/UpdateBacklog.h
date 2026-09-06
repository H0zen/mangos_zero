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

#include <set>

class Object;

/**
 * @brief The objects on one map whose changed fields still owe their observers a packet.
 *
 * A tick marks things dirty as it goes and this is where they wait. Draining it once
 * at the end is what turns a great many small changes into one update packet per
 * observer, so the set is the unit of coalescing, not each object.
 */
class UpdateBacklog
{
    public:
        /// Something changed on it, and whoever can see it has not been told yet.
        void Add(Object* what) { m_waiting.insert(what); }

        /// It is going away, so nobody is owed anything about it.
        void Forget(Object* what) { m_waiting.erase(what); }

        bool Empty() const { return m_waiting.empty(); }
        std::size_t Size() const { return m_waiting.size(); }

        /// Builds one packet per observer and sends it, then owes nothing.
        void Send();

    private:
        std::set<Object*> m_waiting;
};
