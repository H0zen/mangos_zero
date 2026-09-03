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

#include <cstdint>

namespace spells
{
    /**
     * The identifiers the spell actors pass between themselves.
     *
     * They are distinct types rather than bare integers because almost every
     * one of them is a uint32, and the old engine passed spell ids, effect
     * indices and slot numbers through the same parameter lists. A swapped pair
     * compiled fine there and misbehaved at runtime.
     */

    /// A row in Spell.dbc. The client knows these numbers; they are contract.
    enum class SpellId : uint32_t { None = 0 };

    /// One run of one spell. Unique for the life of the process, never sent.
    enum class CastId : uint64_t { None = 0 };

    /// Which of a spell's three effects is meant.
    enum class EffectIndex : uint8_t { First = 0, Second = 1, Third = 2 };

    /// Position in the 48 aura slots the client can display.
    enum class VisibleSlot : uint8_t { None = 0xFF };

    constexpr uint8_t EFFECTS_PER_SPELL = 3;

    /// The client shows this many auras and no more; the split between positive
    /// and negative is part of what it expects.
    constexpr uint8_t VISIBLE_AURA_SLOTS = 48;
    constexpr uint8_t VISIBLE_POSITIVE_SLOTS = 32;

    constexpr uint32_t Raw(SpellId id) { return static_cast<uint32_t>(id); }
    constexpr uint8_t Raw(EffectIndex index) { return static_cast<uint8_t>(index); }
    constexpr uint8_t Raw(VisibleSlot slot) { return static_cast<uint8_t>(slot); }
    constexpr uint64_t Raw(CastId id) { return static_cast<uint64_t>(id); }

    constexpr bool Valid(SpellId id) { return id != SpellId::None; }
    constexpr bool Shown(VisibleSlot slot) { return slot != VisibleSlot::None; }
}
