/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 1.12.x client.
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

// Mapping a file, and the bounds check every reader must pass before it points
// at a section of one. A truncated or corrupt tile has to be refused, not read
// past the end of the mapping.

#include "doctest.h"

#include "terrain/MappedFile.hpp"

#include <cstdio>
#include <string>
#include <vector>

using world::terrain::MappedFile;

namespace
{
    /// A scratch file that removes itself, so a failed case cannot leave litter
    /// that makes the next run pass for the wrong reason.
    class TempFile
    {
        public:
            explicit TempFile(const std::vector<uint8_t>& bytes)
            {
                m_path = std::tmpnam(nullptr);
                if (std::FILE* f = std::fopen(m_path.c_str(), "wb"))
                {
                    if (!bytes.empty())
                    {
                        std::fwrite(bytes.data(), 1, bytes.size(), f);
                    }
                    std::fclose(f);
                    m_written = true;
                }
            }

            ~TempFile()
            {
                if (m_written)
                {
                    std::remove(m_path.c_str());
                }
            }

            const std::string& Path() const { return m_path; }
            bool Written() const { return m_written; }

        private:
            std::string m_path;
            bool m_written = false;
    };
}

TEST_CASE("A file maps to exactly its bytes")
{
    const std::vector<uint8_t> bytes{0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
    TempFile file(bytes);
    REQUIRE(file.Written());

    auto mapped = MappedFile::Open(file.Path());
    REQUIRE(static_cast<bool>(mapped));

    REQUIRE(mapped->Size() == bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        CHECK(mapped->Data()[i] == bytes[i]);
    }
}

TEST_CASE("A missing file maps to nothing")
{
    auto mapped = MappedFile::Open("this-file-does-not-exist.tile");
    CHECK_FALSE(static_cast<bool>(mapped));
}

TEST_CASE("An empty file maps to nothing rather than a zero-length mapping")
{
    TempFile file(std::vector<uint8_t>{});
    REQUIRE(file.Written());

    auto mapped = MappedFile::Open(file.Path());
    CHECK_FALSE(static_cast<bool>(mapped));
}

TEST_CASE("Covers accepts a section inside the mapping")
{
    const std::vector<uint8_t> bytes(64, 0x5A);
    TempFile file(bytes);
    REQUIRE(file.Written());

    auto mapped = MappedFile::Open(file.Path());
    REQUIRE(static_cast<bool>(mapped));

    CHECK(mapped->Covers(0, 64));
    CHECK(mapped->Covers(0, 0));
    CHECK(mapped->Covers(64, 0));
    CHECK(mapped->Covers(32, 32));
    CHECK(mapped->Covers(63, 1));
}

TEST_CASE("Covers refuses a section that runs past the end")
{
    const std::vector<uint8_t> bytes(64, 0x5A);
    TempFile file(bytes);
    REQUIRE(file.Written());

    auto mapped = MappedFile::Open(file.Path());
    REQUIRE(static_cast<bool>(mapped));

    CHECK_FALSE(mapped->Covers(0, 65));
    CHECK_FALSE(mapped->Covers(64, 1));
    CHECK_FALSE(mapped->Covers(65, 0));
    CHECK_FALSE(mapped->Covers(32, 33));
}

TEST_CASE("Covers does not wrap on a length near the top of size_t")
{
    // The check is written as `bytes <= size - offset` for this reason: an
    // `offset + bytes` formulation overflows and lets a huge length through.
    const std::vector<uint8_t> bytes(64, 0x5A);
    TempFile file(bytes);
    REQUIRE(file.Written());

    auto mapped = MappedFile::Open(file.Path());
    REQUIRE(static_cast<bool>(mapped));

    const size_t huge = static_cast<size_t>(-1);
    CHECK_FALSE(mapped->Covers(1, huge));
    CHECK_FALSE(mapped->Covers(huge, 1));
    CHECK_FALSE(mapped->Covers(huge, huge));
}

TEST_CASE("The mapping outlives the shared pointer that opened it")
{
    const std::vector<uint8_t> bytes{1, 2, 3, 4};
    TempFile file(bytes);
    REQUIRE(file.Written());

    std::shared_ptr<const MappedFile> kept;
    {
        auto mapped = MappedFile::Open(file.Path());
        REQUIRE(static_cast<bool>(mapped));
        kept = mapped;
    }

    REQUIRE(static_cast<bool>(kept));
    REQUIRE(kept->Size() == 4);
    CHECK(kept->Data()[3] == 4);
}
