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

// Which of the found targets survive the cap is drawn by lot, so the cases below
// pin the two things that are not luck: how many are left, and whether the one
// the caster aimed at is among them.

#include "doctest.h"

#include "Cast/Targets/Trim.h"

#include <algorithm>
#include <vector>

namespace
{
    /// Ten things standing in the area, addressed by pointer the way units are.
    class Crowd
    {
        public:

            explicit Crowd(size_t many) : m_bodies(many)
            {
                for (size_t i = 0; i < many; ++i)
                {
                    m_bodies[i] = static_cast<int>(i);
                    m_found.push_back(&m_bodies[i]);
                }
            }

            std::list<int*>& Found() { return m_found; }
            int* At(size_t i) { return &m_bodies[i]; }

            bool Holds(int* one) const
            {
                return std::find(m_found.begin(), m_found.end(), one) != m_found.end();
            }

        private:

            std::vector<int> m_bodies;
            std::list<int*> m_found;
    };
}

TEST_CASE("a list within the cap is left alone")
{
    Crowd crowd(4);
    cast::KeepAtMost(crowd.Found(), 4u, crowd.At(0), true);

    CHECK(crowd.Found().size() == 4u);
    CHECK(crowd.Holds(crowd.At(0)));
}

TEST_CASE("a spell with no cap keeps everyone it found")
{
    Crowd crowd(30);
    cast::KeepAtMost(crowd.Found(), 0u, crowd.At(7), true);

    CHECK(crowd.Found().size() == 30u);
}

TEST_CASE("the one the caster aimed at survives the draw")
{
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        Crowd crowd(10);
        cast::KeepAtMost(crowd.Found(), 3u, crowd.At(6), true);

        CHECK(crowd.Found().size() == 3u);
        CHECK(crowd.Holds(crowd.At(6)));
    }
}

TEST_CASE("a cap of one leaves only the one the caster aimed at")
{
    Crowd crowd(8);
    cast::KeepAtMost(crowd.Found(), 1u, crowd.At(2), true);

    CHECK(crowd.Found().size() == 1u);
    CHECK(crowd.Found().front() == crowd.At(2));
}

TEST_CASE("when the caster aimed at nobody the whole cap goes by lot")
{
    Crowd crowd(10);
    cast::KeepAtMost(crowd.Found(), 4u, static_cast<int*>(nullptr), true);

    CHECK(crowd.Found().size() == 4u);
}

TEST_CASE("a target the caster aimed at that was never found changes nothing")
{
    Crowd crowd(10);
    int elsewhere = 99;
    cast::KeepAtMost(crowd.Found(), 4u, &elsewhere, true);

    CHECK(crowd.Found().size() == 4u);
    CHECK_FALSE(crowd.Holds(&elsewhere));
}

TEST_CASE("a chosen gameobject is dropped and not put back, so one fewer is left")
{
    Crowd crowd(10);
    cast::KeepAtMost(crowd.Found(), 4u, crowd.At(5), false);

    CHECK(crowd.Found().size() == 3u);
    CHECK_FALSE(crowd.Holds(crowd.At(5)));
}

TEST_CASE("a gameobject cap with nothing chosen fills to the cap")
{
    Crowd crowd(10);
    cast::KeepAtMost(crowd.Found(), 4u, static_cast<int*>(nullptr), false);

    CHECK(crowd.Found().size() == 4u);
}
