# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# =============================================================================
# Which vendored dependencies this fork builds.
#
# The selection lives here rather than in dep/, so one file states what is
# configured and what is not. Anything left out is simply never configured, so a
# library the fork has dropped costs no build time and cannot be linked back in
# by accident.
#
# This is also what decides the GATING: StormLib builds unconditionally, because
# the extractor is what produces the tiles the server reads -- it is not an
# optional extra.
# =============================================================================

set(MANGOS_DEP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dep")

if(NOT EXISTS "${MANGOS_DEP_DIR}")
    message(FATAL_ERROR "The vendored 'dep' directory is missing.")
endif()

if(NOT TARGET "ZLIB::ZLIB")
    add_subdirectory(${MANGOS_DEP_DIR}/zlib dep/zlib)
endif()

if(NOT TARGET "BZip2::BZip2")
    add_subdirectory(${MANGOS_DEP_DIR}/bzip2 dep/bzip2)
endif()

add_subdirectory(${MANGOS_DEP_DIR}/utf8cpp dep/utf8cpp)

# Recast and Detour are needed by the server's pathfinder AND by the baker's navmesh
# stage, and the baker always builds, so this is not gated either.
add_subdirectory(${MANGOS_DEP_DIR}/recastnavigation dep/recastnavigation)

# The MPQ reader is not gated: mangos-extractor is what produces the tiles the server
# reads, so it is always built rather than being an optional extra that a fresh clone
# can leave out and then have nothing to run on.
add_subdirectory(${MANGOS_DEP_DIR}/StormLib dep/StormLib)
