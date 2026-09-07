#include "ClientArchive/MpqArchive.hpp"

#include "ClientArchive/PkwareExplode.hpp"
#include "ClientArchive/Inflate.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>

namespace client
{
    namespace
    {
        constexpr std::uint32_t kSignature = 0x1A51504D;   // 'MPQ\x1A'

        constexpr std::uint32_t kFlagImplode    = 0x00000100u;
        constexpr std::uint32_t kFlagCompress   = 0x00000200u;
        constexpr std::uint32_t kFlagEncrypted  = 0x00010000u;
        constexpr std::uint32_t kFlagFixKey     = 0x00020000u;
        constexpr std::uint32_t kFlagSingleUnit = 0x01000000u;
        constexpr std::uint32_t kFlagDeleted    = 0x02000000u;
        constexpr std::uint32_t kFlagSectorCrc  = 0x04000000u;
        constexpr std::uint32_t kFlagExists     = 0x80000000u;

        constexpr std::uint32_t kHashTableIndex = 0;
        constexpr std::uint32_t kHashNameA = 1;
        constexpr std::uint32_t kHashNameB = 2;
        constexpr std::uint32_t kHashFileKey = 3;

        constexpr std::uint32_t kHashEntryEmpty = 0xFFFFFFFFu;
        constexpr std::uint32_t kHashEntryDeleted = 0xFFFFFFFEu;

        std::uint32_t g_cryptTable[0x500];

        void PrepareCryptTable()
        {
            std::uint32_t seed = 0x00100001u;
            for (std::uint32_t index1 = 0; index1 < 0x100; ++index1)
            {
                std::uint32_t index2 = index1;
                for (int i = 0; i < 5; ++i, index2 += 0x100)
                {
                    seed = (seed * 125 + 3) % 0x2AAAABu;
                    const std::uint32_t high = (seed & 0xFFFFu) << 16;
                    seed = (seed * 125 + 3) % 0x2AAAABu;
                    const std::uint32_t low = seed & 0xFFFFu;
                    g_cryptTable[index2] = high | low;
                }
            }
        }

        void EnsureCryptTable()
        {
            static std::once_flag once;
            std::call_once(once, PrepareCryptTable);
        }

        std::uint32_t HashString(const std::string& text, std::uint32_t type)
        {
            EnsureCryptTable();

            std::uint32_t seed1 = 0x7FED7FEDu;
            std::uint32_t seed2 = 0xEEEEEEEEu;

            for (char raw : text)
            {
                int ch = static_cast<unsigned char>(raw);
                if (ch == '/')
                    ch = '\\';
                ch = std::toupper(ch);

                seed1 = g_cryptTable[(type << 8) + static_cast<std::uint32_t>(ch)] ^ (seed1 + seed2);
                seed2 = static_cast<std::uint32_t>(ch) + seed1 + seed2 + (seed2 << 5) + 3;
            }

            return seed1;
        }

        void DecryptBlock(std::uint32_t* data, std::size_t dwordCount, std::uint32_t key)
        {
            EnsureCryptTable();

            std::uint32_t seed = 0xEEEEEEEEu;
            for (std::size_t i = 0; i < dwordCount; ++i)
            {
                seed += g_cryptTable[0x400 + (key & 0xFF)];
                const std::uint32_t value = data[i] ^ (key + seed);
                key = ((~key << 0x15) + 0x11111111u) | (key >> 0x0B);
                seed = value + seed + (seed << 5) + 3;
                data[i] = value;
            }
        }

        std::uint32_t ReadU32(const std::uint8_t* p)
        {
            return static_cast<std::uint32_t>(p[0]) |
                   (static_cast<std::uint32_t>(p[1]) << 8) |
                   (static_cast<std::uint32_t>(p[2]) << 16) |
                   (static_cast<std::uint32_t>(p[3]) << 24);
        }

        std::uint16_t ReadU16(const std::uint8_t* p)
        {
            return static_cast<std::uint16_t>(static_cast<std::uint32_t>(p[0]) |
                                              (static_cast<std::uint32_t>(p[1]) << 8));
        }

        std::string BaseName(const std::string& path)
        {
            const std::size_t slash = path.find_last_of("\\/");
            return slash == std::string::npos ? path : path.substr(slash + 1);
        }

        /// One sector or single-unit block: strip the compression mask and expand.
        bool Decompress(const std::uint8_t* src, std::size_t srcSize,
                        std::uint8_t* dst, std::size_t dstSize,
                        bool implodeOnly, std::string* error)
        {
            if (srcSize == 0)
                return false;

            std::size_t produced = 0;

            if (implodeOnly)
            {
                if (!PkwareExplode(src, srcSize, dst, dstSize, &produced))
                {
                    *error = "pkware explode failed";
                    return false;
                }
                return produced == dstSize;
            }

            const std::uint8_t mask = src[0];
            const std::uint8_t* payload = src + 1;
            const std::size_t payloadSize = srcSize - 1;

            switch (mask)
            {
                case 0x02:
                    if (!InflateZlib(payload, payloadSize, dst, dstSize, &produced))
                    {
                        *error = "zlib inflate failed";
                        return false;
                    }
                    break;

                case 0x08:
                    if (!PkwareExplode(payload, payloadSize, dst, dstSize, &produced))
                    {
                        *error = "pkware explode failed";
                        return false;
                    }
                    break;

                default:
                    *error = "unsupported MPQ compression mask 0x" +
                             std::to_string(static_cast<int>(mask));
                    return false;
            }

            return produced == dstSize;
        }
    }

    bool MpqArchive::ReadAt(std::ifstream& file, std::uint64_t offset,
                            void* buffer, std::size_t size)
    {
        if (size == 0)
            return true;
        file.clear();
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!file.good())
            return false;
        file.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return static_cast<std::size_t>(file.gcount()) == size;
    }

    bool MpqArchive::Open(const std::string& path)
    {
        m_path = path;
        m_error.clear();

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            m_error = "cannot open " + path;
            return false;
        }

        // The header is not always at offset 0; Storm scans in 512-byte steps.
        std::uint8_t header[44];
        bool found = false;
        for (std::uint64_t probe = 0; probe < 0x100000; probe += 0x200)
        {
            if (!ReadAt(file, probe, header, sizeof(header)))
                break;
            if (ReadU32(header) == kSignature)
            {
                m_archiveOffset = probe;
                found = true;
                break;
            }
        }

        if (!found)
        {
            m_error = "no MPQ header in " + path;
            return false;
        }

        const std::uint16_t formatVersion = ReadU16(header + 0x0C);
        const std::uint16_t sectorShift = ReadU16(header + 0x0E);
        const std::uint32_t hashTablePos = ReadU32(header + 0x10);
        const std::uint32_t blockTablePos = ReadU32(header + 0x14);
        const std::uint32_t hashTableSize = ReadU32(header + 0x18);
        const std::uint32_t blockTableSize = ReadU32(header + 0x1C);

        if (formatVersion > 1)
        {
            m_error = "unsupported MPQ format version " + std::to_string(formatVersion);
            return false;
        }

        m_sectorSize = 512u << sectorShift;

        if (hashTableSize == 0 || (hashTableSize & (hashTableSize - 1)) != 0)
        {
            m_error = "hash table size is not a power of two";
            return false;
        }

        std::vector<std::uint32_t> raw(static_cast<std::size_t>(hashTableSize) * 4);
        if (!ReadAt(file, m_archiveOffset + hashTablePos, raw.data(), raw.size() * 4))
        {
            m_error = "truncated hash table";
            return false;
        }
        DecryptBlock(raw.data(), raw.size(), HashString("(hash table)", kHashFileKey));

        m_hashTable.resize(hashTableSize);
        for (std::uint32_t i = 0; i < hashTableSize; ++i)
        {
            HashEntry& entry = m_hashTable[i];
            entry.nameA = raw[4 * i + 0];
            entry.nameB = raw[4 * i + 1];
            entry.locale = static_cast<std::uint16_t>(raw[4 * i + 2] & 0xFFFFu);
            entry.platform = static_cast<std::uint16_t>(raw[4 * i + 2] >> 16);
            entry.blockIndex = raw[4 * i + 3];
        }

        raw.assign(static_cast<std::size_t>(blockTableSize) * 4, 0);
        if (!ReadAt(file, m_archiveOffset + blockTablePos, raw.data(), raw.size() * 4))
        {
            m_error = "truncated block table";
            return false;
        }
        DecryptBlock(raw.data(), raw.size(), HashString("(block table)", kHashFileKey));

        m_blockTable.resize(blockTableSize);
        for (std::uint32_t i = 0; i < blockTableSize; ++i)
        {
            BlockEntry& entry = m_blockTable[i];
            entry.filePos = raw[4 * i + 0];
            entry.packedSize = raw[4 * i + 1];
            entry.unpackedSize = raw[4 * i + 2];
            entry.flags = raw[4 * i + 3];
        }

        return true;
    }

    const MpqArchive::HashEntry* MpqArchive::Find(const std::string& name) const
    {
        if (m_hashTable.empty())
            return nullptr;

        const std::uint32_t mask = static_cast<std::uint32_t>(m_hashTable.size()) - 1;
        const std::uint32_t start = HashString(name, kHashTableIndex) & mask;
        const std::uint32_t nameA = HashString(name, kHashNameA);
        const std::uint32_t nameB = HashString(name, kHashNameB);

        const HashEntry* fallback = nullptr;

        std::uint32_t index = start;
        do
        {
            const HashEntry& entry = m_hashTable[index];
            if (entry.blockIndex == kHashEntryEmpty)
                break;

            if (entry.blockIndex != kHashEntryDeleted &&
                entry.nameA == nameA && entry.nameB == nameB)
            {
                if (entry.locale == 0)
                    return &entry;
                if (!fallback)
                    fallback = &entry;
            }

            index = (index + 1) & mask;
        }
        while (index != start);

        return fallback;
    }

    bool MpqArchive::Contains(const std::string& name) const
    {
        const HashEntry* entry = Find(name);
        if (!entry || entry->blockIndex >= m_blockTable.size())
            return false;

        const BlockEntry& block = m_blockTable[entry->blockIndex];
        return (block.flags & kFlagExists) != 0 && (block.flags & kFlagDeleted) == 0;
    }

    bool MpqArchive::Read(const std::string& name, std::vector<std::uint8_t>* out,
                          std::string* error) const
    {
        std::string ignored;
        std::string& fail = error ? *error : ignored;

        if (!out)
            return false;

        const HashEntry* entry = Find(name);
        if (!entry || entry->blockIndex >= m_blockTable.size())
        {
            fail = "not found: " + name;
            return false;
        }

        const BlockEntry& block = m_blockTable[entry->blockIndex];
        if ((block.flags & kFlagExists) == 0 || (block.flags & kFlagDeleted) != 0)
        {
            fail = "not present: " + name;
            return false;
        }

        // A handle of our own, so any number of threads may read at once.
        std::ifstream file(m_path, std::ios::binary);
        if (!file.is_open())
        {
            fail = "cannot open " + m_path;
            return false;
        }

        const std::uint64_t base = m_archiveOffset + block.filePos;
        const std::size_t fileSize = block.unpackedSize;
        out->assign(fileSize, 0);
        if (fileSize == 0)
            return true;

        std::uint32_t key = 0;
        if (block.flags & kFlagEncrypted)
        {
            key = HashString(BaseName(name), kHashFileKey);
            if (block.flags & kFlagFixKey)
                key = (key + block.filePos) ^ block.unpackedSize;
        }

        const bool compressed = (block.flags & kFlagCompress) != 0;
        const bool imploded = (block.flags & kFlagImplode) != 0;

        if (block.flags & kFlagSingleUnit)
        {
            std::vector<std::uint8_t> packed(block.packedSize);
            if (!ReadAt(file, base, packed.data(), packed.size()))
            {
                fail = "truncated file data: " + name;
                return false;
            }

            if (block.flags & kFlagEncrypted)
                DecryptBlock(reinterpret_cast<std::uint32_t*>(packed.data()),
                             packed.size() / 4, key);

            if (block.packedSize == block.unpackedSize)
            {
                std::memcpy(out->data(), packed.data(), fileSize);
                return true;
            }

            if (!Decompress(packed.data(), packed.size(), out->data(), fileSize,
                            imploded && !compressed, &fail))
            {
                fail += " (" + name + ")";
                return false;
            }
            return true;
        }

        const std::size_t sectorCount = (fileSize + m_sectorSize - 1) / m_sectorSize;

        std::vector<std::uint32_t> offsets;
        if (compressed || imploded)
        {
            std::size_t entries = sectorCount + 1;
            if (block.flags & kFlagSectorCrc)
                ++entries;

            offsets.assign(entries, 0);
            if (!ReadAt(file, base, offsets.data(), offsets.size() * 4))
            {
                fail = "truncated sector table: " + name;
                return false;
            }

            if (block.flags & kFlagEncrypted)
                DecryptBlock(offsets.data(), offsets.size(), key - 1);
        }
        else
        {
            offsets.assign(sectorCount + 1, 0);
            for (std::size_t i = 0; i <= sectorCount; ++i)
            {
                const std::size_t position = i * m_sectorSize;
                offsets[i] = static_cast<std::uint32_t>(position < fileSize ? position : fileSize);
            }
        }

        std::vector<std::uint8_t> sector;

        for (std::size_t i = 0; i < sectorCount; ++i)
        {
            if (offsets[i + 1] < offsets[i])
            {
                fail = "corrupt sector table: " + name;
                return false;
            }

            const std::size_t packedSize = offsets[i + 1] - offsets[i];
            const std::size_t plainSize = std::min<std::size_t>(m_sectorSize,
                                                                fileSize - i * m_sectorSize);
            if (packedSize == 0)
            {
                fail = "empty sector: " + name;
                return false;
            }

            sector.assign(packedSize, 0);
            if (!ReadAt(file, base + offsets[i], sector.data(), packedSize))
            {
                fail = "truncated sector: " + name;
                return false;
            }

            if (block.flags & kFlagEncrypted)
                DecryptBlock(reinterpret_cast<std::uint32_t*>(sector.data()),
                             packedSize / 4, key + static_cast<std::uint32_t>(i));

            std::uint8_t* destination = out->data() + i * m_sectorSize;

            if (packedSize >= plainSize)
            {
                std::memcpy(destination, sector.data(), plainSize);
                continue;
            }

            if (!Decompress(sector.data(), packedSize, destination, plainSize,
                            imploded && !compressed, &fail))
            {
                fail += " (" + name + " sector " + std::to_string(i) + ")";
                return false;
            }
        }

        return true;
    }
}
