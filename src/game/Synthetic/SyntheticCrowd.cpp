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
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "MotionGenerators/MotionMaster.h"
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
        /// Far above any account a realm would hand out, so a synthetic session
        /// can never be mistaken for somebody's.
        const uint32 SYNTHETIC_ACCOUNT_BASE = 0x7F000000;

        /// How fast a bot walks its circle, in radians per second. Slow enough
        /// that it keeps crossing cells rather than spinning inside one.
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

            // The order below follows WorldSession::HandlePlayerLogin, because
            // anything it does before the map sees the player is something the
            // player is expected to already have. The movement generator stack
            // is the first of those: Unit::Update walks it every tick and
            // asserts that it is not empty, so a player who never got one dies
            // on the first tick after being added.
            bot->GetMotionMaster()->Initialize();

            char name[16];
            std::snprintf(name, sizeof(name), "Synth%u", i);

            // A human warrior: the race and class matter only in that they must
            // exist, and a melee class keeps the bot out of the spell paths
            // while movement is what is being measured.
            const uint32 lowGuid = sObjectMgr.GeneratePlayerLowGuid();
            if (!bot->Create(lowGuid, name, RACE_HUMAN, CLASS_WARRIOR, GENDER_MALE,
                             0, 0, 0, 0, 0, 0))
            {
                delete bot;
                delete session;
                error = "Player::Create refused; is the world database loaded?";
                break;
            }

            // Scattered, so they do not all land in one cell -- a crowd in a
            // single cell is a case the grid never actually sees.
            const float spread = radius > 0.f ? radius : 1.f;
            const float a = static_cast<float>(i) * 2.399963f;            // golden angle, an even scatter
            const float r = spread * std::sqrt(static_cast<float>(i + 1) / static_cast<float>(count));
            const float px = x + r * std::cos(a);
            const float py = y + r * std::sin(a);

            Map* map = sMapMgr.CreateMap(mapId, bot);
            if (!map)
            {
                delete bot;
                delete session;
                error = "no such map";
                break;
            }

            // SetMap is what puts the map id on the object; the pose is set
            // after, so it is measured in the frame the map names.
            bot->SetMap(map);
            bot->Place().MoveTo(px, py, z, 0.f);

            // Before the map, as in login: adding a player reaches back through
            // his session, and a session that does not know its player yet is a
            // null waiting to be dereferenced.
            session->SetPlayer(bot);

            if (!map->Add(bot))
            {
                session->SetPlayer(nullptr);
                delete bot;
                delete session;
                error = "the map refused the player";
                break;
            }

            // The registry is how everything else finds a player by guid: the
            // name query answers from it, and so does the driver below. Without
            // it a bot is a body with no identity -- the client asks who it is,
            // the lookup falls through to a character row that does not exist,
            // and it renders as Unknown.
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
            // A synthetic character has no life outside the run, so it is taken
            // out directly instead of being logged out: a logout persists the
            // character, announces it to friends and guild, and expects a body
            // of loaded state that was never built here. Map::Remove does the
            // whole retirement -- cleanup, unlink, deregister, delete.
            if (Player* player = sObjectMgr.GetPlayer(bot.guid, false))
            {
                if (Map* map = player->GetMap())
                {
                    map->Remove(player, true);
                }
            }

            if (bot.session)
            {
                // With no player left on it the session's logout is a no-op,
                // which is the point: nothing of this reaches the database.
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

            // Exactly the bytes a 1.12 client sends, which is a MovementInfo and
            // nothing before it. The guid belongs to the RELAY the server writes
            // on the way out, not to what comes in; putting one here shifts every
            // field by six bytes, the position reads as nonsense, and the whole
            // packet is dropped by the coordinate check without a word.
            WorldPacket* move = new WorldPacket(MSG_MOVE_HEARTBEAT, 32);
            *move << static_cast<uint32>(MOVEFLAG_FORWARD);
            *move << static_cast<uint32>(getMSTime());
            *move << static_cast<float>(px) << static_cast<float>(py) << static_cast<float>(bot.homeZ) << static_cast<float>(bot.angle);
            *move << static_cast<uint32>(0);                  // fall time

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
