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
#include "Pet.h"

class Player;

class PetMgr
{
    public:
        explicit PetMgr(Player* owner)
            : m_owner(owner), m_stableSlots(0), m_temporaryUnsummonedPetNumber(0)
        {
        }

        uint32 GetStableSlots() const { return m_stableSlots; }
        void SetStableSlots(uint32 slots) { m_stableSlots = slots; }

        void LoadStableSlotsFromField(uint32 raw);

        uint32 GetTemporaryUnsummonedPetNumber() const { return m_temporaryUnsummonedPetNumber; }
        void SetTemporaryUnsummonedPetNumber(uint32 petnumber) { m_temporaryUnsummonedPetNumber = petnumber; }

        void Remove(PetSaveMode mode);

        void RemoveActionBar();

        void UnsummonTemporaryIfAny();

        void ResummonTemporaryUnsummonedIfAny();

    private:
        Player* m_owner;
        uint32  m_stableSlots;
        uint32  m_temporaryUnsummonedPetNumber;
};
