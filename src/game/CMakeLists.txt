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

#AH Bot group
file(GLOB SRC_GRP_AHBOT AuctionHouseBot/*.cpp AuctionHouseBot/*.h)
source_group("AhBot" FILES ${SRC_GRP_AHBOT})

#Battlegrounds group
file(GLOB SRC_GRP_BATTLEGROUND BattleGround/*.cpp BattleGround/*.h)
source_group("BattleGround" FILES ${SRC_GRP_BATTLEGROUND})

#Chat Commands group
file(GLOB SRC_GRP_CHAT_COMMANDS ChatCommands/*.cpp ChatCommands/*.h)
source_group("Chat Commands" FILES ${SRC_GRP_CHAT_COMMANDS})

#Map Threading group
file(GLOB SRC_GRP_MAP_THREAD Maps/*.cpp Maps/*.h)
source_group("Map Threading" FILES ${SRC_GRP_MAP_THREAD})

#Motion Generator group
file(GLOB SRC_GRP_MOTION_GEN MotionGenerators/*.cpp MotionGenerators/*.h)
source_group("Motion generators" FILES ${SRC_GRP_MOTION_GEN})

#Movement group
file(GLOB SRC_GRP_MOVEMENT movement/*.cpp movement/*.h)
source_group("Movement" FILES ${SRC_GRP_MOVEMENT})

#Object group
file(GLOB SRC_GRP_OBJECT Object/*.cpp Object/*.h)
source_group("Object" FILES ${SRC_GRP_OBJECT})

#Outdoors PvP group
file(GLOB SRC_GRP_OUTDOOR_PVP OutdoorPvP/*.cpp OutdoorPvP/*.h)
source_group("Outdoor PvP" FILES ${SRC_GRP_OUTDOOR_PVP})

#References group
file(GLOB SRC_GRP_REFERENCES References/*.cpp References/*.h)
source_group("References" FILES ${SRC_GRP_REFERENCES})

#Server group
file(GLOB SRC_GRP_SERVER Server/*.cpp Server/*.h)
source_group("Server" FILES ${SRC_GRP_SERVER})

#Time group
file(GLOB SRC_GRP_TIME Time/*.cpp Time/*.h)
source_group("Time" FILES ${SRC_GRP_TIME})

#Tools group
file(GLOB SRC_GRP_TOOL Tools/*.cpp Tools/*.h)
source_group("Tool" FILES ${SRC_GRP_TOOL})

#VMap group

#Warden group
file(GLOB SRC_GRP_WARDEN Warden/*.cpp Warden/*.h)
source_group("Warden" FILES ${SRC_GRP_WARDEN})

#World and Handlers group
file(GLOB SRC_GRP_WORLD_HANDLERS WorldHandlers/*.cpp WorldHandlers/*.h)
source_group("World\\Handlers" FILES ${SRC_GRP_WORLD_HANDLERS})

# Build the Eluna library if enabled
if(SCRIPT_LIB_ELUNA)
    file(GLOB SRC_GRP_ELUNA
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/*.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/*.h
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/hooks/*.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/hooks/*.h
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/methods/*.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/methods/*.h
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/methods/Mangos/*.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna/methods/Mangos/*.h
    )
    source_group("Eluna" FILES ${SRC_GRP_ELUNA})

    # Eluna is an untouched submodule; see cmake/SubmoduleCompat.cmake. The forced
    # include goes to Eluna's own sources only; the include DIRECTORY has to be
    # visible to all of `game`, because core files include Eluna headers and those
    # open with #include "Common.h".
    mangos_submodule_compat_sources("${SRC_GRP_ELUNA}"
        "${CMAKE_SOURCE_DIR}/src/shared/Compat/eluna")
endif()

configure_file(AuctionHouseBot/ahbot.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/AuctionHouseBot/ahbot.conf.dist)

add_library(game STATIC
    ${SRC_GRP_AHBOT}
    ${SRC_GRP_BATTLEGROUND}
    ${SRC_GRP_CHAT_COMMANDS}
    ${SRC_GRP_MAP_THREAD}
    ${SRC_GRP_MOTION_GEN}
    ${SRC_GRP_MOVEMENT}
    ${SRC_GRP_OBJECT}
    ${SRC_GRP_OUTDOOR_PVP}
    ${SRC_GRP_REFERENCES}
    ${SRC_GRP_SERVER}
    ${SRC_GRP_TIME}
    ${SRC_GRP_TOOL}
    ${SRC_GRP_WARDEN}
    ${SRC_GRP_WORLD_HANDLERS}
    $<$<BOOL:${SCRIPT_LIB_ELUNA}>:${SRC_GRP_ELUNA}>
)

# Eluna headers are included by core files and open with #include "Common.h";
# see the note above. Passive -- nothing in the core names that header.
if(SCRIPT_LIB_ELUNA)
    target_include_directories(game
        PRIVATE "${CMAKE_SOURCE_DIR}/src/shared/Compat/eluna")
endif()

target_include_directories(game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        AuctionHouseBot
        BattleGround
        ChatCommands
        Maps
        MotionGenerators
        Object
        OutdoorPvP
        References
        Server
        Time
        Tools
        Warden
        WorldHandlers
        $<$<BOOL:${SCRIPT_LIB_ELUNA}>:
            ${CMAKE_SOURCE_DIR}/src/modules/Eluna
            ${CMAKE_SOURCE_DIR}/src/modules/Eluna/hooks
            ${CMAKE_SOURCE_DIR}/src/modules/Eluna/methods/Mangos
        >
)

target_compile_definitions(game
    PUBLIC
        $<$<BOOL:${SOAP}>:ENABLE_SOAP>
        # The old coordinate-owning spatial API. SD3 no longer needs it -- it calls
        # Where()/Place() -- so this is Eluna's alone. See Object/ScriptApiCompat.inl.
        $<$<BOOL:${SCRIPT_LIB_ELUNA}>:MANGOS_SCRIPT_COMPAT>
        $<$<BOOL:${SCRIPT_LIB_SD3}>:ENABLE_SD3>
        $<$<BOOL:${SCRIPT_LIB_ELUNA}>:ENABLE_ELUNA ELUNA_EXPANSION=0 ELUNA_MANGOS>
)

target_link_libraries(game
    PUBLIC
        # The terrain engine: geometry and baked tiles, and no knowledge of the
        # world, the database or any DBC.
        terrain
        geometry
        proto
        shared
        ah_ipc
        RecastNavigation::Detour
        ZLIB::ZLIB
        $<$<BOOL:${SCRIPT_LIB_SD3}>:mangosscript>
        $<$<BOOL:${SCRIPT_LIB_ELUNA}>:lualib>
    PRIVATE
        mangos_openssl_strict
)

# Generate precompiled header
if(PCH)
    add_cxx_pch(game pchdef.h pchdef.cpp)
endif()

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/AuctionHouseBot/ahbot.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)

if(SCRIPT_LIB_ELUNA)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/src/modules/Eluna/extensions
    DESTINATION ${BIN_DIR}/lua_scripts
)
endif()
