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

#include "Behaviour.h"
#include "UserTally.h"
#include "Chest.h"
#include "CapturePoint.h"

class CountingBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        UserTally& Tally() { return m_tally; }
        UserTally const& Tally() const { return m_tally; }

    protected:
        UserTally m_tally;
};

class DoorBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
        Tick TimedOut() override;
        void InUse(uint32 elapsed) override;
};

class ButtonBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
        Tick TimedOut() override;
        void InUse(uint32 elapsed) override;
};

class QuestGiverBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class ChestBehaviour : public CountingBehaviour
{
    public:
        using CountingBehaviour::CountingBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
        void Arming() override;
        void InUse(uint32 elapsed) override;
        Tick Spent() override;
        void Respawning() override;

        Chest& Lock() { return m_lock; }

    private:
        Chest m_lock;
};

class GenericBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class TrapBehaviour : public CountingBehaviour
{
    public:
        using CountingBehaviour::CountingBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
        void Arming() override;
        Tick Standing() override;
};

class ChairBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class SpellFocusBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class GooberBehaviour : public CountingBehaviour
{
    public:
        using CountingBehaviour::CountingBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
        void InUse(uint32 elapsed) override;
        Tick Spent() override;
};

class CameraBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class FishingNodeBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
        void Arming() override;
        Tick TimedOut() override;
};

class RitualBehaviour : public CountingBehaviour
{
    public:
        using CountingBehaviour::CountingBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class SpellCasterBehaviour : public CountingBehaviour
{
    public:
        using CountingBehaviour::CountingBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class FlagStandBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class FishingHoleBehaviour : public CountingBehaviour
{
    public:
        using CountingBehaviour::CountingBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class FlagDropBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

class CapturePointBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        void InUse(uint32 elapsed) override;
        Tick Spent() override;

        CapturePoint& Bar() { return m_bar; }
        CapturePoint const& Bar() const { return m_bar; }

        void Restore(float value, bool isLocked);

        void Tick();

    private:
        CapturePoint m_bar;
};
