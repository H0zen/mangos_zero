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

/**
 * The numbers a unit fights with, worked out and written into its fields.
 *
 * The arithmetic itself lives in the `stats` namespace and knows nothing of
 * units. This is the other half: it reads the unit's modifiers and stats, hands
 * them to that arithmetic, and puts the answer where the client will read it.
 *
 * Who the unit is decides how the numbers come out -- a character derives nearly
 * everything from stats, an ordinary creature folds its modifiers and stops, a
 * pet takes some of each -- so there is a sheet per kind rather than a switch.
 */
class StatSheet
{
    public:

        virtual ~StatSheet() = default;

        StatSheet(StatSheet const&) = delete;
        StatSheet& operator=(StatSheet const&) = delete;

        /// One stat, and every number that follows from it.
        virtual void Stat(Stats stat) = 0;

        /// Everything at once, for a level gained or a sheet built from nothing.
        virtual void Everything() = 0;

        /// A school's resistance. Every unit works this out the same way, and
        /// the normal school is armour, which does not.
        virtual void Resistance(uint32 school);

        virtual void Armour() = 0;
        virtual void MaxHealth() = 0;
        virtual void MaxPower(Powers power) = 0;
        virtual void AttackPower(bool ranged) = 0;

        /// What a swing of that hand does, at its least and its most.
        virtual void Swing(WeaponAttackType attType) = 0;

        /// What it stops with a shield when a blow is blocked.
        virtual uint32 ShieldBlock() const = 0;

    protected:

        explicit StatSheet(Unit& whose) : m_unit(whose) {}

        Unit& m_unit;
};

/**
 * The sheet of something that fights with no numbers of its own.
 *
 * A totem is a creature the client draws and the server never asks to swing,
 * resist or bleed. Nothing is worked out for it, and this says so once instead
 * of eight times.
 */
class BlankSheet : public StatSheet
{
    public:

        explicit BlankSheet(Unit& whose) : StatSheet(whose) {}

        void Stat(Stats /*stat*/) override {}
        void Everything() override {}
        void Resistance(uint32 /*school*/) override {}
        void Armour() override {}
        void MaxHealth() override {}
        void MaxPower(Powers /*power*/) override {}
        void AttackPower(bool /*ranged*/) override {}
        void Swing(WeaponAttackType /*attType*/) override {}
        uint32 ShieldBlock() const override { return 0; }
};
