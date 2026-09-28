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

#include <memory>
#include "ExtractorConsole.hpp"
#include "nav/NavMeshBuilder.hpp"
#include "MoveMapSharedDefines.h"
#include "client/ModelLoaders.hpp"
#include "client/MpqTileSource.hpp"
#include "client/StormLibArchive.hpp"
#include "stores/LiquidTypeStore.hpp"
#include "stores/GameObjectDisplayInfoStore.hpp"
#include "stores/MapDbcStore.hpp"
#include "terrain/TileSerializer.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifndef MANGOS_CLIENT_NAME
#define MANGOS_CLIENT_NAME "unknown client"
#endif

using namespace world::terrain;

namespace
{
    struct Options
    {
        std::string src = "Data";
        std::string dest = "extracted_data";
        std::string locale;
        bool allLocales = false;
        int mapFilter = -1;
        bool dbc = false;
        bool tiles = false;
        bool goModels = false;
        bool vessels = false;
        bool nav = false;
        std::string vesselList;

        bool named = false;
        bool noMenu = false;
        bool help = false;
        std::string offMesh;
        int threads = 0;
    };

    ExtractorConsole g_console;
    std::chrono::steady_clock::time_point g_started;

    void Tick()
    {
        const auto now = std::chrono::steady_clock::now();
        g_console.SetElapsed(unsigned(
            std::chrono::duration_cast<std::chrono::seconds>(now - g_started).count()));
    }

    std::vector<std::string> FindLocales(const std::string& dataDir)
    {
        std::vector<std::string> found;
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(dataDir, ec))
        {
            if (!e.is_directory(ec))
            {
                continue;
            }
            const std::string name = e.path().filename().string();
            if (std::filesystem::exists(e.path() / ("locale-" + name + ".MPQ"), ec))
            {
                found.push_back(name);
            }
        }

        std::sort(found.begin(), found.end());

        const auto en = std::find(found.begin(), found.end(), std::string("enUS"));
        if (en != found.end())
        {
            std::rotate(found.begin(), en, en + 1);
        }
        return found;
    }

    std::string DetectLocale(const std::string& dataDir)
    {
        const std::vector<std::string> found = FindLocales(dataDir);
        return found.empty() ? std::string() : found.front();
    }

    void Usage()
    {
        std::printf(
"mangos-extractor -- bakes a WoW client into the caches mangosd reads.\n"
"  built for: " MANGOS_CLIENT_NAME "\n"
"\n"
"  usage: mangos-extractor [component ...] [option ...]\n"
"\n"
"COMPONENTS -- name none and it bakes them all.\n"
"\n"
"  dbc        the client databases (.dbc), copied out whole.\n"
"  gomodels   one collision body per game-object display id. Doors, bridges\n"
"             and ship hulls are models, not terrain, and this is where they\n"
"             come from. Needed by tile and by trans.\n"
"  tile       the world itself: ground, liquid, area and static collision\n"
"             fused into one .tile per map square. The long one.\n"
"  trans      vessel decks. Each ship's hull is baked as a MAP of its own, so\n"
"             a passenger stands on real ground rather than on an offset.\n"
"             Reads baked gomodels only -- no client needed.\n"
"  nav        the navmesh, built FROM THE TILES, never from the client, so\n"
"             the pathfinder walks exactly the surface collision answers with.\n"
"  all        every one of the above, in that order.\n"
"\n"
"WHERE\n"
"\n"
"  --src <dir>     the client's Data directory              (default: Data)\n"
"  --dest <dir>    where the caches are written   (default: extracted_data)\n"
"  --locale <loc>  the client's locale folder            (default: detected)\n"
"                  Detected by looking for locale-<loc>.MPQ inside a folder\n"
"                  of that name. Only the DBC set is locale-dependent; all\n"
"                  terrain and models live in shared archives.\n"
"                  --locale all bakes EVERY language the client carries:\n"
"                  the first (enUS if present) into dbc/, each of the rest\n"
"                  into dbc/<loc>/, which is where the server looks.\n"
"\n"
"AUTHORED INPUT -- both default to a file beside the executable\n"
"\n"
"  --vessels <f>   \"<mapId> <displayId>\" per line: which hull to bake into\n"
"                  which vessel map.                  (default: vessels.txt)\n"
"  --offmesh <f>   navmesh links the generated mesh cannot bridge -- a jump\n"
"                  down a dock, a gap over water.     (default: offmesh.txt)\n"
"\n"
"OTHER\n"
"\n"
"  --map <id>      bake only this map id. For chasing one map, not for a\n"
"                  real bake.\n"
"  --threads <n>   worker threads                    (default: all cores)\n"
"  --no-menu       never ask, even on a terminal.\n"
"\n"
"On a terminal, naming no component opens the menu -- that is the front door\n"
"and it explains every choice. With no terminal to open it on (a pipe, a CI\n"
"log) everything is baked instead, so an unattended run needs no arguments.\n");
    }

    bool ParseArgs(int argc, char** argv, Options& out)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string a = argv[i];
            const bool hasValue = (i + 1 < argc);

            if (a == "dbc") { out.dbc = out.named = true; }
            else if (a == "tile") { out.tiles = out.named = true; }
            else if (a == "gomodels") { out.goModels = out.named = true; }
            else if (a == "trans" || a == "vessels") { out.vessels = out.named = true; }
            else if (a == "nav") { out.nav = out.named = true; }
            else if (a == "all")
            {
                out.dbc = out.tiles = out.goModels = out.vessels = out.nav =
                    out.named = true;
            }
            else if (a == "--vessels" && hasValue) { out.vesselList = argv[++i]; }
            else if (a == "--src" && hasValue) { out.src = argv[++i]; }
            else if (a == "--dest" && hasValue) { out.dest = argv[++i]; }
            else if (a == "--locale" && hasValue)
            {
                const std::string v = argv[++i];
                out.allLocales = (v == "all");
                out.locale = out.allLocales ? std::string() : v;
            }
            else if ((a == "--map" || a == "--threads") && hasValue)
            {
                const std::string value = argv[++i];
                int& target = (a == "--map") ? out.mapFilter : out.threads;
                if (!ExtractorConsole::ParseNonNegative(value, target))
                {
                    std::printf("invalid value for %s: %s\n", a.c_str(), value.c_str());
                    return false;
                }
            }
            else if (a == "--offmesh" && hasValue) { out.offMesh = argv[++i]; }
            else if (a == "--no-menu") { out.noMenu = true; }
            else if (a == "-h" || a == "--help") { out.help = true; return false; }
            else
            {
                std::printf("unknown argument: %s\n", a.c_str());
                return false;
            }
        }

        return true;
    }

    int ExtractDbc(StormLibArchive& mpq, const std::string& dest)
    {
        std::error_code ec;
        std::filesystem::create_directories(dest, ec);

        int written = 0;
        for (const std::string& name : mpq.FindFiles("DBFilesClient\\*.dbc"))
        {
            std::vector<uint8_t> bytes;
            if (!mpq.Read(name, bytes))
            {
                continue;
            }
            const size_t slash = name.find_last_of('\\');
            const std::string leaf = slash == std::string::npos ? name
                                                                : name.substr(slash + 1);
            if ((written % 16) == 0)
            {
                g_console.Activity("dbc " + leaf);
                Tick();
            }
            std::FILE* f = std::fopen((dest + "/" + leaf).c_str(), "wb");
            if (!f)
            {
                continue;
            }
            const bool ok = bytes.empty() ||
                            std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
            std::fclose(f);
            written += ok ? 1 : 0;
        }

        char msg[512];
        std::snprintf(msg, sizeof(msg), "dbc: %d files -> %s", written, dest.c_str());
        g_console.Success(msg);
        return written;
    }

    void BakeGoModels(WmoLoader& wmo, M2Loader& m2,
                      const world::GameObjectDisplayInfoStore& display,
                      const std::string& dest)
    {
        std::error_code ec;
        std::filesystem::create_directories(dest, ec);

        int written = 0, empty = 0;
        for (const auto& entry : display.All())
        {
            std::shared_ptr<const ICollisionModel> model =
                world::GameObjectDisplayInfoStore::IsWmo(entry.second)
                    ? wmo.Load(entry.second)
                    : m2.Load(entry.second);

            if (!model || model->Empty())
            {
                ++empty;
                continue;
            }

            if ((written % 32) == 0)
            {
                g_console.Activity("gomodel " + entry.second);
                Tick();
            }

            TerrainTile tile;
            StaticInstance inst;
            inst.model = model;
            inst.worldBounds = model->Bounds();
            tile.instances.push_back(std::move(inst));

            if (WriteTile(tile, dest + "/" + GoModelFileName(entry.first)))
            {
                ++written;
            }
        }
        char msg[512];
        std::snprintf(msg, sizeof(msg), "gomodels: %d written, %d without collision",
                      written, empty);
        g_console.Success(msg);
    }

    void NavProgress(void* ctx, uint32_t, const char* label, size_t done, size_t total)
    {
        (void)ctx;
        g_console.SetCounts(done, total);
        if (label && *label)
        {
            g_console.Activity(std::string("nav ") + label);
        }
        Tick();
    }

    void NavMapDone(void* ctx, uint32_t, const char* label, int written, size_t total)
    {
        (void)ctx;
        char msg[128];
        std::snprintf(msg, sizeof(msg), "nav %s -> %d/%zu mmtiles",
                      label ? label : "", written, total);
        g_console.Detail(msg);
        Tick();
    }

    struct VesselMap
    {
        uint32_t mapId;
        uint32_t displayId;
    };

    std::filesystem::path ExecutablePath(const char* argv0)
    {
        std::error_code ec;
#if defined(_WIN32)
        char* module = nullptr;
        if (_get_pgmptr(&module) == 0 && module && *module)
        {
            return module;
        }
#elif defined(__linux__)
        const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec)
        {
            return self;
        }
#elif defined(__FreeBSD__)
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        char buffer[PATH_MAX];
        size_t size = sizeof(buffer);
        if (sysctl(mib, 4, buffer, &size, nullptr, 0) == 0)
        {
            return buffer;
        }
#endif
        return std::filesystem::absolute(argv0 ? argv0 : "mangos-extractor", ec);
    }

    std::string DefaultDataFile(const char* argv0, const char* name)
    {
        std::error_code ec;
        const std::filesystem::path beside = ExecutablePath(argv0).parent_path() / name;
        return std::filesystem::exists(beside, ec) ? beside.string() : std::string(name);
    }

    std::string DefaultVesselList(const char* argv0)
    {
        return DefaultDataFile(argv0, "vessels.txt");
    }

    std::string DefaultOffMeshList(const char* argv0)
    {
        return DefaultDataFile(argv0, "offmesh.txt");
    }

    std::vector<VesselMap> ReadVesselMaps(const std::string& path)
    {
        std::vector<VesselMap> out;
        std::ifstream in(path);
        if (!in)
        {
            g_console.Error("vessels: could not open " + path);
            return out;
        }

        std::string line;
        while (std::getline(in, line))
        {
            const size_t hash = line.find('#');
            if (hash != std::string::npos)
            {
                line.erase(hash);
            }

            std::istringstream ls(line);
            VesselMap v{};
            if (ls >> v.mapId >> v.displayId)
            {
                out.push_back(v);
            }
        }
        return out;
    }

    int BakeVesselMaps(const std::string& goDir, const std::string& tileDir,
                       const std::vector<VesselMap>& vessels)
    {
        int written = 0;
        for (const VesselMap& v : vessels)
        {
            auto tile = ReadTile(goDir + "/" + GoModelFileName(v.displayId));
            if (!tile || tile->instances.empty())
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "vessels: no baked collision for display id %u",
                              v.displayId);
                g_console.Error(msg);
                continue;
            }

            tile->isGlobalWmo = true;

            char msg[256];
            if (WriteTile(*tile, tileDir + "/" + GlobalWmoFileName(v.mapId)))
            {
                ++written;
                std::snprintf(msg, sizeof(msg), "  map %5u <- display %5u", v.mapId,
                              v.displayId);
                g_console.Detail(msg);
            }
            else
            {
                std::snprintf(msg, sizeof(msg),
                              "vessels: could not write map %u", v.mapId);
                g_console.Error(msg);
            }
            Tick();
        }

        char msg[256];
        std::snprintf(msg, sizeof(msg), "vessels: %d hull maps -> %s", written,
                      tileDir.c_str());
        g_console.Success(msg);
        return written;
    }

    bool BakeNav(const Options& opt, const std::string& tileDir)
    {
        if (!opt.nav)
        {
            return true;
        }

        g_console.SetStage("nav");

        for (const NavAgentShape& agent : NAV_AGENTS)
        {
            world::nav::NavConfig cfg;
            cfg.threads = opt.threads;
            cfg.offMeshFile = opt.offMesh;
            cfg.maxWalkableAngle = agent.maxSlope;
            cfg.walkableRadius = int(std::ceil(agent.radius / cfg.cellSize));

            const std::string outDir = opt.dest + "/mmaps/" + agent.dir;
            world::nav::NavMeshBuilder builder(tileDir, outDir, cfg);
            builder.SetProgress(&NavProgress, nullptr);
            builder.SetMapDone(&NavMapDone);

            const int written = builder.BakeAll(opt.mapFilter);
            if (written < 0)
            {
                g_console.Error("nav: bake failed; inspect the earlier diagnostics and " +
                                tileDir);
                return false;
            }

            char msg[256];
            std::snprintf(msg, sizeof(msg), "nav: %d mmtile files -> %s", written,
                          outDir.c_str());
            g_console.Success(msg);
        }
        return true;
    }

    void RemoveMapTiles(const std::string& dir, uint32_t mapId)
    {
        std::error_code ec;
        std::vector<std::filesystem::path> stale;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            const std::string leaf = entry.path().filename().string();
            unsigned id = 0;
            int tx = 0, ty = 0;
            const bool gridTile = std::sscanf(leaf.c_str(), "t_%u_%d_%d.tile", &id, &tx, &ty) == 3;
            if ((gridTile && id == mapId) || leaf == GlobalWmoFileName(mapId))
            {
                stale.push_back(entry.path());
            }
        }
        for (const auto& path : stale)
        {
            std::filesystem::remove(path, ec);
        }
    }

    void BakeMap(MpqTileSource& source, uint32_t mapId, const std::string& name,
                 const std::string& dest)
    {
        const WdtData* wdt = source.Wdt(mapId);
        if (!wdt)
        {
            return;
        }

        RemoveMapTiles(dest, mapId);

        if (!wdt->HasAnyAdt())
        {
            auto tile = source.Load(mapId, 0, 0);
            if (tile && tile->isGlobalWmo)
            {
                const bool ok =
                    WriteTile(*tile, dest + "/" + GlobalWmoFileName(mapId));
                char msg[256];
                std::snprintf(msg, sizeof(msg), "  map %4u %-24s global WMO %s", mapId,
                              name.c_str(), ok ? "ok" : "FAILED");
                if (ok) { g_console.Detail(msg); } else { g_console.Error(msg); }
            }
            return;
        }

        size_t expected = 0;
        for (int ty = 0; ty < 64; ++ty)
        {
            for (int tx = 0; tx < 64; ++tx)
            {
                expected += wdt->HasAdt(tx, ty) ? 1 : 0;
            }
        }

        int written = 0, failed = 0;
        for (int ty = 0; ty < 64; ++ty)
        {
            for (int tx = 0; tx < 64; ++tx)
            {
                if (!wdt->HasAdt(tx, ty))
                {
                    continue;
                }
                g_console.SetCounts(size_t(written), expected);
                Tick();
                auto tile = source.Load(mapId, tx, ty);
                if (!tile || !tile->hasTerrain)
                {
                    ++failed;
                    continue;
                }
                if (WriteTile(*tile, dest + "/" + TileFileName(mapId, tx, ty)))
                {
                    ++written;
                }
                else
                {
                    ++failed;
                }
            }
        }
        char msg[256];
        std::snprintf(msg, sizeof(msg), "  map %4u %-24s %5d tiles%s", mapId,
                      name.c_str(), written, failed ? " (SOME FAILED)" : "");
        if (failed) { g_console.Warn(msg); } else { g_console.Detail(msg); }
    }
}

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace
{
    bool StdoutIsPiped()
    {
#if defined(_WIN32)
        return _isatty(_fileno(stdout)) == 0;
#else
        return isatty(fileno(stdout)) == 0;
#endif
    }
}

int main(int argc, char** argv)
{
    if (StdoutIsPiped())
    {
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    }

    Options opt;
    if (!ParseArgs(argc, argv, opt))
    {
        Usage();
        return opt.help ? 0 : 2;
    }

    opt.src = ExtractorConsole::ToUnixPath(opt.src);
    opt.dest = ExtractorConsole::ToUnixPath(opt.dest);

    if (opt.vesselList.empty())
    {
        opt.vesselList = DefaultVesselList(argc > 0 ? argv[0] : nullptr);
    }

    if (opt.offMesh.empty())
    {
        opt.offMesh = DefaultOffMeshList(argc > 0 ? argv[0] : nullptr);
    }

    g_started = std::chrono::steady_clock::now();
    g_console.Start(opt.src, opt.dest, "1.12.1");

    if (!opt.named && !opt.noMenu && g_console.Active())
    {
        ExtractorConsole::Choice choice;
        if (!g_console.RunMenu(choice, opt.dest))
        {
            g_console.Stop();
            return 0;
        }
        opt.dbc = choice.dbc;
        opt.tiles = choice.tiles;
        opt.goModels = choice.goModels;
        opt.vessels = choice.vessels;
        opt.nav = choice.nav;
        if (choice.mapFilter >= 0)
        {
            opt.mapFilter = choice.mapFilter;
        }
        if (!choice.src.empty())
        {
            opt.src = choice.src;
        }
        if (!choice.dest.empty())
        {
            opt.dest = choice.dest;
        }
    }
    else if (!opt.named)
    {
        opt.dbc = opt.tiles = opt.goModels = opt.vessels = opt.nav = true;
    }

    {
        const auto baked = [&](const char* dir)
        {
            std::error_code ec;
            auto it = std::filesystem::directory_iterator(opt.dest + dir, ec);
            return !ec && it != std::filesystem::directory_iterator();
        };
        struct Need { bool wanted; bool feeds; const char* who; const char* needs;
                      const char* dir; };
        const Need needs[] = {
            { opt.nav,     opt.tiles,    "nav",   "tile",     "/tiles"    },
            { opt.tiles,   opt.goModels, "tile",  "gomodels", "/gomodels" },
            { opt.vessels, opt.goModels, "trans", "gomodels", "/gomodels" },
        };
        for (const Need& n : needs)
        {
            if (n.wanted && !n.feeds && !baked(n.dir))
            {
                g_console.Error(std::string("  ") + n.who + " reads " + n.needs +
                                " and none is baked under " + opt.dest +
                                " -- name " + n.needs + " too, or run bare for all");
                g_console.Stop();
                return 1;
            }
        }
    }

    if (opt.locale.empty())
    {
        opt.locale = DetectLocale(opt.src);
        if (opt.locale.empty())
        {
            g_console.Detail("  locale: none -- a single-locale client, everything at "
                             "the Data root");
        }
        else
        {
            g_console.Detail("  locale: " + opt.locale + " (detected)");
        }
    }
    g_console.SetLocale(opt.locale);

    const std::string tileDir = opt.dest + "/tiles";

    const auto BakeVessels = [&opt, &tileDir]()
    {
        if (!opt.vessels)
        {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(tileDir, ec);
        g_console.SetStage("vessels");
        BakeVesselMaps(opt.dest + "/gomodels", tileDir, ReadVesselMaps(opt.vesselList));
    };

    if (!opt.dbc && !opt.tiles && !opt.goModels)
    {
        BakeVessels();
        const bool ok = BakeNav(opt, tileDir);
        g_console.SetStage("done");
        g_console.Progress(-1);
        g_console.Stop();
        return ok ? 0 : 1;
    }

    StormLibArchive mpq;
    const int opened = mpq.OpenClientData(opt.src, ClientArchives112(),
                                          ClientLocaleArchives112(), opt.locale);
    if (!opened)
    {
        g_console.Error("no client archives opened under " + opt.src);
        g_console.Stop();
        return 1;
    }
    {
        char msg[512];
        std::snprintf(msg, sizeof(msg), "opened %d archives from %s (%s)", opened,
                      opt.src.c_str(), opt.locale.c_str());
        g_console.Log(msg);
    }

    if (opt.dbc)
    {
        g_console.SetStage("dbc");
        ExtractDbc(mpq, opt.dest + "/dbc");
    }

    if (opt.dbc && opt.allLocales)
    {
        for (const std::string& loc : FindLocales(opt.src))
        {
            if (loc == opt.locale)
            {
                continue;
            }

            StormLibArchive other;
            if (!other.OpenClientData(opt.src, ClientArchives112(),
                                       ClientLocaleArchives112(), loc))
            {
                g_console.Warn("  no archives for locale " + loc + " -- skipped");
                continue;
            }

            g_console.SetLocale(loc);
            ExtractDbc(other, opt.dest + "/dbc/" + loc);
        }
        g_console.SetLocale(opt.locale);
    }

    if (!opt.tiles && !opt.goModels)
    {
        BakeVessels();
        const bool ok = BakeNav(opt, tileDir);
        g_console.SetStage("done");
        g_console.Progress(-1);
        g_console.Stop();
        return ok ? 0 : 1;
    }

    world::MapDbcStore maps;
    world::LiquidTypeStore liquids;
    if (!maps.LoadFromDbc(mpq) || !liquids.LoadFromDbc(mpq))
    {
        g_console.Error("Map.dbc or LiquidType.dbc could not be read");
        g_console.Stop();
        return 1;
    }

    std::error_code ec;
    std::filesystem::create_directories(tileDir, ec);

    if (opt.goModels)
    {
        g_console.SetStage("gomodels");
        world::GameObjectDisplayInfoStore display;
        if (display.LoadFromDbc(mpq))
        {
            WmoLoader wmo(mpq, &liquids);
            M2Loader m2(mpq);
            BakeGoModels(wmo, m2, display, opt.dest + "/gomodels");
        }
        else
        {
            g_console.Error("GameObjectDisplayInfo.dbc could not be read");
        }
    }

    BakeVessels();

    if (!opt.tiles)
    {
        const bool ok = BakeNav(opt, tileDir);
        g_console.SetStage("done");
        g_console.Progress(-1);
        g_console.Stop();
        return ok ? 0 : 1;
    }

    g_console.SetStage("tiles");
    MpqTileSource source(mpq, &maps, &liquids);
    g_console.Log("tiles -> " + tileDir);

    for (const auto& entry : maps.All())
    {
        if (opt.mapFilter >= 0 && uint32_t(opt.mapFilter) != entry.first)
        {
            continue;
        }
        BakeMap(source, entry.first, entry.second, tileDir);
    }

    if (!BakeNav(opt, tileDir))
    {
        g_console.Stop();
        return 1;
    }

    g_console.SetStage("done");
    g_console.Progress(-1);
    Tick();
    g_console.Success("bake complete");
    g_console.Stop();
    return 0;
}
