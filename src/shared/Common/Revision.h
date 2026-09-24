/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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

#ifndef MANGOS_REVISION_H
#define MANGOS_REVISION_H

#include "Define.h"

/**
 * What this build is: revision, versions, required database schema and the
 * configuration files it reads. All values are set in cmake/Revision.cmake,
 * generated into BuildRevision.h in the build tree, and read by Revision.cpp
 * alone. This header only declares them, so nothing that includes it is
 * rebuilt when the revision or a version changes.
 */
namespace Revision
{
    /// "<hash> <date> (<branch> branch)" of the core, Eluna and SD3; a module
    /// that is not built reads "not built n/a (disabled branch)".
    char const* GetRevisionStr();
    char const* GetElunaRevisionStr();
    char const* GetSD3RevisionStr();
    char const* GetFullRevision();          ///< "Mangos revision: " GetRevisionStr()
    char const* GetHash();
    char const* GetDate();

    char const* GetPackageName();
    char const* GetProjectRevision();
    char const* GetBuildHost();             ///< OS the binary was compiled on

    /// The `db_version` row the core requires of each database.
    struct DbVersion
    {
        char const* name;
        char const* version;
        char const* structure;
        char const* content;
        char const* description;
    };
    DbVersion const& GetWorldDb();
    DbVersion const& GetRealmDb();
    DbVersion const& GetCharDb();

    /// The configuration files this build ships a .conf.dist for.
    enum class ConfigFile
    {
        Mangosd,
        Realmd,
        AhBot,
        Playerbot,
    };
    char const* GetConfigName(ConfigFile file);     ///< "mangosd.conf"
    char const* GetConfigPath(ConfigFile file);     ///< install config dir + name
    uint32 GetConfigVersion(ConfigFile file);       ///< ConfVersion it requires
}

#endif
