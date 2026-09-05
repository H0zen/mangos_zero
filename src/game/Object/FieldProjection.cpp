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

/**
 * Who may be told what, and what the value looks like when they are told.
 *
 * This is the one place that is allowed to know every kind of object at once,
 * so that the serializer in Object needs to know none of them. Nothing here
 * writes: a projection answers what a value looks like to one observer, and
 * leaves the object as it found it.
 */

#include "FieldTable.h"
#include "Object.h"
#include "Creature.h"
#include "GameObject.h"
#include "Player.h"
#include "Item.h"
#include "SpellAuras.h"
#include "UpdateFields.h"
#include "SharedDefines.h"

namespace Fields
{
    Audience AudienceFor(Object const& object, Player const& observer)
    {
        // Everyone who can see the object at all gets these two, and the dynamic
        // fields are the ones rewritten per observer rather than withheld.
        Audience audience = VisPublic | VisDynamic;

        if (&object == static_cast<Object const*>(&observer))
        {
            return audience | VisPrivate | VisOwner | VisItemOwner | VisSpecial | VisParty;
        }

        ObjectGuid owner;
        if (Unit const* unit = ToUnit(&object))
        {
            owner = unit->GetCharmerOrOwnerGuid();
        }
        else if (object.GetTypeId() == TYPEID_ITEM || object.GetTypeId() == TYPEID_CONTAINER)
        {
            owner = static_cast<Item const&>(object).GetOwnerGuid();
        }
        else if (GameObject const* go = ToGameObject(&object))
        {
            owner = go->GetOwnerGuid();
        }

        if (owner == observer.GetObjectGuid())
        {
            audience |= VisOwner | VisItemOwner;
        }

        if (Player const* other = ToPlayer(&object))
        {
            if (observer.IsInSameGroupWith(other))
            {
                audience |= VisParty;
            }
        }

        // Beast Lore and its kin: the caster, and only the caster, is shown the
        // detail the client calls special info.
        if (Unit const* unit = ToUnit(&object))
        {
            for (const auto* aura : unit->GetAurasByType(SPELL_AURA_EMPATHY))
            {
                if (aura->GetCasterGuid() == observer.GetObjectGuid())
                {
                    audience |= VisSpecial;
                    break;
                }
            }
        }

        return audience;
    }

    void MaskFor(Table const& table, Audience audience, uint32* out)
    {
        for (uint16 block = 0; block < table.blocks; ++block)
        {
            out[block] = 0;
        }

        for (uint8 bit = 0; bit < 9; ++bit)
        {
            if (!(audience & (1u << bit)))
            {
                continue;
            }

            uint32 const* admits = table.MaskForBit(bit);
            for (uint16 block = 0; block < table.blocks; ++block)
            {
                out[block] |= admits[block];
            }
        }
    }

    /**
     * Fields the server keeps in a float although the wire wants an integer.
     *
     * This is a fact about the server's storage, not about the protocol, which
     * is why it cannot come out of the generated table: the client's descriptor
     * says INT for every one of them. It replaces a chain of hand-written index
     * ranges that sat in the middle of the serializer.
     */
    static bool IsFloatBacked(uint16 index)
    {
        return (index >= UNIT_FIELD_BASEATTACKTIME && index <= UNIT_FIELD_RANGEDATTACKTIME)
            || (index >= PLAYER_FIELD_POSSTAT0 && index <= PLAYER_FIELD_POSSTAT4)
            || (index >= PLAYER_FIELD_NEGSTAT0 && index <= PLAYER_FIELD_NEGSTAT4)
            || (index >= PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE && index <= PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + 6)
            || (index >= PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE && index <= PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + 6);
    }

    uint32 HealthAsPercent(uint32 current, uint32 max)
    {
        if (!max || !current)
        {
            return 0;
        }

        uint32 const percent = uint32(uint64(current) * 100 / max);
        return percent ? percent : 1;
    }

    bool ReadsRealHitPoints(ObjectGuid const& unit, ObjectGuid const& owner, ObjectGuid const& observer)
    {
        return unit == observer || (!owner.IsEmpty() && owner == observer);
    }

    static uint32 ProjectUnit(Unit const& unit, Player& observer, uint16 index, uint32 raw)
    {
        bool const real = ReadsRealHitPoints(unit.GetObjectGuid(), unit.GetCharmerOrOwnerGuid(),
                                             observer.GetObjectGuid());

        switch (index)
        {
            // Hit points are the unit's own state, so the stored slot has
            // nothing to say and `raw` is ignored.
            case UNIT_FIELD_HEALTH:
                return real ? unit.GetHealth() : HealthAsPercent(unit.GetHealth(), unit.GetMaxHealth());

            case UNIT_FIELD_MAXHEALTH:
                return real ? unit.GetMaxHealth() : (unit.GetMaxHealth() ? 100 : 0);

            case UNIT_FIELD_FLAGS:
                // A game master must be able to click anything.
                return observer.isGameMaster() ? (raw & ~UNIT_FLAG_NOT_SELECTABLE) : raw;

            default:
                break;
        }

        Creature const* creature = ToCreature(&unit);
        if (!creature)
        {
            return raw;
        }

        // The queries below are answers about the creature, but the accessors
        // that give them were never made const.
        Creature* asked = const_cast<Creature*>(creature);

        switch (index)
        {
            case UNIT_NPC_FLAGS:
            {
                uint32 value = raw;
                if ((value & UNIT_NPC_FLAG_TRAINER) && !creature->IsTrainerOf(&observer, false))
                {
                    value &= ~UNIT_NPC_FLAG_TRAINER;
                }

                if ((value & UNIT_NPC_FLAG_STABLEMASTER) && observer.getClass() != CLASS_HUNTER)
                {
                    value &= ~UNIT_NPC_FLAG_STABLEMASTER;
                }

                return value;
            }

            case UNIT_DYNAMIC_FLAGS:
            {
                uint32 value = raw;

                // A body with loot left in it is lootable, whatever the stored
                // flag says; the flag itself is the loot code's to keep.
                if (!asked->loot.isLooted())
                {
                    value |= UNIT_DYNFLAG_LOOTABLE;
                }

                if (!observer.isAllowedToLoot(asked))
                {
                    value &= ~UNIT_DYNFLAG_LOOTABLE;
                }

                if (observer.IsTappedByMeOrMyGroup(asked))
                {
                    value &= ~UNIT_DYNFLAG_TAPPED;
                }

                // Special info is the empathy caster's alone; the audience says
                // who that is, so the flag follows it.
                if ((value & UNIT_DYNFLAG_SPECIALINFO) && unit.IsAlive())
                {
                    if (!(AudienceFor(unit, observer) & VisSpecial))
                    {
                        value &= ~UNIT_DYNFLAG_SPECIALINFO;
                    }
                }

                return value;
            }

            default:
                return raw;
        }
    }

    static uint32 ProjectGameObject(GameObject const& go, Player& observer, uint16 index, uint32 raw)
    {
        if (index != GAMEOBJECT_DYN_FLAGS || go.IsMovingPlatform())
        {
            return raw;
        }

        if (!go.ActivateToQuest(&observer) && !observer.isGameMaster())
        {
            return 0;
        }

        switch (go.GetGoType())
        {
            case GAMEOBJECT_TYPE_QUESTGIVER:
            case GAMEOBJECT_TYPE_CHEST:
            case GAMEOBJECT_TYPE_GENERIC:
            case GAMEOBJECT_TYPE_SPELL_FOCUS:
            case GAMEOBJECT_TYPE_GOOBER:
                return GO_DYNFLAG_LO_ACTIVATE;

            default:
                return 0;
        }
    }

    uint32 Project(Object const& object, Player& observer, uint16 index, uint32 raw)
    {
        if (IsFloatBacked(index))
        {
            float const stored = object.GetFloatValue(index);
            return stored < 0.0f ? 0 : uint32(stored);
        }

        if (Unit const* unit = ToUnit(&object))
        {
            return ProjectUnit(*unit, observer, index, raw);
        }

        if (GameObject const* go = ToGameObject(&object))
        {
            return ProjectGameObject(*go, observer, index, raw);
        }

        return raw;
    }

    bool LivesOutside(uint8 typeId, uint16 index)
    {
        if (typeId != TYPEID_UNIT && typeId != TYPEID_PLAYER)
        {
            return false;
        }

        return index == UNIT_FIELD_HEALTH || index == UNIT_FIELD_MAXHEALTH;
    }

    bool AlwaysResend(uint8 typeId, uint16 index)
    {
        // Most observer-dependent fields need no help: they are projected on the
        // way out, and the moments when the answer changes without the stored
        // value changing are already announced -- a looted body calls
        // ResendField, a widened health pool does the same.
        //
        // A gameobject's dynamic flags are the exception. Whether it is a quest
        // objective is a question about the observer alone, and nothing on the
        // object side ever moves when the answer does.
        return typeId == TYPEID_GAMEOBJECT
            && (index == GAMEOBJECT_DYN_FLAGS || index == GAMEOBJECT_ANIMPROGRESS);
    }
}
