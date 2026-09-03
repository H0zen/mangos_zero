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

#include "Spells/CastSlot.h"
#include "Spells/Caster.h"
#include "Spells/Effects.h"
#include "Spells/Ids.h"
#include "Spells/Wire.h"

#include <cstdint>
#include <vector>

namespace spells
{
    class SpellRow;

    /**
     * How a cast moves through the engine.
     *
     * ```
     *   Intent ──▶ Check ──▶ Spend ──▶ Launch ──▶ Aim ──▶ Resolve ──▶ Effects ──▶ Report
     *              (pure)    (mutates)                     (pure)      (mutate)
     * ```
     *
     * Two properties hold the whole thing together, and both are the opposite of
     * how the old engine worked.
     *
     * **Asking is free of consequence.** Every check is a pure function of the
     * spell's data and the current state. Nothing about the world changes while
     * the engine is deciding whether a cast may happen, so a check can be run
     * twice, run speculatively by AI, or run in a test with no world at all.
     * The old engine spent power and started cooldowns from inside functions
     * that were also deciding whether the cast was legal.
     *
     * **Changing is done in two named places.** `Spend` takes what the cast
     * costs; `combat::apply` is the only thing that moves a health bar. An
     * effect that wants to deal damage produces a result and hands it over; it
     * does not reach into a unit. When something goes wrong with health, there
     * are two functions to read, not two hundred.
     */

    // -------------------------------------------------------------------------
    // 1. Check -- pure
    // -------------------------------------------------------------------------

    /// Everything the checks are allowed to look at.
    struct CheckInput
    {
        const Caster* caster = nullptr;
        const SpellRow* row = nullptr;
        const Intent* intent = nullptr;
        Target target;
        uint64_t nowMs = 0;

        /// Answered by the map, because line of sight is terrain, not spells.
        bool lineOfSight = true;
        float distance = 0.f;
    };

    /**
     * Whether this cast may start.
     *
     * A caster aboard a vessel measuring a target ashore fails closed: the two
     * are in different frames, and a distance between frames is not a smaller
     * or larger number, it is an absent one.
     */
    wire::Refusal Check(const CheckInput& in);

    /// The individual checks, exposed so a caller can ask one thing.
    wire::Refusal CheckKnown(const CheckInput& in);
    wire::Refusal CheckReady(const CheckInput& in);
    wire::Refusal CheckPower(const CheckInput& in);
    wire::Refusal CheckRange(const CheckInput& in);
    wire::Refusal CheckFacing(const CheckInput& in);
    wire::Refusal CheckTarget(const CheckInput& in);
    wire::Refusal CheckCasterState(const CheckInput& in);

    // -------------------------------------------------------------------------
    // 2. Spend -- the only place a cast costs anything
    // -------------------------------------------------------------------------

    /// What a cast is about to cost, computed before anything is taken.
    struct Cost
    {
        int32_t power = 0;
        uint32_t cooldownMs = 0;
        uint32_t categoryMs = 0;
        uint32_t categoryId = 0;
        uint32_t globalMs = 0;
        std::vector<uint32_t> reagents;
        std::vector<uint32_t> reagentCounts;
    };

    Cost Reckon(const Caster& caster, const SpellRow& row, const Intent& intent);

    /// Takes the cost. Called once, after the checks have passed.
    void Spend(Caster& caster, const Cost& cost, uint64_t nowMs);

    // -------------------------------------------------------------------------
    // 3. Launch -- the cast becomes visible
    // -------------------------------------------------------------------------

    /// Fills the SPELL_START report. An instant spell skips straight to Aim.
    wire::CastReport Launch(const Caster& caster, const SpellRow& row, const Intent& intent, uint32_t castTimeMs);

    // -------------------------------------------------------------------------
    // 4. Aim -- pure, one list per effect
    // -------------------------------------------------------------------------

    /// Who each of the spell's effects reaches.
    struct Aimed
    {
        std::vector<Target> perEffect[EFFECTS_PER_SPELL];

        /// The union, in the order the client is told about them.
        std::vector<Target> all;
    };

    /// A map's answer to "who is near here", so targeting stays pure.
    using Sweep = void (*)(const Where& centre, float radius, std::vector<Target>& found);

    Aimed Aim(const Caster& caster, const SpellRow& row, const Intent& intent, Sweep sweep);

    // -------------------------------------------------------------------------
    // 5. Resolve and run -- per target
    // -------------------------------------------------------------------------

    /// What happened to one target, kept until the whole cast is reported.
    struct Landing
    {
        Target target;
        wire::Miss reason = wire::Miss::None;
        EffectResult results[EFFECTS_PER_SPELL];
    };

    /**
     * Runs the spell against everything it reached.
     *
     * Damage is resolved once per target and handed to the effects already
     * mitigated. An effect never rolls or absorbs a second time.
     */
    void Strike(const Caster& caster, const SpellRow& row, const Intent& intent,
                const Aimed& aimed, CastId cast, std::vector<Landing>& landings);

    // -------------------------------------------------------------------------
    // 6. Procs -- queued, never reentrant
    // -------------------------------------------------------------------------

    /// A cast that something else's landing asked for.
    struct Triggered
    {
        Intent intent;
        uint8_t depth = 0;
    };

    /**
     * Procs are collected while a cast resolves and run after it finishes.
     *
     * Calling them where they occur is what let a shield proc a spell that
     * procced the shield, halfway through the first one's bookkeeping. The queue
     * carries a depth, and the depth is capped, so a cycle costs a bounded
     * number of casts and a log line instead of the stack.
     */
    class ProcQueue
    {
        public:
            static constexpr uint8_t MAX_DEPTH = 8;

            void Push(const Intent& intent, uint8_t depth);

            /// Takes the queued casts, leaving the queue empty.
            void Drain(std::vector<Triggered>& out);

            bool Empty() const { return m_queued.empty(); }

        private:
            std::vector<Triggered> m_queued;
    };

    // -------------------------------------------------------------------------
    // 7. Report -- the wire, and nothing but
    // -------------------------------------------------------------------------

    /// Turns what happened into what the client is told.
    wire::CastReport Fired(const Caster& caster, const SpellRow& row, const Intent& intent,
                           const std::vector<Landing>& landings);

    wire::Refused Denied(const Intent& intent, wire::Refusal reason, uint32_t detail);
}
