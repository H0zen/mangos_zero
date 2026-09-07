#include "ClientArchive/PkwareExplode.hpp"

namespace client
{
    namespace
    {
        constexpr int kMaxBits = 13;

        struct Huffman
        {
            short count[kMaxBits + 1];
            short symbol[256];
        };

        struct Reader
        {
            const std::uint8_t* src;
            std::size_t size;
            std::size_t pos;
            std::uint32_t buffer;
            int count;
            bool error;

            int Get(int need)
            {
                std::uint32_t value = buffer;
                while (count < need)
                {
                    if (pos >= size)
                    {
                        error = true;
                        return 0;
                    }
                    value |= static_cast<std::uint32_t>(src[pos++]) << count;
                    count += 8;
                }
                buffer = value >> need;
                count -= need;
                return static_cast<int>(value & ((1u << need) - 1));
            }
        };

        /// The DCL tables are stored run-length encoded: high nibble + 1 is the repeat
        /// count, low nibble is the code length.
        bool Construct(Huffman& huff, const std::uint8_t* rep, int repCount)
        {
            short lengths[256];
            int symbols = 0;

            for (int i = 0; i < repCount; ++i)
            {
                int length = rep[i];
                int repeat = (length >> 4) + 1;
                length &= 15;
                while (repeat-- > 0)
                {
                    if (symbols >= 256)
                        return false;
                    lengths[symbols++] = static_cast<short>(length);
                }
            }

            for (int len = 0; len <= kMaxBits; ++len)
                huff.count[len] = 0;
            for (int i = 0; i < symbols; ++i)
            {
                if (lengths[i] > kMaxBits)
                    return false;
                ++huff.count[lengths[i]];
            }

            if (huff.count[0] == symbols)
                return true;

            int left = 1;
            for (int len = 1; len <= kMaxBits; ++len)
            {
                left <<= 1;
                left -= huff.count[len];
                if (left < 0)
                    return false;
            }

            short offsets[kMaxBits + 2];
            offsets[1] = 0;
            for (int len = 1; len <= kMaxBits; ++len)
                offsets[len + 1] = static_cast<short>(offsets[len] + huff.count[len]);

            for (int i = 0; i < symbols; ++i)
            {
                if (lengths[i] != 0)
                    huff.symbol[offsets[lengths[i]]++] = static_cast<short>(i);
            }

            return true;
        }

        /// Codes are stored inverted relative to DEFLATE, hence the `^ 1`.
        int Decode(Reader& reader, const Huffman& huff)
        {
            int code = 0;
            int first = 0;
            int index = 0;

            for (int len = 1; len <= kMaxBits; ++len)
            {
                code |= reader.Get(1) ^ 1;
                if (reader.error)
                    return -1;

                const int count = huff.count[len];
                if (code < first + count)
                    return huff.symbol[index + (code - first)];

                index += count;
                first += count;
                first <<= 1;
                code <<= 1;
            }

            reader.error = true;
            return -1;
        }

        const std::uint8_t kLitLen[] = {
            11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
            9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
            7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
            8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
            44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
            44, 173
        };

        const std::uint8_t kLenLen[] = { 2, 35, 36, 53, 38, 23 };
        const std::uint8_t kDistLen[] = { 2, 20, 53, 230, 247, 151, 248 };

        const short kLengthBase[16] = {
            3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264
        };
        const char kLengthExtra[16] = {
            0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8
        };

        /// The DCL tables are fixed, so they are built once inside a function-local
        /// static, whose initialisation the language guarantees to be thread-safe.
        /// A `static bool built` flag set after the fact is a data race instead.
        struct DclTables
        {
            Huffman literal;
            Huffman length;
            Huffman distance;
            bool valid = false;

            DclTables()
            {
                valid = Construct(literal, kLitLen, sizeof(kLitLen)) &&
                        Construct(length, kLenLen, sizeof(kLenLen)) &&
                        Construct(distance, kDistLen, sizeof(kDistLen));
            }
        };

        const DclTables& Tables()
        {
            static const DclTables tables;
            return tables;
        }
    }

    bool PkwareExplode(const std::uint8_t* src, std::size_t srcSize,
                 std::uint8_t* dst, std::size_t dstSize, std::size_t* produced)
    {
        if (!src || !dst || srcSize < 4)
            return false;

        const int literalsCoded = src[0];
        const int dictBits = src[1];

        if (literalsCoded != 0 && literalsCoded != 1)
            return false;
        if (dictBits < 4 || dictBits > 6)
            return false;

        const DclTables& tables = Tables();
        if (!tables.valid)
            return false;

        Reader reader;
        reader.src = src + 2;
        reader.size = srcSize - 2;
        reader.pos = 0;
        reader.buffer = 0;
        reader.count = 0;
        reader.error = false;

        std::size_t out = 0;

        for (;;)
        {
            const int isPair = reader.Get(1);
            if (reader.error)
                return false;

            if (isPair)
            {
                const int lengthSymbol = Decode(reader, tables.length);
                if (lengthSymbol < 0 || lengthSymbol >= 16)
                    return false;

                const int length = kLengthBase[lengthSymbol] +
                                   reader.Get(kLengthExtra[lengthSymbol]);
                if (reader.error)
                    return false;

                if (length == 519)
                    break;                      // end of stream

                const int lowBits = (length == 2) ? 2 : dictBits;
                const int distSymbol = Decode(reader, tables.distance);
                if (distSymbol < 0)
                    return false;

                int distance = (distSymbol << lowBits) + reader.Get(lowBits);
                if (reader.error)
                    return false;
                ++distance;

                if (static_cast<std::size_t>(distance) > out)
                    return false;
                if (out + static_cast<std::size_t>(length) > dstSize)
                    return false;

                std::size_t from = out - static_cast<std::size_t>(distance);
                for (int i = 0; i < length; ++i)
                    dst[out++] = dst[from++];
            }
            else
            {
                const int symbol = literalsCoded ? Decode(reader, tables.literal) : reader.Get(8);
                if (symbol < 0 || reader.error)
                    return false;
                if (out >= dstSize)
                    return false;
                dst[out++] = static_cast<std::uint8_t>(symbol);
            }
        }

        if (produced)
            *produced = out;
        return true;
    }
}
