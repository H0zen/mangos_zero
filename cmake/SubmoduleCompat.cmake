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
# Compatibility glue for the vendored trees.
#
# SD3 and realmd are vendored here as ordinary directories, but their sources
# are kept byte-identical to upstream so a newer upstream drop can be copied in
# without a merge. Every adaptation they need in order to build against this
# fork therefore lives outside them, under src/shared/Compat/<name>/, and is
# attached to their targets from the outside.
#
# Two things are needed, and the second is the reason this file exists.
#
#   1. Headers this fork removed or moved, supplied under their original names
#      so an unmodified `#include "Common.h"` still resolves. A plain
#      target_include_directories is enough for that.
#
#   2. Declarations that used to arrive *transitively*. Most of those sources
#      never included Common.h themselves -- they got its contents through some
#      core header that included it. Nothing in their include chain names the
#      shim, so no include directory can reach them, and editing them is what
#      this layer exists to avoid. The only lever left from outside a
#      translation unit is a forced include, so the shim is injected at the top
#      of every source in the target.
#
# Restricted to C++: these targets can also compile vendored C, which must not
# see a C++ header.
#
# Do not extend a shim to cover new divergence: that is a sign the fix belongs
# in the vendored tree (and upstream) instead.
# =============================================================================

function(mangos_submodule_compat target compat_dir)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "mangos_submodule_compat: no such target '${target}'. The vendored "
            "tree's CMakeLists likely renamed it; the compat layer would "
            "silently do nothing, so this is fatal rather than skipped.")
    endif()

    if(NOT IS_DIRECTORY "${compat_dir}")
        message(FATAL_ERROR
            "mangos_submodule_compat: '${compat_dir}' does not exist.")
    endif()

    target_include_directories(${target} PRIVATE "${compat_dir}")

    set(_prelude "${compat_dir}/Common.h")
    if(EXISTS "${_prelude}")
        if(MSVC)
            target_compile_options(${target} PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX>:/FI${_prelude}>")
        else()
            target_compile_options(${target} PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX>:-include${_prelude}>")
        endif()
    endif()
endfunction()
