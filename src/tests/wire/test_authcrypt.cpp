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

// The 1.12 header cipher.
//
// These are known-answer tests, computed by hand from the algorithm rather than
// captured from a run, because their job is to pin the algorithm down. The
// standing hazard is somebody "modernising" this into real RC4 on the strength
// of the comment that used to sit on the class: RC4 keyed by an HMAC-SHA1 of a
// fixed seed is what WotLK does, and swapping it in here would break every
// login while looking like a cleanup.

#include "doctest.h"

#include "Auth/AuthCrypt.h"

#include <cstdint>
#include <vector>

namespace
{
    /// A key shorter than either header, so every test also exercises the index
    /// wrapping back to the start of the key.
    std::vector<uint8> ShortKey()
    {
        return { 0x01, 0x02, 0x03 };
    }

    AuthCrypt Armed(std::vector<uint8>& key)
    {
        AuthCrypt crypt;
        crypt.SetKey(key.data(), key.size());
        crypt.Init();
        return crypt;
    }
}

TEST_CASE("EncryptSend chains each byte into the next")
{
    std::vector<uint8> key = ShortKey();
    AuthCrypt crypt = Armed(key);

    // Four bytes: the whole outbound 1.12 header.
    uint8 header[4] = { 0x10, 0x20, 0x30, 0x40 };
    crypt.EncryptSend(header, sizeof(header));

    //  t=0: (0x10 ^ 0x01) + 0x00 = 0x11
    //  t=1: (0x20 ^ 0x02) + 0x11 = 0x33
    //  t=2: (0x30 ^ 0x03) + 0x33 = 0x66
    //  t=3: (0x40 ^ 0x01) + 0x66 = 0xA7   <- key index wrapped to 0
    CHECK(header[0] == 0x11);
    CHECK(header[1] == 0x33);
    CHECK(header[2] == 0x66);
    CHECK(header[3] == 0xA7);
}

TEST_CASE("DecryptRecv is the exact inverse of EncryptSend")
{
    std::vector<uint8> key = ShortKey();
    AuthCrypt crypt = Armed(key);

    // The ciphertext produced above, padded out to the six bytes of an inbound
    // header. The first four must come back as the plaintext they came from.
    uint8 header[6] = { 0x11, 0x33, 0x66, 0xA7, 0x00, 0x00 };
    crypt.DecryptRecv(header, sizeof(header));

    CHECK(header[0] == 0x10);
    CHECK(header[1] == 0x20);
    CHECK(header[2] == 0x30);
    CHECK(header[3] == 0x40);

    //  t=4: (0x00 - 0xA7) ^ 0x02 = 0x59 ^ 0x02 = 0x5B
    //  t=5: (0x00 - 0x00) ^ 0x03 = 0x03
    // Kept explicit because the subtraction underflows: the code promotes to int
    // and narrows back, and this asserts that path still agrees with modular
    // uint8 arithmetic on every compiler we build with.
    CHECK(header[4] == 0x5B);
    CHECK(header[5] == 0x03);
}

TEST_CASE("The same plaintext byte encrypts differently after a different one")
{
    // This is the property that separates the real cipher from a stateless XOR
    // against the key. Byte 1 is 0x00 in both streams; it must not come out the
    // same, because the previous CIPHER byte is folded into it.
    std::vector<uint8> key = ShortKey();

    AuthCrypt a = Armed(key);
    uint8 first[4] = { 0xAA, 0x00, 0x00, 0x00 };
    a.EncryptSend(first, sizeof(first));

    AuthCrypt b = Armed(key);
    uint8 second[4] = { 0xBB, 0x00, 0x00, 0x00 };
    b.EncryptSend(second, sizeof(second));

    CHECK(first[1] == 0xAD);   // (0x00 ^ 0x02) + 0xAB
    CHECK(second[1] == 0xBC);  // (0x00 ^ 0x02) + 0xBA
    CHECK(first[1] != second[1]);
}

TEST_CASE("Headers travel in clear until the cipher is armed")
{
    // Invariant 13: the cipher is armed between verifying the client's proof and
    // the world learning about the session. Before Init(), a keyed but unarmed
    // AuthCrypt must leave bytes strictly alone -- if it did anything else, the
    // handshake packets that precede arming would go out mangled.
    std::vector<uint8> key = ShortKey();

    AuthCrypt crypt;
    crypt.SetKey(key.data(), key.size());
    CHECK_FALSE(crypt.IsInitialized());

    uint8 header[4] = { 0x10, 0x20, 0x30, 0x40 };
    crypt.EncryptSend(header, sizeof(header));

    CHECK(header[0] == 0x10);
    CHECK(header[1] == 0x20);
    CHECK(header[2] == 0x30);
    CHECK(header[3] == 0x40);
}

TEST_CASE("A short buffer is refused rather than partially transformed")
{
    // Both directions have a fixed header length and bail out if handed less.
    // Transforming a prefix would advance the chain state and desynchronise
    // every later header on the connection, which is unrecoverable.
    std::vector<uint8> key = ShortKey();
    AuthCrypt crypt = Armed(key);

    uint8 tooShort[3] = { 0x10, 0x20, 0x30 };
    crypt.EncryptSend(tooShort, sizeof(tooShort));

    CHECK(tooShort[0] == 0x10);
    CHECK(tooShort[1] == 0x20);
    CHECK(tooShort[2] == 0x30);

    // And the state was not advanced: a full header now encrypts exactly as it
    // would have on a fresh cipher.
    uint8 header[4] = { 0x10, 0x20, 0x30, 0x40 };
    crypt.EncryptSend(header, sizeof(header));
    CHECK(header[0] == 0x11);
    CHECK(header[3] == 0xA7);
}
