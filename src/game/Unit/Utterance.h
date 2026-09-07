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

/**
 * What an object makes the people around it hear or see.
 *
 * A line differs from another line in two respects only: which chat message it
 * goes out as, and who hears it. `ChatType` already names both -- a say carries
 * one range, a yell another, a whisper reaches one listener and a zone yell a
 * whole zone -- so it is the parameter, not a function name.
 *
 * The literal form takes the text as given; the other reads it from the string
 * table in each listener's own locale, and takes the kind from the entry.
 */
void Utter(Occupant const& speaker, ChatType kind, char const* text,
           Unit const* target = nullptr, Language language = LANG_UNIVERSAL);

void Utter(Occupant const& speaker, MangosStringLocale const* line, Unit const* target = nullptr);

/// How a sound reaches the listener: placed at the object so the client can
/// attenuate it by distance, played flat wherever the listener is, or as music,
/// which the client crossfades rather than layers.
enum class SoundKind
{
    AtObject,
    Flat,
    Music,
};

/// One sound, to one client or to everyone who can see the source.
void PlaySound(Occupant const& source, SoundKind kind, uint32 soundId, Player const* target = nullptr);

/// One sound to a whole map, or to one zone of it. For a script that plays to a room
/// nobody is standing in the middle of.
void PlaySoundToMap(Map& map, uint32 soundId, uint32 zoneId = 0);

/// A creature's yell heard by the whole map, whether or not the creature is on it: the
/// line is read from the string table in each listener's own locale. The speaker is named
/// by guid, or by template when nothing is spawned to speak.
void YellToMap(Map& map, ObjectGuid speaker, int32 textId, Language language, Unit const* target);
void YellToMap(Map& map, CreatureInfo const* speaker, int32 textId, Language language,
               Unit const* target, uint32 senderLowGuid = 0);

/// The puff of an object vanishing.
void SendDespawnAnimation(Occupant const& what);
