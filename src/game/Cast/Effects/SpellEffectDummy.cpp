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

#include <iterator>
#include "Reaction.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
#include <cstdlib>
#include <vector>
#include <list>
#include <algorithm>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Geometry/Vector3.h"
#include <random>
#include "Cast/Recipe/RecipeBook.h"

void Spell::EffectDummy(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget && !gameObjTarget && !itemTarget)
    {
        return;
    }

    if (ThrowWhatTheTableNames(operation))
    {
        return;
    }

    switch (m_spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (m_spellInfo->ID)
            {
                case 9204:
                case 20538:
                case 26569:
                case 26637:
                {
                    m_caster->GetThreatManager().modifyThreatPercent(unitTarget, -100);
                    return;
                }
                case 9976:
                {
                    if (!unitTarget || !IsCreature(unitTarget))
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 9998, true);

                    ((Creature*)unitTarget)->ForcedDespawn(100);
                    return;
                }
                case 10254:
                {
                    if (!IsCreature(m_caster))
                    {
                        return;
                    }

                    m_caster->clearUnitState(UNIT_STAT_ROOT);
                    m_caster->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                    return;
                }
                case 12975:
                {
                    int32 healthModSpellBasePoints0 = int32(m_caster->GetMaxHealth() * 0.3);
                    m_caster->CastCustomSpell(m_caster, 12976, &healthModSpellBasePoints0, nullptr, nullptr, true, nullptr);
                    return;
                }
                case 13120:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = 0;

                    uint32 roll = urand(0, 99);

                    if (roll < 2)
                    {
                        spell_id = 16566;
                    }
                    else if (roll < 4)
                    {
                        spell_id = 13119;
                    }
                    else
                    {
                        spell_id = 13099;
                    }

                    m_caster->CastSpell(unitTarget, spell_id, true, nullptr);
                    return;
                }
                case 13006:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 roll = urand(0,99);
                    uint32 inner_roll = urand(1,3);

                    if (roll < 5)
                    {
                        switch (inner_roll)
                        {
                            case 1:
                                m_caster->CastSpell(m_caster, 13003, true, m_CastItem);
                                break;
                            case 2:
                                m_caster->CastSpell(m_caster, 13010, true, m_CastItem);
                                break;
                            default:
                                unitTarget->CastSpell(unitTarget, 13004, true, nullptr);
                                break;
                        }
                    }
                    else if (roll < 25)
                    {
                        m_caster->CastSpell(m_caster, 13004, true, m_CastItem);
                    }
                    else
                    {
                        m_caster->CastSpell(unitTarget, 13003, true, m_CastItem);
                    }

                    return;
                }
                case 13180:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 roll = urand(0,99);

                    if (roll < 5)
                    {
                        unitTarget->CastSpell(m_caster, 13181, true, nullptr);
                    }
                    else if (roll < 35)
                    {
                        return;
                    }
                    else
                    {
                        AddTriggeredSpell(13181);
                    }

                    return;
                }
                case 13280:
                {
                    if (unitTarget)
                    {
                        uint32 roll = urand(0,7);
                        int32 dmg[8] = {900, 1200, 1500, 1800, 2100, 2400, 2700, 3000};

                        m_caster->CastCustomSpell(unitTarget, 13279, &dmg[roll], nullptr, nullptr, true);
                    }
                    return;
                }
                case 13535:
                {
                    if (!m_originalCaster || !IsPlayer(m_originalCaster))
                    {
                        return;
                    }

                    Creature* channelTarget = m_originalCaster->GetMap()->GetCreature(m_originalCaster->GetChannelObjectGuid());

                    if (!channelTarget)
                    {
                        return;
                    }

                    m_originalCaster->CastSpell(channelTarget, 13481, true, nullptr, nullptr, m_originalCasterGUID, m_spellInfo);
                    return;
                }
                case 13567:
                {

                    if (!m_triggeredByAuraSpell || !unitTarget)
                    {
                        return;
                    }

                    switch (m_triggeredByAuraSpell->ID)
                    {
                        case 26467:
                            m_caster->CastCustomSpell(unitTarget, 26470, &damage, nullptr, nullptr, true);
                            break;
                        default:
                            sLog.outError("EffectDummy: Non-handled case for spell 13567 for triggered aura %u", m_triggeredByAuraSpell->ID);
                            break;
                    }
                    return;
                }
                case 14185:
                {
                    if (!IsPlayer(m_caster))
                    {
                        return;
                    }

                    const SpellCooldowns& cm = ((Player*)m_caster)->GetSpellCooldownMap();
                    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
                    {
                        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

                        if (spellInfo->SpellClassSet == SPELLFAMILY_ROGUE &&
                            spellInfo->ID != m_spellInfo->ID && GetSpellRecoveryTime(spellInfo) > 0)
                        {
                            ((Player*)m_caster)->RemoveSpellCooldown((itr++)->first, true);
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                    return;
                }
                case 14537:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    Unit* newTarget = unitTarget;
                    uint32 spell_id = 0;
                    uint32 roll = urand(0, 99);
                    if (roll < 25)
                    {
                        spell_id = 15662;
                    }
                    else if (roll < 50)
                    {
                        spell_id = 11538;
                    }
                    else if (roll < 70)
                    {
                        spell_id = 21179;
                    }
                    else if (roll < 77)
                    {
                        spell_id = 14621;
                    }
                    else if (roll < 80)
                    {
                        spell_id = 14621;
                        newTarget = m_caster;
                    }
                    else if (roll < 95)
                    {
                        spell_id = 25189;
                    }
                    else
                    {
                        spell_id = 14642;
                        newTarget = m_caster;
                    }

                    m_caster->CastSpell(newTarget, spell_id, true, m_CastItem);
                    return;
                }
                case 15998:
                case 19614:
                {
                    if (!unitTarget || !IsCreature(unitTarget))
                    {
                        return;
                    }

                    Creature* creatureTarget = (Creature*)unitTarget;

                    creatureTarget->ForcedDespawn();
                    return;
                }
                case 17009:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(0, 6))
                    {
                        case 0: spell_id = 16707; break;
                        case 1: spell_id = 16708; break;
                        case 2: spell_id = 16709; break;
                        case 3: spell_id = 16711; break;
                        case 4: spell_id = 16712; break;
                        case 5: spell_id = 16713; break;
                        case 6: spell_id = 16716; break;
                    }

                    m_caster->CastSpell(unitTarget, spell_id, true, nullptr, nullptr, m_originalCasterGUID, m_spellInfo);
                    return;
                }
                case 17251:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    Unit* caster = GetAffectiveCaster();

                    if (caster &&IsPlayer(caster))
                    {
                        WorldPacket data(SMSG_SPIRIT_HEALER_CONFIRM, 8);
                        data << unitTarget->GetObjectGuid();
                        ((Player*)caster)->GetSession()->SendPacket(&data);
                    }
                    return;
                }
                case 17271:
                {
                    if (!itemTarget && !IsPlayer(m_caster))
                    {
                        return;
                    }

                    uint32 spell_id = urand(0, 1)
                        ? 17269
                        : 17270;

                    m_caster->CastSpell(m_caster, spell_id, true, nullptr);
                    return;
                }
                case 18269:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    if (IsPlayer(unitTarget))
                    {
                        return;
                    }

                    ((Creature*)unitTarget)->ForcedDespawn();
                    return;
                }
                case 18350:
                {
                    if (!IsPlayer(unitTarget))
                    {
                        return;
                    }

                    Unit::SpellAuraHolderMap const& auras = unitTarget->GetSpellAuraHolderMap();
                    for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                    {
                        SpellEntry const* spell = itr->second->GetSpellProto();
                        if (spell->SpellClassSet == SPELLFAMILY_SHAMAN &&
                            (spell->SpellClassMask & UI64LIT(0x0000000000000400)))
                        {
                            return;
                        }
                    }
                    unitTarget->RemoveAuras(28820);
                    return;
                }
                case 20572:
                {
                    if (!IsPlayer(m_caster))
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, 23230, true);

                    damage = uint32(damage * (m_caster->GetTotalAttackPowerValue(BASE_ATTACK)) / 100);
                    m_caster->CastCustomSpell(m_caster, 23234, &damage, nullptr, nullptr, true, nullptr);
                    return;
                }
                case 20577:
                {
                    if (unitTarget)
                    {
                        AddTriggeredSpell(20578);
                    }
                    return;
                }
                case 21147:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    if (m_caster->GetThreatManager().getThreat(unitTarget))
                    {
                        m_caster->GetThreatManager().modifyThreatPercent(unitTarget, -100);
                    }

                    if (!unitTarget->HasAura(23186))
                    {
                        m_caster->CastSpell(unitTarget, 21150, true);
                    }

                    return;
                }
                case 23019:
                {
                    if (!unitTarget || !unitTarget->IsAlive() || !IsCreature(unitTarget) || ((Creature*)unitTarget)->IsPet())
                    {
                        return;
                    }

                    Creature* creatureTarget = (Creature*)unitTarget;
                    if (creatureTarget->IsPet())
                    {
                        return;
                    }

                    creatureTarget->CastSpell(creatureTarget, 23022, true);
                    creatureTarget->ForcedDespawn();
                    return;
                }
                case 23448:
                {
                    int32 r = irand(0, 119);
                    if (r < 20)
                    {
                        m_caster->CastSpell(m_caster, 23444, true);
                    }
                    else if (r < 100)
                    {
                        m_caster->CastSpell(m_caster, 23445, true);
                    }
                    else
                    {
                        m_caster->CastSpell(m_caster, 36902, true);
                    }

                    return;
                }
                case 23453:
                {
                    if (roll_chance_i(50))
                    {
                        m_caster->CastSpell(m_caster, 23441, true);
                    }
                    else
                    {
                        m_caster->CastSpell(m_caster, 23446, true);
                    }

                    return;
                }
                case 23645:
                    m_caster->RemoveAuras(23170);
                    return;
                case 23725:
                {
                    int32 basepoints = m_caster->GetMaxHealth() * 0.15;
                    m_caster->CastCustomSpell(m_caster, 23782, &basepoints, nullptr, nullptr, true, nullptr);
                    m_caster->CastCustomSpell(m_caster, 23783, &basepoints, nullptr, nullptr, true, nullptr);
                    return;
                }
                case 24781:
                {
                    if (!IsCreature(m_caster) || !unitTarget)
                    {
                        return;
                    }

                    ((Creature*)m_caster)->AI()->AttackStart(unitTarget);
                    return;
                }
                case 25860:
                {
                    if (!m_caster->HasAuraType(SPELL_AURA_MOUNTED))
                    {
                        return;
                    }

                    float speed = m_caster->Pacing().RateOf(MOVE_RUN);

                    m_caster->RemoveAurasOfType(SPELL_AURA_MOUNTED);

                    if (speed >= 2.0f)
                    {
                        m_caster->CastSpell(m_caster, 25859, true);
                    }
                    else

                    {
                        m_caster->CastSpell(m_caster, 25858, true);
                    }

                    return;
                }
                case 26074:

                    return;
                case 28098:
                case 28110:
                {
                    if (!IsCreature(unitTarget))
                    {
                        return;
                    }

                    if (m_caster->getVictim() && !InReach(*m_caster, *unitTarget, 60.0f))
                    {

                        if (Unit* pTarget = ((Creature*)m_caster)->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                        {
                            unitTarget->CastSpell(pTarget, 28099, false);
                        }
                    }
                    else
                    {

                        unitTarget->RemoveAuras(m_spellInfo->ID == 28098 ? 28097 : 28109);
                        unitTarget->DeleteThreatList();
                        unitTarget->CombatStop(true);

                        unitTarget->CastSpell(m_caster, m_spellInfo->ID == 28098 ? 28096 : 28111, false);
                    }
                    return;
                }
            }

            switch (m_spellInfo->SpellIconID)
            {

                case 1661:
                {
                    uint32 healthPerc = uint32((float(m_caster->GetHealth()) / m_caster->GetMaxHealth()) * 100);
                    int32 speed_mod = 10;
                    if (healthPerc <= 40)
                    {
                        speed_mod = 30;
                    }
                    if (healthPerc < 100 && healthPerc > 40)
                    {
                        speed_mod = 10 + (100 - healthPerc) / 3;
                    }

                    int32 hasteModBasePoints0 = speed_mod;
                    int32 hasteModBasePoints1 = speed_mod;
                    int32 hasteModBasePoints2 = speed_mod;

                    m_caster->ModifyAuraState(AURA_STATE_BERSERKING, true);
                    m_caster->CastCustomSpell(m_caster, 26635, &hasteModBasePoints0, &hasteModBasePoints1, &hasteModBasePoints2, true, nullptr);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            switch (m_spellInfo->ID)
            {
                case 11189:
                case 28332:
                {
                    if (!unitTarget || !IsPlayer(unitTarget))
                    {
                        return;
                    }

                    SpellModifier *mod = new SpellModifier(SPELLMOD_RESIST_MISS_CHANCE, SPELLMOD_FLAT, damage, m_spellInfo->ID, UI64LIT(0x0000000000000100));
                    ((Player*)unitTarget)->SpellMods().Add(mod, true);
                    break;
                }
                case 12472:
                {
                    if (!IsPlayer(m_caster))
                    {
                        return;
                    }

                    const SpellCooldowns& cm = ((Player*)m_caster)->GetSpellCooldownMap();
                    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
                    {
                        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

                        if (spellInfo->SpellClassSet == SPELLFAMILY_MAGE &&
                            (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST) &&
                            spellInfo->ID != m_spellInfo->ID && GetSpellRecoveryTime(spellInfo) > 0)
                        {
                            ((Player*)m_caster)->RemoveSpellCooldown((itr++)->first, true);
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {

            if (m_spellInfo->SpellClassMask & UI64LIT(0x20000000))
            {
                if (!unitTarget)
                {
                    return;
                }

                int32 basePoints0 = damage + int32(m_caster->GetPower(POWER_RAGE) * operation.chainAmplitude);
                m_caster->CastCustomSpell(unitTarget, 20647, &basePoints0, nullptr, nullptr, true, 0);
                m_caster->SetPower(POWER_RAGE, 0);
                return;
            }

            if (m_spellInfo->ID == 21977)
            {
                if (!unitTarget)
                {
                    return;
                }

                m_caster->CastSpell(unitTarget, 21887, true);
                return;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {

            if (m_spellInfo->SpellClassMask & UI64LIT(0x0000000000040000))
            {
                float cost = m_currentBasePoints[EFFECT_INDEX_0];

                if (Player* modOwner = m_caster->GetSpellModOwner())
                {
                    modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_COST, cost, this);
                }

                int32 dmg = m_caster->SpellDamageBonusDone(m_caster, m_spellInfo, uint32(cost > 0 ? cost : 0), SPELL_DIRECT_DAMAGE);
                dmg = m_caster->SpellDamageBonusTaken(m_caster, m_spellInfo, dmg, SPELL_DIRECT_DAMAGE);

                if (int32(m_caster->GetHealth()) > dmg)
                {

                    m_caster->ModifyHealth(-dmg);

                    int32 mana = dmg;

                    const auto auraDummy = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto* aura : auraDummy)
                    {
                        if (aura->GetSpellProto()->SpellClassSet == SPELLFAMILY_WARLOCK && aura->GetSpellProto()->SpellIconID == 208)
                        {
                            mana = (aura->GetModifier()->m_amount + 100) * mana / 100;
                        }
                    }

                    m_caster->CastCustomSpell(m_caster, 31818, &mana, nullptr, nullptr, true);

                    int32 manaFeedVal = m_caster->CalculateSpellDamage(m_caster, Recipe(), Recipe().At(EFFECT_INDEX_1));
                    manaFeedVal = manaFeedVal * mana / 100;
                    if (manaFeedVal > 0)
                    {
                        m_caster->CastCustomSpell(m_caster, 32553, &manaFeedVal, nullptr, nullptr, true, nullptr);
                    }
                }
                else
                {
                    SendCastResult(SPELL_FAILED_FIZZLE);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            switch (m_spellInfo->ID)
            {
                case 28598:
                {
                    if (!unitTarget || !m_triggeredByAuraSpell)
                    {
                        return;
                    }

                    uint32 spellid = 0;
                    switch (m_triggeredByAuraSpell->ID)
                    {
                        case 2652:  spellid =  2943; break;
                        case 19261: spellid = 19249; break;
                        case 19262: spellid = 19251; break;
                        case 19264: spellid = 19252; break;
                        case 19265: spellid = 19253; break;
                        case 19266: spellid = 19254; break;
                        case 25461: spellid = 25460; break;
                        default:
                            sLog.outError("Spell::EffectDummy: Spell 28598 triggered by unhandeled spell %u", m_triggeredByAuraSpell->ID);
                            return;
                    }
                    m_caster->CastSpell(unitTarget, spellid, true, nullptr);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {

            if (m_spellInfo->ID == 29201)
            {
                uint32 spellid = 0;
                switch (unitTarget->getClass())
                {
                    case CLASS_PALADIN: spellid = 29196; break;
                    case CLASS_PRIEST: spellid = 29185; break;
                    case CLASS_SHAMAN: spellid = 29198; break;
                    case CLASS_DRUID: spellid = 29194; break;
                    default: break;
                }
                if (spellid != 0)
                {
                    m_caster->CastSpell(unitTarget, spellid, true, nullptr);
                }
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
            break;
        case SPELLFAMILY_HUNTER:
        {

            if (m_spellInfo->SpellClassMask & UI64LIT(0x100000000))
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }

                bool found = false;

                const auto decSpeedList = unitTarget->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
                for (auto* aura : decSpeedList)
                {
                    if (aura->GetSpellProto()->SpellIconID == 15 && aura->GetSpellProto()->DispelType == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    m_damage += damage;
                }
                return;
            }

            switch (m_spellInfo->ID)
            {
                case 23989:
                {
                    if (!IsPlayer(m_caster))
                    {
                        return;
                    }

                    const SpellCooldowns& cm = ((Player*)m_caster)->GetSpellCooldownMap();
                    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
                    {
                        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

                        if (spellInfo->SpellClassSet == SPELLFAMILY_HUNTER && spellInfo->ID != 23989 && GetSpellRecoveryTime(spellInfo) > 0)
                        {
                            ((Player*)m_caster)->RemoveSpellCooldown((itr++)->first, true);
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            switch (m_spellInfo->SpellIconID)
            {
                case 156:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    int hurt = 0;
                    int heal = 0;

                    switch (m_spellInfo->ID)
                    {
                        case 20473: hurt = 25912; heal = 25914; break;
                        case 20929: hurt = 25911; heal = 25913; break;
                        case 20930: hurt = 25902; heal = 25903; break;
                        default:
                            sLog.outError("Spell::EffectDummy: Spell %u not handled in HS", m_spellInfo->ID);
                            return;
                    }

                    if (IsFriendly(*m_caster, *unitTarget))
                    {
                        m_caster->CastSpell(unitTarget, heal, true);
                    }
                    else
                    {
                        m_caster->CastSpell(unitTarget, hurt, true);
                    }

                    return;
                }
                case 561:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = m_currentBasePoints[eff_idx];
                    SpellEntry const* spell_proto = sSpellStore.LookupEntry(spell_id);
                    if (!spell_proto)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, spell_proto, true, nullptr);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {

            if (m_spellInfo->SpellClassMask & UI64LIT(0x0000000000200000))
            {
                if (!m_CastItem)
                {
                    sLog.outError("Spell::EffectDummy: spell %i requires cast Item", m_spellInfo->ID);
                    return;
                }

                int32 spellDamage = m_caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(m_spellInfo));
                float weaponSpeed = (1.0f / IN_MILLISECONDS) * m_CastItem->GetProto()->Delay;
                int32 totalDamage = int32((damage + 3.85f * spellDamage) * 0.01 * weaponSpeed);

                m_caster->CastCustomSpell(unitTarget, 10444, &totalDamage, nullptr, nullptr, true, m_CastItem);
                return;
            }

            break;
        }
    }

    if (PetAura const* petSpell = sSpellMgr.GetPetAura(m_spellInfo->ID))
    {
        m_caster->AddPetAura(petSpell);
        return;
    }

    bool libraryResult = false;
    if (gameObjTarget)
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, eff_idx, gameObjTarget, m_originalCasterGUID);
    }
    else if (unitTarget && (IsCreature(unitTarget) ||IsPlayer(unitTarget)))
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, eff_idx, unitTarget, m_originalCasterGUID);
    }
    else if (itemTarget)
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, eff_idx, itemTarget, m_originalCasterGUID);
    }

    if (libraryResult || !unitTarget)
    {
        return;
    }

    if (!ScriptMgr::CanSpellEffectStartDBScript(m_spellInfo, eff_idx))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectDummy", m_spellInfo->ID);
    m_caster->GetMap()->Scripts().Start(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
}
