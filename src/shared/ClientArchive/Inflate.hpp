#pragma once

#include <cstddef>
#include <cstdint>

namespace client
{
    /// Raw DEFLATE (RFC 1951) into a caller-provided buffer.
    /// Returns false on malformed input or if the output would overflow \a dstSize.
    bool InflateRaw(const std::uint8_t* src, std::size_t srcSize,
                    std::uint8_t* dst, std::size_t dstSize, std::size_t* produced);

    /// zlib stream (RFC 1950): validates the two-byte header, then inflates.
    /// The trailing Adler-32 is not checked - MPQ sectors carry their own CRC.
    bool InflateZlib(const std::uint8_t* src, std::size_t srcSize,
                     std::uint8_t* dst, std::size_t dstSize, std::size_t* produced);
}
