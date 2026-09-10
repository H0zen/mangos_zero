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

#include "Utilities/LinkedReference/Reference.h"
#include "Map.h"

class MapReference : public Reference<Map, Player>
{
    protected:

        void targetObjectBuildLink() override
        {
            getTarget()->m_mapRefManager.insertFirst(this);
            getTarget()->m_mapRefManager.incSize();
        }

        void targetObjectDestroyLink() override
        {
            if (isValid())
            {
                getTarget()->m_mapRefManager.decSize();
            }
        }

        void sourceObjectDestroyLink() override
        {
            getTarget()->m_mapRefManager.decSize();
        }

    public:

        MapReference() : Reference<Map, Player>() {}

        ~MapReference()
        {
            unlink();
        }

        MapReference* next()
        {
            return (MapReference*)Reference<Map, Player>::next();
        }

        MapReference const* next() const { return (MapReference const*)Reference<Map, Player>::next(); }

        MapReference* nockeck_prev()
        {
            return (MapReference*)Reference<Map, Player>::nocheck_prev();
        }

        MapReference const* nocheck_prev() const { return (MapReference const*)Reference<Map, Player>::nocheck_prev(); }
};
