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

#pragma once

#include "Platform/Define.h"
#include <vector>

class BigNumber;

/**
 * @brief Authentication encryption/decryption for World of Warcraft protocol
 *
 * AuthCrypt handles the session key-based encryption and decryption of the
 * 1.12.x packet HEADERS -- 6 bytes inbound, 4 outbound; payloads travel in the
 * clear. It maintains separate encryption and decryption states for
 * bidirectional communication.
 *
 * This is NOT ARC4. RC4 keyed by an HMAC-SHA1 of a fixed seed is the WotLK
 * cipher. The 1.12 one is a byte-wise chain over the raw 40-byte session key:
 *
 *     send: x = (b ^ key[i]) + j,   then j = x
 *     recv: x = (b - j) ^ key[i],   then j = b
 *
 * with `i` cycling over the key and `j` carrying the previous ciphertext byte,
 * so each header byte depends on the one before it and the streams cannot be
 * resynchronised after a gap.
 */
class AuthCrypt
{
    public:
        /**
         * @brief Constructor - initializes the crypt object
         */
        AuthCrypt();

        /**
         * @brief Destructor
         */
        ~AuthCrypt();

        /**
         * @brief Initialize the encryption/decryption state
         */
        void Init();

        /**
         * @brief Set the session key for encryption/decryption
         * @param key Pointer to the session key data
         * @param len Length of the key in bytes
         */
        void SetKey(uint8* key, size_t len);
        /**
         * @brief Decrypt received data from client
         * @param data Pointer to data buffer to decrypt
         * @param len Length of data to decrypt
         */
        void DecryptRecv(uint8*, size_t);

        /**
         * @brief Encrypt data to send to client
         * @param data Pointer to data buffer to encrypt
         * @param len Length of data to encrypt
         */
        void EncryptSend(uint8*, size_t);

        /**
         * @brief Check if the crypt object is initialized
         * @return True if initialized, false otherwise
         */
        bool IsInitialized()
        {
            return _initialized;
        }

    private:
        std::vector<uint8> _key; /**< Session key for encryption */
        uint8 _send_i, _send_j, _recv_i, _recv_j; /**< key index and previous cipher byte, per direction */
        bool _initialized; /**< Initialization status */
};
