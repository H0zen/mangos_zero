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

// One Outcome, told three ways.
//
// The core answers in a Landing. The client wants a HitInfo mask and a
// VictimState; the proc system wants its own mask. All three say the same thing,
// so the translation belongs in one place rather than being rebuilt at each
// site that needs a different dialect -- which is how they drift, and how a
// blocked hit ends up procing as a plain one.

#include "Combat/Attempt.h"

namespace combat
{
    /// The HitInfo mask an attacker-state update carries for this outcome.
    uint32 ToHitInfo(const Outcome& outcome);

    /// The VictimState field that accompanies it.
    uint32 ToVictimState(const Outcome& outcome);

    /// The mask the proc system matches against.
    uint32 ToProcEx(const Outcome& outcome);
}
