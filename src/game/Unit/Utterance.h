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
#include "SharedDefines.h"
#include "Creature.h"

class Map;
class Player;
class Unit;
class Occupant;
struct MangosStringLocale;

void Utter(Occupant const& speaker, ChatType kind, char const* text,
           Unit const* target = nullptr, Language language = LANG_UNIVERSAL);

void Utter(Occupant const& speaker, MangosStringLocale const* line, Unit const* target = nullptr);

enum class SoundKind
{
    AtObject,
    Flat,
    Music,
};

void PlaySound(Occupant const& source, SoundKind kind, uint32 soundId, Player const* target = nullptr);

void PlaySoundToMap(Map& map, uint32 soundId, uint32 zoneId = 0);

void YellToMap(Map& map, ObjectGuid speaker, int32 textId, Language language, Unit const* target);
void YellToMap(Map& map, CreatureInfo const* speaker, int32 textId, Language language,
               Unit const* target, uint32 senderLowGuid = 0);

void SendDespawnAnimation(Occupant const& what);
