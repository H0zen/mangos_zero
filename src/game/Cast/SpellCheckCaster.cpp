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
 * @file SpellCheckCaster.cpp
 * @brief Everything about the caster himself that can refuse a cast.
 * Cooldowns, the global cooldown, where a battleground has got to, whether he
 * is stealthed, mounted or dead, and what his own auras leave him able to do.
 */

#include "Reaction.h"
#include "Utilities/MathDefines.h"
#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "CreatureRecord.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "LineOfSightExemptions.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "Cast/Recipe/RecipeBook.h"

/**
 * @brief Asks whether the caster is in any state to cast this at all.
 *
 * @param strict True while the cast is being started rather than finished.
 *
 * @return The reason the cast is refused, or SPELL_CAST_OK.
 */
SpellCastResult Spell::CheckTheCasterMay(bool strict)
{
    // check cooldowns to prevent cheating (ignore passive spells, that client side visual only)
    if (m_caster->IsPlayer() && !Recipe().Says().passive &&
        ((Player*)m_caster)->HasSpellCooldown(m_spellInfo->ID))
    {
        if (m_triggeredByAuraSpell)
        {
            return SPELL_FAILED_DONT_REPORT;
        }
        else
        {
            return SPELL_FAILED_NOT_READY;
        }
    }

    // check global cooldown
    if (strict && !m_IsTriggeredSpell && HasGlobalCooldown())
    {
        return SPELL_FAILED_NOT_READY;
    }

    // only allow triggered spells if at an ended battleground
    if (!m_IsTriggeredSpell && m_caster->IsPlayer())
    {
        if (BattleGround* bg = ((Player*)m_caster)->Battle().Ground())
        {
            if (bg->GetStatus() == STATUS_WAIT_LEAVE)
            {
                return SPELL_FAILED_DONT_REPORT;
            }
        }
    }

    if (!m_IsTriggeredSpell && m_spellInfo->ID == 2479) //honorless target as non-triggered spell
    {
        return SPELL_FAILED_DONT_REPORT;
    }

    if (!m_IsTriggeredSpell && IsNonCombatSpell(m_spellInfo) &&
        m_caster->IsInCombat())
    {
        return SPELL_FAILED_AFFECTING_COMBAT;
    }

    // Backstab position check
    if (m_spellInfo->ID == 53 || m_spellInfo->ID == 2589 || m_spellInfo->ID == 7159)
    {
        if (Unit* target = m_targets.getUnitTarget())
        {
            if (target->Where().HasInArc(m_caster->Where(), M_PI_F))
            {
                SendCastResult(SPELL_FAILED_NOT_BEHIND);
                return SPELL_FAILED_NOT_BEHIND;
            }
        }
    }

    if (m_caster->IsPlayer() && !((Player*)m_caster)->isGameMaster() &&
        sWorld.getConfig(CONFIG_BOOL_VMAP_INDOOR_CHECK))
    {
        if (Recipe().Says().outdoorsOnly &&
            !m_caster->GetMap()->GetTerrain()->IsOutdoors(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z()))
        {
            return SPELL_FAILED_ONLY_OUTDOORS;
        }

        if (Recipe().Says().indoorsOnly &&
            m_caster->GetMap()->GetTerrain()->IsOutdoors(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z()))
        {
            return SPELL_FAILED_ONLY_INDOORS;
        }
    }
    // only check at first call, Stealth auras are already removed at second call
    // for now, ignore triggered spells
    if (strict && !m_IsTriggeredSpell)
    {
        // Can not be used in this stance/form
        SpellCastResult shapeError = GetErrorAtShapeshiftedCast(m_spellInfo, m_caster->GetShapeshiftForm());
        if (shapeError != SPELL_CAST_OK)
        {
            return shapeError;
        }

        if (Recipe().Says().onlyWhileStealthed && !(m_caster->HasStealthAura()))
        {
            return SPELL_FAILED_ONLY_STEALTHED;
        }
    }

    // caster state requirements
    if (m_spellInfo->CasterAuraState && !m_caster->HasAuraState(AuraState(m_spellInfo->CasterAuraState)))
    {
        return SPELL_FAILED_CANT_DO_THAT_YET;
    }

    if (m_caster->IsPlayer())
    {
        // cancel autorepeat spells if cast start when moving
        // (not wand currently autorepeat cast delayed to moving stop anyway in spell update code)
        if (((Player*)m_caster)->isMoving())
        {
            // skip stuck spell to allow use it in falling case and apply spell limitations at movement
            if ((!((Player*)m_caster)->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLINGFAR) || Recipe().At(EFFECT_INDEX_0).verb != SPELL_EFFECT_STUCK) &&
                (IsAutoRepeat() || (m_spellInfo->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED) != 0))
            {
                return SPELL_FAILED_MOVING;
            }
        }

        if (!m_IsTriggeredSpell && NeedsComboPoints(m_spellInfo) &&
            (!m_targets.getUnitTarget() || m_targets.getUnitTarget()->GetObjectGuid() != ((Player*)m_caster)->GetComboTargetGuid()))
        {
            // warrior not have real combo-points at client side but use this way for mark allow Overpower use
            return m_caster->getClass() == CLASS_WARRIOR ? SPELL_FAILED_CANT_DO_THAT_YET : SPELL_FAILED_NO_COMBO_POINTS;
        }

        // Loatheb Corrupted Mind spell failed
        switch (m_spellInfo->SpellClassSet)
        {
            case SPELLFAMILY_DRUID:
            case SPELLFAMILY_PRIEST:
            case SPELLFAMILY_SHAMAN:
            case SPELLFAMILY_PALADIN:
            {
                if (m_spellInfo->HasSpellEffect(SPELL_EFFECT_HEAL) || IsSpellHaveAura(m_spellInfo, SPELL_AURA_PERIODIC_HEAL) ||
                    m_spellInfo->HasSpellEffect(SPELL_EFFECT_DISPEL))
                {
                    const auto auraClassScripts = m_caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                    for (const auto* script : auraClassScripts)
                    {
                        if (script->GetModifier()->m_miscvalue == 4327)
                        {
                            return SPELL_FAILED_FIZZLE;
                        }
                    }
                }
            }
        }
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Asks whether the ground the caster stands on allows this cast.
 *
 * Some spells belong to one map, zone or area and nowhere else, and a mounted
 * player casts almost nothing at all.
 *
 * @return The reason the cast is refused, or SPELL_CAST_OK.
 */
SpellCastResult Spell::CheckWhereTheCasterStands()
{
    uint32 zone, area;
    m_caster->GetTerrain()->GetZoneAndAreaId(zone, area, m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z());

    SpellCastResult here = sSpellMgr.GetSpellAllowedInLocationError(m_spellInfo, m_caster->GetMapId(), zone, area,
        m_caster->GetCharmerOrOwnerPlayerOrPlayerItself());
    if (here != SPELL_CAST_OK)
    {
        return here;
    }

    // a creature may cast from the saddle, a player may not
    if (m_caster->IsMounted() && m_caster->IsPlayer() && !m_IsTriggeredSpell &&
        !(Recipe().Starts() == cast::Start::Passive) && !Recipe().Says().castableWhileMounted)
    {
        return m_caster->IsTaxiFlying() ? SPELL_FAILED_NOT_ON_TAXI : SPELL_FAILED_NOT_MOUNTED;
    }

    return SPELL_CAST_OK;
}
