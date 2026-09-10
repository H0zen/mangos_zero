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

#include <atomic>
#include <optional>
#include <vector>

class Player;

enum class InitialWorldEntryPacket
{
    InitWorldStates,
    TriggerCinematic,
    ExplorationExperience,
    LoginEffectResult,
    LoginTimeSpeed
};

inline std::vector<InitialWorldEntryPacket>
InitialWorldEntryPacketOrder(bool cinematic)
{

    std::vector<InitialWorldEntryPacket> order = {
        InitialWorldEntryPacket::InitWorldStates
    };
    if (cinematic)
    {
        order.push_back(InitialWorldEntryPacket::TriggerCinematic);
        order.push_back(InitialWorldEntryPacket::ExplorationExperience);
    }
    order.push_back(InitialWorldEntryPacket::LoginEffectResult);
    order.push_back(InitialWorldEntryPacket::LoginTimeSpeed);
    return order;
}

struct InitialWorldEntryContext
{
    uint32 anchorMapId = 0;
    uint32 zoneId = 0;
    uint32 areaId = 0;
    bool initialWorldStatesSent = false;
    bool cinematicStarted = false;
};

enum class LoginEffectPhase
{
    Start,
    Go,
    Complete
};

constexpr uint32 LoginEffectDelayBefore(LoginEffectPhase phase)
{

    return phase == LoginEffectPhase::Start ? 1000 : 1;
}

class LoginEffectSequenceState
{

    public:
        std::optional<LoginEffectPhase> TakeNext(bool inWorld)
        {
            if (!inWorld || m_phase == LoginEffectPhase::Complete)
            {
                m_phase = LoginEffectPhase::Complete;
                return std::nullopt;
            }

            LoginEffectPhase current = m_phase;
            m_phase = current == LoginEffectPhase::Start ?
                LoginEffectPhase::Go : LoginEffectPhase::Complete;
            return current;
        }

        bool IsComplete() const
        {
            return m_phase == LoginEffectPhase::Complete;
        }

    private:
        LoginEffectPhase m_phase = LoginEffectPhase::Start;
};

class LoginCinematicRootOwnership
{
    public:
        bool Claim()
        {
            bool expected = false;
            return m_owned.compare_exchange_strong(expected, true);
        }

        bool ReleaseOnce(bool canRelease)
        {

            if (!canRelease)
            {
                return false;
            }
            return m_owned.exchange(false);
        }

        void Clear()
        {
            m_owned.store(false);
        }

        bool IsOwned() const
        {
            return m_owned.load();
        }

    private:
        std::atomic_bool m_owned{false};
};

class InitialWorldEntryHook
{
    public:
        explicit InitialWorldEntryHook(uint32 cinematicSequenceId)
            : m_cinematicSequenceId(cinematicSequenceId)
        {
        }

        void AfterAddToWorld(Player& player);

        InitialWorldEntryContext const* GetContext() const
        {
            return m_context ? &*m_context : nullptr;
        }

    private:
        uint32 m_cinematicSequenceId;
        std::optional<InitialWorldEntryContext> m_context;
};

constexpr uint32 LOGIN_CINEMATIC_ROOT_TIMEOUT_MS = 120000;
