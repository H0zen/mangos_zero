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

#include "Platform/Define.h"

#include <list>

class Player;
class Spell;
struct SpellEntry;
struct SpellModifier;

typedef std::list<SpellModifier*> SpellModList;

/**
 * The standing changes a character's talents and auras make to his own spells.
 *
 * Each change names one number it touches -- the cost, the cast time, the range,
 * the duration -- and the changes touching the same number are kept together, so
 * applying them is one walk over one list rather than a search.
 *
 * A change may be flat or by the hundred. Both kinds are gathered before either
 * is used: the percentages are summed and applied to the base, then the flats
 * are added, so two talents that each give a tenth give a fifth and never a
 * tenth of a tenth.
 *
 * Some changes are spent by use rather than by time. A change with charges loses
 * one each time it is applied, but only once per spell -- a spell that asks
 * about its cost and then about its cast time must not be charged twice. When
 * the last charge goes the change is not removed at once: it is marked spent and
 * held until the spell that spent it has finished, because that spell is still
 * asking about numbers the change is meant to have altered. A count of how many
 * are waiting like that is kept so the sweep can be skipped when none are.
 *
 * A spell that is cancelled instead of cast gives back what it spent, and the
 * change stands again.
 */
class SpellModifiers
{
    public:

        explicit SpellModifiers(Player& who) : m_owner(who), m_awaitingRemoval(0) {}

        /// Puts a change in force, or takes it out and destroys it. Either way
        /// the client is told the new total for every spell family it touches.
        void Add(SpellModifier* mod, bool apply);

        /// The change on the given number that came from the given spell.
        SpellModifier* From(SpellModOp op, uint32 spellId) const;

        /// Whether a change bears on a spell at all.
        bool Affects(SpellEntry const* spellInfo, SpellModifier* mod, Spell const* spell) const;

        /// Alters `base` by every change bearing on the spell, and hands back
        /// the difference it made.
        template <class T>
        T Apply(uint32 spellId, SpellModOp op, T& base, Spell const* spell = nullptr);

        /// Takes away the changes that the finished spell spent the last charge of.
        void Spent(Spell const* spell);

        /// Gives back what a cancelled spell spent.
        void Restore(Spell const* spell);

    private:

        Player& m_owner;

        SpellModList m_byNumber[MAX_SPELLMOD];

        /// How many changes are spent but still held for a spell in flight.
        int32 m_awaitingRemoval;
};
