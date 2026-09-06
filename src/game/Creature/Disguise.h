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

/**
 * When a borrowed faction is given back.
 *
 * A creature can be made to wear another side's colours -- an escort that turns
 * on its charge, a townsman who joins a fight, a boss who stops being anyone's
 * friend. Whether it ever takes them off, and at which moment, is said when they
 * are put on.
 *
 * Nothing at all means it keeps them until something takes them off by hand.
 */
enum TemporaryFactionFlags
{
    TEMPFACTION_NONE                    = 0x00,
    TEMPFACTION_RESTORE_RESPAWN         = 0x01,             // given back when it respawns
    TEMPFACTION_RESTORE_COMBAT_STOP     = 0x02,             // ... when the fight ends, which death counts as
    TEMPFACTION_RESTORE_REACH_HOME      = 0x04,             // ... on reaching home, if the fight ending did not do it

    TEMPFACTION_TOGGLE_NON_ATTACKABLE   = 0x08,             // while worn, it may be attacked
    TEMPFACTION_TOGGLE_OOC_NOT_ATTACK   = 0x10,             // ... even out of combat
    TEMPFACTION_TOGGLE_PASSIVE          = 0x20,             // ... and it stops being passive
    TEMPFACTION_TOGGLE_PACIFIED         = 0x40,             // ... and stops being pacified
    TEMPFACTION_TOGGLE_NOT_SELECTABLE   = 0x80,             // ... and can be clicked on

    TEMPFACTION_ALL,
};

/**
 * Another side's colours, worn for a while.
 *
 * Wearing them is not only a change of faction. Five of the unit flags that keep
 * a creature out of a fight -- unattackable, unattackable out of combat, passive,
 * pacified, unselectable -- are dropped while the colours are on and put back
 * when they come off, and each is asked for separately when they go on.
 *
 * Putting them back is not simply undoing: a flag only returns if the row it was
 * made from had it in the first place. A creature that was always attackable
 * does not become unattackable because it wore someone else's colours.
 *
 * The one exception is the out-of-combat flag, which is not put back while it is
 * still fighting -- it would say something false about a creature mid-swing.
 *
 * A charmed creature gives nothing back. Whoever is driving it decides what side
 * it is on, and restoring the row's faction underneath them would take it away.
 */
class Disguise
{
    public:

        explicit Disguise(Unit& who) : m_owner(who) {}

        /// Nothing is being worn.
        bool None() const { return m_flags == TEMPFACTION_NONE; }

        /// When it comes off, and which flags were dropped for it.
        uint32 Flags() const { return m_flags; }

        /// Puts another side's colours on, dropping the flags asked for.
        void Wear(uint32 factionId, uint32 flags);

        /// Gives them back, restoring only the flags the row itself had.
        void TakeOff();

    private:

        Unit& m_owner;

        uint32 m_flags = TEMPFACTION_NONE;
};
