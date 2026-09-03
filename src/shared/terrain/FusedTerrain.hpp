#pragma once

// The runtime terrain query engine for one map: a cache of baked tiles and the
// geometric questions the server asks of them.
//
// It is deliberately the ENGINE HALF only. It knows nothing of AreaTable, of
// WMOAreaTable, of liquid spells or of the navmesh -- everything that needs a DBC or
// the world lives above it, in the game. What comes back from here is geometry and the
// identifiers baked into the tile; translating those into MaNGOS's DBC world is the
// caller's job. That is what lets the offline probe tools ask the terrain exactly what
// mangosd asks it, and link none of the server to do so.

#include "terrain/Column.hpp"
#include "terrain/ILiveGeometry.hpp"
#include "terrain/Terrain.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace world::terrain
{
    class FusedTerrain
    {
    public:
        static constexpr int GRID_COUNT = FusedTerrainGridCount;

        /// Without a source the tiles come from files named after `mapId`, as they always
        /// have. WITH one the source is the only supplier -- a map backed by a model has
        /// no grid on disk and must never half-resolve against a file that happens to
        /// carry its id.
        explicit FusedTerrain(uint32_t mapId,
                              std::shared_ptr<ITileSource> source = nullptr);

        FusedTerrain(const FusedTerrain&) = delete;
        FusedTerrain& operator=(const FusedTerrain&) = delete;

        uint32_t MapId() const { return m_mapId; }

        static void SetTileDir(const std::string& dir);
        static const std::string& TileDir();
        static bool HasTile(uint32_t mapId, int tx, int ty);

        // THE height query. Every surface crossing the window [zBottom, zTop] over (x,y):
        // the ADT heightmap, each baked static, the liquid surface, and -- when `live` is
        // given -- the bodies the game poses at runtime. `filter` reaches that provider
        // untouched; this side does not know what it selects.
        //
        // There is deliberately no GetHeight/GetFloor/GetWaterOrGround here. Each was the
        // same gather with a different selection already applied, so a caller could not
        // ask a question the entry point had not anticipated, and four layers each kept
        // their own idea of which surface "the ground" meant. Select on the Column.
        Column ColumnAt(float x, float y, float zTop, float zBottom,
                        const ILiveGeometry* live = nullptr, uint32_t filter = 0) const;

        // Nearest static hit along a->b as a fraction of the segment; > 1 when nothing
        // blocks. A fraction rather than a point on purpose: the static and dynamic
        // worlds are queried over the same segment and the nearer fraction wins, so only
        // the winner is ever turned back into a point.
        //
        // The segment is used VERBATIM. Any agent-height lift belongs to the caller,
        // which already applies it; adding one here would make static line of sight more
        // permissive than the eyes it models, and would sample a door at a different
        // height than the doorway it sits in.
        float NearestHitFraction(float x1, float y1, float z1, float x2, float y2,
                                 float z2) const;

        bool IsInLineOfSight(float x1, float y1, float z1, float x2, float y2,
                             float z2) const;

        // AreaTable.dbc id of the MCNK chunk under (x,y), or 0 when unknown.
        uint16_t GetAreaId(float x, float y) const;

        // The WMO group whose floor is under (x,y) near z: the WMOAreaTable triple plus
        // the group's MOGP flags, with groundZ snapped to that floor. Deciding what the
        // flags MEAN needs WMOAreaTable and belongs to the caller.
        bool GetAreaInfo(float x, float y, float z, uint32_t& mogpFlags, int32_t& adtId,
                         int32_t& rootId, int32_t& groupId, float& groundZ) const;

        // Tiles are mapped, not read, so a resident one costs page cache the
        // kernel reclaims on its own rather than heap the server must sweep.
        // The cache is therefore monotonic on purpose and there is nothing to
        // pin, evict or age.
        size_t ResidentTiles() const;

        struct PreloadStats
        {
            uint32_t mapped = 0;   ///< cells that had a tile behind them
            uint32_t absent = 0;   ///< cells this map simply does not cover
        };

        /**
         * @brief Map every tile this map has, now, rather than on first touch.
         *
         * Pays the whole cost at startup so no map thread ever takes a page
         * fault's worth of file opening mid-tick, and so address space and
         * resident size are what they are going to be from the start. The
         * absent cells are recorded too, which spares the runtime a failed open
         * every time a query lands over open ocean.
         *
         * Counts come back rather than being logged: this library has no
         * console, and whoever calls it does.
         */
        PreloadStats PreloadAll();

    private:
        using TilePtr = std::shared_ptr<const TerrainTile>;

        TilePtr TileAt(float x, float y) const;
        TilePtr GlobalWmo() const;
        TilePtr LoadCell(int tx, int ty) const;

        void CollectSegmentInstances(const Vec3& a, const Vec3& b,
                                     std::vector<const StaticInstance*>& out,
                                     std::vector<TilePtr>& keepAlive) const;

        const uint32_t m_mapId;
        const std::shared_ptr<ITileSource> m_source;

        // Every query from every map-update thread goes through this cache, so the hit
        // path takes the lock SHARED: readers do not serialise against each other. All
        // instances of one dungeon share a single FusedTerrain, so they would otherwise
        // contend on one exclusive lock for every height, liquid and sight query.
        //
        // A null-but-probed entry is a memo that the map has no such tile; it costs one
        // byte and spares a failed file open per query, so the sweep keeps it.
        mutable std::array<std::array<TilePtr, GRID_COUNT>, GRID_COUNT> m_tiles;
        mutable std::array<std::array<uint8_t, GRID_COUNT>, GRID_COUNT> m_loaded{};
        mutable TilePtr m_globalWmo;
        mutable uint8_t m_globalWmoProbed = 0;
        mutable std::shared_mutex m_mutex;
    };
}
