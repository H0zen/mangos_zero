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

#include <list>

class Player;
class Spell;
struct SpellEntry;
struct SpellModifier;

typedef std::list<SpellModifier*> SpellModList;

class SpellModifiers
{
    public:

        explicit SpellModifiers(Player& who) : m_owner(who), m_awaitingRemoval(0) {}

        void Add(SpellModifier* mod, bool apply);

        SpellModifier* From(SpellModOp op, uint32 spellId) const;

        bool Affects(SpellEntry const* spellInfo, SpellModifier* mod, Spell const* spell) const;

        template <class T>
        T Apply(uint32 spellId, SpellModOp op, T& base, Spell const* spell = nullptr);

        void Spent(Spell const* spell);

        void Restore(Spell const* spell);

    private:

        Player& m_owner;

        SpellModList m_byNumber[MAX_SPELLMOD];

        int32 m_awaitingRemoval;
};
