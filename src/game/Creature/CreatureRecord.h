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

struct CreatureInfo;

/**
 * What the client is told when it asks what kind of thing this is.
 *
 * This is SMSG_CREATURE_QUERY_RESPONSE, field for field and in its order, and it
 * is the whole of what the client keeps about a creature outside the update
 * fields. It hangs off the unit at [unit+0xB30] there, cached by entry, because
 * nothing in it varies between two creatures of the same entry -- which is why
 * the client asks once and never again.
 *
 * Pairing what this core sends with the offsets the client reads settles the
 * layout completely:
 *
 *     +0x00  name              +0x20  rank
 *     +0x10  subname           +0x24  a zero, sent and never read
 *     +0x14  type flags        +0x28  pet spell data id
 *     +0x18  creature type     +0x2C  display id
 *     +0x1C  family            +0x30  civilian, +0x31 racial leader
 *
 * A unit that has no such record is a player: nobody asks what kind of creature
 * a player is, because the answer comes from his race instead.
 */
class CreatureRecord
{
    public:
        /// Nothing to tell.
        CreatureRecord() = default;

        explicit CreatureRecord(CreatureInfo const& of) : m_of(&of) {}

        /// Whether there is anything here at all.
        explicit operator bool() const { return m_of != nullptr; }

        /* ****************** What goes on the wire ****************** */

        char const* Name() const;

        /// The line under the name: <Blacksmith>, <Guard Captain>.
        char const* Title() const;

        /// CreatureType.dbc: beast, humanoid, undead and so on.
        uint32 Kind() const;

        /// CreatureFamily.dbc, and zero for anything that is not a beast.
        uint32 Family() const;

        /// Normal, elite, rare elite, world boss, rare.
        uint32 Rank() const;

        /// CreatureSpellData.dbc, for a pet that comes with spells of its own.
        uint32 PetSpells() const;

        uint32 Flags() const;

        /// It will not fight back, and killing it is a crime.
        bool IsCivilian() const;

        /// One of the racial leaders, worth a bounty and an announcement.
        bool IsRacialLeader() const;

        /* ****************** What the flags say ****************** */

        /// A hunter may tame it. It has to be a beast, and a beast with a family:
        /// the client gates on the flag and then reads the family, so one without
        /// leaves it with nothing to look up.
        bool IsTameable() const;

        /// Its tooltip says Boss where a level would be.
        bool IsBoss() const;

        /// A dead player can see it: spirit healers, spirit guides, and the few
        /// others that only have business with the dead.
        bool IsVisibleToGhosts() const;

        /// Anyone may help it in a fight. Every one of these is an escort.
        bool CanBeAssisted() const;

        /// Heard from further off than its size would suggest. Raid bosses.
        bool IsMoreAudible() const;

        /// Which profession opens its corpse.
        SkillType RequiredLootSkill() const;

    private:
        CreatureInfo const* m_of = nullptr;
};
