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

#include <ctime>
#include <deque>
#include <unordered_map>

class Item;
class Player;
struct Mail;

typedef std::deque<Mail*> PlayerMails;

/**
 * The letters waiting for a character, and what came attached to them.
 *
 * A letter reaches him before its hour: it sits here undelivered until its
 * delivery time passes, and only then does it count as unread and light the
 * envelope on his screen. The next such hour is remembered so the world need not
 * walk the pile every tick.
 *
 * Attachments are held apart from what he carries. An item posted to him exists
 * and has a row of its own long before he opens the letter, and it must not be
 * reachable through his bags until he takes it out, so it is kept in a map of
 * its own keyed by the item's low guid.
 *
 * Removing a letter from the pile does not destroy it: returning a letter to its
 * sender goes through the same door, and the letter lives on at the other end.
 * Only the mailbox's own end destroys what is still in it.
 */
class Mailbox
{
    public:

        explicit Mailbox(Player& who) : m_owner(who), m_unread(0), m_nextDelivery(0), m_changed(false) {}
        ~Mailbox();

        Mailbox(Mailbox const&) = delete;
        Mailbox& operator=(Mailbox const&) = delete;

        /// Puts a letter at the front of the pile.
        void Add(Mail* letter) { m_letters.push_front(letter); }

        /// Takes a letter out of the pile without destroying it.
        void Remove(uint32 id);

        Mail* Find(uint32 id) const;

        uint32 Count() const { return uint32(m_letters.size()); }

        PlayerMails& Letters() { return m_letters; }
        PlayerMails const& Letters() const { return m_letters; }

        PlayerMails::iterator begin() { return m_letters.begin(); }
        PlayerMails::iterator end() { return m_letters.end(); }

        /// Empties the pile without destroying the letters, for a fresh load.
        void Clear() { m_letters.clear(); }

        uint8 Unread() const { return m_unread; }

        /// One letter has been read.
        void Read()
        {
            if (m_unread)
            {
                --m_unread;
            }
        }
        time_t NextDelivery() const { return m_nextDelivery; }

        /// Walks the pile and works out the unread count and the next hour.
        void Recount();

        /// Notes a letter due at the given hour, and tells him at once if it is due now.
        void Expecting(time_t when);

        /// Lights the envelope on his screen.
        void TellHim();

        /// Whether the pile has changed since it was last written down.
        bool Changed() const { return m_changed; }
        void Changed(bool changed) { m_changed = changed; }

        typedef std::unordered_map<uint32, Item*> ItemMap;

        /// An item posted to him, by its low guid.
        Item* Attachment(uint32 lowGuid) const;

        void Keep(Item* what);
        bool Drop(uint32 lowGuid) { return m_attached.erase(lowGuid) != 0; }

        ItemMap& Attachments() { return m_attached; }
        ItemMap const& Attachments() const { return m_attached; }

    private:

        Player& m_owner;

        PlayerMails m_letters;
        ItemMap m_attached;

        uint8 m_unread;
        time_t m_nextDelivery;
        bool m_changed;
};
