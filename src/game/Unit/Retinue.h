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

#include "ObjectGuid.h"
#include "SharedDefines.h"

class Pet;
class Totem;
class Unit;

/**
 * The creatures a unit keeps about it.
 *
 * Two sorts, and neither is the one pet it drives from a bar. A guardian fights
 * for its master without being told to and is called back when the master falls
 * or leaves the world; there may be any number of them. A totem stands in one of
 * four grounds -- fire, earth, water, air -- and a second totem of the same
 * ground puts the first one out.
 *
 * Both are held by guid and looked up in the map, because either may be taken
 * away by something other than its master.
 */
class Retinue
{
    public:

        explicit Retinue(Unit& whose) : m_owner(whose) {}

        void AddGuardian(Pet& guardian);
        void RemoveGuardian(Pet& guardian);

        /// Calls every guardian back and forgets it.
        void RemoveGuardians();

        /// The guardian of that entry it already keeps, if it keeps one.
        Pet* GuardianOfEntry(uint32 entry) const;

        /// Who they are, for the roll-calls that walk them.
        GuidSet const& Guardians() const { return m_guardians; }

        /// What stands on that ground.
        Totem* TotemIn(TotemSlot slot) const;

        /// The same, for the roll-calls in `Unit.h` that cannot see a `Totem`.
        Unit* UnitIn(TotemSlot slot) const;

        void PutTotem(TotemSlot slot, Totem& totem);
        void TakeTotem(Totem& totem);

        /// Puts out every totem it has standing.
        void UnsummonAllTotems();

    private:

        Unit& m_owner;

        GuidSet m_guardians;
        ObjectGuid m_totems[MAX_TOTEM_SLOT];
};
