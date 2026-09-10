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
#include "Utilities/LinkedReference/RefManager.h"

class Unit;
class ThreatManager;
class HostileReference;
struct SpellEntry;

class HostileRefManager : public RefManager<Unit, ThreatManager>
{
    public:

        explicit HostileRefManager(Unit* pOwner);

        ~HostileRefManager();

        Unit* getOwner()
        {
            return iOwner;
        }

        void threatAssist(Unit* pVictim, float threat, SpellEntry const* threatSpell = 0, bool pSingleTarget = false);

        void addThreatPercent(int32 pValue);

        void deleteReferences();

        void deleteReferencesForFaction(uint32 faction);

        HostileReference* getFirst()
        {
            return ((HostileReference*) RefManager<Unit, ThreatManager>::getFirst());
        }

        void updateThreatTables();

        void setOnlineOfflineState(bool pIsOnline);

        void setOnlineOfflineState(Unit* pCreature, bool pIsOnline);

        void deleteReference(Unit* pCreature);

    private:
        Unit* iOwner;

};
