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

#include "Reaction.h"
#include "TotemAI.h"
#include "Totem.h"
#include "Creature.h"
#include "DBCStores.h"
#include "SpellMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

int TotemAI::Permissible(const Creature* creature)
{
    if (creature->IsTotem())
    {
        return PERMIT_BASE_PROACTIVE;
    }

    return PERMIT_BASE_NO;
}

TotemAI::TotemAI(Creature* c) : CreatureAI(c)
{
}

void TotemAI::MoveInLineOfSight(Unit*)
{
}

void TotemAI::EnterEvadeMode()
{
    m_creature->CombatStop(true);
}

void TotemAI::UpdateAI(const uint32 )
{
    if (getTotem().GetTotemType() != TOTEM_ACTIVE)
    {
        return;
    }

    if (!m_creature->IsAlive() || m_creature->IsNonMeleeSpellCasted(false))
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(getTotem().GetSpell());
    if (!spellInfo)
    {
        return;
    }

    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(spellInfo->RangeIndex);
    float max_range = GetSpellMaxRange(srange);

    Unit* victim = m_creature->GetMap()->GetUnit(i_victimGuid);

    if (!victim ||
        !victim->IsTargetableForAttack() || !InReach(*m_creature, *victim, max_range) ||
        IsFriendly(*m_creature, *victim) || !victim->IsVisibleForOrDetect(m_creature, m_creature, false))
    {
        victim = nullptr;

        MaNGOS::NearestAttackableUnitInObjectRangeCheck u_check(m_creature, m_creature, max_range);
        MaNGOS::UnitLastSearcher<MaNGOS::NearestAttackableUnitInObjectRangeCheck> checker(victim, u_check);
        Cell::VisitAllObjects(m_creature, checker, max_range);
    }

    if (victim)
    {

        i_victimGuid = victim->GetObjectGuid();

        m_creature->SetInFront(victim);
        m_creature->CastSpell(victim, getTotem().GetSpell(), false);
    }
    else
    {
        i_victimGuid = 0;
    }
}

bool TotemAI::IsVisible(Unit*) const
{
    return false;
}

void TotemAI::AttackStart(Unit*)
{
}

Totem& TotemAI::getTotem()
{
    return static_cast<Totem&>(*m_creature);
}
