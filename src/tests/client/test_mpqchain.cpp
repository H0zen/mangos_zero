// A patch takes a file away by carrying an entry over its name -- zero bytes,
// flagged deleted -- rather than by touching the archive that shipped the file.
// Four DBCs are gone from 1.12.1 exactly this way, so a chain that reads past a
// deletion hands back files the client itself refuses to open.
//
// The archives here are built byte by byte: a header, an encrypted hash table and
// an encrypted block table are all it takes to make a name resolvable, and that is
// the whole of what the case is about.

#include "doctest.h"

#include "ClientArchive/MpqChain.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    constexpr std::uint32_t kFlagExists = 0x80000000u;
    constexpr std::uint32_t kFlagDeleted = 0x02000000u;
    constexpr std::uint32_t kFlagSingleUnit = 0x01000000u;

    std::uint32_t g_crypt[0x500];

    void PrepareCrypt()
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
                g_crypt[index2] = high | (seed & 0xFFFFu);
            }
        }
    }

    std::uint32_t Hash(const std::string& text, std::uint32_t type)
    {
        std::uint32_t seed1 = 0x7FED7FEDu;
        std::uint32_t seed2 = 0xEEEEEEEEu;

        for (char raw : text)
        {
            int ch = static_cast<unsigned char>(raw);
            if (ch == '/')
            {
                ch = '\\';
            }
            ch = std::toupper(ch);

            seed1 = g_crypt[(type << 8) + static_cast<std::uint32_t>(ch)] ^ (seed1 + seed2);
            seed2 = static_cast<std::uint32_t>(ch) + seed1 + seed2 + (seed2 << 5) + 3;
        }

        return seed1;
    }

    /// The inverse of what the reader does: the running seed follows the plain word.
    void Encrypt(std::vector<std::uint32_t>& data, std::uint32_t key)
    {
        std::uint32_t seed = 0xEEEEEEEEu;
        for (std::uint32_t& word : data)
        {
            seed += g_crypt[0x400 + (key & 0xFF)];
            const std::uint32_t plain = word;
            word = plain ^ (key + seed);
            key = ((~key << 0x15) + 0x11111111u) | (key >> 0x0B);
            seed = plain + seed + (seed << 5) + 3;
        }
    }

    void PutU32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value)
    {
        bytes[at + 0] = static_cast<std::uint8_t>(value & 0xFF);
        bytes[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        bytes[at + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        bytes[at + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }

    void AppendWords(std::vector<std::uint8_t>& bytes, const std::vector<std::uint32_t>& words)
    {
        for (std::uint32_t word : words)
        {
            const std::size_t at = bytes.size();
            bytes.resize(at + 4);
            PutU32(bytes, at, word);
        }
    }

    /// One archive holding one name, either as content or as a deletion.
    void WriteArchive(const std::filesystem::path& path, const std::string& name,
                      const std::string& content, bool deleted)
    {
        PrepareCrypt();

        constexpr std::uint32_t kHeaderSize = 32;
        constexpr std::uint32_t kHashSize = 4;

        std::vector<std::uint8_t> bytes(kHeaderSize, 0);
        const std::uint32_t filePos = static_cast<std::uint32_t>(bytes.size());
        bytes.insert(bytes.end(), content.begin(), content.end());

        const std::uint32_t hashPos = static_cast<std::uint32_t>(bytes.size());

        std::vector<std::uint32_t> hash(kHashSize * 4, 0xFFFFFFFFu);
        for (std::uint32_t i = 0; i < kHashSize; ++i)
        {
            hash[4 * i + 2] = 0xFFFFFFFFu;
        }

        const std::uint32_t slot = Hash(name, 0) & (kHashSize - 1);
        hash[4 * slot + 0] = Hash(name, 1);
        hash[4 * slot + 1] = Hash(name, 2);
        hash[4 * slot + 2] = 0;
        hash[4 * slot + 3] = 0;

        std::vector<std::uint32_t> block(4, 0);
        block[0] = filePos;
        block[1] = deleted ? 0 : static_cast<std::uint32_t>(content.size());
        block[2] = deleted ? 0 : static_cast<std::uint32_t>(content.size());
        block[3] = kFlagExists | (deleted ? kFlagDeleted : kFlagSingleUnit);

        Encrypt(hash, Hash("(hash table)", 3));
        AppendWords(bytes, hash);

        const std::uint32_t blockPos = static_cast<std::uint32_t>(bytes.size());
        Encrypt(block, Hash("(block table)", 3));
        AppendWords(bytes, block);

        bytes[0] = 'M';
        bytes[1] = 'P';
        bytes[2] = 'Q';
        bytes[3] = 0x1A;
        PutU32(bytes, 0x04, kHeaderSize);
        PutU32(bytes, 0x08, static_cast<std::uint32_t>(bytes.size()));
        PutU32(bytes, 0x0C, 0);
        PutU32(bytes, 0x10, hashPos);
        PutU32(bytes, 0x14, blockPos);
        PutU32(bytes, 0x18, kHashSize);
        PutU32(bytes, 0x1C, 1);

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    struct Archives
    {
        Archives()
        {
            dir = std::filesystem::temp_directory_path() / "mangos_mpqchain_test";
            std::filesystem::remove_all(dir);
            std::filesystem::create_directories(dir);
        }

        ~Archives()
        {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }

        std::filesystem::path dir;
    };

    const std::string kName = "DBFilesClient\\Sample.dbc";
    const std::string kContent = "the row the base archive shipped";
}

TEST_CASE("a name resolves to the archive that holds it")
{
    Archives temp;
    WriteArchive(temp.dir / "base.mpq", kName, kContent, false);

    client::MpqChain chain;
    REQUIRE(chain.AddArchive((temp.dir / "base.mpq").string()));

    std::vector<std::uint8_t> bytes;
    CHECK(chain.Contains(kName));
    REQUIRE(chain.Read(kName, &bytes));
    CHECK(std::string(bytes.begin(), bytes.end()) == kContent);
}

TEST_CASE("a later archive replaces what an earlier one holds")
{
    Archives temp;
    const std::string patched = "the row the patch shipped instead";
    WriteArchive(temp.dir / "base.mpq", kName, kContent, false);
    WriteArchive(temp.dir / "patch.mpq", kName, patched, false);

    client::MpqChain chain;
    REQUIRE(chain.AddArchive((temp.dir / "base.mpq").string()));
    REQUIRE(chain.AddArchive((temp.dir / "patch.mpq").string()));

    std::vector<std::uint8_t> bytes;
    REQUIRE(chain.Read(kName, &bytes));
    CHECK(std::string(bytes.begin(), bytes.end()) == patched);
}

TEST_CASE("a deletion in a later archive hides the earlier copy")
{
    Archives temp;
    WriteArchive(temp.dir / "base.mpq", kName, kContent, false);
    WriteArchive(temp.dir / "patch.mpq", kName, std::string(), true);

    client::MpqChain chain;
    REQUIRE(chain.AddArchive((temp.dir / "base.mpq").string()));
    REQUIRE(chain.AddArchive((temp.dir / "patch.mpq").string()));

    std::vector<std::uint8_t> bytes;
    CHECK_FALSE(chain.Contains(kName));
    CHECK_FALSE(chain.Read(kName, &bytes));
}

TEST_CASE("a deletion in an earlier archive does not hide a later copy")
{
    Archives temp;
    const std::string restored = "the row a later patch brought back";
    WriteArchive(temp.dir / "base.mpq", kName, std::string(), true);
    WriteArchive(temp.dir / "patch.mpq", kName, restored, false);

    client::MpqChain chain;
    REQUIRE(chain.AddArchive((temp.dir / "base.mpq").string()));
    REQUIRE(chain.AddArchive((temp.dir / "patch.mpq").string()));

    std::vector<std::uint8_t> bytes;
    CHECK(chain.Contains(kName));
    REQUIRE(chain.Read(kName, &bytes));
    CHECK(std::string(bytes.begin(), bytes.end()) == restored);
}
