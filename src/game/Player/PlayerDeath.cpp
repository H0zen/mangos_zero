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

#include "Utilities/Errors.h"
#include "Player.h"
#include "Reclaim.h"
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
#include "Mint.h"
#include "CorpseManager.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "TransportMap.h"
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

void Player::BuildPlayerRepop()
{
    if (getRace() == RACE_NIGHTELF)
    {
        CastSpell(this, 20584, true);
    }
    CastSpell(this, 8326, true);

    if (GetCorpse())
    {
        sLog.outError("BuildPlayerRepop: player %s(%d) already has a corpse", GetName(), GetGUIDLow());
        MANGOS_ASSERT(false);
    }

    Corpse* corpse = nullptr;

    if (!GetMap()->AsTransport())
    {

        corpse = CreateCorpse();
        if (!corpse)
        {
            sLog.outError("Error creating corpse for Player %s [%u]", GetName(), GetGUIDLow());
            return;
        }
        GetMap()->Add(corpse);
    }

    SetHealth(1);

    SetWaterWalk(true);
    if (!GetSession()->isLogingOut())
    {
        SetRoot(false);
    }

    RemoveUnitFlag(UNIT_FLAG_SKINNABLE);

    SendCorpseReclaimDelay();

    if (corpse)
    {
        corpse->ResetGhostTime();
    }

    StopMirrorTimers();

    SetBearing(UNIT_BYTE1_FLAG_ALWAYS_STAND);
}

void Player::ResurrectPlayer(float restore_percent, bool applySickness)
{

    SetBearing(0x00);

    SetDeathState(ALIVE);

    if (getRace() == RACE_NIGHTELF)
    {
        RemoveAuras(20584);
    }
    RemoveAuras(8326);

    SetWaterWalk(false);
    SetRoot(false);

    if (restore_percent > 0.0f)
    {
        SetHealth(uint32(GetMaxHealth()*restore_percent));
        SetPower(POWER_MANA, uint32(GetMaxPower(POWER_MANA)*restore_percent));
        SetPower(POWER_RAGE, 0);
        SetPower(POWER_ENERGY, uint32(GetMaxPower(POWER_ENERGY)*restore_percent));
    }

    uint32 newzone, newarea;
    GetTerrain()->GetZoneAndAreaId(newzone, newarea, Where().X(), Where().Y(), Where().Z());
    UpdateZone(newzone, newarea);

    m_deathTimer = 0;

    m_camera.UpdateVisibilityForOwner();

    UpdateObjectVisibility();

    if (!applySickness)
    {
        return;
    }

    int32 startLevel = sWorld.getConfig(CONFIG_INT32_DEATH_SICKNESS_LEVEL);

    if (int32(getLevel()) >= startLevel)
    {

        CastSpell(this, SPELL_ID_PASSIVE_RESURRECTION_SICKNESS, true);

        if (int32(getLevel()) < startLevel + 9)
        {
            int32 delta = (int32(getLevel()) - startLevel + 1) * MINUTE;

            if (SpellAuraHolder* holder = GetSpellAuraHolder(SPELL_ID_PASSIVE_RESURRECTION_SICKNESS))
            {
                holder->SetAuraDuration(delta * IN_MILLISECONDS);
                holder->UpdateAuraDuration();
            }
        }
    }
}

void Player::KillPlayer()
{
    SetRoot(true);

    StopMirrorTimers();

    SetDeathState(CORPSE);

    SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
    ShowReleaseTimer(!sMapStore.LookupEntry(GetMapId())->Instanceable());

    m_deathTimer = 6 * MINUTE * IN_MILLISECONDS;

    UpdateCorpseReclaimDelay();

    UpdateObjectVisibility();
}

Corpse* Player::CreateCorpse()
{

    SpawnCorpseBones();

    Corpse* corpse = new Corpse((m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) ? CORPSE_RESURRECTABLE_PVP : CORPSE_RESURRECTABLE_PVE);
    SetPvPDeath(false);

    if (!corpse->Create(sMint.CorpseGuids().Next(), this))
    {
        delete corpse;
        return nullptr;
    }

    uint8 skin       = GetByteValue(PLAYER_BYTES, 0);
    uint8 face       = GetByteValue(PLAYER_BYTES, 1);
    uint8 hairstyle  = GetByteValue(PLAYER_BYTES, 2);
    uint8 haircolor  = GetByteValue(PLAYER_BYTES, 3);
    uint8 facialhair = GetByteValue(PLAYER_BYTES_2, 0);

    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 1, getRace());
    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 2, getGender());
    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 3, skin);

    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 0, face);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 1, hairstyle);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 2, haircolor);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 3, facialhair);

    uint32 flags = CORPSE_FLAG_UNK2;
    if (HasPlayerFlag(PLAYER_FLAGS_HIDE_HELM))
    {
        flags |= CORPSE_FLAG_HIDE_HELM;
    }
    if (HasPlayerFlag(PLAYER_FLAGS_HIDE_CLOAK))
    {
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    }
    if (Battle().InOne())
    {
        flags |= CORPSE_FLAG_LOOTABLE;
    }
    corpse->SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    corpse->SetUInt32Value(CORPSE_FIELD_DISPLAY_ID, GetNativeDisplayId());

    corpse->SetUInt32Value(CORPSE_FIELD_GUILD, GetGuildId());

    uint32 iDisplayID;
    uint32 iIventoryType;
    uint32 _cfi;
    for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (m_inventory.Own(i))
        {
            iDisplayID = m_inventory.Own(i)->GetProto()->DisplayInfoID;
            iIventoryType = m_inventory.Own(i)->GetProto()->InventoryType;

            _cfi =  iDisplayID | (iIventoryType << 24);
            corpse->SetUInt32Value(CORPSE_FIELD_ITEM + i, _cfi);
        }
    }

    if (!GetMap()->IsBattleGround())
    {
        corpse->SaveToDB();
    }

    sCorpseManager.Add(corpse);
    return corpse;
}

void Player::SpawnCorpseBones()
{
    if (sCorpseManager.ConvertCorpseForPlayer(GetObjectGuid()))
    {
        if (!GetSession()->PlayerLogoutWithSave())
        {
            SaveToDB();
        }
    }
}

Corpse* Player::GetCorpse() const
{
    return sCorpseManager.FindForPlayer(GetObjectGuid());
}

void Player::RepopAtGraveyard()
{

    uint32 graveMap;
    float graveX, graveY, graveZ;
    GetWorldAnchor(graveMap, graveX, graveY, graveZ);

    if (!IsAlive() && GetMap()->AsTransport())
    {
        ResurrectPlayer(0.5f);
        SpawnCorpseBones();
    }

    WorldSafeLocsEntry const* ClosestGrave = nullptr;

    if (BattleGround* bg = Battle().Ground())
    {
        ClosestGrave = bg->GetClosestGraveYard(this);
    }
    else
    {
        ClosestGrave = sObjectMgr.GetClosestGraveYard(graveX, graveY, graveZ, graveMap, GetTeam());
    }

    m_deathTimer = 0;

    if (ClosestGrave)
    {
        bool updateVisibility = IsInWorld() && GetMapId() == ClosestGrave->map_id;
        TeleportTo(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, Where().Facing(), 0, GetSession()->isLogingOut());
        if (updateVisibility && IsInWorld())
        {
            UpdateVisibilityAndView();
        }
    }
}

static reclaim::Climbs LadderClimbedOn()
{
    reclaim::Climbs which;
    which.onPvP = sWorld.getConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP);
    which.onPvE = sWorld.getConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE);

    return which;
}

uint32 Player::GetCorpseReclaimDelay(bool pvp) const
{
    if (!reclaim::Climbing(pvp, LadderClimbedOn()))
    {
        return reclaim::Wait(0);
    }

    return reclaim::Wait(reclaim::Rung(time(nullptr), m_deathExpireTime));
}

void Player::UpdateCorpseReclaimDelay()
{
    const bool pvp = m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH;

    if (!reclaim::Climbing(pvp, LadderClimbedOn()))
    {
        return;
    }

    m_deathExpireTime = reclaim::Climbed(time(nullptr), m_deathExpireTime);
}

void Player::SendCorpseReclaimDelay(bool load)
{
    Corpse* corpse = GetCorpse();
    if (!corpse)
    {
        return;
    }

    uint32 delay;
    if (load)
    {
        if (corpse->GetGhostTime() > m_deathExpireTime)
        {
            return;
        }

        bool pvp = corpse->GetType() == CORPSE_RESURRECTABLE_PVP;

        const uint32 rung = reclaim::Climbing(pvp, LadderClimbedOn())
                          ? reclaim::Rung(corpse->GetGhostTime(), m_deathExpireTime)
                          : 0;

        time_t expected_time = corpse->GetGhostTime() + reclaim::Wait(rung);

        time_t now = time(nullptr);
        if (now >= expected_time)
        {
            return;
        }

        delay = uint32(expected_time - now);
    }
    else
    {
        delay = GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP);
    }

    WorldPacket data(SMSG_CORPSE_RECLAIM_DELAY, 4);
    data << uint32(delay * IN_MILLISECONDS);
    GetSession()->SendPacket(&data);
}
