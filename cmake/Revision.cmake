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
# Revision -- the single point of versioning of this core.
#
# Every version, schema level and ConfVersion is set in the block right below
# and nowhere else. The flow:
#
#   this file + cmake/BuildRevision.h.in
#     -> <build>/revision/BuildRevision.h           (never in the source tree)
#     -> included by src/shared/Common/Revision.cpp only
#     -> which implements src/shared/Common/Revision.h, used by everything else
#
# So a new commit or a version bump recompiles Revision.cpp and nothing else.
#
# The same values also fill every *.conf.dist.in (mangos_install_conf) and the
# Windows version resource (cmake/win/VersionInfo.h.in).
#
# Runs in two modes, same code:
#   configure   included from CMakeLists.txt; also creates the `revision` target.
#   build       `cmake -P` of this file, run by `revision` when the HEAD or index
#               of the core or of a built submodule moved. Script mode has no
#               cache, so the target passes in what it needs.
# =============================================================================

# `cmake -P` starts with every policy unset (CMake 3.x would reject IN_LIST);
# give script mode the policy level of the project so both modes run alike.
if(CMAKE_SCRIPT_MODE_FILE)
    cmake_minimum_required(VERSION 3.18)
endif()

# -----------------------------------------------------------------------------
# Edit here.
# -----------------------------------------------------------------------------

# MANGOS_EXP is also the compile definition that selects the client.
set(MANGOS_EXP                  "CLASSIC")
set(MANGOS_PKG                  "Mangos Zero")
set(MANGOS_VERSION              "0.22.0")
set(MANGOS_REVISION_NR          "2201075")
set(MANGOS_COMPANY              "MaNGOS Developers")
set(MANGOS_COPYRIGHT_FIRST_YEAR 2005)

# Database schema this core requires. Bump together with the matching
# Rel##_##_###_*.sql migration in the mangoszero/database repository.
set(MANGOS_REALMD_DB_VERSION    22)
set(MANGOS_REALMD_DB_STRUCTURE  5)
set(MANGOS_REALMD_DB_CONTENT    1)
set(MANGOS_REALMD_DB_UPDATE     "Cata Warden identity")

set(MANGOS_CHAR_DB_VERSION      22)
set(MANGOS_CHAR_DB_STRUCTURE    5)
set(MANGOS_CHAR_DB_CONTENT      4)
set(MANGOS_CHAR_DB_UPDATE       "Remove_Warden_Action")

set(MANGOS_WORLD_DB_VERSION     22)
set(MANGOS_WORLD_DB_STRUCTURE   6)
set(MANGOS_WORLD_DB_CONTENT     4)
set(MANGOS_WORLD_DB_UPDATE      "Warden_Locale_Hardening")

# ConfVersion of each shipped .conf.dist, YYYYMMDDRR (RR = revision of the day).
set(MANGOS_WORLD_VER            2026092400)   # mangosd.conf
set(MANGOS_REALM_VER            2026060300)   # realmd.conf
set(MANGOS_AHBOT_VER            2026071400)   # ahbot.conf, ah-service.conf
set(MANGOS_PLAYERBOT_VER        2026080700)   # aiplayerbot.conf

# -----------------------------------------------------------------------------
# Derived and checked. Nothing below needs editing for a version bump.
# -----------------------------------------------------------------------------

set(MANGOS_KNOWN_EXPANSIONS CLASSIC TBC WOTLK CATA MISTS)
if(NOT MANGOS_EXP IN_LIST MANGOS_KNOWN_EXPANSIONS)
    message(FATAL_ERROR "MANGOS_EXP '${MANGOS_EXP}' is not one of: ${MANGOS_KNOWN_EXPANSIONS}")
endif()

if(NOT MANGOS_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "MANGOS_VERSION '${MANGOS_VERSION}' must be MAJOR.MINOR.PATCH")
endif()
set(MANGOS_VERSION_MAJOR ${CMAKE_MATCH_1})
set(MANGOS_VERSION_MINOR ${CMAKE_MATCH_2})
set(MANGOS_VERSION_PATCH ${CMAKE_MATCH_3})

foreach(_conf_ver MANGOS_WORLD_VER MANGOS_REALM_VER MANGOS_AHBOT_VER MANGOS_PLAYERBOT_VER)
    if(NOT ${_conf_ver} MATCHES "^20[0-9][0-9][01][0-9][0-3][0-9][0-9][0-9]$")
        message(FATAL_ERROR "${_conf_ver} '${${_conf_ver}}' is not a YYYYMMDDRR ConfVersion")
    endif()
endforeach()

if(MANGOS_EXP STREQUAL "CLASSIC")
    set(MANGOS_CLIENT_NAME "World of Warcraft 1.12.x")
elseif(MANGOS_EXP STREQUAL "TBC")
    set(MANGOS_CLIENT_NAME "The Burning Crusade 2.4.3")
elseif(MANGOS_EXP STREQUAL "WOTLK")
    set(MANGOS_CLIENT_NAME "Wrath of the Lich King 3.3.5a")
elseif(MANGOS_EXP STREQUAL "CATA")
    set(MANGOS_CLIENT_NAME "Cataclysm 4.3.4")
else()
    set(MANGOS_CLIENT_NAME "Mists of Pandaria 5.4.8")
endif()

# -----------------------------------------------------------------------------
# mangos_configure(<input> <output>)
#
# configure_file(@ONLY) that refuses a template naming an unset variable, which
# plain configure_file would silently turn into an empty string.
# -----------------------------------------------------------------------------
function(mangos_configure input output)
    if(NOT IS_ABSOLUTE "${input}")
        set(input "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
    endif()

    file(STRINGS "${input}" _lines REGEX "@[A-Za-z_][A-Za-z0-9_]*@")
    string(REGEX MATCHALL "@[A-Za-z_][A-Za-z0-9_]*@" _refs "${_lines}")
    if(_refs)
        list(REMOVE_DUPLICATES _refs)
    endif()
    foreach(_ref IN LISTS _refs)
        string(REGEX REPLACE "^@(.*)@$" "\\1" _var "${_ref}")
        if(NOT DEFINED ${_var})
            message(FATAL_ERROR "${input} references @${_var}@, which is not set.")
        endif()
    endforeach()

    configure_file("${input}" "${output}" @ONLY)
endfunction()

# -----------------------------------------------------------------------------
# mangos_install_conf(<name.conf.dist.in> [DESTINATION <dir>])
#
# The one way a .conf.dist leaves the tree: generated into the current binary
# directory and installed (default: CONF_INSTALL_DIR).
# -----------------------------------------------------------------------------
function(mangos_install_conf template)
    cmake_parse_arguments(ARG "" "DESTINATION" "" ${ARGN})
    if(NOT ARG_DESTINATION)
        set(ARG_DESTINATION "${CONF_INSTALL_DIR}")
    endif()

    get_filename_component(_name "${template}" NAME)
    string(REGEX REPLACE "\\.in$" "" _name "${_name}")
    set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_name}")

    mangos_configure("${template}" "${_out}")
    install(FILES "${_out}" DESTINATION "${ARG_DESTINATION}")
endfunction()

# -----------------------------------------------------------------------------
# git
# -----------------------------------------------------------------------------

get_filename_component(MANGOS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(CMAKE_SCRIPT_MODE_FILE)
    foreach(_required MANGOS_BINARY_DIR MANGOS_BUILD_HOST CONF_INSTALL_DIR)
        if(NOT DEFINED ${_required})
            message(FATAL_ERROR "Revision.cmake: -D${_required}= is required in script mode")
        endif()
    endforeach()
else()
    set(MANGOS_BINARY_DIR "${CMAKE_BINARY_DIR}")
    # Taken here: `cmake -P` leaves CMAKE_HOST_SYSTEM_VERSION empty.
    set(MANGOS_BUILD_HOST "${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_VERSION}")
endif()

# mangos_git_revision(<prefix> <dir> <enabled>)
#
# Sets <prefix> to "<hash> <date> (<branch> branch)", <prefix>_hash,
# <prefix>_date, and <prefix>_state_files (the HEAD/index whose change makes the
# values stale). A module that is not built says so instead.
function(mangos_git_revision prefix dir enabled)
    set(_hash   "unknown")
    set(_date   "1970-01-01 00:00:00 +0000")
    set(_branch "Archived")
    set(_state)

    if(NOT enabled)
        set(_hash   "not built")
        set(_date   "n/a")
        set(_branch "disabled")
    elseif(GIT_EXECUTABLE AND NOT WITHOUT_GIT AND EXISTS "${dir}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --long --match init --dirty=+ --abbrev=12 --always
            WORKING_DIRECTORY "${dir}"
            RESULT_VARIABLE _rc
            OUTPUT_VARIABLE _info
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)

        if(_rc EQUAL 0 AND _info)
            string(REGEX REPLACE "init-|[0-9]+-g" "" _hash "${_info}")

            execute_process(
                COMMAND "${GIT_EXECUTABLE}" show -s --format=%ci
                WORKING_DIRECTORY "${dir}"
                OUTPUT_VARIABLE _out
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if(_out)
                set(_date "${_out}")
            endif()

            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY "${dir}"
                OUTPUT_VARIABLE _out
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if(_out)
                set(_branch "${_out}")
            endif()

            # .git/modules/<name> for a submodule, the worktree's own dir for a
            # linked worktree.
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --absolute-git-dir
                WORKING_DIRECTORY "${dir}"
                OUTPUT_VARIABLE _gitdir
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            foreach(_file HEAD index)
                if(_gitdir AND EXISTS "${_gitdir}/${_file}")
                    list(APPEND _state "${_gitdir}/${_file}")
                endif()
            endforeach()
        endif()
    endif()

    set(${prefix}             "${_hash} ${_date} (${_branch} branch)" PARENT_SCOPE)
    set(${prefix}_hash        "${_hash}"  PARENT_SCOPE)
    set(${prefix}_date        "${_date}"  PARENT_SCOPE)
    set(${prefix}_state_files "${_state}" PARENT_SCOPE)
endfunction()

mangos_git_revision(MANGOS_REVISION       "${MANGOS_SOURCE_DIR}"                   ON)
mangos_git_revision(MANGOS_ELUNA_REVISION "${MANGOS_SOURCE_DIR}/src/modules/Eluna" "${SCRIPT_LIB_ELUNA}")
mangos_git_revision(MANGOS_SD3_REVISION   "${MANGOS_SOURCE_DIR}/src/modules/SD3"   "${SCRIPT_LIB_SD3}")

if(MANGOS_REVISION_hash STREQUAL "unknown" AND NOT WITHOUT_GIT AND NOT CMAKE_SCRIPT_MODE_FILE)
    message(STATUS "Could not read the git revision; it will read \"${MANGOS_REVISION}\".")
endif()

# The copyright runs to the year of the commit being built; without git, the
# build year stands in.
if(NOT MANGOS_REVISION_hash STREQUAL "unknown" AND MANGOS_REVISION_date MATCHES "^([0-9][0-9][0-9][0-9])-")
    set(_year "${CMAKE_MATCH_1}")
else()
    string(TIMESTAMP _year "%Y" UTC)
endif()
set(MANGOS_COPYRIGHT "Copyright (C) ${MANGOS_COPYRIGHT_FIRST_YEAR}-${_year} MaNGOS")

set(MANGOS_REVISION_INCLUDE_DIR "${MANGOS_BINARY_DIR}/revision")
set(MANGOS_REVISION_HEADER "${MANGOS_REVISION_INCLUDE_DIR}/BuildRevision.h")
mangos_configure("${CMAKE_CURRENT_LIST_DIR}/BuildRevision.h.in" "${MANGOS_REVISION_HEADER}")

if(CMAKE_SCRIPT_MODE_FILE)
    return()
endif()

# -----------------------------------------------------------------------------
# `revision`: re-runs this file at build time when git state moved. configure_file
# leaves BuildRevision.h untouched when nothing changed, so that alone recompiles
# nothing; the stamp records that the check ran. Always defined, so `shared` can
# depend on it unconditionally.
# -----------------------------------------------------------------------------
set(_revision_state_files
    ${MANGOS_REVISION_state_files}
    ${MANGOS_ELUNA_REVISION_state_files}
    ${MANGOS_SD3_REVISION_state_files})

if(_revision_state_files)
    set(_revision_stamp "${CMAKE_BINARY_DIR}/revision.stamp")

    add_custom_command(
        OUTPUT "${_revision_stamp}"
        BYPRODUCTS "${MANGOS_REVISION_HEADER}"
        COMMAND "${CMAKE_COMMAND}"
                "-DMANGOS_BINARY_DIR=${CMAKE_BINARY_DIR}"
                "-DMANGOS_BUILD_HOST=${MANGOS_BUILD_HOST}"
                "-DCONF_INSTALL_DIR=${CONF_INSTALL_DIR}"
                "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                "-DSCRIPT_LIB_ELUNA=${SCRIPT_LIB_ELUNA}"
                "-DSCRIPT_LIB_SD3=${SCRIPT_LIB_SD3}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${_revision_stamp}"
        DEPENDS ${_revision_state_files}
                "${CMAKE_CURRENT_LIST_DIR}/BuildRevision.h.in"
                "${CMAKE_CURRENT_LIST_FILE}"
        COMMENT "Checking git revision"
        VERBATIM)

    # BuildRevision.h was just written above; the first build need not repeat it.
    file(TOUCH "${_revision_stamp}")

    add_custom_target(revision DEPENDS "${_revision_stamp}")
else()
    add_custom_target(revision)
endif()
