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

#include <string>
#include "ExtractorConsole.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#endif

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <thread>

namespace world::terrain
{
    namespace
    {
        using MaNGOS::Console::ConsoleUI;
        using MaNGOS::Console::Style;

        ConsoleUI& Ui() { return ConsoleUI::Instance(); }

        enum StatusSlot
        {
            SLOT_STAGE = 0,
            SLOT_PROGRESS = 1,
            SLOT_ELAPSED = 2,
            SLOT_SOURCE = 3,
            SLOT_DEST = 4,
            SLOT_LOCALE = 5
        };

        std::string Trim(const std::string& in)
        {
            const size_t first = in.find_first_not_of(" \t\r\n\"'");
            if (first == std::string::npos)
            {
                return std::string();
            }
            const size_t last = in.find_last_not_of(" \t\r\n\"'");
            return in.substr(first, last - first + 1);
        }

        std::string Shorten(const std::string& path, size_t width)
        {
            if (path.size() <= width)
            {
                return path;
            }
            return "..." + path.substr(path.size() - (width - 3));
        }
    }

    bool ExtractorConsole::ParseNonNegative(const std::string& text, int& value)
    {
        if (text.empty())
        {
            return false;
        }

        char* end = nullptr;
        errno = 0;
        const long parsed = std::strtol(text.c_str(), &end, 10);
        if (errno != 0 || *end != '\0' || parsed < 0 || parsed > INT_MAX)
        {
            return false;
        }
        value = int(parsed);
        return true;
    }

    std::string ExtractorConsole::ToUnixPath(std::string path)
    {
        for (char& c : path)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }
        return path;
    }

#if defined(_WIN32)
    bool ExtractorConsole::BrowseForFolder(const std::string& title, std::string& path)
    {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool weInitialised = SUCCEEDED(init);

        BROWSEINFOA info{};
        char display[MAX_PATH] = {0};
        info.hwndOwner = GetConsoleWindow();
        info.pszDisplayName = display;
        info.lpszTitle = title.c_str();
        info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

        bool picked = false;
        if (LPITEMIDLIST idl = SHBrowseForFolderA(&info))
        {
            char chosen[MAX_PATH] = {0};
            if (SHGetPathFromIDListA(idl, chosen))
            {
                path = ToUnixPath(chosen);
                picked = true;
            }
            CoTaskMemFree(idl);
        }

        if (weInitialised)
        {
            CoUninitialize();
        }
        return picked;
    }
    bool ExtractorConsole::BrowseForFile(const std::string& title, const char* filter,
                                         std::string& path)
    {
        char chosen[MAX_PATH] = {0};

        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetConsoleWindow();
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = chosen;
        ofn.nMaxFile = sizeof(chosen);
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameA(&ofn))
        {
            return false;
        }
        path = ToUnixPath(chosen);
        return true;
    }
#else
    bool ExtractorConsole::BrowseForFolder(const std::string&, std::string&)
    {
        return false;
    }

    bool ExtractorConsole::BrowseForFile(const std::string&, const char*, std::string&)
    {
        return false;
    }
#endif

    bool ExtractorConsole::Start(const std::string& src, const std::string& dest,
                                 const std::string& client)
    {
        m_active = Ui().Start("MaNGOS Extractor", client + " client data");
        if (!m_active)
        {
            return false;
        }

        Ui().SetStatus(SLOT_SOURCE, "client", Shorten(src, 48), Style::STYLE_DETAIL);
        Ui().SetStatus(SLOT_DEST, "output", Shorten(dest, 48), Style::STYLE_DETAIL);
        Ui().SetStatus(SLOT_STAGE, "stage", "idle", Style::STYLE_ACCENT);
        Ui().SetProgress(-1);
        Draw();
        return true;
    }

    void ExtractorConsole::Stop()
    {
        if (!m_active)
        {
            return;
        }

        if (m_interactive)
        {
            Ui().PushLog("Finished. Type q and press enter to close.", Style::STYLE_WARN);
            Ui().SetPrompt("q> ");
            Ui().SetHint("q closes this window");
            Draw();

            std::string line;
            while (true)
            {
                if (!Ui().PollInput(line))
                {
                    Ui().Render();
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    continue;
                }
                const std::string cmd = Trim(line);
                if (cmd == "q" || cmd == "quit" || cmd == "exit")
                {
                    break;
                }
            }
        }

        Ui().Stop();
        m_active = false;
    }

    bool ExtractorConsole::Active() const { return m_active; }

    void ExtractorConsole::Draw()
    {
        if (m_active)
        {
            Ui().Render();
        }
    }

    void ExtractorConsole::Log(const std::string& text)
    {
        if (m_active)
        {
            Ui().PushLog(text, Style::STYLE_NORMAL);
            Draw();
        }
        else
        {
            std::printf("%s\n", text.c_str());
        }
    }

    void ExtractorConsole::Detail(const std::string& text)
    {
        if (m_active)
        {
            Ui().PushLog(text, Style::STYLE_DETAIL);
            Draw();
        }
        else
        {
            std::printf("%s\n", text.c_str());
        }
    }

    void ExtractorConsole::Success(const std::string& text)
    {
        if (m_active)
        {
            Ui().PushLog(text, Style::STYLE_SUCCESS);
            Draw();
        }
        else
        {
            std::printf("%s\n", text.c_str());
        }
    }

    void ExtractorConsole::Warn(const std::string& text)
    {
        if (m_active)
        {
            Ui().PushLog(text, Style::STYLE_WARN);
            Draw();
        }
        else
        {
            std::printf("%s\n", text.c_str());
        }
    }

    void ExtractorConsole::Error(const std::string& text)
    {
        if (m_active)
        {
            Ui().PushLog(text, Style::STYLE_ERROR);
            Draw();
        }
        else
        {
            std::fprintf(stderr, "%s\n", text.c_str());
        }
    }

    void ExtractorConsole::Activity(const std::string& text)
    {
        if (m_active)
        {
            Ui().SetActivity(text);
            Draw();
        }
    }

    void ExtractorConsole::Progress(int percent)
    {
        if (m_active)
        {
            Ui().SetProgress(percent);
            Draw();
        }
    }

    void ExtractorConsole::SetStage(const std::string& stage)
    {
        if (m_active)
        {
            Ui().SetStatus(SLOT_STAGE, "stage", stage, Style::STYLE_ACCENT);
            Draw();
        }
    }

    void ExtractorConsole::SetCounts(size_t done, size_t total)
    {
        if (!m_active)
        {
            return;
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%zu / %zu", done, total);
        Ui().SetStatus(SLOT_PROGRESS, "written", buf, Style::STYLE_NORMAL);
        if (total)
        {
            Ui().SetProgress(int(done * 100 / total));
        }
        Draw();
    }

    void ExtractorConsole::SetElapsed(unsigned seconds)
    {
        if (!m_active)
        {
            return;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u", seconds / 3600,
                      (seconds / 60) % 60, seconds % 60);
        Ui().SetStatus(SLOT_ELAPSED, "elapsed", buf, Style::STYLE_DETAIL);
        Draw();
    }

    void ExtractorConsole::SetLocale(const std::string& locale)
    {
        if (m_active)
        {
            Ui().SetStatus(SLOT_LOCALE, "locale",
                           locale.empty() ? "none (single-locale client)" : locale,
                           Style::STYLE_DETAIL);
            Draw();
        }
    }

    namespace
    {
        bool AlreadyBaked(const std::string& dir)
        {
            std::error_code ec;
            auto it = std::filesystem::directory_iterator(dir, ec);
            return !ec && it != std::filesystem::directory_iterator();
        }
    }

    bool ExtractorConsole::RunMenu(Choice& out, const std::string& dest)
    {
        if (!m_active)
        {
            return true;
        }

        const std::string pad("  ");

        auto row = [&](const char* text, Style style)
        {
            Ui().PushLog(*text ? pad + text : std::string(), style);
        };

        auto showMenu = [&]()
        {
            row("Entry                  Explanation", Style::STYLE_ACCENT);
            row("1                      Extract all", Style::STYLE_NORMAL);
            row("2                      Extract DBC/DB2", Style::STYLE_NORMAL);
            row("3                      Extract gomodels (doors, lifts, ship hulls)",
                Style::STYLE_NORMAL);
            row("4                      Extract tiles (ground, water, area, collision)",
                Style::STYLE_NORMAL);
            row("5                      Extract transports (needs 3)", Style::STYLE_NORMAL);
            row("6                      Extract navmesh (needs 4, takes hours)",
                Style::STYLE_NORMAL);
            row("src [path, optional]   Set source path (mpq directories)",
                Style::STYLE_DETAIL);
            row("dest [path, optional]  Set destination path", Style::STYLE_DETAIL);
            row("map <id>               Restrict to one map id", Style::STYLE_DETAIL);
            row("vessels [path, opt.]   Set vessels.txt (which hull -> which map)",
                Style::STYLE_DETAIL);
            row("offmesh [path, opt.]   Set offmesh.txt (hand-made navmesh links)",
                Style::STYLE_DETAIL);
            row("?                      Show this list", Style::STYLE_DETAIL);
            row("q                      Quit", Style::STYLE_DETAIL);
            row("Type any entry above at the bake> prompt and press enter.",
                Style::STYLE_WARN);
        };

        auto setFile = [&](const char* what, const char* filter,
                           const std::string& typed, std::string& slot)
        {
            std::string chosen = typed;
            if (chosen.empty())
            {
                if (!BrowseForFile(std::string("Choose ") + what, filter, chosen))
                {
                    Ui().PushLog("  cancelled -- or no file dialog on this platform; "
                                 "type the path instead", Style::STYLE_WARN);
                    Draw();
                    return;
                }
            }
            slot = ToUnixPath(chosen);
            Ui().PushLog(std::string("  ") + what + " = " + slot, Style::STYLE_SUCCESS);
            Draw();
        };

        auto setPath = [&](const char* what, const std::string& typed, std::string& slot)
        {
            std::string chosen = typed;
            if (chosen.empty())
            {
                if (!BrowseForFolder(std::string("Choose the ") + what + " folder", chosen))
                {
                    Ui().PushLog("  cancelled -- or no folder browser on this platform; "
                                 "type the path instead", Style::STYLE_WARN);
                    Draw();
                    return;
                }
            }
            slot = ToUnixPath(chosen);
            Ui().PushLog(std::string("  ") + what + " = " + slot, Style::STYLE_SUCCESS);
            Ui().SetStatus(std::string("src") == what ? SLOT_SOURCE : SLOT_DEST,
                           std::string("src") == what ? "client" : "output",
                           Shorten(slot, 48), Style::STYLE_DETAIL);
            Draw();
        };

        m_interactive = true;
        showMenu();
        Ui().SetPrompt("bake> ");
        Ui().SetHint("a number or a word from the menu, then Enter");
        Draw();

        std::string line;
        while (true)
        {
            if (!Ui().PollInput(line))
            {
                Ui().Render();
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                continue;
            }

            const std::string cmd = Trim(line);
            if (cmd.empty())
            {
                continue;
            }

            if (cmd == "q" || cmd == "quit" || cmd == "exit")
            {
                return false;
            }
            if (cmd.rfind("map ", 0) == 0)
            {
                int mapId = 0;
                if (!ParseNonNegative(cmd.substr(4), mapId))
                {
                    Ui().PushLog("  not a map id: " + cmd.substr(4), Style::STYLE_ERROR);
                    Draw();
                    continue;
                }
                out.mapFilter = mapId;
                char buf[64];
                std::snprintf(buf, sizeof(buf), "  restricted to map %d", out.mapFilter);
                Ui().PushLog(buf, Style::STYLE_SUCCESS);
                Draw();
                continue;
            }
            if (cmd == "src" || cmd.rfind("src ", 0) == 0)
            {
                setPath("src", cmd.size() > 4 ? Trim(cmd.substr(4)) : std::string(),
                        out.src);
                continue;
            }
            if (cmd == "dest" || cmd.rfind("dest ", 0) == 0)
            {
                setPath("dest", cmd.size() > 5 ? Trim(cmd.substr(5)) : std::string(),
                        out.dest);
                continue;
            }
            if (cmd == "vessels" || cmd.rfind("vessels ", 0) == 0)
            {
                setFile("vessels.txt", "Text files\0*.txt\0All files\0*.*\0\0",
                        cmd.size() > 8 ? Trim(cmd.substr(8)) : std::string(),
                        out.vesselList);
                continue;
            }
            if (cmd == "offmesh" || cmd.rfind("offmesh ", 0) == 0)
            {
                setFile("offmesh.txt", "Text files\0*.txt\0All files\0*.*\0\0",
                        cmd.size() > 8 ? Trim(cmd.substr(8)) : std::string(),
                        out.offMesh);
                continue;
            }
            if (cmd == "?" || cmd == "help")
            {
                showMenu();
                Draw();
                continue;
            }

            const int keptFilter = out.mapFilter;
            const std::string keptSrc = out.src;
            const std::string keptDest = out.dest;

            bool any = false, bad = false;
            std::istringstream picks(cmd);
            std::string pick;
            while (picks >> pick)
            {
                if (pick == "1")
                {
                    out.dbc = out.goModels = out.tiles = out.vessels = out.nav = true;
                }
                else if (pick == "2") { out.dbc = true; }
                else if (pick == "3") { out.goModels = true; }
                else if (pick == "4") { out.tiles = true; }
                else if (pick == "5") { out.vessels = true; }
                else if (pick == "6") { out.nav = true; }
                else
                {
                    Ui().PushLog("  \"" + pick + "\" is not one of the choices",
                                 Style::STYLE_WARN);
                    bad = true;
                    break;
                }
                any = true;
            }

            if (bad || !any)
            {
                if (!any && !bad)
                {
                    Ui().PushLog("  nothing chosen -- type a number, or q to leave",
                                 Style::STYLE_WARN);
                }
                out = Choice();
                out.mapFilter = keptFilter;
                out.src = keptSrc;
                out.dest = keptDest;
                Draw();
                continue;
            }

            struct Need { bool wanted; bool feeds; const char* who; const char* needs;
                          const char* dir; };
            const Need needs[] = {
                { out.nav,     out.tiles,    "nav",   "tiles (4)",    "/tiles"    },
                { out.tiles,   out.goModels, "tiles", "gomodels (3)", "/gomodels" },
                { out.vessels, out.goModels, "trans", "gomodels (3)", "/gomodels" },
            };

            bool impossible = false;
            for (const Need& n : needs)
            {
                if (n.wanted && !n.feeds && !AlreadyBaked(dest + n.dir))
                {
                    Ui().PushLog(std::string("  ") + n.who + " reads " + n.needs +
                                 " and there is none baked yet -- add it, or pick 1",
                                 Style::STYLE_ERROR);
                    impossible = true;
                }
            }
            if (impossible)
            {
                out = Choice();
                out.mapFilter = keptFilter;
                out.src = keptSrc;
                out.dest = keptDest;
                Draw();
                continue;
            }

            Ui().SetPrompt("");
            Ui().SetHint("baking; this takes a while");
            Draw();
            return true;
        }
    }
}
