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

#include "Spells/Ids.h"

#include <cstdint>
#include <string>
#include <vector>

/**
 * The contract with the 1.12 client, and the only thing in this subsystem that
 * is not ours to choose.
 *
 * Everything the client sends or expects to receive is described here as data.
 * Nothing in this header performs logic; the serialisers read these structures
 * and the rest of the engine fills them. Keeping the contract in one place is
 * what lets the rest be redesigned freely: if a change does not alter anything
 * in this file, it cannot be seen from outside the process.
 */
namespace spells::wire
{
    /// Bits of the target block's mask, as the client writes them.
    enum class TargetBit : uint16_t
    {
        Self            = 0x0000,
        Unit            = 0x0002,
        Item            = 0x0010,
        SourceLocation  = 0x0020,
        DestLocation    = 0x0040,
        Object          = 0x0800,
        TradeItem       = 0x1000,
        String          = 0x2000,
        Corpse          = 0x8000,
    };

    constexpr uint16_t Bit(TargetBit b) { return static_cast<uint16_t>(b); }

    /// Flags on SMSG_SPELL_START / SMSG_SPELL_GO that tell the client what tail
    /// to expect. The tail is what the flags say it is; nothing else decides.
    enum class CastFlag : uint16_t
    {
        None    = 0x0000,
        Hidden  = 0x0002,   ///< start-side marker the client expects
        Ammo    = 0x0020,   ///< projectile block follows
        Fired   = 0x0100,   ///< go-side marker the client expects
    };

    /**
     * The target block, parsed once from CMSG_CAST_SPELL and written back
     * unchanged in SPELL_START and SPELL_GO.
     *
     * Coordinates are transport-relative when the caster is aboard a vessel and
     * world coordinates otherwise; there is no second flag saying which, because
     * the frame is a property of the caster, not of the packet.
     */
    struct Targets
    {
        uint16_t mask = 0;

        uint64_t unit = 0;
        uint64_t object = 0;
        uint64_t item = 0;
        uint64_t corpse = 0;

        float srcX = 0.f, srcY = 0.f, srcZ = 0.f;
        float dstX = 0.f, dstY = 0.f, dstZ = 0.f;

        std::string text;

        bool Has(TargetBit bit) const { return (mask & Bit(bit)) != 0; }
    };

    /// Why a target did not take the spell. The numbering is the client's.
    enum class Miss : uint8_t
    {
        None     = 0,
        Miss     = 1,
        Resist   = 2,
        Dodge    = 3,
        Parry    = 4,
        Block    = 5,
        Evade    = 6,
        Immune   = 7,
        Immune2  = 8,
        Deflect  = 9,
        Absorb   = 10,
        Reflect  = 11,
    };

    /**
     * One line of the SPELL_GO hit list.
     *
     * In 1.12 the client is handed every target in the hit list and reads the
     * miss reasons from a second, shorter run -- so a landing and a miss are the
     * same record here, distinguished by `reason`.
     */
    struct Landing
    {
        uint64_t target = 0;    ///< full GUID; this list is not packed
        Miss reason = Miss::None;

        bool Hit() const { return reason == Miss::None; }
    };

    /// Everything SMSG_SPELL_START and SMSG_SPELL_GO need, gathered by the cast.
    struct CastReport
    {
        uint64_t caster = 0;        ///< packed on the wire
        uint64_t castItem = 0;      ///< written in the caster's place when set
        SpellId spell = SpellId::None;
        uint16_t flags = 0;
        uint32_t timer = 0;         ///< remaining cast time, START only

        Targets targets;
        std::vector<Landing> landings;

        /// Ammo display id and inventory type, written only when CastFlag::Ammo.
        uint32_t ammoDisplayId = 0;
        uint32_t ammoInventoryType = 0;
    };

    /**
     * The outcome of asking whether a cast may begin.
     *
     * The numbering is contract: the client turns it into the red text on the
     * screen, so these values are never renumbered to suit the server. The set
     * here is the subset the engine can actually produce; unknown reasons are
     * not invented.
     */
    enum class Refusal : uint8_t
    {
        Allowed          = 0xFF,   ///< not sent; means the cast proceeds

        OutOfRange       = 74,
        NotInFrontOf     = 76,
        LineOfSight      = 46,
        NoPower          = 79,
        NotReady         = 68,
        Interrupted      = 47,
        Moving           = 61,
        BadTargets       = 12,
        NoTarget         = 86,
        Immune           = 39,
        Silenced         = 96,
        Pacified         = 63,
        Stunned          = 106,
        Dead             = 20,
        Fizzle           = 27,
        NotKnown         = 66,
        ItemMissing      = 87,
        ReagentsMissing  = 69,
    };

    constexpr bool Allowed(Refusal r) { return r == Refusal::Allowed; }

    /// What the client is told when a cast is refused.
    struct Refused
    {
        SpellId spell = SpellId::None;
        Refusal reason = Refusal::Allowed;
        uint32_t detail = 0;    ///< item or reagent id, when the reason names one
    };

    /**
     * One visible aura slot as the client models it.
     *
     * The client shows exactly VISIBLE_AURA_SLOTS of these, split so that
     * positive auras take the low slots. An aura with no slot is real and fully
     * simulated -- it is simply not drawn.
     */
    struct VisibleAura
    {
        SpellId spell = SpellId::None;
        uint8_t flags = 0;
        uint8_t level = 1;
        uint8_t stacks = 1;
        int32_t durationMs = 0;
        int32_t maxDurationMs = 0;
        bool positive = true;
    };
}
