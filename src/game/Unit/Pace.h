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

/**
 * How fast a unit goes, walking, running, swimming and turning.
 *
 * The client is told a multiple of the ordinary pace for each, never the yards
 * a second, so what is kept here is that multiple. Six of them, one to a way of
 * moving, and all six are sent when a unit first comes into view.
 *
 * A change of pace travels: everything the unit controls -- its pet, its
 * guardians, whatever it has charmed -- reckons its own pace again, because a
 * hasted master drags a haste-less pet behind him otherwise.
 */
class Pace
{
    public:

        explicit Pace(Unit& whose);
        virtual ~Pace() = default;

        Pace(Pace const&) = delete;
        Pace& operator=(Pace const&) = delete;

        /// Works out that pace afresh from the auras it carries, the shape it is
        /// in and the row it was spawned from. `forced` makes the client accept
        /// the answer rather than merely being told of it.
        virtual void Reckon(UnitMoveType how, bool forced, float ratio = 1.0f);

        /// Yards a second: the multiple against the ordinary pace for that way
        /// of moving.
        float At(UnitMoveType how) const;

        /// The multiple itself.
        float RateOf(UnitMoveType how) const { return m_rate[how]; }

        /// Sets the multiple, tells everyone who can see it, and passes the
        /// change on to everything the unit controls.
        void SetRate(UnitMoveType how, float rate, bool forced = false);

    protected:

        Unit& m_owner;

    private:

        float m_rate[MAX_MOVE_TYPE];
};

/**
 * How fast a hunter's or a warlock's pet goes.
 *
 * A pet owned by a character does not reckon its pace on its own: it keeps up
 * with its master, and only its own haste and slow auras move it off that. A
 * pet nobody owns is an ordinary creature and reckons the ordinary way.
 */
class PetPace : public Pace
{
    public:

        explicit PetPace(Unit& whose) : Pace(whose) {}

        void Reckon(UnitMoveType how, bool forced, float ratio = 1.0f) override;
};
