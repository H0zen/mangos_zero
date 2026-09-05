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

#include "Platform/Define.h"

class Group;

/**
 * The invitations put to a character that he has not yet answered.
 *
 * A party invitation names the party itself, because the party holds the
 * invited list on its own side and both sides are cleared together: the party
 * is a live object for as long as the invitation stands. A guild invitation
 * names only the guild's id, so the guild is looked up when he answers -- it may
 * have been disbanded while the box sat on his screen.
 *
 * Each of them is answered once. Accepting and declining both clear it, and so
 * does leaving the world with the box still open.
 *
 * He can hold one of each at a time. A second invitation of the same kind is
 * refused by the handler before it ever reaches here, so what is kept is a
 * single party and a single guild rather than a list.
 */
class Invitations
{
    public:

        /// The party that has asked him to join, or nothing.
        Group* ToParty() const { return m_party; }
        void ToParty(Group* party) { m_party = party; }

        /// The guild that has asked him to join, by id, or nothing.
        uint32 ToGuild() const { return m_guild; }
        void ToGuild(uint32 guildId) { m_guild = guildId; }

        /// Nothing stands open.
        void None()
        {
            m_party = nullptr;
            m_guild = 0;
        }

    private:

        Group* m_party = nullptr;
        uint32 m_guild = 0;
};
