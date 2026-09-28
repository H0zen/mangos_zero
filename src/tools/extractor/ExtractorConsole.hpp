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

#include "Console/ConsoleUI.h"

#include <string>

namespace world::terrain
{
    class ExtractorConsole
    {
    public:
        bool Start(const std::string& src, const std::string& dest,
                   const std::string& client);
        void Stop();
        bool Active() const;

        void Log(const std::string& text);
        void Detail(const std::string& text);
        void Success(const std::string& text);
        void Warn(const std::string& text);
        void Error(const std::string& text);

        void Activity(const std::string& text);
        void Progress(int percent);

        void SetStage(const std::string& stage);
        void SetCounts(size_t done, size_t total);
        void SetElapsed(unsigned seconds);

        void SetLocale(const std::string& locale);

        struct Choice
        {
            bool dbc = false;
            bool tiles = false;
            bool goModels = false;
            bool vessels = false;
            bool nav = false;
            int mapFilter = -1;
            std::string src;
            std::string dest;
            std::string vesselList;
            std::string offMesh;
        };
        bool RunMenu(Choice& out, const std::string& dest);

        static bool BrowseForFolder(const std::string& title, std::string& path);
        static bool BrowseForFile(const std::string& title, const char* filter,
                                  std::string& path);

        static std::string ToUnixPath(std::string path);
        static bool ParseNonNegative(const std::string& text, int& value);

    private:
        void Draw();

        bool m_active = false;
        bool m_interactive = false;
    };
}
