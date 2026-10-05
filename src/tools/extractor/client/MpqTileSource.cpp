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
#include <string>
#include <vector>
#include "MpqTileSource.hpp"

#include "AdtParser.hpp"

#include <cmath>

namespace world::terrain
{
    namespace
    {
        constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;
        constexpr float MID = MAP_CENTER * TILE_SIZE;

        Aabb WorldBoundsOf(const Aabb& local, const Transform& xf)
        {
            Aabb out;
            if (!local.valid())
            {
                return out;
            }
            for (int i = 0; i < 8; ++i)
            {
                const Vec3 c{(i & 1) ? local.hi.x : local.lo.x,
                             (i & 2) ? local.hi.y : local.lo.y,
                             (i & 4) ? local.hi.z : local.lo.z};
                out.expand(xf.localToWorld(c));
            }
            return out;
        }

        Transform PlacementTransform(const Placement& p)
        {
            Transform xf;
            xf.pos = {MID - p.pos.z, MID - p.pos.x, p.pos.y};
            xf.rot = Mat3::fromEuler(p.rotDeg.z * DEG2RAD, p.rotDeg.x * DEG2RAD,
                                     (p.rotDeg.y + 180.0f) * DEG2RAD);
            xf.scale = p.scale;
            return xf;
        }

        Transform GlobalWmoTransform(const Placement& p)
        {
            Transform xf;
            xf.pos = {p.pos.z, p.pos.x, p.pos.y};
            xf.rot = Mat3::fromEuler(p.rotDeg.z * DEG2RAD, p.rotDeg.x * DEG2RAD,
                                     (p.rotDeg.y + 180.0f) * DEG2RAD);
            xf.scale = p.scale;
            return xf;
        }

        Transform WmoDoodadTransform(const Transform& wmoXf, const WmoDoodad& d)
        {
            Mat3 r = Mat3::fromQuat(d.quat[0], d.quat[1], d.quat[2], d.quat[3]);
            r.m[1] = -r.m[1];
            r.m[4] = -r.m[4];
            r.m[7] = -r.m[7];

            Transform xf;
            xf.pos = wmoXf.localToWorld(d.pos);
            xf.rot = Mat3::mulm(wmoXf.rot, r);
            xf.scale = wmoXf.scale * d.scale;
            return xf;
        }
    }

    std::string MpqTileSource::MapDirectory(uint32_t mapId) const
    {
        if (m_maps)
        {
            if (const std::string* dir = m_maps->Find(mapId))
            {
                return *dir;
            }
        }
        return std::string();
    }

    std::string MpqTileSource::AdtPath(uint32_t mapId, int tx, int ty) const
    {
        const std::string name = MapDirectory(mapId);
        if (name.empty())
        {
            return std::string();
        }
        return "World\\Maps\\" + name + "\\" + name + "_" + std::to_string(ty) + "_" +
               std::to_string(tx) + ".adt";
    }

    std::string MpqTileSource::WdtPath(uint32_t mapId) const
    {
        const std::string name = MapDirectory(mapId);
        if (name.empty())
        {
            return std::string();
        }
        return "World\\Maps\\" + name + "\\" + name + ".wdt";
    }

    const WdtData* MpqTileSource::Wdt(uint32_t mapId)
    {
        auto it = m_wdtCache.find(mapId);
        if (it != m_wdtCache.end())
        {
            return &it->second;
        }

        const std::string path = WdtPath(mapId);
        std::vector<uint8_t> bytes;
        WdtData wdt;
        if (path.empty() || !m_archive.Read(path, bytes) || !ParseWdt(bytes, wdt))
        {
            return nullptr;
        }
        return &m_wdtCache.emplace(mapId, std::move(wdt)).first->second;
    }

    void MpqTileSource::AttachWmoDoodads(const Placement& p, const std::string& wmoPath,
                                         const Transform& wmoXf, TerrainTile& tile)
    {
        const WmoRootData* root = m_wmo.Root(wmoPath);
        if (!root || root->sets.empty())
        {
            return;
        }

        const auto attachSet = [&](const WmoDoodadSet& set)
        {
            const uint64_t end = uint64_t(set.start) + set.count;
            for (uint64_t i = set.start; i < end && i < root->doodads.size(); ++i)
            {
                const WmoDoodad& d = root->doodads[size_t(i)];
                if (d.name.empty())
                {
                    continue;
                }
                auto model = m_m2.Load(d.name);
                if (!model || model->Empty())
                {
                    continue;
                }

                StaticInstance inst;
                inst.xf = WmoDoodadTransform(wmoXf, d);
                inst.model = model;
                inst.worldBounds = WorldBoundsOf(model->Bounds(), inst.xf);
                inst.adtId = p.nameSet;
                tile.instances.push_back(std::move(inst));
            }
        };

        constexpr uint32_t DEFAULT_GLOBAL_SET = 0;
        attachSet(root->sets[DEFAULT_GLOBAL_SET]);
        if (p.doodadSet != DEFAULT_GLOBAL_SET && p.doodadSet < root->sets.size())
        {
            attachSet(root->sets[p.doodadSet]);
        }
    }

    std::shared_ptr<TerrainTile> MpqTileSource::LoadAdt(uint32_t mapId, int tx, int ty)
    {
        const std::string path = AdtPath(mapId, tx, ty);
        if (path.empty())
        {
            return nullptr;
        }

        std::vector<uint8_t> bytes;
        if (!m_archive.Read(path, bytes))
        {
            return nullptr;
        }

        AdtData adt;
        if (!ParseAdt(bytes, adt) || !adt.hasTerrain)
        {
            return nullptr;
        }

        auto tile = std::make_shared<TerrainTile>();
        tile->tx = tx;
        tile->ty = ty;
        tile->hasTerrain = true;
        tile->v9 = std::move(adt.v9);
        tile->v8 = std::move(adt.v8);
        tile->holes = adt.holes;
        tile->areaIds = adt.areaIds;
        tile->hasLiquid = adt.hasLiquid;
        tile->liquidHeight = std::move(adt.liquidHeight);
        tile->liquidShow = std::move(adt.liquidShow);
        tile->liquidEntry = std::move(adt.liquidEntry);

        if (tile->hasLiquid)
        {
            const size_t cells = tile->liquidShow.size();
            tile->liquidKind.assign(cells, uint8_t(LiquidKind::None));
            tile->liquidDeep.assign(cells, 0);
            for (size_t i = 0; i < cells; ++i)
            {
                if (!tile->liquidShow[i])
                {
                    continue;
                }
                const LiquidKind kind =
                    world::ClassifyLiquid(tile->liquidEntry[i], m_liquidTypes);
                tile->liquidKind[i] = uint8_t(kind);
                tile->liquidDeep[i] =
                    (adt.liquidDark[i] ||
                     (kind == LiquidKind::Ocean && adt.liquidNoLight[i]))
                        ? 1
                        : 0;
            }
        }

        if (!m_loadStatics)
        {
            return tile;
        }

        auto attach = [&](const Placement& p,
                          const std::shared_ptr<const ICollisionModel>& model)
        {
            if (!model || model->Empty())
            {
                return;
            }
            StaticInstance inst;
            inst.xf = PlacementTransform(p);
            inst.model = model;
            inst.worldBounds = WorldBoundsOf(model->Bounds(), inst.xf);
            inst.adtId = p.nameSet;
            tile->instances.push_back(std::move(inst));
        };

        for (const Placement& p : adt.wmoPlacements)
        {
            if (p.nameIndex >= adt.wmoNames.size())
            {
                continue;
            }
            const std::string& wmoPath = adt.wmoNames[p.nameIndex];
            attach(p, m_wmo.Load(wmoPath));
            AttachWmoDoodads(p, wmoPath, PlacementTransform(p), *tile);
        }

        for (const Placement& p : adt.m2Placements)
        {
            if (p.nameIndex < adt.m2Names.size())
            {
                attach(p, m_m2.Load(adt.m2Names[p.nameIndex]));
            }
        }

        return tile;
    }

    std::shared_ptr<TerrainTile> MpqTileSource::LoadGlobalWmo(uint32_t mapId)
    {
        auto cached = m_globalWmoCache.find(mapId);
        if (cached != m_globalWmoCache.end())
        {
            return cached->second;
        }

        const WdtData* wdt = Wdt(mapId);
        if (!wdt || !wdt->hasGlobalWmo || wdt->globalWmoName.empty() ||
            !wdt->globalWmoPlacement)
        {
            return nullptr;
        }

        auto model = m_wmo.Load(wdt->globalWmoName);
        if (!model || model->Empty())
        {
            return nullptr;
        }

        auto tile = std::make_shared<TerrainTile>();
        tile->isGlobalWmo = true;

        const Transform xf = GlobalWmoTransform(*wdt->globalWmoPlacement);
        StaticInstance inst;
        inst.xf = xf;
        inst.model = model;
        inst.worldBounds = WorldBoundsOf(model->Bounds(), inst.xf);
        inst.adtId = wdt->globalWmoPlacement->nameSet;
        tile->instances.push_back(std::move(inst));

        AttachWmoDoodads(*wdt->globalWmoPlacement, wdt->globalWmoName, xf, *tile);

        m_globalWmoCache[mapId] = tile;
        return tile;
    }

    std::shared_ptr<TerrainTile> MpqTileSource::Load(uint32_t mapId, int tx, int ty)
    {
        if (auto adtTile = LoadAdt(mapId, tx, ty))
        {
            return adtTile;
        }
        return LoadGlobalWmo(mapId);
    }
}
