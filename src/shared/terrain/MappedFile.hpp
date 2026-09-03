#pragma once

// A read-only view of a whole file, mapped rather than read.
//
// Tiles are a same-machine cache written by the extractor and never modified at
// runtime, which is exactly the shape a mapping wants: the pages are clean, so
// the kernel can drop and refetch them under pressure without the server owning
// a byte of it. That is what lets a tile stay resident for the life of the map
// instead of being parsed, counted against RSS, and swept out again.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace world::terrain
{
    class MappedFile
    {
        public:

            /// Maps the whole file. Nothing on any failure, including an empty
            /// file: a zero-length mapping is not a thing every platform allows,
            /// and a caller that got one would have to special-case it anyway.
            static std::shared_ptr<const MappedFile> Open(const std::string& path);

            ~MappedFile();

            MappedFile(const MappedFile&) = delete;
            MappedFile& operator=(const MappedFile&) = delete;

            const uint8_t* Data() const { return m_data; }
            size_t Size() const { return m_size; }

            /// True when [offset, offset + bytes) lies inside the mapping. Every
            /// section a reader points at must pass this, or a truncated or
            /// corrupt file turns into reads past the end of the mapping.
            bool Covers(size_t offset, size_t bytes) const
            {
                return offset <= m_size && bytes <= m_size - offset;
            }

        private:

            MappedFile() = default;

            const uint8_t* m_data = nullptr;
            size_t m_size = 0;

            // Platform handles, kept opaque so this header stays free of
            // <windows.h>. On POSIX only the first is used, as an fd.
            void* m_handle = nullptr;
            void* m_mapping = nullptr;
    };
}
