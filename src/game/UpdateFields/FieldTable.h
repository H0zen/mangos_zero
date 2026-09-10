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
#include "ObjectGuid.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

class Object;
class Player;

namespace Fields
{

    enum class Kind : uint8
    {
        Int,
        Float,
        Guid,
        Bytes,
        TwoShort,
    };

    enum Visibility : uint16
    {
        VisNone      = 0x000,
        VisPublic    = 0x001,
        VisPrivate   = 0x002,
        VisOwner     = 0x004,
        VisItemOwner = 0x010,
        VisSpecial   = 0x020,
        VisParty     = 0x040,
        VisDynamic   = 0x100,
    };

    typedef uint16 Audience;

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
        uint16            count;
        uint16            blocks;
        uint32 const*     visibility;

        Descriptor const& At(uint16 index) const
        {
            MANGOS_ASSERT(index < count);
            return fields[index];
        }

        uint32 const* MaskForBit(uint8 bit) const { return visibility + bit * blocks; }
    };

    uint16 const MaxBlocks = 41;

    Table const& For(uint8 typeId);

    Audience AudienceFor(Object const& object, Player const& observer);

    void MaskFor(Table const& table, Audience audience, uint32* out);

    uint32 Project(Object const& object, Player& observer, uint16 index, uint32 raw);

    uint32 HealthAsPercent(uint32 current, uint32 max);

    bool ReadsRealHitPoints(ObjectGuid unit, ObjectGuid owner, ObjectGuid observer);

    bool LivesOutside(uint8 typeId, uint16 index);

    uint32 const* OutsideMask(uint8 typeId);

    inline uint8 LowestSet(uint32 word)
    {
#if defined(_MSC_VER)
        unsigned long bit;
        _BitScanForward(&bit, word);
        return uint8(bit);
#else
        return uint8(__builtin_ctz(word));
#endif
    }

    template<typename Fn>
    void ForEachSet(uint32 const* mask, uint16 blocks, Fn&& fn)
    {
        for (uint16 block = 0; block < blocks; ++block)
        {
            uint32 word = mask[block];
            while (word)
            {
                uint8 const bit = LowestSet(word);
                fn(uint16(block * 32 + bit));
                word &= word - 1;
            }
        }
    }
}
