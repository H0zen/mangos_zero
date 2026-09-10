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

#include "MovementDefines.h"

class Unit;

class Pace
{
    public:

        explicit Pace(Unit& whose);
        virtual ~Pace() = default;

        Pace(Pace const&) = delete;
        Pace& operator=(Pace const&) = delete;

        virtual void Reckon(UnitMoveType how, bool forced, float ratio = 1.0f);

        float At(UnitMoveType how) const;

        float RateOf(UnitMoveType how) const { return m_rate[how]; }

        void SetRate(UnitMoveType how, float rate, bool forced = false);

    protected:

        Unit& m_owner;

    private:

        float m_rate[MAX_MOVE_TYPE];
};

class PetPace : public Pace
{
    public:

        explicit PetPace(Unit& whose) : Pace(whose) {}

        void Reckon(UnitMoveType how, bool forced, float ratio = 1.0f) override;
};
