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

#include <cmath>
#include "Utterance.h"
#include "Totem.h"
#include "Log.h"
#include "Group.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "DBCStores.h"
#include "CreatureAI.h"
#include "InstanceData.h"
#include "ObjectLookup.h"
#include "Cast/Recipe/RecipeBook.h"

Totem::Totem() : Creature(CREATURE_SUBTYPE_TOTEM), m_sheet(*this)
{
    m_type = TOTEM_PASSIVE;
}

bool Totem::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, Unit* owner)
{
    SetMap(cPos.GetMap());

    Team team =IsPlayer(owner) ? ((Player*)owner)->GetTeam() : TEAM_NONE;

    if (!CreateFromProto(guidlow, cinfo, team))
    {
        return false;
    }

    cPos.SelectFinalPoint(this);

    if (fabs(cPos.m_pos.z - owner->Where().Z()) > 5.0f)
    {
        cPos.m_pos.z = owner->Where().Z();
    }

    if (!cPos.PlaceOn(this))
    {
        return false;
    }

    if (InstanceData* iData = GetMap()->GetInstanceData())
    {
        iData->OnCreatureCreate(this);
    }

    LoadCreatureAddon(false);

    return true;
}

void Totem::Update(uint32 update_diff, uint32 time)
{
    Unit* owner = GetOwner();
    if (!owner || !owner->IsAlive() || !IsAlive())
    {
        UnSummon();
        return;
    }

    if (Term().RunsOut(update_diff, tenure::BodyOf(*this)))
    {
        UnSummon();
        return;
    }

    Creature::Update(update_diff, time);
}

void Totem::Summon(Unit* owner)
{
    owner->GetMap()->Add((Creature*)this);
    AIM_Initialize();

    WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
    data << GetObjectGuid();
    Deliver(Audience::Around(*this).AndSubject(), &data);

    if (IsCreature(owner) && ((Creature*)owner)->AI())
    {
        ((Creature*)owner)->AI()->JustSummoned((Creature*)this);
    }

    if (!GetSpell())
    {
        return;
    }

    switch (m_type)
    {
        case TOTEM_PASSIVE:
            CastSpell(this, GetSpell(), true);
            break;
        case TOTEM_STATUE:
            CastSpell(GetOwner(), GetSpell(), true);
            break;
        default: break;
    }
}

void Totem::UnSummon()
{
    SendDespawnAnimation(*this);

    CombatStop();
    RemoveAuras(GetSpell());

    if (Unit* owner = GetOwner())
    {
        owner->Retainers().TakeTotem(*this);
        owner->RemoveAuras(GetSpell());

        if (IsPlayer(owner))
        {

            if (Group* pGroup = ((Player*)owner)->GetGroup())
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    Player* Target = itr->getSource();
                    if (Target && pGroup->SameSubGroup((Player*)owner, Target))
                    {
                        Target->RemoveAuras(GetSpell());
                    }
                }
            }
        }

        if (IsCreature(owner) && ((Creature*)owner)->AI())
        {
            ((Creature*)owner)->AI()->SummonedCreatureDespawn((Creature*)this);
        }
    }

    if (IsAlive())
    {
        SetDeathState(DEAD);
    }

    AddObjectToRemoveList();
}

void Totem::SetOwner(Unit* owner)
{
    SetCreatorGuid(owner->GetObjectGuid());
    SetOwnerGuid(owner->GetObjectGuid());
    Term().SummonedBy(owner->GetObjectGuid());
    setFaction(owner->getFaction());
    SetLevel(owner->getLevel());
}

Unit* Totem::GetOwner()
{
    if (ObjectGuid ownerGuid = GetOwnerGuid())
    {
        return ObjectLookup::GetUnit(*this, ownerGuid);
    }

    return nullptr;
}

void Totem::SetTypeBySummonSpell(SpellEntry const* spellProto)
{

    SpellEntry const* totemSpell = sSpellStore.LookupEntry(GetSpell());
    if (totemSpell)
    {

        if (GetSpellCastTime(totemSpell))
        {
            m_type = TOTEM_ACTIVE;
        }
    }
    if (spellProto->SpellIconID == 2056)
    {
        m_type = TOTEM_STATUE;
    }
}

bool Totem::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const
{

    if (spellInfo->SpellClassSet == SPELLFAMILY_SHAMAN && spellInfo->IsFitToFamilyMask(UI64LIT(0x00004006000)))
    {
        return false;
    }

    switch (spellInfo->Effect[index])
    {
        case SPELL_EFFECT_ATTACK_ME:

        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_HEAL_MAX_HEALTH:
        case SPELL_EFFECT_HEAL_MECHANICAL:
        case SPELL_EFFECT_ENERGIZE:
            return true;
        default:
            break;
    }

    if (!cast::RecipeOf(*spellInfo).IsPositive())
    {

        if (IsAuraApplyEffect(spellInfo, index))
        {
            return true;
        }
    }
    else
    {

        if (IsPeriodicRegenerateEffect(spellInfo, index))
        {
            return true;
        }
    }

    return Creature::IsImmuneToSpellEffect(spellInfo, index, castOnSelf);
}
