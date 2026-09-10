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

#include "Platform/Define.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "SkillAnswers.h"

void skills::LearnTalent(Player& who, WorldPacket& recv_data)
{
    uint32 talent_id, requested_rank;
    recv_data >> talent_id >> requested_rank;

    who.LearnTalent(talent_id, requested_rank);

    if (who.GetPet())
    {
        who.GetPet()->CastOwnerTalentAuras();
    }
}

void skills::TalentWipeConfirm(Player& who, WorldPacket& recv_data)
{
    DETAIL_LOG("MSG_TALENT_WIPE_CONFIRM");
    ObjectGuid guid = 0;
    recv_data >> guid;

    Creature* unit = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTalentWipeConfirmOpcode - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (!unit->CanTrainAndResetTalentsOf(&who))
    {
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (!(who.resetTalents()))
    {
        WorldPacket data(MSG_TALENT_WIPE_CONFIRM, 8 + 4);
        data << uint64(0);
        data << uint32(0);
        who.GetSession()->SendPacket(&data);
        return;
    }

    unit->CastSpell(&who, 14867, true);

    if (who.GetPet())
    {
        who.GetPet()->CastOwnerTalentAuras();
    }
}

void skills::UnlearnSkill(Player& who, WorldPacket& recv_data)
{
    uint32 skill_id;
    recv_data >> skill_id;
    who.SetSkill(skill_id, 0, 0);
}
