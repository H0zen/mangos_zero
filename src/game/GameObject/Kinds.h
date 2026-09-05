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

#include "Behaviour.h"

/**
 * The sixteen kinds of gameobject a player can click, one class each.
 *
 * A kind that is not here is one nothing happens to when it is used: those
 * get the base behaviour, which says so in the log.
 */

/// A door swings open and shuts itself again.
class DoorBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A button is a door that trips something else as it opens.
class ButtonBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A thing that hands out quests and takes them back.
class QuestGiverBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A container with loot in it, and sometimes a trap under it.
class ChestBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A thing with no behaviour of its own beyond being clicked.
class GenericBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// Something laid to go off when it is touched.
class TrapBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A seat, with a slot per person it holds.
class ChairBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A thing a spell has to be cast near.
class SpellFocusBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// The catch-all clickable: levers, orbs, the odd quest prop.
class GooberBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A point the client is told to look from.
class CameraBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// The bobber a fishing cast puts on the water.
class FishingNodeBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A circle that needs several people standing in it.
class RitualBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A thing that casts one spell at whoever uses it.
class SpellCasterBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// The plinth a battleground flag stands on.
class FlagStandBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A patch of water with more in it than the rest.
class FishingHoleBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};

/// A battleground flag lying where it was dropped.
class FlagDropBehaviour : public GameObjectBehaviour
{
    public:
        using GameObjectBehaviour::GameObjectBehaviour;

        Casting UsedBy(Unit* user, bool scriptSaidYes) override;
};
