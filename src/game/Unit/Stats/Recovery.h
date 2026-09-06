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
#include "SharedDefines.h"

/**
 * @brief The two clocks a unit's own recovery runs on.
 *
 * One says when the next tick falls due. The other holds the spirit-fed share
 * back for a while after mana is spent.
 */
class Recovery
{
    public:
        /// How long the spirit-fed share holds off after mana is spent.
        static constexpr uint32 HOLD = 5000;

        /// Spends the elapsed time toward the next tick.
        void Run(uint32 elapsed)
        {
            m_next = m_next <= elapsed ? 0 : m_next - elapsed;
        }

        /// Whether a tick falls due now.
        bool Due() const { return m_next == 0; }

        /// The next tick falls this far off.
        void NextIn(uint32 period) { m_next = period; }
        uint32 Left() const { return m_next; }

        /// Mana was just spent, so the spirit-fed share stops for a while.
        void ManaSpent() { m_hold = HOLD; }

        /// Spends the elapsed time on that hold.
        void RunHold(uint32 elapsed)
        {
            m_hold = m_hold <= elapsed ? 0 : m_hold - elapsed;
        }

        /// Whether the spirit-fed share is still being held back.
        bool HoldingBack() const { return m_hold != 0; }

    private:
        uint32 m_next = 0;
        uint32 m_hold = 0;
};

/**
 * @brief What a creature gets back of its own, in one tick, before auras.
 */
namespace regen
{
    /// The rates the world is configured with, so the sums stay pure.
    struct Rates
    {
        float mana = 1.0f;
        float energy = 1.0f;
        float focus = 1.0f;
        float health = 1.0f;
    };

    /// A power that does not come back on its own says nothing at all.
    struct Share
    {
        bool any = false;
        float amount = 0.0f;
    };

    /**
     * @brief How much power comes back in one tick.
     *
     * @param which        The power being refilled.
     * @param spirit       The unit's spirit, which feeds mana.
     * @param maxPower     The full bar, a third of which comes back at rest.
     * @param answering    In a fight, or answering to a master. Either way it does
     *                     not get the resting share.
     * @param spentRecently Mana was spent inside the hold, so spirit gives nothing.
     * @param rates        What the world is configured to give back.
     */
    Share PowerTick(Powers which, float spirit, uint32 maxPower,
                    bool answering, bool spentRecently, Rates const& rates);

    /**
     * @brief How much health comes back in one tick.
     *
     * A creature with a master heals off its spirit, at a lower share while it
     * still has mana to spend. One with no master, and one whose spirit rounds
     * the sum down to nothing, gets a third of the bar instead.
     */
    uint32 HealthTick(float spirit, uint32 maxHealth, bool mastered, bool hasMana, float rate);
}
