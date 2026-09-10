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

#include "Synthetic/SyntheticCrowd.h"
#include "Synthetic/SyntheticLink.h"

#include "Log.h"
#include "Map.h"
#include "MapFoundry.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "Opcodes.h"
#include "Player.h"
#include "Movement/Generators/MotionMaster.h"
#include "PlayerRegistry.h"
#include "SessionMailbox.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Utilities/Timer.h"

#include <cmath>

namespace synthetic
{
    namespace
    {

        const uint32 SYNTHETIC_ACCOUNT_BASE = 0x7F000000;

        const float BOT_ANGULAR_SPEED = 0.6f;

        const float BOT_ORBIT_RADIUS = 8.f;
    }

    SyntheticCrowd& SyntheticCrowd::Instance()
    {
        static SyntheticCrowd instance;
        return instance;
    }

    uint32 SyntheticCrowd::Spawn(uint32 count, uint32 mapId, float x, float y, float z,
                                 float radius, std::string& error)
    {
        if (!m_bots.empty())
        {
            error = "a crowd is already spawned; despawn it first";
            return 0;
        }

        if (count == 0)
        {
            error = "nothing asked for";
            return 0;
        }

        uint32 placed = 0;

        for (uint32 i = 0; i < count; ++i)
        {
            auto link = std::make_shared<SyntheticLink>("synthetic");
            auto mailbox = std::make_shared<SessionMailbox>();

            const uint32 account = SYNTHETIC_ACCOUNT_BASE + m_nextAccount++;

            WorldSession* session = new WorldSession(account, link, mailbox,
                                                     SEC_PLAYER, 0, LOCALE_enUS);

            Player* bot = new Player(session);

            bot->GetMotionMaster()->Initialize();

            char name[16];
            std::snprintf(name, sizeof(name), "Synth%u", i);

            const uint32 lowGuid = sMint.PlayerGuids().Next();
            if (!bot->Create(lowGuid, name, RACE_HUMAN, CLASS_WARRIOR, GENDER_MALE,
                             0, 0, 0, 0, 0, 0))
            {
                delete bot;
                delete session;
                error = "Player::Create refused; is the world database loaded?";
                break;
            }

            const float spread = radius > 0.f ? radius : 1.f;
            const float a = static_cast<float>(i) * 2.399963f;
            const float r = spread * std::sqrt(static_cast<float>(i + 1) / static_cast<float>(count));
            const float px = x + r * std::cos(a);
            const float py = y + r * std::sin(a);

            Map* map = sMapFoundry.OpenFor(*bot, mapId);
            if (!map)
            {
                delete bot;
                delete session;
                error = "no such map";
                break;
            }

            bot->SetMap(map);
            bot->Place().MoveTo(px, py, z, 0.f);

            session->SetPlayer(bot);

            if (!map->Add(bot))
            {
                session->SetPlayer(nullptr);
                delete bot;
                delete session;
                error = "the map refused the player";
                break;
            }

            sPlayerRegistry.Add(bot);

            sWorld.AddSession(session);

            Bot record;
            record.session = session;
            record.link = link.get();
            record.guid = bot->GetObjectGuid();
            record.homeX = px;
            record.homeY = py;
            record.homeZ = z;
            record.angle = a;
            m_bots.push_back(record);

            ++placed;
        }

        sLog.outString("Synthetic crowd: %u placed on map %u.", placed, mapId);
        return placed;
    }

    uint32 SyntheticCrowd::Despawn()
    {
        const uint32 had = static_cast<uint32>(m_bots.size());

        for (Bot& bot : m_bots)
        {

            if (Player* player = sObjectMgr.GetPlayer(bot.guid, false))
            {
                if (Map* map = player->GetMap())
                {
                    map->Remove(player, true);
                }
            }

            if (bot.session)
            {

                bot.session->SetPlayer(nullptr);
                bot.session->KickPlayer();
            }

            if (bot.link)
            {
                bot.link->Close();
            }
        }

        m_bots.clear();
        sLog.outString("Synthetic crowd: %u released.", had);
        return had;
    }

    void SyntheticCrowd::Drive(uint32 diff)
    {
        if (m_bots.empty())
        {
            return;
        }

        const float step = BOT_ANGULAR_SPEED * static_cast<float>(diff) / 1000.f;

        for (Bot& bot : m_bots)
        {
            Player* player = sObjectMgr.GetPlayer(bot.guid);
            if (!player || !player->IsInWorld())
            {
                continue;
            }

            bot.angle += step;

            const float px = bot.homeX + BOT_ORBIT_RADIUS * std::cos(bot.angle);
            const float py = bot.homeY + BOT_ORBIT_RADIUS * std::sin(bot.angle);

            WorldPacket* move = new WorldPacket(MSG_MOVE_HEARTBEAT, 32);
            *move << static_cast<uint32>(MOVEFLAG_FORWARD);
            *move << static_cast<uint32>(getMSTime());
            *move << static_cast<float>(px) << static_cast<float>(py) << static_cast<float>(bot.homeZ) << static_cast<float>(bot.angle);
            *move << static_cast<uint32>(0);

            bot.session->QueuePacket(move);
        }
    }

    CrowdReport SyntheticCrowd::Report(uint32 elapsedMs)
    {
        CrowdReport report;
        report.bots = static_cast<uint32>(m_bots.size());

        if (m_bots.empty() || elapsedMs == 0)
        {
            return report;
        }

        uint64 peak = 0;
        for (Bot& bot : m_bots)
        {
            if (!bot.link)
            {
                continue;
            }
            const uint64 bytes = bot.link->TakeBytes();
            report.bytes += bytes;
            report.packets += bot.link->TakePackets();

            if (bytes > peak)
            {
                peak = bytes;
            }
        }

        report.bytesPerSecPeak = static_cast<uint32>(peak * 1000 / elapsedMs);
        report.bytesPerSecMean = static_cast<uint32>(report.bytes * 1000 / elapsedMs / report.bots);
        return report;
    }
}
