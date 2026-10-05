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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace world::terrain
{
    class IMpqArchive
    {
    public:
        virtual ~IMpqArchive() = default;

        virtual bool Read(const std::string& path, std::vector<uint8_t>& out) = 0;
        virtual bool Contains(const std::string& path) const = 0;
    };

    class MemoryArchive : public IMpqArchive
    {
    public:
        void Put(const std::string& path, std::vector<uint8_t> bytes)
        {
            m_files[Normalize(path)] = std::move(bytes);
        }

        bool Read(const std::string& path, std::vector<uint8_t>& out) override
        {
            auto it = m_files.find(Normalize(path));
            if (it == m_files.end())
            {
                return false;
            }
            out = it->second;
            return true;
        }

        bool Contains(const std::string& path) const override
        {
            return m_files.count(Normalize(path)) != 0;
        }

    private:
        static std::string Normalize(const std::string& p)
        {
            std::string s;
            s.reserve(p.size());
            for (char c : p)
            {
                if (c == '/')
                {
                    c = '\\';
                }
                if (c >= 'A' && c <= 'Z')
                {
                    c = static_cast<char>(c - 'A' + 'a');
                }
                s.push_back(c);
            }
            return s;
        }

        std::unordered_map<std::string, std::vector<uint8_t>> m_files;
    };
}
