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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "Platform/Define.h"

class Unit;

enum TemporaryFactionFlags
{
    TEMPFACTION_NONE                    = 0x00,
    TEMPFACTION_RESTORE_RESPAWN         = 0x01,
    TEMPFACTION_RESTORE_COMBAT_STOP     = 0x02,
    TEMPFACTION_RESTORE_REACH_HOME      = 0x04,

    TEMPFACTION_TOGGLE_NON_ATTACKABLE   = 0x08,
    TEMPFACTION_TOGGLE_OOC_NOT_ATTACK   = 0x10,
    TEMPFACTION_TOGGLE_PASSIVE          = 0x20,
    TEMPFACTION_TOGGLE_PACIFIED         = 0x40,
    TEMPFACTION_TOGGLE_NOT_SELECTABLE   = 0x80,

    TEMPFACTION_ALL,
};

class Disguise
{
    public:

        explicit Disguise(Unit& who) : m_owner(who) {}

        bool None() const { return m_flags == TEMPFACTION_NONE; }

        uint32 Flags() const { return m_flags; }

        void Wear(uint32 factionId, uint32 flags);

        void TakeOff();

    private:

        Unit& m_owner;

        uint32 m_flags = TEMPFACTION_NONE;
};
