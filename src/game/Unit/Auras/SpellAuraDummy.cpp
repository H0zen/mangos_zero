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

#include "SpellAuras.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Language.h"
#include "TemporarySummon.h"

void Aura::HandleAuraDummy(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target || !target->IsAlive())
    {
        return;
    }

    if (apply)
    {
        switch (GetSpellProto()->SpellClassSet)
        {
            case SPELLFAMILY_GENERIC:
            {
                switch (GetId())
                {
                    case 7057:

                        m_isPeriodic = true;
                        m_modifier.periodictime = 30 * IN_MILLISECONDS;
                        m_periodicTimer = m_modifier.periodictime;
                        return;
                    case 10255:
                    {
                        if (Unit* caster = GetCaster())
                        {
                            if (!IsCreature(caster))
                            {
                                return;
                            }

                            caster->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                            caster->addUnitState(UNIT_STAT_ROOT);
                        }
                        return;
                    }
                    case 13139:

                        if (Unit* caster = GetCaster())
                        {
                            caster->CastSpell(caster, 13138, true, nullptr, this);
                        }
                        return;
                    case 23183:
                    {
                        if (Unit* target = GetTarget())
                        {
                            if (target->HasAura(23182))
                            {
                                target->CastSpell(target, 23186, true, nullptr, nullptr, GetCaster()->GetObjectGuid());
                            }
                        }
                        return;
                    }
                    case 25042:
                    {
                        if (Unit* target = GetTarget())
                        {
                            if (target->HasAura(25040))
                            {
                                target->CastSpell(target, 25043, true, nullptr, nullptr, GetCaster()->GetObjectGuid());
                            }
                        }
                        return;
                    }
                    case 28832:
                    case 28833:
                    case 28834:
                    case 28835:
                    {
                        int32 damage;
                        switch (GetStackAmount())
                        {
                            case 1:
                                return;
                            case 2: damage =   500; break;
                            case 3: damage =  1500; break;
                            case 4: damage =  4000; break;
                            case 5: damage = 12500; break;
                            default:
                                damage = 14000 + 1000 * GetStackAmount();
                                break;
                        }

                        if (Unit* caster = GetCaster())
                        {
                            caster->CastCustomSpell(target, 28836, &damage, nullptr, nullptr, true, nullptr, this);
                        }
                        return;
                    }
                }
                break;
            }
        }
    }

    else
    {
        if (IsQuestTameSpell(GetId()) && (GetAuraDuration() == 0))
        {
            Unit* caster = GetCaster();
            if (!caster || !caster->IsAlive())
            {
                return;
            }

            uint32 finalSpellId = 0;
            switch (GetId())
            {
                case 19548: finalSpellId = 19597; break;
                case 19674: finalSpellId = 19677; break;
                case 19687: finalSpellId = 19676; break;
                case 19688: finalSpellId = 19678; break;
                case 19689: finalSpellId = 19679; break;
                case 19692: finalSpellId = 19680; break;
                case 19693: finalSpellId = 19684; break;
                case 19694: finalSpellId = 19681; break;
                case 19696: finalSpellId = 19682; break;
                case 19697: finalSpellId = 19683; break;
                case 19699: finalSpellId = 19685; break;
                case 19700: finalSpellId = 19686; break;
            }

            if (finalSpellId)
            {
                caster->CastSpell(target, finalSpellId, true, nullptr, this);
            }

            return;
        }

        switch (GetId())
        {
            case 126:
            {
                Unit* caster = GetCaster();
                if (!caster || !IsPlayer(caster))
                {
                    return;
                }
                TemporarySummon* eye = static_cast<TemporarySummon*>(caster->GetCharm());
                if (eye)
                {
                    if (eye->GetUInt32Value(UNIT_CREATED_BY_SPELL) == GetId())
                    {
                        eye->UnSummon();
                    }
                }
                return;
            }
            case 10255:
            {
                if (Unit* caster = GetCaster())
                {
                    if (!IsCreature(caster))
                    {
                        return;
                    }

                    caster->CastSpell(caster, 10254, true);
                }
                return;
            }
            case 11826:
                if (m_removeMode != AURA_REMOVE_BY_EXPIRE)
                {
                    return;
                }

                if (Unit* caster = GetCaster())
                {
                    if (IsPlayer(caster))
                    {
                        caster->CastSpell(target, 11828, true, ((Player*) caster)->GetItemByGuid(this->GetCastItemGuid()), this);
                    }
                }
                return;
            case 12479:
                target->CastSpell(target, 12480, true, nullptr, this);
                return;
            case 12774:
            {
                if (m_removeMode == AURA_REMOVE_BY_DEATH)
                {
                    return;
                }

                if (Unit* caster = GetCaster())
                {
                    caster->CastSpell(caster, 12816, true);
                }

                return;
            }
            case 28169:
            {

                target->CastSpell(target, 28206, true, nullptr, this);

                target->CastSpell(target, 28240, true, nullptr, this);
                return;
            }
        }

        if (m_removeMode == AURA_REMOVE_BY_DEATH)
        {

            if (GetSpellProto()->SpellClassSet == SPELLFAMILY_MAGE && (GetSpellProto()->SpellClassMask & UI64LIT(0x0000000000000800)))
            {
                if (Unit* caster = GetCaster())
                {
                    caster->InterruptSpell(CURRENT_CHANNELED_SPELL);
                }

                return;
            }
        }
    }

    switch (GetSpellProto()->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (GetId())
            {
                case 6606:
                {
                    if (apply)
                    {
                        target->SetStandState(UNIT_STAND_STATE_SLEEP);
                        target->addUnitState(UNIT_STAT_ROOT);
                    }
                    else
                    {
                        target->clearUnitState(UNIT_STAT_ROOT);
                        target->SetStandState(UNIT_STAND_STATE_STAND);
                    }

                    return;
                }
                case 24658:
                {
                    if (apply)
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                        {
                            return;
                        }

                        caster->CastSpell(target, 24659, true, nullptr, nullptr, GetCasterGuid());
                    }
                    else
                    {
                        target->RemoveAuras(24659);
                    }
                    return;
                }
                case 24661:
                {
                    if (apply)
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                        {
                            return;
                        }

                        caster->CastSpell(target, 24662, true, nullptr, nullptr, GetCasterGuid());
                    }
                    else
                    {
                        target->RemoveAuras(24662);
                    }
                    return;
                }
                case 29266:
                {

                    if (IsCreature(target))
                    {
                        target->SetFeignDeath(apply);
                    }

                    return;
                }
                case 27978:
                    if (apply)
                    {
                        target->m_AuraFlags |= UNIT_AURAFLAG_ALIVE_INVISIBLE;
                    }
                    else
                    {
                        target->m_AuraFlags &= ~UNIT_AURAFLAG_ALIVE_INVISIBLE;
                    }
                    return;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {

            if (IsPlayer(target) && GetSpellProto()->SpellIconID == 1563)
            {
                ((Player*)target)->Sheet().AttackPower(false);
                return;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            switch (GetId())
            {
                case 6495:
                {
                    if (!IsPlayer(target))
                    {
                        return;
                    }

                    Totem* totem = target->Retainers().TotemIn(TOTEM_SLOT_AIR);

                    if (totem && apply)
                    {
                        ((Player*)target)->GetCamera().SetView(totem);
                    }
                    else
                    {
                        ((Player*)target)->GetCamera().ResetView();
                    }

                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {

            if (GetSpellProto()->SpellIconID == 237 && GetSpellProto()->SpellClassMask & UI64LIT(0x00000200))
            {
                stats::Apply(*target, UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, -28.0f, apply);
            }

            break;
        }
    }

    if (PetAura const* petSpell = sSpellMgr.GetPetAura(GetId()))
    {
        if (apply)
        {
            target->AddPetAura(petSpell);
        }
        else
        {
            target->RemovePetAura(petSpell);
        }
        return;
    }

    if (IsPlayer(target))
    {
        SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAuraMapBounds(GetId());
        if (saBounds.first != saBounds.second)
        {
            uint32 zone, area;
            target->GetTerrain()->GetZoneAndAreaId(zone, area, target->Where().X(), target->Where().Y(), target->Where().Z());

            for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
            {
                itr->second->ApplyOrRemoveSpellIfCan((Player*)target, zone, area, false);
            }
        }
    }

    if (IsCreature(target))
    {
        sScriptMgr.OnAuraDummy(this, apply);
    }
}
