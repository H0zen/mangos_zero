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

#pragma once

#include "IMpqArchive.hpp"

#include <string>
#include <vector>

namespace world::terrain
{
    class StormLibArchive : public IMpqArchive
    {
    public:
        StormLibArchive() = default;
        ~StormLibArchive() override;

        StormLibArchive(const StormLibArchive&) = delete;
        StormLibArchive& operator=(const StormLibArchive&) = delete;

        bool AddArchive(const std::string& mpqPath);

        int OpenClientData(const std::string& dataDir,
                           const std::vector<std::string>& archives,
                           const std::vector<std::string>& localeArchives,
                           const std::string& locale);

        bool Read(const std::string& path, std::vector<uint8_t>& out) override;
        bool Contains(const std::string& path) const override;

        std::vector<std::string> FindFiles(const std::string& pattern) const;

        size_t ArchiveCount() const { return m_handles.size(); }

    private:
        std::vector<void*> m_handles;
    };

    const std::vector<std::string>& ClientArchives112();
    const std::vector<std::string>& ClientLocaleArchives112();
}
