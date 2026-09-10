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

#include <sstream>
#include "MassMailMgr.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "SharedDefines.h"
#include "World.h"
#include "ObjectMgr.h"

void MassMailMgr::AddMassMailTask(MailDraft* mailProto, const MailSender &sender, uint32 raceMask)
{
    if (RACEMASK_ALL_PLAYABLE & ~raceMask)
    {
        std::ostringstream ss;
        ss << "SELECT `guid` FROM `characters` WHERE (1 << (`race` - 1)) & " << raceMask << " AND `deleteDate` IS NULL";
        AddMassMailTask(mailProto, sender, ss.str().c_str());
    }
    else
    {
        AddMassMailTask(mailProto, sender, "SELECT `guid` FROM `characters` WHERE `deleteDate` IS NULL");
    }
}

struct MassMailerQueryHandler
{

    void HandleQueryCallback(QueryResult* result, MailDraft* mailProto, MailSender sender)
    {
        if (!result)
        {
            return;
        }

        MassMailMgr::ReceiversList& recievers = sMassMailMgr.AddMassMailTask(mailProto, sender);

        do
        {
            Field* fields = result->Fetch();
            recievers.insert(fields[0].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }
} massMailerQueryHandler;

void MassMailMgr::AddMassMailTask(MailDraft* mailProto, const MailSender &sender, char const* query)
{
    CharacterDatabase.AsyncPQuery([mailProto, sender](QueryResult* result)
                                  {
                                      massMailerQueryHandler.HandleQueryCallback(result, mailProto, sender);
                                  }, "%s", query);
}

void MassMailMgr::Update(bool sendall )
{
    if (m_massMails.empty())
    {
        return;
    }

    uint32 maxcount = sWorld.getConfig(CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK);

    do
    {
        MassMail& task = m_massMails.front();

        while (!task.m_receivers.empty() && (sendall || maxcount > 0))
        {
            uint32 receiver_lowguid = *task.m_receivers.begin();
            task.m_receivers.erase(task.m_receivers.begin());

            ObjectGuid receiver_guid = MakeGuid(HIGHGUID_PLAYER, receiver_lowguid);
            Player* receiver = sObjectMgr.GetPlayer(receiver_guid);

            if (task.m_receivers.empty())
            {

                task.m_protoMail->SendMailTo(MailReceiver(receiver, receiver_guid), task.m_sender, MAIL_CHECK_MASK_RETURNED);

                if (!sendall)
                {
                    --maxcount;
                }
                break;
            }

            MailDraft draft;
            draft.CloneFrom(*task.m_protoMail);

            draft.SendMailTo(MailReceiver(receiver, receiver_guid), task.m_sender, MAIL_CHECK_MASK_RETURNED);

            if (!sendall)
            {
                --maxcount;
            }
        }

        if (task.m_receivers.empty())
        {
            m_massMails.pop_front();
        }
    }
    while (!m_massMails.empty() && (sendall || maxcount > 0));
}

void MassMailMgr::GetStatistic(uint32& tasks, uint32& mails, uint32& needTime) const
{
    tasks = m_massMails.size();

    uint32 mailsCount = 0;
    for (MassMailList::const_iterator mailItr = m_massMails.begin(); mailItr != m_massMails.end(); ++mailItr)
    {
        mailsCount += mailItr->m_receivers.size();
    }

    mails = mailsCount;

    needTime = 50 * mailsCount / sWorld.getConfig(CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK) / IN_MILLISECONDS;
}
