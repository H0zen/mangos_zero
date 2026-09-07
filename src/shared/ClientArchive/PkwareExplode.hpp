#pragma once

#include <cstddef>
#include <cstdint>

namespace client
{
    /// PKWARE Data Compression Library "implode" stream, the 0x08 compression MPQ
    /// archives use for older files. Returns false on malformed input or overflow.
    bool PkwareExplode(const std::uint8_t* src, std::size_t srcSize,
                 std::uint8_t* dst, std::size_t dstSize, std::size_t* produced);
}
