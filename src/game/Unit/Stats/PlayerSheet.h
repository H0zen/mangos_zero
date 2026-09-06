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

class Player;

/**
 * The numbers a character fights with.
 *
 * Nearly everything here is derived rather than folded: armour from agility and
 * intellect, health from stamina, mana from intellect, attack power from
 * strength and agility by class and by whatever shape a druid is in. The rules
 * are a table, and a table is either exercised or trusted.
 *
 * Three of the numbers have nowhere on the wire to live -- the crit chance of
 * each spell school, and the two rates a character regains mana at, standing
 * and casting -- so they are kept here and asked for by name.
 */
class PlayerSheet : public StatSheet
{
    public:

        explicit PlayerSheet(Player& whose);

        void Stat(Stats stat) override;
        void Everything() override;
        void Armour() override;
        void MaxHealth() override;
        void MaxPower(Powers power) override;
        void AttackPower(bool ranged) override;
        void Swing(WeaponAttackType attType) override;

        /// What a swing of that hand comes to, without writing it down. This is
        /// the one the blow itself is rolled from.
        void SwingRange(WeaponAttackType attType, bool normalized, float& least, float& most);

        /// The damage and healing his gear and auras add, per school, which the
        /// client shows him and the server never reads.
        void SpellDamageAndHealing();

        void Defences();
        void Block();
        void Parry();
        void Dodge();
        void Crit(WeaponAttackType attType);
        void AllCrits();

        void SpellCrit(uint32 school);
        void AllSpellCrits();
        float SpellCritChance(uint32 school) const { return m_spellCrit[school]; }

        void ManaRegen();
        float ManaRegenStanding() const { return m_manaRegenStanding; }
        float ManaRegenCasting() const { return m_manaRegenCasting; }

    private:

        /// The health the first twenty stamina buy, and the rest.
        float HealthFromStamina() const;
        /// The same rule for mana out of intellect.
        float ManaFromIntellect() const;

        Player& m_owner;

        float m_spellCrit[MAX_SPELL_SCHOOL] = {};
        float m_manaRegenStanding = 0.0f;
        float m_manaRegenCasting = 0.0f;
};
