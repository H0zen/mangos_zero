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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <list>

/// One immunity, and the spell that is holding it open.
struct SpellImmune
{
    uint32 type;
    uint32 spellId;
};

typedef std::list<SpellImmune> SpellImmuneList;

/**
 * @brief What cannot touch a unit, and what is keeping it that way.
 *
 * Six separate questions, kept apart because they are asked apart: a school of magic, a
 * kind of damage, a dispel, a mechanic, a spell effect, an applied state. Each entry
 * remembers the spell that granted it, because an immunity lasts exactly as long as its
 * source and goes when that source does.
 *
 * One source per kind: granting an immunity of a kind already held replaces what was there,
 * so a second Divine Shield does not have to be counted, only remembered.
 */
class Immunities
{
    public:

        /// Hold this kind open, for as long as `spellId` lasts.
        void Grant(uint32 spellId, uint32 op, uint32 type);

        /// Let go of everything this spell was holding open in that bucket.
        void Revoke(uint32 spellId, uint32 op);

        /// Nothing is immune to anything.
        void Clear();

        SpellImmuneList const& Of(uint32 op) const { return m_lists[op]; }

        /// Is anything held open here whose type meets this mask?
        bool AnyOf(uint32 op, uint32 mask) const;

        /// Is exactly this type held open here?
        bool Exactly(uint32 op, uint32 type) const;

    private:

        SpellImmuneList m_lists[MAX_SPELL_IMMUNITY];
};
