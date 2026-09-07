#include "ClientArchive/Inflate.hpp"

#include <cstring>

namespace client
{
    namespace
    {
        constexpr int kMaxBits = 15;
        constexpr int kMaxLitCodes = 288;
        constexpr int kMaxDistCodes = 30;
        constexpr int kFixedLitCodes = 288;

        /// How many stream bits the lookup table resolves in one step; anything longer
        /// falls back to walking the canonical code a bit at a time. Measured on the
        /// 1.12.1 archives: 9 bits gives 183 MB/s, 10 gives 191, 11 gives 187 - past
        /// ten the table costs more to build than the extra coverage returns.
        constexpr int kFastBits = 10;
        constexpr unsigned kFastSize = 1u << kFastBits;

        /// Canonical Huffman table: count[n] symbols of length n, symbols in order,
        /// plus a direct lookup for the short codes.
        struct Huffman
        {
            short count[kMaxBits + 1];
            short symbol[kMaxLitCodes];

            /// (length << 16) | symbol, or 0 when the code needs the slow walk.
            std::uint32_t fast[kFastSize];
        };

        unsigned ReverseBits(unsigned code, int length)
        {
            unsigned reversed = 0;
            for (int i = 0; i < length; ++i)
            {
                reversed = (reversed << 1) | (code & 1u);
                code >>= 1;
            }
            return reversed;
        }

        /// Fills the direct lookup. DEFLATE writes a code most significant bit first
        /// but the stream is read least significant bit first, so each code is
        /// reversed and then written into every slot that shares those low bits.
        void BuildFastTable(Huffman& huff)
        {
            for (unsigned i = 0; i < kFastSize; ++i)
                huff.fast[i] = 0;

            unsigned first = 0;
            int index = 0;

            for (int len = 1; len <= kMaxBits; ++len)
            {
                const int count = huff.count[len];
                if (len <= kFastBits)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        const unsigned reversed = ReverseBits(first + static_cast<unsigned>(i), len);
                        const std::uint32_t entry =
                            (static_cast<std::uint32_t>(len) << 16) |
                            static_cast<std::uint32_t>(
                                static_cast<unsigned short>(huff.symbol[index + i]));

                        for (unsigned slot = reversed; slot < kFastSize; slot += (1u << len))
                            huff.fast[slot] = entry;
                    }
                }

                index += count;
                first = (first + static_cast<unsigned>(count)) << 1;
            }
        }

        struct State
        {
            const std::uint8_t* src;
            std::size_t srcSize;
            std::size_t srcPos;
            std::uint32_t bitBuffer;
            int bitCount;

            std::uint8_t* dst;
            std::size_t dstSize;
            std::size_t dstPos;

            bool error;
        };

        int Bits(State& s, int need)
        {
            std::uint32_t value = s.bitBuffer;
            while (s.bitCount < need)
            {
                if (s.srcPos >= s.srcSize)
                {
                    s.error = true;
                    return 0;
                }
                value |= static_cast<std::uint32_t>(s.src[s.srcPos++]) << s.bitCount;
                s.bitCount += 8;
            }
            s.bitBuffer = value >> need;
            s.bitCount -= need;
            return static_cast<int>(value & ((1u << need) - 1));
        }

        int DecodeSlow(State& s, const Huffman& huff)
        {
            int code = 0;
            int first = 0;
            int index = 0;

            for (int len = 1; len <= kMaxBits; ++len)
            {
                code |= Bits(s, 1);
                if (s.error)
                    return -1;

                const int count = huff.count[len];
                if (code - count < first)
                    return huff.symbol[index + (code - first)];

                index += count;
                first += count;
                first <<= 1;
                code <<= 1;
            }

            s.error = true;
            return -1;
        }

        int Decode(State& s, const Huffman& huff)
        {
            // Top up the bit buffer so a whole short code can be read in one look.
            // Near the end of the stream there may not be enough bytes left, and then
            // the slow walk - which knows how to stop - takes over.
            while (s.bitCount < kFastBits && s.srcPos < s.srcSize)
            {
                s.bitBuffer |= static_cast<std::uint32_t>(s.src[s.srcPos++]) << s.bitCount;
                s.bitCount += 8;
            }

            if (s.bitCount >= kFastBits)
            {
                const std::uint32_t entry = huff.fast[s.bitBuffer & (kFastSize - 1)];
                if (entry != 0)
                {
                    const int length = static_cast<int>(entry >> 16);
                    s.bitBuffer >>= length;
                    s.bitCount -= length;
                    return static_cast<int>(entry & 0xFFFFu);
                }
            }

            return DecodeSlow(s, huff);
        }

        bool BuildHuffman(Huffman& huff, const short* lengths, int count)
        {
            for (int len = 0; len <= kMaxBits; ++len)
                huff.count[len] = 0;
            for (int symbol = 0; symbol < count; ++symbol)
                ++huff.count[lengths[symbol]];

            if (huff.count[0] == count)
            {
                BuildFastTable(huff);   // empty, but never left holding stale entries
                return true;            // no codes at all, still a valid (unused) table
            }

            int left = 1;
            for (int len = 1; len <= kMaxBits; ++len)
            {
                left <<= 1;
                left -= huff.count[len];
                if (left < 0)
                    return false;   // over-subscribed
            }

            short offsets[kMaxBits + 1];
            offsets[1] = 0;
            for (int len = 1; len < kMaxBits; ++len)
                offsets[len + 1] = static_cast<short>(offsets[len] + huff.count[len]);

            for (int symbol = 0; symbol < count; ++symbol)
            {
                if (lengths[symbol] != 0)
                    huff.symbol[offsets[lengths[symbol]]++] = static_cast<short>(symbol);
            }

            BuildFastTable(huff);
            return true;
        }

        bool Stored(State& s)
        {
            s.bitBuffer = 0;
            s.bitCount = 0;

            if (s.srcPos + 4 > s.srcSize)
                return false;

            const unsigned length = static_cast<unsigned>(s.src[s.srcPos]) |
                                    (static_cast<unsigned>(s.src[s.srcPos + 1]) << 8);
            const unsigned check = static_cast<unsigned>(s.src[s.srcPos + 2]) |
                                   (static_cast<unsigned>(s.src[s.srcPos + 3]) << 8);
            s.srcPos += 4;

            if ((length ^ 0xFFFFu) != check)
                return false;
            if (s.srcPos + length > s.srcSize)
                return false;
            if (s.dstPos + length > s.dstSize)
                return false;

            std::memcpy(s.dst + s.dstPos, s.src + s.srcPos, length);
            s.srcPos += length;
            s.dstPos += length;
            return true;
        }

        const short kLengthBase[29] = {
            3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
            35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
        };
        const short kLengthExtra[29] = {
            0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
            3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
        };
        const short kDistBase[30] = {
            1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
            257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
        };
        const short kDistExtra[30] = {
            0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
            7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
        };

        bool Codes(State& s, const Huffman& lit, const Huffman& dist)
        {
            for (;;)
            {
                const int symbol = Decode(s, lit);
                if (symbol < 0)
                    return false;

                if (symbol < 256)
                {
                    if (s.dstPos >= s.dstSize)
                        return false;
                    s.dst[s.dstPos++] = static_cast<std::uint8_t>(symbol);
                    continue;
                }

                if (symbol == 256)
                    return true;

                const int lengthIndex = symbol - 257;
                if (lengthIndex >= 29)
                    return false;

                const int length = kLengthBase[lengthIndex] + Bits(s, kLengthExtra[lengthIndex]);

                const int distSymbol = Decode(s, dist);
                if (distSymbol < 0 || distSymbol >= kMaxDistCodes)
                    return false;

                const int distance = kDistBase[distSymbol] + Bits(s, kDistExtra[distSymbol]);
                if (s.error)
                    return false;
                if (static_cast<std::size_t>(distance) > s.dstPos)
                    return false;
                if (s.dstPos + static_cast<std::size_t>(length) > s.dstSize)
                    return false;

                std::size_t from = s.dstPos - static_cast<std::size_t>(distance);
                for (int i = 0; i < length; ++i)
                    s.dst[s.dstPos++] = s.dst[from++];
            }
        }

        /// The fixed-block tables never change, so they are built once inside a
        /// function-local static: C++ guarantees that initialisation is thread-safe,
        /// where a `static bool built` flag set after the fact is a data race.
        struct FixedTables
        {
            Huffman lit;
            Huffman dist;

            FixedTables()
            {
                short lengths[kFixedLitCodes];
                int symbol = 0;
                for (; symbol < 144; ++symbol) lengths[symbol] = 8;
                for (; symbol < 256; ++symbol) lengths[symbol] = 9;
                for (; symbol < 280; ++symbol) lengths[symbol] = 7;
                for (; symbol < 288; ++symbol) lengths[symbol] = 8;
                BuildHuffman(lit, lengths, kFixedLitCodes);

                for (symbol = 0; symbol < kMaxDistCodes; ++symbol)
                    lengths[symbol] = 5;
                BuildHuffman(dist, lengths, kMaxDistCodes);
            }
        };

        bool Fixed(State& s)
        {
            static const FixedTables tables;
            return Codes(s, tables.lit, tables.dist);
        }

        bool Dynamic(State& s)
        {
            static const short kOrder[19] = {
                16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
            };

            const int litCount = Bits(s, 5) + 257;
            const int distCount = Bits(s, 5) + 1;
            const int codeCount = Bits(s, 4) + 4;
            if (s.error || litCount > kMaxLitCodes || distCount > kMaxDistCodes)
                return false;

            short lengths[kMaxLitCodes + kMaxDistCodes];
            std::memset(lengths, 0, sizeof(lengths));

            for (int i = 0; i < codeCount; ++i)
                lengths[kOrder[i]] = static_cast<short>(Bits(s, 3));
            if (s.error)
                return false;

            Huffman lengthTable;
            if (!BuildHuffman(lengthTable, lengths, 19))
                return false;

            int index = 0;
            while (index < litCount + distCount)
            {
                const int symbol = Decode(s, lengthTable);
                if (symbol < 0)
                    return false;

                if (symbol < 16)
                {
                    lengths[index++] = static_cast<short>(symbol);
                    continue;
                }

                short value = 0;
                int repeat = 0;
                if (symbol == 16)
                {
                    if (index == 0)
                        return false;
                    value = lengths[index - 1];
                    repeat = 3 + Bits(s, 2);
                }
                else if (symbol == 17)
                {
                    repeat = 3 + Bits(s, 3);
                }
                else
                {
                    repeat = 11 + Bits(s, 7);
                }

                if (s.error || index + repeat > litCount + distCount)
                    return false;

                while (repeat-- > 0)
                    lengths[index++] = value;
            }

            if (lengths[256] == 0)
                return false;   // no end-of-block code

            Huffman lit;
            Huffman dist;
            if (!BuildHuffman(lit, lengths, litCount))
                return false;
            if (!BuildHuffman(dist, lengths + litCount, distCount))
                return false;

            return Codes(s, lit, dist);
        }
    }

    bool InflateRaw(const std::uint8_t* src, std::size_t srcSize,
                    std::uint8_t* dst, std::size_t dstSize, std::size_t* produced)
    {
        if (!src || !dst)
            return false;

        State s;
        s.src = src;
        s.srcSize = srcSize;
        s.srcPos = 0;
        s.bitBuffer = 0;
        s.bitCount = 0;
        s.dst = dst;
        s.dstSize = dstSize;
        s.dstPos = 0;
        s.error = false;

        int last = 0;
        do
        {
            last = Bits(s, 1);
            const int type = Bits(s, 2);
            if (s.error)
                return false;

            bool ok = false;
            switch (type)
            {
                case 0: ok = Stored(s); break;
                case 1: ok = Fixed(s); break;
                case 2: ok = Dynamic(s); break;
                default: ok = false; break;
            }

            if (!ok || s.error)
                return false;
        }
        while (!last);

        if (produced)
            *produced = s.dstPos;
        return true;
    }

    bool InflateZlib(const std::uint8_t* src, std::size_t srcSize,
                     std::uint8_t* dst, std::size_t dstSize, std::size_t* produced)
    {
        if (srcSize < 2)
            return false;

        const unsigned cmf = src[0];
        const unsigned flg = src[1];
        if ((cmf & 0x0F) != 8)
            return false;                       // not deflate
        if (((cmf << 8) + flg) % 31 != 0)
            return false;                       // bad header check
        if (flg & 0x20)
            return false;                       // preset dictionary, never used by MPQ

        return InflateRaw(src + 2, srcSize - 2, dst, dstSize, produced);
    }
}
