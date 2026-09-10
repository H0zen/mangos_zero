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

#include "Reaction.h"
#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CorpseManager.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#include "Corpse.h"
#include "SpoilsHolder.h"

void Player::RemovedInsignia(Player* looterPlr)
{
    if (!Battle().Id())
    {
        return;
    }

    if (m_deathTimer > 0)
    {
        m_deathTimer = 0;
        BuildPlayerRepop();
        RepopAtGraveyard();
    }

    Corpse* corpse = GetCorpse();
    if (!corpse)
    {
        return;
    }

    Corpse* bones = sCorpseManager.ConvertCorpseForPlayer(GetObjectGuid(), true);
    if (!bones)
    {
        return;
    }

    bones->SetCorpseDynFlag(CORPSE_DYNFLAG_LOOTABLE);

    bones->loot.gold = getLevel();
    bones->lootRecipient = looterPlr;
    looterPlr->SendLoot(bones->GetObjectGuid(), LOOT_INSIGNIA);
}

void Player::SendLootRelease(ObjectGuid guid)
{
    WorldPacket data(SMSG_LOOT_RELEASE_RESPONSE, (8 + 1));
    data << guid;
    data << uint8(1);
    SendDirectMessage(&data);
}

void Player::SendLoot(ObjectGuid guid, LootType loot_type)
{
    if (ObjectGuid lootGuid = GetLootGuid())
    {
        m_session->DoLootRelease(lootGuid);
    }

    PermissionTypes permission = ALL_PERMISSION;

    DEBUG_LOG("Player::SendLoot");

    Spoilable* holder = spoils::Holder(*this, guid);
    if (!holder)
    {
        sLog.outError("%s is unsupported for looting.", GuidString(guid).c_str());
        return;
    }

    if (!holder->FillSpoilsFor(*this, loot_type, permission))
    {
        SendLootRelease(guid);
        return;
    }

    Loot* loot = holder->Spoils();

    SetLootGuid(guid);

    loot->loot_type = loot_type;

    switch (loot_type)
    {
        case LOOT_SKINNING:     loot_type = LOOT_PICKPOCKETING; break;
        case LOOT_INSIGNIA:     loot_type = LOOT_PICKPOCKETING; break;
        case LOOT_FISHING_FAIL: loot_type = LOOT_FISHING;       break;
        case LOOT_FISHINGHOLE:  loot_type = LOOT_FISHING;       break;
        default: break;
    }

    WorldPacket data(SMSG_LOOT_RESPONSE, (9 + 50));
    data << static_cast<ObjectGuid>(guid);
    data << uint8(loot_type);
    data << LootView(*loot, this, permission);
    SendDirectMessage(&data);

    if (permission != NONE_PERMISSION)
    {
        loot->AddLooter(GetObjectGuid());
    }

    if (loot_type == LOOT_CORPSE && !(GuidHigh(guid) == HIGHGUID_ITEM))
    {
        SetUnitFlag(UNIT_FLAG_LOOTING);
    }
}

void Player::SendNotifyLootMoneyRemoved()
{
    WorldPacket data(SMSG_LOOT_CLEAR_MONEY, 0);
    GetSession()->SendPacket(&data);
}

void Player::SendNotifyLootItemRemoved(uint8 lootSlot)
{
    WorldPacket data(SMSG_LOOT_REMOVED, 1);
    data << uint8(lootSlot);
    GetSession()->SendPacket(&data);
}
