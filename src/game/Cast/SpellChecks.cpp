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
 * @file Spell.cpp
 * @brief Spell casting and effect implementation
 *
 * This file implements the Spell class which handles spell casting:
 * - Spell validation and casting requirements
 * - Spell effect execution (damage, healing, summon, etc.)
 * - Spell targeting and area effects
 * - Spell cooldowns and resource costs
 * - Spell interruption and pushback
 * - Spell aura application
 * - Spell hit/miss calculations
 *
 * Spells are the primary combat mechanic in WoW, encompassing
 * abilities, talents, and item effects.
 *
 * @see Spell for the spell class
 * @see SpellAura for spell auras
 * @see SpellMgr for spell management
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
 * @brief Validates whether the spell can currently be cast.
 *
 * @param strict True to perform full pre-cast validation including global cooldown checks.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckCast(bool strict)
{
    SpellCastResult refusal = CheckTheCasterMay(strict);
    if (refusal != SPELL_CAST_OK)
    {
        return refusal;
    }

    refusal = CheckTheTargetChosen(strict);
    if (refusal != SPELL_CAST_OK)
    {
        return refusal;
    }

    refusal = CheckWhereTheCasterStands();
    if (refusal != SPELL_CAST_OK)
    {
        return refusal;
    }

    // a focus object can be wanted by any kind of cast, so the items are asked
    // about for everything except a passive
    if (!(Recipe().Starts() == cast::Start::Passive))
    {
        refusal = CheckItems();
        if (refusal != SPELL_CAST_OK)
        {
            return refusal;
        }
    }

    refusal = EnrolScriptedTargets();
    if (refusal != SPELL_CAST_OK)
    {
        return refusal;
    }

    if (!m_IsTriggeredSpell)
    {
        if (!m_triggeredByAuraSpell)
        {
            refusal = CheckRange(strict);
            if (refusal != SPELL_CAST_OK)
            {
                return refusal;
            }

            if (Unit* target = m_targets.getUnitTarget())
            {
                if (m_caster->IsPlayer() &&
                    (sSpellMgr.GetSpellFacingFlag(m_spellInfo->ID) & SPELL_FACING_FLAG_INFRONT) &&
                    !m_caster->Where().HasInArc(target->Where(), M_PI_F))
                {
                    return SPELL_FAILED_UNIT_NOT_INFRONT;
                }
            }
        }

        // a triggered spell pays nothing and is not stopped by what holds its caster
        refusal = CheckPower();
        if (refusal != SPELL_CAST_OK)
        {
            return refusal;
        }

        refusal = CheckCasterAuras();
        if (refusal != SPELL_CAST_OK)
        {
            return refusal;
        }
    }

    refusal = CheckEachSlotCanRun();
    if (refusal != SPELL_CAST_OK)
    {
        return refusal;
    }

    refusal = CheckEachAuraCanHold();
    if (refusal != SPELL_CAST_OK)
    {
        return refusal;
    }

    // last, so that any other problem with the cast is caught first
    return CheckTheTradeSlot();
}

/**
 * @brief Asks whether an item put up for trade may be the target of this cast.
 *
 * Enchanting what someone is offering is allowed only while the trade is being
 * accepted; before that the cast is remembered and quietly dropped.
 *
 * @return The reason the cast is refused, or SPELL_CAST_OK.
 */
SpellCastResult Spell::CheckTheTradeSlot()
{
    if (!(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM))
    {
        return SPELL_CAST_OK;
    }

    if (!m_caster->IsPlayer())
    {
        return SPELL_FAILED_NOT_TRADING;
    }

    Player* pCaster = ((Player*)m_caster);
    TradeData* my_trade = pCaster->GetTradeData();

    if (!my_trade)
    {
        return SPELL_FAILED_NOT_TRADING;
    }

    TradeSlots slot = TradeSlots(m_targets.getItemTargetGuid().GetRawValue());
    if (slot != TRADE_SLOT_NONTRADED)
    {
        return SPELL_FAILED_ITEM_NOT_READY;
    }

    if (!my_trade->IsInAcceptProcess())
    {
        my_trade->SetSpell(m_spellInfo->ID, m_CastItem);
        return SPELL_FAILED_DONT_REPORT;
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Validates whether a pet or charmed unit can cast the spell.
 *
 * @param target An optional explicit target override.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckPetCast(Unit* target)
{
    if (!m_caster->IsAlive())
    {
        return SPELL_FAILED_CASTER_DEAD;
    }

    if (m_caster->IsNonMeleeSpellCasted(false))             // prevent spellcast interruption by another spellcast
    {
        return SPELL_FAILED_SPELL_IN_PROGRESS;
    }
    if (m_caster->IsInCombat() && IsNonCombatSpell(m_spellInfo))
    {
        return SPELL_FAILED_AFFECTING_COMBAT;
    }

    if (m_caster->IsCreature() && (((Creature*)m_caster)->IsPet() || m_caster->IsCharmed()))
    {
        // dead owner (pets still alive when owners ressed?)
        if (m_caster->GetCharmerOrOwner() && !m_caster->GetCharmerOrOwner()->IsAlive())
        {
            return SPELL_FAILED_CASTER_DEAD;
        }

        if (!target && m_targets.getUnitTarget())
        {
            target = m_targets.getUnitTarget();
        }

        bool need = false;
        for (const auto& operation : Recipe().Does())
        {
            if (operation.targetA == TARGET_CHAIN_DAMAGE ||
                operation.targetA == TARGET_SINGLE_FRIEND ||
                operation.targetA == TARGET_SINGLE_FRIEND_2 ||
                operation.targetA == TARGET_DUELVSPLAYER ||
                operation.targetA == TARGET_SINGLE_PARTY ||
                operation.targetA == TARGET_CURRENT_ENEMY_COORDINATES)
            {
                need = true;
                if (!target)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }
                break;
            }
        }
        if (need)
        {
            m_targets.setUnitTarget(target);
        }

        Unit* _target = m_targets.getUnitTarget();

        // for target dead/target not valid
        if (_target)
        {
            if (!_target->IsTargetableForAttack())
            {
                return SPELL_FAILED_BAD_TARGETS;             // guessed error
            }

            if (Recipe().IsPositive())
            {
                if (IsHostile(*m_caster, *_target))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }
            else
            {
                bool duelvsplayertar = false;
                for (const auto& other : Recipe().Does())
                {
                    // TARGET_DUELVSPLAYER is positive AND negative
                    duelvsplayertar |= (other.targetA == TARGET_DUELVSPLAYER);
                }
                if (IsFriendly(*m_caster, *target) && !duelvsplayertar)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }
        }
        // cooldown
        if (((Creature*)m_caster)->HasSpellCooldown(m_spellInfo->ID))
        {
            return SPELL_FAILED_NOT_READY;
        }
    }

    return CheckCast(true);
}

/**
 * @brief Checks whether active caster auras prevent this spell from being cast.
 *
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckCasterAuras() const
{
    // Flag drop spells totally immuned to caster auras
    // FIXME: find more nice check for all totally immuned spells
    // HasAttribute(SPELL_ATTR_EX3_UNK28) ?
    if (m_spellInfo->ID == 23336 ||                         // Alliance Flag Drop
        m_spellInfo->ID == 23334)                       // Horde Flag Drop
    {
        return SPELL_CAST_OK;
    }

    uint8 school_immune = 0;
    uint32 mechanic_immune = 0;
    uint32 dispel_immune = 0;

    // Check if the spell grants school or mechanic immunity.
    // We use bitmasks so the loop is done only once and not on every aura check below.
    if (Recipe().Says().dispelsOnImmunity)
    {
        for (const auto& operation : Recipe().Does())
        {
            if (operation.aura == SPELL_AURA_SCHOOL_IMMUNITY)
            {
                school_immune |= uint32(operation.miscValue);
            }
            else if (operation.aura == SPELL_AURA_MECHANIC_IMMUNITY)
            {
                mechanic_immune |= 1 << uint32(operation.miscValue - 1);
            }
            else if (operation.aura == SPELL_AURA_MECHANIC_IMMUNITY_MASK)
            {
                mechanic_immune |= uint32(operation.miscValue);
            }
            else if (operation.aura == SPELL_AURA_DISPEL_IMMUNITY)
            {
                dispel_immune |= GetDispellMask(DispelType(operation.miscValue));
            }
        }
    }

    // Check whether the cast should be prevented by any state you might have.
    SpellCastResult prevented_reason = SPELL_CAST_OK;
    // Have to check if there is a stun aura. Otherwise will have problems with ghost aura apply while logging out
    uint32 unitflag = m_caster->GetUInt32Value(UNIT_FIELD_FLAGS);     // Get unit state
    /** [-ZERO]
     * if (unitflag & UNIT_FLAG_STUNNED && !(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_STUNNED))
     *     prevented_reason = SPELL_FAILED_STUNNED;
     * else if (unitflag & UNIT_FLAG_CONFUSED && !(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_CONFUSED))
     * {
     *     prevented_reason = SPELL_FAILED_CONFUSED;
     * }
     * else if (unitflag & UNIT_FLAG_FLEEING && !(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_FEARED))
     * {
     *      prevented_reason = SPELL_FAILED_FLEEING;
     * }
     * else
     */
    if (unitflag & UNIT_FLAG_SILENCED && m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
    {
        prevented_reason = SPELL_FAILED_SILENCED;
    }
    else if (unitflag & UNIT_FLAG_PACIFIED && m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY)
    {
        prevented_reason = SPELL_FAILED_PACIFIED;
    }

    if (m_caster->IsSchoolLockedOut(GetSpellSchoolMask(m_spellInfo)))
    {
        prevented_reason = SPELL_FAILED_SILENCED;
    }

    // Attr must make flag drop spell totally immune from all effects
    if (prevented_reason != SPELL_CAST_OK)
    {
        if (school_immune || mechanic_immune || dispel_immune)
        {
            // Checking auras is needed now, because you are prevented by some state but the spell grants immunity.
            Unit::SpellAuraHolderMap const& auras = m_caster->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
            {
                SpellAuraHolder* holder = itr->second;
                SpellEntry const* pEntry = holder->GetSpellProto();

                if ((GetSpellSchoolMask(pEntry) & school_immune) && !cast::RecipeOf(*pEntry).Says().ignoresSchoolImmunity)
                {
                    continue;
                }
                if ((1 << (pEntry->DispelType)) & dispel_immune)
                {
                    continue;
                }

                for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i));
                    if (!aura)
                    {
                        continue;
                    }

                    if (GetSpellMechanicMask(pEntry, 1 << i) & mechanic_immune)
                    {
                        continue;
                    }
                    // Make a second check for spell failed so the right SPELL_FAILED message is returned.
                    // That is needed when your casting is prevented by multiple states and you are only immune to some of them.
                    switch (aura->GetModifier()->m_auraname)
                    {
                        /** Zero
                         *  case SPELL_AURA_MOD_STUN:
                         *     if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_STUNNED))
                         *     {
                         *         return SPELL_FAILED_STUNNED;
                         *     }
                         *     break;
                         *  case SPELL_AURA_MOD_CONFUSE:
                         *     if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_CONFUSED))
                         *     {
                         *         return SPELL_FAILED_CONFUSED;
                         *     }
                         *     break;
                         *  case SPELL_AURA_MOD_FEAR:
                         *     if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_FEARED))
                         *     {
                         *         return SPELL_FAILED_FLEEING;
                         *     }
                         *     break;
                         */
                        case SPELL_AURA_MOD_SILENCE:
                        case SPELL_AURA_MOD_PACIFY:
                        case SPELL_AURA_MOD_PACIFY_SILENCE:
                            if (m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY)
                            {
                                return SPELL_FAILED_PACIFIED;
                            }
                            else if (m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
                            {
                                return SPELL_FAILED_SILENCED;
                            }
                            break;
                        default: break;
                    }
                }
            }
        }
        // You are prevented from casting and the spell casted does not grant immunity. Return a failed error.
        else
        {
            return prevented_reason;
        }
    }
    return SPELL_CAST_OK;
}

/**
 * @brief Checks whether the spell can be automatically cast on a target.
 *
 * @param target The target being evaluated.
 * @return True if automatic casting is allowed; otherwise, false.
 */
bool Spell::CanAutoCast(Unit* target)
{
    ObjectGuid targetguid = target->GetObjectGuid();

    for (const auto& operation : Recipe().Does())
    {
        const int j = operation.slot;

        if (operation.verb == SPELL_EFFECT_APPLY_AURA)
        {
            if (m_spellInfo->CumulativeAura <= 1)
            {
                if (target->HasAura(m_spellInfo->ID, SpellEffectIndex(j)))
                {
                    return false;
                }
            }
            else
            {
                if (Aura* aura = target->GetAura(m_spellInfo->ID, SpellEffectIndex(j)))
                {
                    if (aura->GetStackAmount() >= m_spellInfo->CumulativeAura)
                    {
                        return false;
                    }
                }
            }
        }
        else if (IsAreaAuraEffect(operation.verb))
        {
            if (target->HasAura(m_spellInfo->ID, SpellEffectIndex(j)))
            {
                return false;
            }
        }
    }

    SpellCastResult result = CheckPetCast(target);

    if (result == SPELL_CAST_OK || result == SPELL_FAILED_UNIT_NOT_INFRONT)
    {
        FillTargetMap();
        // check if among target units, our WANTED target is as well (->only self cast spells return false)
        for (const auto& enrolled : m_roster.Units())
        {
            if (enrolled.guid == targetguid)
            {
                return true;
            }
        }
    }
    return false;                                           // target invalid
}

/**
 * @brief Validates spell range requirements for the current targets.
 *
 * @param strict True to use strict range validation.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckRange(bool strict)
{
    Unit* target = m_targets.getUnitTarget();

    // special range cases
    switch (m_spellInfo->RangeIndex)
    {
        // self cast doesn't need range checking -- also for Starshards fix
        // spells that can be cast anywhere also need no check
        case SPELL_RANGE_IDX_SELF_ONLY:
        case SPELL_RANGE_IDX_ANYWHERE:
            return SPELL_CAST_OK;
        // combat range spells are treated differently
        case SPELL_RANGE_IDX_COMBAT:
        {
            if (target)
            {
                if (target == m_caster)
                {
                    return SPELL_CAST_OK;
                }

                float range_mod = strict ? 0.0f : 5.0f;
                if (Player* modOwner = m_caster->GetSpellModOwner())
                {
                    float base = ATTACK_DISTANCE;
                    range_mod += modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_RANGE, base, this);
                }

                // with additional 5 dist for non stricted case (some melee spells have delay in apply
                return InMeleeReach(*m_caster, *target, range_mod) ? SPELL_CAST_OK : SPELL_FAILED_OUT_OF_RANGE;
            }
            break;                                          // let continue in generic way for no target
        }
        case SPELL_RANGE_IDX_SHORT:
        {
            if ((m_spellInfo->SpellClassSet == SPELLFAMILY_HUNTER && (m_spellInfo->SpellClassMask & UI64LIT(0x00000080) || m_spellInfo->SpellClassMask & 0x800000)))
            {
                Pet* pet = m_caster->GetPet();
                if (pet)
                {
                    float max_range = GetSpellMaxRange(sSpellRangeStore.LookupEntry(SPELL_RANGE_IDX_SHORT));
                    return InReach(*m_caster, *pet, max_range) ? SPELL_CAST_OK : SPELL_FAILED_OUT_OF_RANGE;
                }
            }
            break;
        }
    }

    // add radius of caster and ~5 yds "give" for non stricred (landing) check
    float range_mod = strict ? 1.25f : 6.25;

    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex);
    float max_range = GetSpellMaxRange(srange) + range_mod;
    float min_range = GetSpellMinRange(srange);

    if (Player* modOwner = m_caster->GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_RANGE, max_range, this);
    }

    if (target && target != m_caster)
    {
        // distance from target in checks
        float dist = CombatDistanceBetween(*m_caster, *target, m_spellInfo->RangeIndex == SPELL_RANGE_IDX_COMBAT);

        if (dist > max_range)
        {
            return SPELL_FAILED_OUT_OF_RANGE;
        }
        if (min_range && dist < min_range)
        {
            return SPELL_FAILED_TOO_CLOSE;
        }
    }

    // TODO verify that such spells really use bounding radius
    if (m_targets.m_targetMask == TARGET_FLAG_DEST_LOCATION && m_targets.m_destX != 0 && m_targets.m_destY != 0 && m_targets.m_destZ != 0)
    {
        if (!m_caster->Where().WithinDist(Geometry::Vector3(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ), max_range))
        {
            return SPELL_FAILED_OUT_OF_RANGE;
        }
        if (min_range && m_caster->Where().WithinDist(Geometry::Vector3(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ), min_range))
        {
            return SPELL_FAILED_TOO_CLOSE;
        }
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Calculates the final power cost for a spell cast.
 *
 * @param spellInfo The spell prototype being cast.
 * @param caster The casting unit.
 * @param spell The active spell instance, if available.
 * @param castItem The cast item, if the spell originates from an item.
 * @return The resulting power cost.
 */
uint32 Spell::CalculatePowerCost(SpellEntry const* spellInfo, Unit* caster, Spell const* spell, Item* castItem)
{
    // item cast not used power
    if (castItem)
    {
        return 0;
    }

    // Spell drain all exist power on cast (Only paladin lay of Hands)
    if (cast::RecipeOf(*spellInfo).Says().drainsAllPower)
    {
        // If power type - health drain all
        if (spellInfo->PowerType == POWER_HEALTH)
        {
            return caster->GetHealth();
        }
        // Else drain all power
        if (spellInfo->PowerType < MAX_POWERS)
        {
            return caster->GetPower(Powers(spellInfo->PowerType));
        }
        sLog.outError("Spell::CalculateManaCost: Unknown power type '%d' in spell %d", spellInfo->PowerType, spellInfo->ID);
        return 0;
    }

    // Base powerCost
    int32 powerCost = spellInfo->ManaCost;
    // PCT cost from total amount
    if (spellInfo->ManaCostPct)
    {
        switch (spellInfo->PowerType)
        {
            // health as power used
            case POWER_HEALTH:
                powerCost += spellInfo->ManaCostPct * caster->GetCreateHealth() / 100;
                break;
            case POWER_MANA:
                powerCost += spellInfo->ManaCostPct * caster->GetCreateMana() / 100;
                break;
            case POWER_RAGE:
            case POWER_FOCUS:
            case POWER_ENERGY:
            case POWER_HAPPINESS:
                powerCost += spellInfo->ManaCostPct * caster->GetMaxPower(Powers(spellInfo->PowerType)) / 100;
                break;
            default:
                sLog.outError("Spell::CalculateManaCost: Unknown power type '%d' in spell %d", spellInfo->PowerType, spellInfo->ID);
                return 0;
        }
    }

    SpellSchools school = GetFirstSchoolInMask(spell ? spell->m_spellSchoolMask : GetSpellSchoolMask(spellInfo));
    // Flat mod from caster auras by spell school
    powerCost += caster->GetInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + school);
    // Apply cost mod by spell
    if (spell)
    {
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(spellInfo->ID, SPELLMOD_COST, powerCost, spell);
        }
    }

    if (cast::RecipeOf(*spellInfo).Says().damageScalesWithLevel)
    {
        powerCost = int32(powerCost / (1.117f * spellInfo->SpellLevel / caster->getLevel() - 0.1327f));
    }

    // PCT mod from user auras by school
    powerCost = int32(powerCost * (1.0f + caster->GetPowerCostMultiplier(school)));
    if (powerCost < 0)
    {
        powerCost = 0;
    }
    return powerCost;
}

/**
 * @brief Checks whether the caster has enough power to cast the spell.
 *
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckPower()
{
    // never check power for triggered spells
    if (m_IsTriggeredSpell)
    {
        return SPELL_CAST_OK;
    }

    // item cast not used power
    if (m_CastItem)
    {
        return SPELL_CAST_OK;
    }

    // Questgivers ignore power requirements for scripts, any other creature (except a per) is checked only in combat
    if (!m_caster->IsPlayer())
    {
        // power for pets should be checked
        if (!m_caster->GetObjectGuid().IsPet() && m_caster->HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER) && !m_caster->IsInCombat())
        {
            return SPELL_CAST_OK;
        }
    }

    // health as power used - need check health amount
    if (m_spellInfo->PowerType == POWER_HEALTH)
    {
        if (m_caster->GetHealth() <= m_powerCost)
        {
            return SPELL_FAILED_CANT_DO_THAT_YET;
        }
    }
    else  // any power except health
    {
        // Check valid power type: since the static data (m_spellInfo) are checked, the check should be done elsewhere
        // [+ZERO] actual DBC power values are 0..3 and uint32(-2)

        // Check power amount
        Powers powerType = Powers(m_spellInfo->PowerType);
        if (m_caster->GetPower(powerType) < m_powerCost)
        {
            return SPELL_FAILED_NO_POWER;
        }
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Determines whether reagent and item requirements should be ignored.
 *
 * @return True if item requirements are ignored; otherwise, false.
 */
bool Spell::IgnoreItemRequirements() const
{
    if (m_IsTriggeredSpell)
    {
        /// Not own traded item (in trader trade slot) req. reagents including triggered spell case
        if (Item* targetItem = m_targets.getItemTarget())
        {
            if (targetItem->GetOwnerGuid() != m_caster->GetObjectGuid())
            {
                return false;
            }
        }

        /// Some triggered spells have same reagents that have master spell
        /// expected in test: master spell have reagents in first slot then triggered don't must use own
        if (m_triggeredBySpellInfo && !m_triggeredBySpellInfo->Reagent[0])
        {
            return false;
        }

        return true;
    }

    return false;
}

/**
 * @brief Validates cast item, target item, reagent, focus, and item-based spell requirements.
 *
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckItems()
{
    if (!m_caster->IsPlayer())
    {
        return SPELL_CAST_OK;
    }

    Player* p_caster = (Player*)m_caster;

    // cast item checks
    if (m_CastItem)
    {
        if (m_CastItem->IsInTrade())
        {
            return SPELL_FAILED_ITEM_GONE;
        }

        uint32 itemid = m_CastItem->GetEntry();
        if (!p_caster->HasItemCount(itemid, 1))
        {
            return SPELL_FAILED_ITEM_NOT_READY;
        }

        ItemPrototype const* proto = m_CastItem->GetProto();
        if (!proto)
        {
            return SPELL_FAILED_ITEM_NOT_READY;
        }

        for (int i = 0; i < 5; ++i)
        {
            if (proto->Spells[i].SpellCharges)
            {
                if (m_CastItem->GetSpellCharges(i) == 0)
                {
                    return SPELL_FAILED_NO_CHARGES_REMAIN;
                }
            }
        }

        // consumable cast item checks
        if (proto->Class == ITEM_CLASS_CONSUMABLE && m_targets.getUnitTarget())
        {
            // such items should only fail if there is no suitable effect at all - see Rejuvenation Potions for example
            SpellCastResult failReason = SPELL_CAST_OK;
            for (const auto& operation : Recipe().Does())
            {
                // skip check, pet not required like checks, and for TARGET_PET m_targets.getUnitTarget() is not the real target but the caster
                if (operation.targetA == TARGET_PET)
                {
                    continue;
                }

                if (operation.verb == SPELL_EFFECT_HEAL)
                {
                    if (m_targets.getUnitTarget()->GetHealth() == m_targets.getUnitTarget()->GetMaxHealth())
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_HEALTH;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }

                // Mana Potion, Rage Potion, Thistle Tea(Rogue), ...
                if (operation.verb == SPELL_EFFECT_ENERGIZE)
                {
                    if (operation.miscValue < 0 || operation.miscValue >= MAX_POWERS)
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_MANA;
                        continue;
                    }

                    Powers power = Powers(operation.miscValue);
                    uint8 targetClass = m_targets.getUnitTarget()->getClass();
                    /* Mana */
                    if (power == POWER_MANA)
                    {
                        if (targetClass == CLASS_WARRIOR || targetClass == CLASS_ROGUE)
                        {
                            failReason = SPELL_FAILED_BAD_TARGETS;
                            continue;
                        }
                    }
                    /* Rage */
                    else if (power == POWER_RAGE)
                    {
                        if (targetClass != CLASS_WARRIOR && targetClass != CLASS_DRUID)
                        {
                            failReason = SPELL_FAILED_BAD_TARGETS;
                            continue;
                        }
                    }
                    /* Energy */
                    else if (power == POWER_ENERGY)
                    {
                        if (targetClass != CLASS_ROGUE && targetClass != CLASS_DRUID)
                        {
                            failReason = SPELL_FAILED_BAD_TARGETS;
                            continue;
                        }
                    }
                    if (m_targets.getUnitTarget()->GetPower(power) == m_targets.getUnitTarget()->GetMaxPower(power))
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_MANA;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }
            }
            if (failReason != SPELL_CAST_OK)
            {
                return failReason;
            }
        }
    }

    // check target item (for triggered case not report error)
    if (m_targets.getItemTargetGuid())
    {
        if (!m_caster->IsPlayer())
        {
            return m_IsTriggeredSpell && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
                ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_BAD_TARGETS;
        }

        if (!m_targets.getItemTarget())
        {
            return m_IsTriggeredSpell  && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
                ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_ITEM_GONE;
        }

        if (!m_targets.getItemTarget()->IsFitToSpellRequirements(m_spellInfo))
        {
            return m_IsTriggeredSpell  && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
                ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;
        }
    }
    // if not item target then required item must be equipped (for triggered case not report error)
    else
    {
        if (m_caster->IsPlayer() && !((Player*)m_caster)->HasItemFitToSpellReqirements(m_spellInfo))
        {
            return m_IsTriggeredSpell ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;
        }
    }

    // check spell focus object
    if (m_spellInfo->RequiresSpellFocus)
    {
        GameObject* ok = nullptr;
        MaNGOS::GameObjectFocusCheck go_check(m_caster, m_spellInfo->RequiresSpellFocus);
        MaNGOS::GameObjectSearcher<MaNGOS::GameObjectFocusCheck> checker(ok, go_check);
        Cell::VisitGridObjects(m_caster, checker, m_caster->GetMap()->GetVisibilityDistance());

        if (!ok)
        {
            return SPELL_FAILED_REQUIRES_SPELL_FOCUS;
        }

        focusObject = ok;                                   // game object found in range
    }

    // check reagents (ignore triggered spells with reagents processed by original spell) and special reagent ignore case.
    if (!IgnoreItemRequirements())
    {
        if (!p_caster->CanNoReagentCast(m_spellInfo))
        {
            for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
            {
                if (m_spellInfo->Reagent[i] <= 0)
                {
                    continue;
                }

                uint32 itemid    = m_spellInfo->Reagent[i];
                uint32 itemcount = m_spellInfo->ReagentCount[i];

                // if CastItem is also spell reagent
                if (m_CastItem && m_CastItem->GetEntry() == itemid)
                {
                    ItemPrototype const* proto = m_CastItem->GetProto();
                    if (!proto)
                    {
                        return SPELL_FAILED_ITEM_NOT_READY;
                    }
                    for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
                    {
                        // CastItem will be used up and does not count as reagent
                        int32 charges = m_CastItem->GetSpellCharges(s);
                        if (proto->Spells[s].SpellCharges < 0 && !(proto->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE) && abs(charges) < 2)
                        {
                            ++itemcount;
                            break;
                        }
                    }
                }

                if (!p_caster->HasItemCount(itemid, itemcount))
                {
                    return SPELL_FAILED_ITEM_NOT_READY;
                }
            }
        }

        // check totem-item requirements (items presence in inventory)
        uint32 totems = MAX_SPELL_TOTEMS;
        for (int i = 0; i < MAX_SPELL_TOTEMS ; ++i)
        {
            if (m_spellInfo->Totem[i] != 0)
            {
                if (p_caster->HasItemCount(m_spellInfo->Totem[i], 1))
                {
                    totems -= 1;
                    continue;
                }
            }
            else
            {
                totems -= 1;
            }
        }

        if (totems != 0)
        {
            return SPELL_FAILED_ITEM_GONE;                   //[-ZERO] not sure of it
        }

        /**[-ZERO] to rewrite?
         * // Check items for TotemCategory  (items presence in inventory)
         * uint32 TotemCategory = MAX_SPELL_TOTEM_CATEGORIES;
         * for (int i= 0; i < MAX_SPELL_TOTEM_CATEGORIES; ++i)
         * {
         *     if (m_spellInfo->TotemCategory[i] != 0)
         *     {
         *         if (p_caster->HasItemTotemCategory(m_spellInfo->TotemCategory[i]))
         *         {
         *             TotemCategory -= 1;
         *             continue;
         *         }
         *     }
         *     else
         *     {
         *         TotemCategory -= 1;
         *     }
         * }

         * if (TotemCategory != 0)
         * {
         *     return SPELL_FAILED_TOTEM_CATEGORY;             // 0x7B
         * }
         */
    }

    // special checks for spell effects
    for (const auto& operation : Recipe().Does())
    {
        switch (operation.verb)
        {
            case SPELL_EFFECT_CREATE_ITEM:
            {
                if (!m_IsTriggeredSpell && operation.itemType)
                {
                    ItemPosCountVec dest;
                    InventoryResult msg = p_caster->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, operation.itemType, 1);
                    if (msg != EQUIP_ERR_OK)
                    {
                        p_caster->SendEquipError(msg, nullptr, nullptr, operation.itemType);
                        return SPELL_FAILED_DONT_REPORT;
                    }
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_ITEM:
            {
                Item* targetItem = m_targets.getItemTarget();
                if (!targetItem)
                {
                    return SPELL_FAILED_ITEM_GONE;
                }

                if (targetItem->GetProto()->ItemLevel < m_spellInfo->BaseLevel)
                {
                    return SPELL_FAILED_LOWLEVEL;
                }
                // Check for armor kit spells: Heavy(2833), Thick(10344), Rugged(19057), Core(22725)
                if (m_CastItem && m_CastItem->GetProto())
                {
                    static uint32 const armorKitSpells[] = { 2833, 10344, 19057, 22725 };
                    for (uint32 const spellId : armorKitSpells)
                    {
                        if (m_spellInfo->ID == spellId)
                        {
                            int32 minLevel = int32(m_CastItem->GetProto()->RequiredLevel) - 5;
                            if (minLevel > 0 && int32(targetItem->GetProto()->ItemLevel) < minLevel)
                                return SPELL_FAILED_LOWLEVEL;
                            break;
                        }
                    }
                }
                // Not allow enchant in trade slot for some enchant type
                if (targetItem->GetOwner() != m_caster)
                {
                    uint32 enchant_id = operation.miscValue;
                    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                    if (!pEnchant)
                    {
                        return SPELL_FAILED_ERROR;
                    }
                    if (pEnchant->Flags & ENCHANTMENT_CAN_SOULBOUND)
                    {
                        return SPELL_FAILED_NOT_TRADEABLE;
                    }
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
            {
                Item* item = m_targets.getItemTarget();
                if (!item)
                {
                    return SPELL_FAILED_ITEM_GONE;
                }
                // Not allow enchant in trade slot for some enchant type
                if (item->GetOwner() != m_caster)
                {
                    uint32 enchant_id = operation.miscValue;
                    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                    if (!pEnchant)
                    {
                        return SPELL_FAILED_ERROR;
                    }
                    if (pEnchant->Flags & ENCHANTMENT_CAN_SOULBOUND)
                    {
                        return SPELL_FAILED_NOT_TRADEABLE;
                    }
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_HELD_ITEM:
                // check item existence in effect code (not output errors at offhand hold item effect to main hand for example
                break;
            case SPELL_EFFECT_DISENCHANT:
            {
                if (!m_targets.getItemTarget())
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }

                // prevent disenchanting in trade slot
                if (m_targets.getItemTarget()->GetOwnerGuid() != m_caster->GetObjectGuid())
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }

                ItemPrototype const* itemProto = m_targets.getItemTarget()->GetProto();
                if (!itemProto)
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }

                // must have disenchant loot (other static req. checked at item prototype loading)
                if (!itemProto->DisenchantID)
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }
                break;
            }
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            {
                if (!m_caster->IsPlayer())
                {
                    return SPELL_FAILED_TARGET_NOT_PLAYER;
                }
                if (Recipe().Swings() != RANGED_ATTACK)
                {
                    break;
                }
                Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(Recipe().Swings(), true, false);
                if (!pItem)
                {
                    return SPELL_FAILED_EQUIPPED_ITEM;
                }

                switch (pItem->GetProto()->SubClass)
                {
                    case ITEM_SUBCLASS_WEAPON_THROWN:
                    {
                        uint32 ammo = pItem->GetEntry();
                        if (!((Player*)m_caster)->HasItemCount(ammo, 1))
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }
                        break;
                    }
                    case ITEM_SUBCLASS_WEAPON_GUN:
                    case ITEM_SUBCLASS_WEAPON_BOW:
                    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                    {
                        uint32 ammo = ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID);
                        if (!ammo)
                        {
                            // Requires No Ammo
                            if (m_caster->GetDummyAura(46699))
                            {
                                break;                       // skip other checks
                            }

                            return SPELL_FAILED_NO_AMMO;
                        }

                        ItemPrototype const* ammoProto = ObjectMgr::GetItemPrototype(ammo);
                        if (!ammoProto)
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }

                        if (ammoProto->Class != ITEM_CLASS_PROJECTILE)
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }

                        // check ammo ws. weapon compatibility
                        switch (pItem->GetProto()->SubClass)
                        {
                            case ITEM_SUBCLASS_WEAPON_BOW:
                            case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                                if (ammoProto->SubClass != ITEM_SUBCLASS_ARROW)
                                {
                                    return SPELL_FAILED_NO_AMMO;
                                }
                                break;
                            case ITEM_SUBCLASS_WEAPON_GUN:
                                if (ammoProto->SubClass != ITEM_SUBCLASS_BULLET)
                                {
                                    return SPELL_FAILED_NO_AMMO;
                                }
                                break;
                            default:
                                return SPELL_FAILED_NO_AMMO;
                        }

                        if (!((Player*)m_caster)->HasItemCount(ammo, 1))
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }
                        break;
                    }
                    case ITEM_SUBCLASS_WEAPON_WAND:
                        break;
                    default:
                        break;
                }
                break;
            }
            default: break;
        }
    }

    return SPELL_CAST_OK;
}
