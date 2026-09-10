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
#include "OutdoorPvP.h"

enum
{

    NPC_SILITHUS_DUST_QUEST_ALLIANCE = 17090,
    NPC_SILITHUS_DUST_QUEST_HORDE    = 18199,

    GO_SILITHYST_MOUND               = 181597,
    GO_SILITHYST_GEYSER              = 181598,

    SPELL_SILITHYST                  = 29519,
    SPELL_TRACES_OF_SILITHYST        = 29534,
    SPELL_CENARION_FAVOR             = 30754,
    SPELL_SILITHYST_FLAG_DROP        = 29533,

    QUEST_SCOURING_DESERT_ALLIANCE   = 9419,
    QUEST_SCOURING_DESERT_HORDE      = 9422,

    AREATRIGGER_SILITHUS_ALLIANCE    = 4162,
    AREATRIGGER_SILITHUS_HORDE       = 4168,

    FACTION_CENARION_CIRCLE          = 609,
    HONOR_REWARD_SILITHYST           = 199,
    REPUTATION_REWARD_SILITHYST      = 20,
    MAX_SILITHYST                    = 200,

    WORLD_STATE_SI_GATHERED_A        = 2313,
    WORLD_STATE_SI_GATHERED_H        = 2314,
    WORLD_STATE_SI_SILITHYST_MAX     = 2317
};

class OutdoorPvPSI : public OutdoorPvP
{
    public:

        OutdoorPvPSI();

        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;

        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;

        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;

        bool HandleAreaTrigger(Player* player, uint32 triggerId) override;

        bool HandleGameObjectUse(Player* player, GameObject* go) override;

        bool HandleDropFlag(Player* player, uint32 spellId) override;

    private:
        uint8 m_resourcesAlliance;
        uint8 m_resourcesHorde;
        Team m_zoneOwner;
};
