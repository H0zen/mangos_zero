/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/// \addtogroup realmd
/// @{
/// \file

#pragma once

#include "RealmSnapshot.h"

#include <memory>
#include <string>

/**
 * @brief Storage object for the list of realms on the server
 *
 */
class RealmList
{
    public:
        /**
         * @brief
         *
         */
        static RealmList& Instance();

        RealmList();
        ~RealmList() {};

        void Initialize(uint32 updateInterval);

        void UpdateIfNeed();

        RealmListView GetRealms() const;

        /**
         * @return the total number of realms available
         */
        uint32 size() const
        {
            return static_cast<uint32>(m_snapshots.Load()->realms.size());
        }
    private:
        /**
         * Adds the given \ref Realm to the list presented to connecting clients. A realm
         * only reaches the list if it names at least one build in the allowedbuilds field
         * of the realm.realmlist database table.
         * @param realm the realm you want to list, should be done for all realms
         */
        void AddRealmToList(RealmSnapshot& snapshot, Realm const& realm);

        std::shared_ptr<RealmSnapshot> BuildSnapshot(bool init);

        /**
         * @brief
         *
         * @param ID
         * @param name
         * @param address
         * @param port
         * @param icon
         * @param realmflags
         * @param timezone
         * @param allowedSecurityLevel
         * @param popu
         * @param builds
         */
        void UpdateRealm(RealmSnapshot& snapshot, uint32 ID, const std::string& name, RealmAddress const& address, RealmAddress const& localAddress, RealmAddress const& localSubnetmask, uint32 port, uint8 icon, RealmFlags realmflags, uint8 timezone, AccountTypes allowedSecurityLevel, float popu, const std::string& builds);
    private:
        RealmSnapshotStore m_snapshots;
        RealmRefreshGate m_refreshGate;
};

#define sRealmList RealmList::Instance()

/// @}
