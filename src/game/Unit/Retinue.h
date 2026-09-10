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

class Retinue
{
    public:

        explicit Retinue(Unit& whose) : m_owner(whose) {}

        void AddGuardian(Pet& guardian);
        void RemoveGuardian(Pet& guardian);

        void RemoveGuardians();

        Pet* GuardianOfEntry(uint32 entry) const;

        GuidSet const& Guardians() const { return m_guardians; }

        Totem* TotemIn(TotemSlot slot) const;

        Unit* UnitIn(TotemSlot slot) const;

        void PutTotem(TotemSlot slot, Totem& totem);
        void TakeTotem(Totem& totem);

        void UnsummonAllTotems();

    private:

        Unit& m_owner;

        GuidSet m_guardians;
        ObjectGuid m_totems[MAX_TOTEM_SLOT] = {};
};
