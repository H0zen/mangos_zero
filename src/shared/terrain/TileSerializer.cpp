#include <memory>
#include <string>
#include <vector>
#include "terrain/TileSerializer.hpp"
#include "terrain/CollisionModel.hpp"
#include "terrain/MappedFile.hpp"
#include "terrain/WmoModel.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace world::terrain
{
    namespace
    {
        constexpr uint32_t MAGIC = 0x30474E4D;  // "MNG0" in file order

        // 2: every array is preceded by padding to its own alignment, so the
        // reader can point at it inside a mapping instead of copying it out. A
        // tile written by an older extractor is refused, and the caller rebakes.
        constexpr uint32_t VERSION = 2;

        constexpr uint32_t MAX_MODELS = 1u << 20;
        constexpr uint32_t MAX_INSTANCES = 1u << 22;

        /// Where the next object of alignment `a` starts at or after `off`.
        inline size_t AlignUp(size_t off, size_t a)
        {
            const size_t rem = off % a;
            return rem ? off + (a - rem) : off;
        }

        /// Sequential writer that knows its own offset, so it can insert the same
        /// padding the reader will skip. The two must agree exactly; they do
        /// because both derive it from the offset and nothing else.
        class Writer
        {
            public:
                explicit Writer(std::FILE* f) : m_f(f) {}

                bool Ok() const { return m_ok; }

                void Pad(size_t align)
                {
                    static const uint8_t zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
                    const size_t target = AlignUp(m_off, align);
                    while (m_ok && m_off < target)
                    {
                        const size_t n = target - m_off;
                        m_ok = std::fwrite(zeros, 1, n, m_f) == n;
                        m_off += n;
                    }
                }

                template <class T>
                void Pod(const T& v)
                {
                    Pad(alignof(T));
                    if (!m_ok)
                    {
                        return;
                    }
                    m_ok = std::fwrite(&v, sizeof(T), 1, m_f) == 1;
                    m_off += sizeof(T);
                }

                template <class T>
                void Raw(const T* data, size_t count)
                {
                    Pad(alignof(T));
                    if (!m_ok || count == 0)
                    {
                        return;
                    }
                    m_ok = std::fwrite(data, sizeof(T), count, m_f) == count;
                    m_off += sizeof(T) * count;
                }

                /// A counted array: the count, then the elements on their own
                /// alignment.
                template <class T>
                void Arr(const T* data, size_t count)
                {
                    Pod(uint32_t(count));
                    Raw(data, count);
                }

                template <class T>
                void Arr(const std::vector<T>& v) { Arr(v.data(), v.size()); }

                template <class T>
                void Arr(const Store<T>& s) { Arr(s.data(), s.size()); }

                template <class T, size_t N>
                void Fixed(const std::array<T, N>& a) { Raw(a.data(), N); }

            private:
                std::FILE* m_f;
                size_t m_off = 0;
                bool m_ok = true;
        };

        /// Sequential reader over a mapping. Nothing is copied unless a caller
        /// asks for it: an array can be pointed at in place, which is what makes
        /// a resident tile cost page cache rather than heap.
        class Cursor
        {
            public:
                Cursor(std::shared_ptr<const MappedFile> file)
                    : m_file(std::move(file)) {}

                bool Ok() const { return m_ok; }
                void Fail() { m_ok = false; }

                template <class T>
                bool Pod(T& v)
                {
                    const size_t at = AlignUp(m_off, alignof(T));
                    if (!m_ok || !m_file->Covers(at, sizeof(T)))
                    {
                        m_ok = false;
                        return false;
                    }
                    std::memcpy(&v, m_file->Data() + at, sizeof(T));
                    m_off = at + sizeof(T);
                    return true;
                }

                /// Point `out` at the array in place.
                template <class T>
                bool View(Store<T>& out)
                {
                    uint32_t count = 0;
                    if (!Pod(count))
                    {
                        return false;
                    }

                    const size_t at = AlignUp(m_off, alignof(T));
                    const size_t bytes = size_t(count) * sizeof(T);
                    if (!m_file->Covers(at, bytes))
                    {
                        m_ok = false;
                        return false;
                    }

                    out.View(reinterpret_cast<const T*>(m_file->Data() + at), count);
                    m_off = at + bytes;
                    return true;
                }

                /// Copy the array out. For the parts a builder still owns.
                template <class T>
                bool Copy(std::vector<T>& out)
                {
                    Store<T> view;
                    if (!View(view))
                    {
                        return false;
                    }
                    out.assign(view.begin(), view.end());
                    return true;
                }

                template <class T, size_t N>
                bool Fixed(std::array<T, N>& out)
                {
                    const size_t at = AlignUp(m_off, alignof(T));
                    const size_t bytes = sizeof(T) * N;
                    if (!m_ok || !m_file->Covers(at, bytes))
                    {
                        m_ok = false;
                        return false;
                    }
                    std::memcpy(out.data(), m_file->Data() + at, bytes);
                    m_off = at + bytes;
                    return true;
                }

                const std::shared_ptr<const MappedFile>& File() const { return m_file; }

            private:
                std::shared_ptr<const MappedFile> m_file;
                size_t m_off = 0;
                bool m_ok = true;
        };

        void WriteGroup(Writer& w, const WmoModel::Group& g)
        {
            w.Pod(g.mogpFlags);
            w.Pod(g.groupWmoId);
            w.Pod(uint8_t(g.hasLiquid ? 1 : 0));
            if (g.hasLiquid)
            {
                w.Pod(g.liquid.tilesX);
                w.Pod(g.liquid.tilesY);
                w.Pod(g.liquid.corner);
                w.Pod(g.liquid.entry);
                w.Pod(g.liquid.kind);
                w.Arr(g.liquid.heights);
                w.Arr(g.liquid.flags);
            }
        }

        bool ReadGroup(Cursor& c, WmoModel::Group& g)
        {
            uint8_t hasLiquid = 0;
            bool ok = c.Pod(g.mogpFlags) && c.Pod(g.groupWmoId) && c.Pod(hasLiquid);
            g.hasLiquid = hasLiquid != 0;
            if (ok && g.hasLiquid)
            {
                ok = c.Pod(g.liquid.tilesX) && c.Pod(g.liquid.tilesY) &&
                     c.Pod(g.liquid.corner) && c.Pod(g.liquid.entry) &&
                     c.Pod(g.liquid.kind) && c.Copy(g.liquid.heights) &&
                     c.Copy(g.liquid.flags);
            }
            return ok;
        }
    }

    std::string TileFileName(uint32_t mapId, int tx, int ty)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "t_%u_%d_%d.tile", mapId, tx, ty);
        return name;
    }

    std::string GlobalWmoFileName(uint32_t mapId)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "w_%u.tile", mapId);
        return name;
    }

    std::string GoModelFileName(uint32_t displayId)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "go_%u.tile", displayId);
        return name;
    }

    bool WriteTile(const TerrainTile& tile, const std::string& path)
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f)
        {
            return false;
        }

        Writer w(f);

        w.Pod(MAGIC);
        w.Pod(VERSION);
        w.Pod(tile.tx);
        w.Pod(tile.ty);
        w.Pod(uint8_t(tile.hasTerrain ? 1 : 0));
        w.Pod(uint8_t(tile.isGlobalWmo ? 1 : 0));
        w.Arr(tile.v9);
        w.Arr(tile.v8);
        w.Fixed(tile.holes);
        w.Fixed(tile.areaIds);
        w.Pod(uint8_t(tile.hasLiquid ? 1 : 0));
        w.Arr(tile.liquidHeight);
        w.Arr(tile.liquidShow);
        w.Arr(tile.liquidKind);
        w.Arr(tile.liquidEntry);
        w.Arr(tile.liquidDeep);

        // Deduped model table: a WMO instanced fifty times is written once and the
        // instances index it.
        std::unordered_map<const ICollisionModel*, uint32_t> modelIndex;
        std::vector<const ICollisionModel*> models;
        for (const StaticInstance& inst : tile.instances)
        {
            const ICollisionModel* m = inst.model.get();
            if (m && !modelIndex.count(m))
            {
                modelIndex[m] = uint32_t(models.size());
                models.push_back(m);
            }
        }

        w.Pod(uint32_t(models.size()));
        for (const ICollisionModel* m : models)
        {
            w.Pod(uint8_t(m->Kind()));
            if (m->Kind() == ModelKind::Wmo)
            {
                const auto* wmo = static_cast<const WmoModel*>(m);
                w.Pod(wmo->RootId());
                w.Pod(uint32_t(wmo->Groups().size()));
                for (const WmoModel::Group& g : wmo->Groups())
                {
                    WriteGroup(w, g);
                }
                // soup.tris is already in the BVH's leaf order, so nothing is rebuilt.
                w.Arr(wmo->Soup().verts);
                w.Arr(wmo->Soup().tris);
                w.Arr(wmo->TriGroups());
                w.Arr(wmo->GetBvh().Nodes());
            }
            else
            {
                const auto* c = static_cast<const CollisionModel*>(m);
                w.Arr(c->Soup().verts);
                w.Arr(c->Soup().tris);
                w.Arr(c->GetBvh().Nodes());
            }
        }

        w.Pod(uint32_t(tile.instances.size()));
        for (const StaticInstance& inst : tile.instances)
        {
            auto found = modelIndex.find(inst.model.get());
            const uint32_t idx = found != modelIndex.end() ? found->second : 0xFFFFFFFFu;
            w.Pod(inst.xf.pos);
            w.Fixed(inst.xf.rot.m);
            w.Pod(inst.xf.scale);
            w.Pod(inst.worldBounds.lo);
            w.Pod(inst.worldBounds.hi);
            w.Pod(idx);
            w.Pod(inst.adtId);
        }

        const bool ok = w.Ok();
        std::fclose(f);
        if (!ok)
        {
            std::remove(path.c_str());
        }
        return ok;
    }

    std::shared_ptr<TerrainTile> ReadTile(const std::string& path)
    {
        std::shared_ptr<const MappedFile> file = MappedFile::Open(path);
        if (!file)
        {
            return nullptr;
        }

        Cursor c(file);

        uint32_t magic = 0, version = 0;
        if (!c.Pod(magic) || !c.Pod(version) || magic != MAGIC || version != VERSION)
        {
            return nullptr;
        }

        auto tile = std::make_shared<TerrainTile>();
        tile->mapping = file;

        uint8_t hasTerrain = 0, globalWmo = 0, hasLiquid = 0;

        bool ok = c.Pod(tile->tx) && c.Pod(tile->ty) && c.Pod(hasTerrain) &&
                  c.Pod(globalWmo) && c.View(tile->v9) && c.View(tile->v8) &&
                  c.Fixed(tile->holes) && c.Fixed(tile->areaIds) &&
                  c.Pod(hasLiquid) && c.View(tile->liquidHeight) &&
                  c.View(tile->liquidShow) && c.View(tile->liquidKind) &&
                  c.View(tile->liquidEntry) && c.View(tile->liquidDeep);

        tile->hasTerrain = hasTerrain != 0;
        tile->isGlobalWmo = globalWmo != 0;
        tile->hasLiquid = hasLiquid != 0;

        uint32_t nModels = 0;
        ok = ok && c.Pod(nModels) && nModels <= MAX_MODELS;

        std::vector<std::shared_ptr<const ICollisionModel>> models;
        if (ok)
        {
            models.resize(nModels);
        }

        for (uint32_t i = 0; ok && i < nModels; ++i)
        {
            uint8_t kind = 0;
            if (!c.Pod(kind))
            {
                ok = false;
                break;
            }

            TriSoup soup;
            std::vector<Bvh::Node> nodes;

            if (kind == uint8_t(ModelKind::Wmo))
            {
                uint32_t rootId = 0, nGroups = 0;
                ok = c.Pod(rootId) && c.Pod(nGroups) && nGroups <= MAX_MODELS;
                std::vector<WmoModel::Group> groups(ok ? nGroups : 0);
                for (uint32_t g = 0; ok && g < nGroups; ++g)
                {
                    ok = ReadGroup(c, groups[g]);
                }

                std::vector<uint16_t> triGroup;
                ok = ok && c.Copy(soup.verts) && c.Copy(soup.tris) &&
                     c.Copy(triGroup) && c.Copy(nodes) &&
                     triGroup.size() == soup.tris.size();
                if (ok)
                {
                    Bvh bvh;
                    bvh.Adopt(std::move(nodes));
                    models[i] = std::make_shared<WmoModel>(std::move(soup),
                                                           std::move(triGroup),
                                                           std::move(groups), rootId,
                                                           std::move(bvh));
                }
            }
            else if (kind == uint8_t(ModelKind::Mesh))
            {
                ok = c.Copy(soup.verts) && c.Copy(soup.tris) && c.Copy(nodes);
                if (ok)
                {
                    Bvh bvh;
                    bvh.Adopt(std::move(nodes));
                    models[i] = std::make_shared<CollisionModel>(std::move(soup),
                                                                 std::move(bvh));
                }
            }
            else
            {
                ok = false;
            }
        }

        uint32_t nInstances = 0;
        ok = ok && c.Pod(nInstances) && nInstances <= MAX_INSTANCES;
        for (uint32_t i = 0; ok && i < nInstances; ++i)
        {
            StaticInstance inst;
            uint32_t idx = 0;
            ok = c.Pod(inst.xf.pos) && c.Fixed(inst.xf.rot.m) &&
                 c.Pod(inst.xf.scale) && c.Pod(inst.worldBounds.lo) &&
                 c.Pod(inst.worldBounds.hi) && c.Pod(idx) && c.Pod(inst.adtId);
            if (ok)
            {
                if (idx < models.size())
                {
                    inst.model = models[idx];
                }
                tile->instances.push_back(std::move(inst));
            }
        }

        return ok ? tile : nullptr;
    }
}
