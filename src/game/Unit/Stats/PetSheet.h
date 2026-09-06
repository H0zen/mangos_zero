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

#include "StatSheet.h"

class Pet;

/**
 * The numbers a pet fights with.
 *
 * A pet sits between the other two. It has stats, and armour, health, mana and
 * attack power grow with them -- but only by what it has gained since it was
 * created, because what it was created with is already in its modifiers. A
 * hunter's pet also hits as well as it feels.
 */
class PetSheet : public StatSheet
{
    public:

        explicit PetSheet(Pet& whose);

        void Stat(Stats stat) override;
        void Everything() override;
        void Armour() override;
        void MaxHealth() override;
        void MaxPower(Powers power) override;
        void AttackPower(bool ranged) override;
        void Swing(WeaponAttackType attType) override;

    private:

        Pet& m_owner;
};
