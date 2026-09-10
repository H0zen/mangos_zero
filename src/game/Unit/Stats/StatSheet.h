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

#pragma once

#include "SharedDefines.h"

class Unit;

class StatSheet
{
    public:

        virtual ~StatSheet() = default;

        StatSheet(StatSheet const&) = delete;
        StatSheet& operator=(StatSheet const&) = delete;

        virtual void Stat(Stats stat) = 0;

        virtual void Everything() = 0;

        virtual void Resistance(uint32 school);

        virtual void Armour() = 0;
        virtual void MaxHealth() = 0;
        virtual void MaxPower(Powers power) = 0;
        virtual void AttackPower(bool ranged) = 0;

        virtual void Swing(WeaponAttackType attType) = 0;

        virtual uint32 ShieldBlock() const = 0;

    protected:

        explicit StatSheet(Unit& whose) : m_unit(whose) {}

        Unit& m_unit;
};

class BlankSheet : public StatSheet
{
    public:

        explicit BlankSheet(Unit& whose) : StatSheet(whose) {}

        void Stat(Stats ) override {}
        void Everything() override {}
        void Resistance(uint32 ) override {}
        void Armour() override {}
        void MaxHealth() override {}
        void MaxPower(Powers ) override {}
        void AttackPower(bool ) override {}
        void Swing(WeaponAttackType ) override {}
        uint32 ShieldBlock() const override { return 0; }
};
