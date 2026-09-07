#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace client
{
    /// A single MPQ archive, format 0 and 1 (everything WoW 1.12.1 ships).
    ///
    /// Files are streamed and decompressed straight into the caller's buffer, which is
    /// what the client does: Storm opens the archive with CreateFileA, seeks with
    /// SetFilePointer and reads with ReadFile (0x651CA0), then decompresses sectors in
    /// memory (0x656B40 -> 0x652620). Nothing is ever extracted.
    ///
    /// After Open() the archive is immutable, and Read() keeps its file handle on the
    /// stack, so any number of threads may read from one archive at once without a
    /// lock. Open() itself is not concurrent-safe and is not meant to be.
    class MpqArchive
    {
    public:
        bool Open(const std::string& path);

        bool Contains(const std::string& name) const;

        /// \a error, when given, receives the reason a read failed. Nothing is written
        /// to the archive itself, which is what makes concurrent reads safe.
        bool Read(const std::string& name, std::vector<std::uint8_t>* out,
                  std::string* error = nullptr) const;

        const std::string& Path() const { return m_path; }
        const std::string& LastError() const { return m_error; }

    private:
        struct HashEntry
        {
            std::uint32_t nameA;
            std::uint32_t nameB;
            std::uint16_t locale;
            std::uint16_t platform;
            std::uint32_t blockIndex;
        };

        struct BlockEntry
        {
            std::uint32_t filePos;
            std::uint32_t packedSize;
            std::uint32_t unpackedSize;
            std::uint32_t flags;
        };

        const HashEntry* Find(const std::string& name) const;
        static bool ReadAt(std::ifstream& file, std::uint64_t offset,
                           void* buffer, std::size_t size);

        std::string m_path;
        std::string m_error;       ///< only written by Open()

        std::uint64_t m_archiveOffset = 0;
        std::uint32_t m_sectorSize = 4096;

        std::vector<HashEntry> m_hashTable;
        std::vector<BlockEntry> m_blockTable;
    };
}
