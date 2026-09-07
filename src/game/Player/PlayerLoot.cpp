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

/**
 * @brief Converts the player's battleground corpse into lootable bones after insignia removal.
 *
 * @param looterPlr The player removing the insignia.
 */
void Player::RemovedInsignia(Player* looterPlr)
{
    if (!Battle().Id())
    {
        return;
    }

    // If not released spirit, do it !
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

    // We have to convert player corpse to bones, not to be able to resurrect there
    // SpawnCorpseBones isn't handy, 'cos it saves player while he in BG
    Corpse* bones = sCorpseManager.ConvertCorpseForPlayer(GetObjectGuid(), true);
    if (!bones)
    {
        return;
    }

    // Now we must make bones lootable, and send player loot
    bones->SetCorpseDynFlag(CORPSE_DYNFLAG_LOOTABLE);

    // We store the level of our player in the gold field
    // We retrieve this information at Player::SendLoot()
    bones->loot.gold = getLevel();
    bones->lootRecipient = looterPlr;
    looterPlr->SendLoot(bones->GetObjectGuid(), LOOT_INSIGNIA);
}

/**
 * @brief Sends a loot release response for a loot object.
 *
 * @param guid The GUID of the released loot source.
 */
void Player::SendLootRelease(ObjectGuid guid)
{
    WorldPacket data(SMSG_LOOT_RELEASE_RESPONSE, (8 + 1));
    data << guid;
    data << uint8(1);
    SendDirectMessage(&data);
}

/**
 * @brief Opens and sends loot contents for a supported loot source.
 *
 * @param guid The GUID of the loot source.
 * @param loot_type The requested loot interaction type.
 */
void Player::SendLoot(ObjectGuid guid, LootType loot_type)
{
    if (ObjectGuid lootGuid = GetLootGuid())
    {
        m_session->DoLootRelease(lootGuid);
    }

    PermissionTypes permission = ALL_PERMISSION;

    DEBUG_LOG("Player::SendLoot");

    Object* holder = spoils::Holder(*this, guid);
    if (!holder)
    {
        sLog.outError("%s is unsupported for looting.", guid.GetString().c_str());
        return;
    }

    if (!holder->FillSpoilsFor(*this, loot_type, permission))
    {
        SendLootRelease(guid);
        return;
    }

    Loot* loot = holder->Spoils();

    SetLootGuid(guid);

    // need know for proper finish item loots (internal pre-switch loot type set in different from 3.x code version)
    // in fact this meaning that it send same loot types for interesting cases like 3.x version code (skip pre-3.x client loot type limitaitons)
    loot->loot_type = loot_type;

    // LOOT_SKINNING, LOOT_PROSPECTING, LOOT_INSIGNIA and LOOT_FISHINGHOLE unsupported by client
    switch (loot_type)
    {
        case LOOT_SKINNING:     loot_type = LOOT_PICKPOCKETING; break;
        case LOOT_INSIGNIA:     loot_type = LOOT_PICKPOCKETING; break;
        case LOOT_FISHING_FAIL: loot_type = LOOT_FISHING;       break;
        case LOOT_FISHINGHOLE:  loot_type = LOOT_FISHING;       break;
        default: break;
    }

    WorldPacket data(SMSG_LOOT_RESPONSE, (9 + 50));         // we guess size
    data << ObjectGuid(guid);
    data << uint8(loot_type);
    data << LootView(*loot, this, permission);
    SendDirectMessage(&data);

    // add 'this' player as one of the players that are looting 'loot'
    if (permission != NONE_PERMISSION)
    {
        loot->AddLooter(GetObjectGuid());
    }

    if (loot_type == LOOT_CORPSE && !guid.IsItem())
    {
        SetUnitFlag(UNIT_FLAG_LOOTING);
    }
}

/**
 * @brief Notifies the client that money was removed from the current loot.
 */
void Player::SendNotifyLootMoneyRemoved()
{
    WorldPacket data(SMSG_LOOT_CLEAR_MONEY, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Notifies the client that a loot slot was removed.
 *
 * @param lootSlot The loot slot index that was removed.
 */
void Player::SendNotifyLootItemRemoved(uint8 lootSlot)
{
    WorldPacket data(SMSG_LOOT_REMOVED, 1);
    data << uint8(lootSlot);
    GetSession()->SendPacket(&data);
}
