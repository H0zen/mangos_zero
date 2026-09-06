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

#include "Mailbox.h"

#include "Item.h"
#include "Mail.h"
#include "Opcodes.h"
#include "Player.h"
#include "Utilities/Errors.h"
#include "WorldPacket.h"
#include "WorldSession.h"

Mailbox::~Mailbox()
{
    for (auto* letter : m_letters)
    {
        delete letter;
    }

    for (auto& kept : m_attached)
    {
        delete kept.second;
    }
}

void Mailbox::Remove(uint32 id)
{
    for (auto itr = m_letters.begin(); itr != m_letters.end(); ++itr)
    {
        if ((*itr)->messageID == id)
        {
            m_letters.erase(itr);
            return;
        }
    }
}

Mail* Mailbox::Find(uint32 id) const
{
    for (auto* letter : m_letters)
    {
        if (letter->messageID == id)
        {
            return letter;
        }
    }

    return nullptr;
}

Item* Mailbox::Attachment(uint32 lowGuid) const
{
    auto itr = m_attached.find(lowGuid);
    return itr != m_attached.end() ? itr->second : nullptr;
}

void Mailbox::Keep(Item* what)
{
    MANGOS_ASSERT(what);
    m_attached[what->GetGUIDLow()] = what;
}

void Mailbox::TellHim()
{
    WorldPacket data(SMSG_RECEIVED_MAIL, 4);
    data << float(0);
    m_owner.GetSession()->SendPacket(&data);
}

void Mailbox::Recount()
{
    time_t const now = time(nullptr);

    m_nextDelivery = 0;
    m_unread = 0;

    for (auto* letter : m_letters)
    {
        if (letter->deliver_time > now)
        {
            if (!m_nextDelivery || m_nextDelivery > letter->deliver_time)
            {
                m_nextDelivery = letter->deliver_time;
            }
        }
        else if ((letter->checked & MAIL_CHECK_MASK_READ) == 0)
        {
            ++m_unread;
        }
    }
}

void Mailbox::Expecting(time_t when)
{
    if (when <= time(nullptr))
    {
        ++m_unread;
        TellHim();
        return;
    }

    if (!m_nextDelivery || m_nextDelivery > when)
    {
        m_nextDelivery = when;
    }
}
