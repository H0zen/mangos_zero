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
#include "ObjectGuid.h"

class Creature;

/**
 * @brief The rule by which a unit that was called up goes away again.
 */
enum TempSpawnType
{
    TEMPSPAWN_MANUAL_DESPAWN = 0,             ///< Stays until someone says otherwise
    TEMPSPAWN_DEAD_DESPAWN = 1,               ///< Goes when the body is gone
    TEMPSPAWN_CORPSE_DESPAWN = 2,             ///< Goes the moment it dies
    TEMPSPAWN_CORPSE_TIMED_DESPAWN = 3,       ///< Goes a while after dying, or when the body is gone
    TEMPSPAWN_TIMED_DESPAWN = 4,              ///< Goes when its time is spent
    TEMPSPAWN_TIMED_OOC_DESPAWN = 5,          ///< Its time only runs while it is out of combat
    TEMPSPAWN_TIMED_OR_DEAD_DESPAWN = 6,      ///< Goes when its time is spent, or when the body is gone
    TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN = 7,    ///< Goes when its time is spent, or when it dies
    TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN = 8,  ///< Out-of-combat time, or when the body is gone
    TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN = 9 ///< Out-of-combat time, or when it dies
};

namespace tenure
{
    /**
     * @brief How the body reads, to whichever rule is asking.
     *
     * A rule wants nothing else about the unit, so this is all that crosses.
     */
    struct Body
    {
        bool inCombat = false;
        bool alive = true;
        bool dead = false;      ///< Dead with the corpse step skipped
        bool corpse = false;
        bool despawned = false;
    };

    /**
     * @brief What the rule says this tick, and what is left on the clock.
     */
    struct Verdict
    {
        bool vanish = false;
        uint32 left = 0;
    };

    /**
     * @brief Reads one rule against the clock and the body.
     *
     * Pure: it takes what it needs and hands back what it decided.
     *
     * @param rule    The rule the term was granted under.
     * @param left    What is still on the clock.
     * @param granted The full length, which an out-of-combat rule restores to.
     * @param elapsed Milliseconds since the last reading.
     * @param body    How the unit reads right now.
     */
    Verdict Tick(TempSpawnType rule, uint32 left, uint32 granted, uint32 elapsed, Body const& body);

    /**
     * @brief Reads a creature's body the way a rule wants it.
     */
    Body BodyOf(Creature const& who);
}

/**
 * @brief The term a called-up unit stays on.
 *
 * Who called it, how long it has, and the rule by which it goes. A totem, a
 * temporary summon and a guardian differ in the rule they are granted, not in
 * the clock they keep.
 */
class Tenure
{
    public:
        /// Who called it up. Known before the length often is.
        void SummonedBy(ObjectGuid who) { m_summoner = who; }

        /// The rule and the length. The clock starts full.
        void Grant(TempSpawnType rule, uint32 howLong)
        {
            m_rule = rule;
            m_granted = howLong;
            m_left = howLong;
        }

        ObjectGuid const& Summoner() const { return m_summoner; }
        TempSpawnType Rule() const { return m_rule; }
        uint32 Left() const { return m_left; }
        uint32 Granted() const { return m_granted; }

        /// Whether a length was ever set. A term of nought is no term at all.
        bool Bounded() const { return m_granted > 0; }

        /// Spends the elapsed time and says whether the term is up.
        bool RunsOut(uint32 elapsed, tenure::Body const& body)
        {
            tenure::Verdict const said = tenure::Tick(m_rule, m_left, m_granted, elapsed, body);
            m_left = said.left;
            return said.vanish;
        }

    private:
        ObjectGuid m_summoner;
        TempSpawnType m_rule = TEMPSPAWN_MANUAL_DESPAWN;
        uint32 m_granted = 0;
        uint32 m_left = 0;
};
