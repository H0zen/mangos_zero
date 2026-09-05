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

#include "Kinds.h"

#include "GameObject.h"
#include "Log.h"

GameObjectInfo const& GameObjectBehaviour::Data() const
{
    return *m_it.GetGOInfo();
}

GameObjectBehaviour::Casting GameObjectBehaviour::UsedBy(Unit* /*user*/, bool /*scriptSaidYes*/)
{
    sLog.outError("GameObject::Use unhandled GameObject type %u (entry %u).",
                  It().GetGoType(), It().GetEntry());

    return Casting();
}

/// A kind with nothing of its own: it is there, and using it does nothing.
namespace
{
    class InertBehaviour : public GameObjectBehaviour
    {
        public:
            using GameObjectBehaviour::GameObjectBehaviour;

            Casting UsedBy(Unit*, bool) override { return Casting(); }
    };
}

std::unique_ptr<GameObjectBehaviour> BehaviourOf(GameObject& it)
{
    switch (it.GetGOInfo()->type)
    {
        case GAMEOBJECT_TYPE_DOOR:             return std::make_unique<DoorBehaviour>(it);
        case GAMEOBJECT_TYPE_BUTTON:           return std::make_unique<ButtonBehaviour>(it);
        case GAMEOBJECT_TYPE_QUESTGIVER:       return std::make_unique<QuestGiverBehaviour>(it);
        case GAMEOBJECT_TYPE_CHEST:            return std::make_unique<ChestBehaviour>(it);
        case GAMEOBJECT_TYPE_GENERIC:          return std::make_unique<GenericBehaviour>(it);
        case GAMEOBJECT_TYPE_TRAP:             return std::make_unique<TrapBehaviour>(it);
        case GAMEOBJECT_TYPE_CHAIR:            return std::make_unique<ChairBehaviour>(it);
        case GAMEOBJECT_TYPE_SPELL_FOCUS:      return std::make_unique<SpellFocusBehaviour>(it);
        case GAMEOBJECT_TYPE_GOOBER:           return std::make_unique<GooberBehaviour>(it);
        case GAMEOBJECT_TYPE_CAMERA:           return std::make_unique<CameraBehaviour>(it);
        case GAMEOBJECT_TYPE_FISHINGNODE:      return std::make_unique<FishingNodeBehaviour>(it);
        case GAMEOBJECT_TYPE_SUMMONING_RITUAL: return std::make_unique<RitualBehaviour>(it);
        case GAMEOBJECT_TYPE_SPELLCASTER:      return std::make_unique<SpellCasterBehaviour>(it);
        case GAMEOBJECT_TYPE_FLAGSTAND:        return std::make_unique<FlagStandBehaviour>(it);
        case GAMEOBJECT_TYPE_FISHINGHOLE:      return std::make_unique<FishingHoleBehaviour>(it);
        case GAMEOBJECT_TYPE_FLAGDROP:         return std::make_unique<FlagDropBehaviour>(it);

        // Everything the player never clicks: banners, meeting stones, the hull of a
        // ship, an aura generator. They are in the world and that is all they do.
        case GAMEOBJECT_TYPE_BINDER:
        case GAMEOBJECT_TYPE_TEXT:
        case GAMEOBJECT_TYPE_TRANSPORT:
        case GAMEOBJECT_TYPE_AREADAMAGE:
        case GAMEOBJECT_TYPE_MAP_OBJECT:
        case GAMEOBJECT_TYPE_MO_TRANSPORT:
        case GAMEOBJECT_TYPE_DUEL_ARBITER:
        case GAMEOBJECT_TYPE_MINI_GAME:
        case GAMEOBJECT_TYPE_LOTTERY_KIOSK:
        case GAMEOBJECT_TYPE_MAILBOX:
        case GAMEOBJECT_TYPE_AUCTIONHOUSE:
        case GAMEOBJECT_TYPE_GUARDPOST:
        case GAMEOBJECT_TYPE_MEETINGSTONE:
        case GAMEOBJECT_TYPE_CAPTURE_POINT:
        case GAMEOBJECT_TYPE_AURA_GENERATOR:
            return std::make_unique<InertBehaviour>(it);

        default:
            break;
    }

    // A type the data invented. The base says so the first time anyone clicks it.
    return std::unique_ptr<GameObjectBehaviour>(new GameObjectBehaviour(it));
}
