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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Duel.h"

#include "GameObject.h"
#include "Map.h"
#include "Opcodes.h"
#include "Player.h"
#include "Reaction.h"
#include "SpellAuras.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <vector>

void Duel::TellCountdown(uint32 milliseconds)
{
    WorldPacket data(SMSG_DUEL_COUNTDOWN, 4);
    data << uint32(milliseconds);
    m_owner.GetSession()->SendPacket(&data);
}

void Duel::CountdownRunsOut(time_t now)
{
    if (!Stands() || m_acceptedAt == 0 || now < m_acceptedAt + 3)
    {
        return;
    }

    m_owner.SetUInt32Value(PLAYER_DUEL_TEAM, 1);
    m_against->SetUInt32Value(PLAYER_DUEL_TEAM, 2);

    m_acceptedAt = 0;
    m_startedAt = now;

    Duel& other = m_against->Duelling();
    other.m_acceptedAt = 0;
    other.m_startedAt = now;
}

void Duel::WatchTheFlag(time_t now)
{
    if (!Stands())
    {
        return;
    }

    GameObject* flag = m_owner.GetMap()->GetGameObject(m_owner.GetDuelArbiterGuid());
    if (!flag)
    {
        return;
    }

    if (m_outOfBoundsSince == 0)
    {
        if (!InReach(m_owner, *flag, 80))
        {
            m_outOfBoundsSince = now;

            WorldPacket data(SMSG_DUEL_OUTOFBOUNDS, 0);
            m_owner.GetSession()->SendPacket(&data);
        }

        return;
    }

    if (InReach(m_owner, *flag, 70))
    {
        m_outOfBoundsSince = 0;

        WorldPacket data(SMSG_DUEL_INBOUNDS, 0);
        m_owner.GetSession()->SendPacket(&data);
    }
    else if (now >= m_outOfBoundsSince + 10)
    {
        Complete(DUEL_FLED);
    }
}

void Duel::Complete(DuelCompleteType type)
{
    if (!Stands())
    {
        return;
    }

    Player* other = m_against;

    WorldPacket data(SMSG_DUEL_COMPLETE, 1);
    data << uint8(type != DUEL_INTERRUPTED ? 1 : 0);
    m_owner.GetSession()->SendPacket(&data);

    if (other->GetSession())
    {
        other->GetSession()->SendPacket(&data);
    }

    if (type != DUEL_INTERRUPTED)
    {
        data.Initialize(SMSG_DUEL_WINNER, (1 + 20));        // we guess size
        data << uint8(type == DUEL_WON ? 0 : 1);            // 0 = just won; 1 = fled
        data << other->GetName();
        data << m_owner.GetName();
        Broadcast(m_owner, &data, true);
    }

    if (type == DUEL_FLED)
    {
        // on the same side, or neither of them open to attack, they stop swinging
        if (m_initiator->GetTeam() == other->GetTeam())
        {
            m_initiator->AttackStop();
            other->AttackStop();
        }
        else
        {
            if (!m_initiator->IsPvP())
            {
                m_initiator->AttackStop();
            }
            if (!other->IsPvP())
            {
                other->AttackStop();
            }
        }
    }

    if (GameObject* flag = m_owner.GetMap()->GetGameObject(m_owner.GetDuelArbiterGuid()))
    {
        m_initiator->RemoveGameObject(flag, true);
    }

    // everything harmful either man laid on the other since it began comes off
    Strip(*other, m_owner.GetObjectGuid());
    Strip(m_owner, other->GetObjectGuid());

    // combo points held on the other man, or on his beast, are dropped
    if (m_owner.GetComboTargetGuid() == other->GetObjectGuid() ||
        m_owner.GetComboTargetGuid() == other->GetPetGuid())
    {
        m_owner.ClearComboPoints();
    }

    if (other->GetComboTargetGuid() == m_owner.GetObjectGuid() ||
        other->GetComboTargetGuid() == m_owner.GetPetGuid())
    {
        other->ClearComboPoints();
    }

    m_owner.SetDuelArbiterGuid(ObjectGuid());
    m_owner.SetUInt32Value(PLAYER_DUEL_TEAM, 0);
    other->SetDuelArbiterGuid(ObjectGuid());
    other->SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    other->Duelling().Forget();
    Forget();
}

void Duel::Strip(Player& from, ObjectGuid castBy)
{
    std::vector<uint32> laidOn;

    for (auto const& held : from.GetSpellAuraHolderMap())
    {
        SpellAuraHolder const* aura = held.second;
        if (!aura->IsPositive() && aura->GetCasterGuid() == castBy && aura->GetAuraApplyTime() >= m_startedAt)
        {
            laidOn.push_back(aura->GetId());
        }
    }

    for (uint32 spellId : laidOn)
    {
        from.RemoveAuras(spellId);
    }
}
