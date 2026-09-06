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

class Creature;

/**
 * The numbers an ordinary creature fights with.
 *
 * The simple case: a creature has no stats to derive anything from, so nearly
 * every number is its modifiers folded and written down. Only the swing is
 * really computed, and only because attack power feeds into it.
 */
class CreatureSheet : public StatSheet
{
    public:

        explicit CreatureSheet(Creature& whose);

        /// A creature's stats buy it nothing, so there is nothing to follow.
        void Stat(Stats stat) override;

        void Everything() override;
        void Armour() override;
        void MaxHealth() override;
        void MaxPower(Powers power) override;
        void AttackPower(bool ranged) override;
        void Swing(WeaponAttackType attType) override;

    private:

        Creature& m_owner;
};
