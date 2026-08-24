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

# Geometry is header-only and sits below everything else: it knows nothing
# about WoW, any client version, or the server.
add_subdirectory(shared/Geometry)
add_subdirectory(shared/terrain)

# Build the mangos shared library
add_subdirectory(shared)

add_subdirectory(proto)

add_subdirectory(genrev)

# Needs to link against mangos_world.lib
if(BUILD_MANGOSD)
    # Build the mangos game library
    add_subdirectory(game)
endif()

if(BUILD_MANGOSD)
    # Build the mangos world server
    add_subdirectory(mangosd)
endif()

# The modules tree holds the game-linked scripts (SD3, which self-gates on
# SCRIPT_LIB_SD3 and requires the `game` target).
if(BUILD_MANGOSD)
    add_subdirectory(modules)
endif()

# Build the mangos realm authentication server
if(BUILD_REALMD)
    add_subdirectory(realmd)
endif()

# The baker and its client-data parsers, configured unconditionally. BUILD_TOOLS
# does not gate it: the tiles it produces are what src/shared/terrain reads at
# runtime, so it is not optional the way the old map/vmap extractors were.
add_subdirectory(tools/extractor)

# Scores those tiles against the world database's own ground-only creature spawns.
# A clean parse is not correct geometry; this is what says whether it is.
add_subdirectory(tools/height-check)

if (BUILD_MANGOSD OR BUILD_REALMD)
    if(WIN32)
        get_filename_component(MYSQL_LIB_DIR ${MySQL_LIBRARIES} DIRECTORY)
        get_filename_component(MYSQL_LIB_DLL ${MySQL_LIBRARIES} NAME)
        STRING(REPLACE ".lib" ".dll" MYSQL_LIB_DLL ${MYSQL_LIB_DLL})
        install(FILES ${MYSQL_LIB_DIR}/${MYSQL_LIB_DLL} DESTINATION ${BIN_DIR})
    endif()
endif()
