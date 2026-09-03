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

// The counters that belong to the process rather than to any one map, and the
// line they are reported on.
//
// These exist to answer questions that are otherwise arguments. Chiefly: does
// visibility oscillate? A viewer standing still while creates and destroys run
// at the same steady rate is an object flapping in and out of his known set, and
// no amount of reading the sweep will show it as reliably as the two numbers
// side by side.

#include "Metrics/Rate.h"

#include <string>

namespace metrics
{
    struct ServerMetrics
    {
        /// Objects appearing and disappearing for viewers. Equal, non-zero and
        /// steady with a still population is the signature of oscillation.
        Rate sightCreates;
        Rate sightDestroys;

        Rate swings;
        Rate casts;
    };

    /// The one instance. Written from map threads, read by the reporter.
    ServerMetrics& Server();

    /**
     * @brief Consume the window and render it as one line.
     *
     * Reading is destructive, which is why there is a single reporter: two would
     * each get part of the truth.
     */
    std::string Report(uint32 elapsedMs);
}
