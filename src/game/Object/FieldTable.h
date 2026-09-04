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
#include "Utilities/Errors.h"

class Object;
class Player;

/**
 * What every update field is, and who may be told about it.
 *
 * The client carries this as data -- a 20-byte record per field giving its name,
 * width, storage kind and a word of visibility flags -- and replicates it per
 * dword so any raw wire index resolves in one step. This is that table.
 *
 * Nothing here is a guess. It is generated from the client's own records, and
 * the generator refuses to run if a single index disagrees with the server's
 * enum.
 */
namespace Fields
{
    /// How the value is stored, which is what says whether it needs converting
    /// on the way out: the server keeps some INT fields in a float.
    enum class Kind : uint8
    {
        Int,
        Float,
        Guid,
        Bytes,
        TwoShort,
    };

    /// The client's own visibility word. A field is sent when this intersects
    /// the observer's audience, so a field flagged VisNone -- the paddings --
    /// can never be sent at all.
    enum Visibility : uint16
    {
        VisNone      = 0x000,
        VisPublic    = 0x001,   ///< anyone who can see the object
        VisPrivate   = 0x002,   ///< the object itself
        VisOwner     = 0x004,   ///< whoever owns it: a pet's master, an item's holder
        VisItemOwner = 0x010,   ///< only ever on ITEM fields, always with VisOwner
        VisSpecial   = 0x020,   ///< inspectable detail: damage, resistances
        VisParty     = 0x040,   ///< only on the twenty quest-log fields
        VisDynamic   = 0x100,   ///< health and the dynamic flags: sent to all, but rewritten per observer
    };

    /// Which visibility bits an observer is entitled to. Built per (object,
    /// observer) pair; the mask to send is the union of the bits it carries.
    typedef uint16 Audience;

    /// One per dword. `index` is the first dword of the field this dword belongs
    /// to, so a multi-dword field is recognisable from any of its parts.
    struct Descriptor
    {
        uint16      index;
        uint16      words;
        Kind        kind;
        uint16      visibility;
        char const* name;
    };

    struct Table
    {
        Descriptor const* fields;
        uint16            count;        ///< dwords in this class, i.e. *_END
        uint16            blocks;       ///< mask blocks: (count + 31) / 32
        uint32 const*     visibility;   ///< nine masks of `blocks` dwords, one per visibility bit

        Descriptor const& At(uint16 index) const
        {
            MANGOS_ASSERT(index < count);
            return fields[index];
        }

        uint32 const* MaskForBit(uint8 bit) const { return visibility + bit * blocks; }
    };

    /// Mask blocks of the widest class, PLAYER: ceil(1282 / 32). The client
    /// checks this same limit before reading a mask, so it is the ceiling for
    /// every object and a send mask fits on the stack.
    uint16 const MaxBlocks = 41;

    /// The table for a TypeID. Generated; see FieldTable.cpp.
    Table const& For(uint8 typeId);

    /// What this observer may be told about this object.
    Audience AudienceFor(Object const& object, Player const& observer);

    /// Fold an audience into a mask of the fields it admits.
    void MaskFor(Table const& table, Audience audience, uint32* out);

    /**
     * The value to put on the wire, which is not always the value that is
     * stored: a creature's health is a percentage to everyone but itself, a
     * trainer is not a trainer to a class it does not teach, and a corpse is
     * only lootable to whoever may loot it.
     *
     * Returns the raw value when the field means the same thing to everybody.
     */
    uint32 Project(Object const& object, Player& observer, uint16 index, uint32 raw);

    /**
     * Hit points as anyone but the unit itself is told them.
     *
     * Established from captures of builds 1.10.0 and 1.12.1: across 282 create
     * blocks every object that was not the observer carried MAXHEALTH exactly
     * 100 -- a level 55 guard included -- while blocks flagged UPDATEFLAG_SELF
     * carried the real pool. Power is never scaled; only health is. Reading a
     * real hit-point count off someone else is a Burning Crusade feature.
     *
     * A unit that is alive never reads as zero, or a sliver of health on a big
     * pool would look dead.
     */
    uint32 HealthAsPercent(uint32 current, uint32 max);

    /// Fields that must go out on every update block, because what they mean to
    /// an observer can change while the stored value stands still.
    bool AlwaysResend(uint8 typeId, uint16 index);
}
