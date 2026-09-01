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

/**
 * @file
 * @brief Supplies to ScriptDev3 what the deleted Common.h used to.
 *
 * Only one SD3 file, system/ScriptDevMgr.h, actually included Common.h; the
 * scripts received its contents transitively, through a core header that
 * included it. This fork deleted Common.h, which breaks both cases -- and the
 * second cannot be fixed by any include directory, because those scripts name
 * no header to redirect.
 *
 * So this file is force-included into every SD3 C++ source, which keeps the
 * scripts compiling without an edit in each one.
 *
 * It is deliberately the minimum needed, not a copy of the old Common.h, and it
 * must not grow into one. A script that needs something else should name the
 * header it needs.
 */

#include <algorithm>
#include <cmath>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Common/ServerDefines.h"
#include "Common/TimeConstants.h"
#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include "Utilities/Util.h"
