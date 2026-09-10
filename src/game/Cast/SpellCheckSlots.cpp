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

SpellCastResult Spell::CheckEachSlotCanRun()
{
    for (const auto& operation : Recipe().Does())
    {
        const int i = operation.slot;

        switch (operation.verb)
        {
            case SPELL_EFFECT_DUMMY:
            {
                if (m_spellInfo->SpellIconID == 1648)
                {
                    if (!m_targets.getUnitTarget() || m_targets.getUnitTarget()->GetHealth() > m_targets.getUnitTarget()->GetMaxHealth() * 0.2)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
                else if (m_spellInfo->SpellIconID == 156)
                {

                    if (m_targets.getUnitTarget() && !IsFriendly(*m_caster, *m_targets.getUnitTarget()) && !m_caster->Where().HasInArc(m_targets.getUnitTarget()->Where(), M_PI_F))
                    {
                        return SPELL_FAILED_UNIT_NOT_INFRONT;
                    }
                }
                break;
            }
            case SPELL_EFFECT_DISTRACT:
            {
                if (m_targets.m_targetMask & (TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_SOURCE_LOCATION))
                {
                    UnitList targetsCombat;
                    float radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(operation.radiusIndex));

                    FillAreaTargets(targetsCombat, radius, cast::Around::Spot, cast::Side::HostileForArea);

                    if (targetsCombat.empty())
                    {
                        break;
                    }

                    for (UnitList::iterator itr = targetsCombat.begin(); itr != targetsCombat.end(); ++itr)
                    {
                        if ((*itr)->IsInCombat())
                        {
                            return SPELL_FAILED_TARGET_IN_COMBAT;
                        }
                    }
                }
                break;
            }
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            {

                if (m_spellInfo->SpellVisualID == 7250)
                {
                    if (!m_targets.getUnitTarget())
                    {
                        return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                    }

                    if (m_targets.getUnitTarget()->GetHealth() > m_targets.getUnitTarget()->GetMaxHealth() * 0.2)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }

                else if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARLOCK && m_spellInfo->SpellClassMask & UI64LIT(0x0000000000000200))
                {
                    if (!m_targets.getUnitTarget())
                    {
                        return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                    }

                    bool found = false;
                    const auto mPeriodic = m_targets.getUnitTarget()->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                    for (auto* aura : mPeriodic)
                    {
                        if (aura->GetSpellProto()->SpellClassSet == SPELLFAMILY_WARLOCK &&
                            aura->GetCasterGuid() == m_caster->GetObjectGuid() &&

                            (aura->GetSpellProto()->SpellClassMask & UI64LIT(0x0000000000000004)))
                        {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        return SPELL_FAILED_TARGET_AURASTATE;
                    }
                }
                break;
            }
            case SPELL_EFFECT_TAMECREATURE:
            {
                if (m_triggeredBySpellInfo == nullptr)
                {
                    SpellCastResult castResult = CanTameUnit(true);
                    if (castResult != SPELL_CAST_OK)
                    {
                        return castResult;
                    }
                }
                break;
            }
            case SPELL_EFFECT_LEARN_SPELL:
            {
                if (operation.targetA != TARGET_PET)
                {
                    break;
                }

                Pet* pet = m_caster->GetPet();

                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                SpellEntry const* learn_spellproto = sSpellStore.LookupEntry(operation.triggerSpell);

                if (!learn_spellproto)
                {
                    return SPELL_FAILED_NOT_KNOWN;
                }

                if (!pet->CanTakeMoreActiveSpells(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TOO_MANY_SKILLS;
                }

                if (m_spellInfo->SpellLevel > pet->getLevel())
                {
                    return SPELL_FAILED_LOWLEVEL;
                }

                if (!pet->HasTPForSpell(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TRAINING_POINTS;
                }

                break;
            }
            case SPELL_EFFECT_LEARN_PET_SPELL:
            {
                Pet* pet = m_caster->GetPet();

                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                SpellEntry const* learn_spellproto = sSpellStore.LookupEntry(operation.triggerSpell);

                if (!learn_spellproto)
                {
                    return SPELL_FAILED_NOT_KNOWN;
                }

                if (!pet->CanTakeMoreActiveSpells(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TOO_MANY_SKILLS;
                }

                if (m_spellInfo->SpellLevel > pet->getLevel())
                {
                    return SPELL_FAILED_LOWLEVEL;
                }

                if (!pet->HasTPForSpell(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TRAINING_POINTS;
                }

                break;
            }
            case SPELL_EFFECT_FEED_PET:
            {
                if (!IsPlayer(m_caster))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                Item* foodItem = m_targets.getItemTarget();
                if (!foodItem)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                Pet* pet = m_caster->GetPet();

                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                if (!pet->HaveInDiet(foodItem->GetProto()))
                {
                    return SPELL_FAILED_WRONG_PET_FOOD;
                }

                if (!pet->GetCurrentFoodBenefitLevel(foodItem->GetProto()->ItemLevel))
                {
                    return SPELL_FAILED_FOOD_LOWLEVEL;
                }

                if (pet->IsInCombat())
                {
                    return SPELL_FAILED_AFFECTING_COMBAT;
                }

                break;
            }
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_POWER_DRAIN:
            {

                if (IsPlayer(m_caster))
                {
                    if (Unit* target = m_targets.getUnitTarget())
                    {
                        if (target != m_caster && int32(target->GetPowerType()) != operation.miscValue)
                        {
                            return SPELL_FAILED_BAD_TARGETS;
                        }
                    }
                }
                break;
            }
            case SPELL_EFFECT_CHARGE:
            {
                if (m_caster->hasUnitState(UNIT_STAT_ROOT))
                {
                    return SPELL_FAILED_ROOTED;
                }

                break;
            }
            case SPELL_EFFECT_SKINNING:
            {
                if (!IsPlayer(m_caster) || !m_targets.getUnitTarget() || !IsCreature(m_targets.getUnitTarget()))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (!m_targets.getUnitTarget()->HasUnitFlag(UNIT_FLAG_SKINNABLE))
                {
                    return SPELL_FAILED_TARGET_UNSKINNABLE;
                }

                Creature* creature = (Creature*)m_targets.getUnitTarget();
                if (creature->GetCreatureType() != CREATURE_TYPE_CRITTER && (!creature->Taking().BodyTaken() || creature->Taking().Skinned() || !creature->loot.empty()))
                {
                    return SPELL_FAILED_TARGET_NOT_LOOTED;
                }

                uint32 skill = creature->Record().RequiredLootSkill();

                int32 skillValue = ((Player*)m_caster)->GetSkillValue(skill);
                int32 TargetLevel = m_targets.getUnitTarget()->getLevel();
                int32 ReqValue = (skillValue < 100 ? (TargetLevel - 10) * 10 : TargetLevel * 5);
                if (ReqValue > skillValue)
                {
                    return SPELL_FAILED_SKILL_NOT_HIGH_ENOUGH;
                }

                if (m_spellState != SPELL_STATE_CREATED &&
                    skillValue < sWorld.GetConfigMaxSkillValue() &&
                    (ReqValue < 0 ? 0 : ReqValue) > irand(skillValue - 25, skillValue + 37))
                {
                    return SPELL_FAILED_TRY_AGAIN;
                }

                break;
            }
            case SPELL_EFFECT_OPEN_LOCK_ITEM:
            case SPELL_EFFECT_OPEN_LOCK:
            {
                if (!IsPlayer(m_caster))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (operation.targetA == TARGET_GAMEOBJECT)
                {
                    if (!m_targets.getGOTarget())
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }

                uint32 lockId;
                if (GameObject* go = m_targets.getGOTarget())
                {

                    if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST && go->GetGoState() == GO_STATE_ACTIVE)
                    {
                        return SPELL_FAILED_CHEST_IN_USE;
                    }

                    if (((Player*)m_caster)->Battle().InOne() &&
                        !((Player*)m_caster)->CanUseBattleGroundObject())
                    {
                        return SPELL_FAILED_TRY_AGAIN;
                    }

                    lockId = go->GetGOInfo()->GetLockId();
                    if (!lockId)
                    {
                        return SPELL_FAILED_ALREADY_OPEN;
                    }

                    if (!IsLockInRange(go) && go->GetGoType() == GAMEOBJECT_TYPE_TRAP)
                    {
                        return SPELL_FAILED_OUT_OF_RANGE;
                    }
                }
                else if (Item* item = m_targets.getItemTarget())
                {

                    if (item->GetOwner() != m_caster)
                    {
                        return SPELL_FAILED_ITEM_GONE;
                    }

                    lockId = item->GetProto()->LockID;

                    if (!lockId || item->HasItemFlag(ITEM_DYNFLAG_UNLOCKED))
                    {
                        return SPELL_FAILED_ALREADY_OPEN;
                    }
                }
                else
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                SkillType skillId = SKILL_NONE;
                int32 reqSkillValue = 0;
                int32 skillValue = 0;

                SpellCastResult res = CanOpenLock(SpellEffectIndex(i), lockId, skillId, reqSkillValue, skillValue);
                if (res != SPELL_CAST_OK)
                {
                    return res;
                }

                if (m_spellState != SPELL_STATE_CREATED && skillId != SKILL_NONE)
                {
                    bool canFailAtMax = skillId != SKILL_HERBALISM && skillId != SKILL_MINING;

                    if ((canFailAtMax || skillValue < sWorld.GetConfigMaxSkillValue()) && reqSkillValue > irand(skillValue - 25, skillValue + 37))
                    {
                        return SPELL_FAILED_TRY_AGAIN;
                    }
                }
                break;
            }
            case SPELL_EFFECT_SUMMON_DEAD_PET:
            {
                Creature* pet = m_caster->GetPet();

                if (pet && pet->IsAlive())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (!pet)
                {
                    if (Player* player = static_cast<Player*>(m_caster))
                    {
                        PetDatabaseStatus status = Pet::GetStatusFromDB(player);
                        if (status == PET_DB_NO_PET)
                        {
                            return SPELL_FAILED_NO_PET;
                        }
                        else if (status == PET_DB_ALIVE)
                        {
                            return SPELL_FAILED_TARGET_NOT_DEAD;
                        }
                    }
                    else
                    {
                        return SPELL_FAILED_NO_PET;
                    }
                }

                break;
            }

            case SPELL_EFFECT_SUMMON:
            case SPELL_EFFECT_SUMMON_POSSESSED:
            case SPELL_EFFECT_SUMMON_PHANTASM:
            case SPELL_EFFECT_SUMMON_DEMON:
            {
                if (m_caster->GetPetGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                break;
            }
            case SPELL_EFFECT_SUMMON_PET:
            {
                Player* plr = static_cast<Player*>(m_caster);
                if (m_caster->GetPetGuid())
                {
                    if (plr && m_caster->getClass() != CLASS_WARLOCK)
                    {
                        return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                    }
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (plr)
                {
                    PetDatabaseStatus status = Pet::GetStatusFromDB(plr);
                    if (status == PET_DB_DEAD)
                    {
                        return SPELL_FAILED_TARGETS_DEAD;
                    }
                    else if ((plr->getClass() == CLASS_HUNTER) && (status == PET_DB_NO_PET))
                    {
                        return SPELL_FAILED_NO_PET;
                    }
                }
                break;
            }
            case SPELL_EFFECT_SUMMON_PLAYER:
            {
                if (!IsPlayer(m_caster))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
                if (!((Player*)m_caster)->GetSelectionGuid())
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                Player* target = sObjectMgr.GetPlayer(((Player*)m_caster)->GetSelectionGuid());
                if (!target || ((Player*)m_caster) == target || !target->IsInSameRaidWith((Player*)m_caster))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (sMapStore.LookupEntry(m_caster->GetMapId())->IsDungeon())
                {
                    InstanceTemplate const* instance = ObjectMgr::GetInstanceTemplate(m_caster->GetMapId());
                    if (m_caster->GetMap() != target->GetMap())
                    {
                        return SPELL_FAILED_TARGET_NOT_IN_INSTANCE;
                    }
                    if (instance->levelMin > target->getLevel())
                    {
                        return SPELL_FAILED_LOWLEVEL;
                    }
                    if (instance->levelMax && instance->levelMax < target->getLevel())
                    {
                        return SPELL_FAILED_HIGHLEVEL;
                    }
                }
                break;
            }
            case SPELL_EFFECT_LEAP:
            case SPELL_EFFECT_TELEPORT_UNITS_FACE_CASTER:
            {
                if (!m_caster || m_caster->IsTaxiFlying())
                {
                    return SPELL_FAILED_NOT_ON_TAXI;
                }

                if (operation.verb != SPELL_EFFECT_LEAP)
                {
                    if (m_caster->hasUnitState(UNIT_STAT_ROOT))
                    {
                        return SPELL_FAILED_ROOTED;
                    }
                }

                if (IsPlayer(m_caster))
                {
                    if (((Player*)m_caster)->HasMovementFlag(MOVEFLAG_ONTRANSPORT))
                    {
                        return SPELL_FAILED_NOT_ON_TRANSPORT;
                    }

                    if (BattleGround const* bg = ((Player*)m_caster)->Battle().Ground())
                    {
                        if (bg->GetStatus() != STATUS_IN_PROGRESS)
                        {
                            return SPELL_FAILED_TRY_AGAIN;
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckEachAuraCanHold()
{
    for (const auto& operation : Recipe().Does())
    {
        const int i = operation.slot;

        if (!IsAuraApplyEffect(m_spellInfo, SpellEffectIndex(i)))
        {
            continue;
        }

        Unit* expectedTarget = m_caster->GetMap() ? m_caster->GetMap()->GetUnit(GetPrefilledOrUnitTargetGuid(SpellEffectIndex(i))) : nullptr;

        switch (operation.aura)
        {
            case SPELL_AURA_MOD_POSSESS:
            {
                if (!IsPlayer(m_caster))
                {
                    return SPELL_FAILED_UNKNOWN;
                }

                if (expectedTarget == m_caster)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (m_caster->GetPetGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (m_caster->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (expectedTarget->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (int32(expectedTarget->getLevel()) > CalculateDamage(SpellEffectIndex(i), expectedTarget))
                {
                    return SPELL_FAILED_HIGHLEVEL;
                }
                break;
            }
            case SPELL_AURA_MOD_CHARM:
            {
                if (expectedTarget == m_caster)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (m_caster->GetPetGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (m_caster->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (expectedTarget->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (int32(expectedTarget->getLevel()) > CalculateDamage(SpellEffectIndex(i), expectedTarget))
                {
                    return SPELL_FAILED_HIGHLEVEL;
                }
                break;
            }
            case SPELL_AURA_MOD_POSSESS_PET:
            {
                if (!IsPlayer(m_caster))
                {
                    return SPELL_FAILED_UNKNOWN;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (m_caster->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                Pet* pet = m_caster->GetPet();
                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                if (pet->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }
                break;
            }
            case SPELL_AURA_MOUNTED:
            {
                if (m_caster->IsInWater())
                {
                    return SPELL_FAILED_ONLY_ABOVEWATER;
                }

                if (IsPlayer(m_caster) && ((Player*)m_caster)->GetTransport())
                {
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;
                }

                bool isAQ40Mounted = false;

                switch (m_spellInfo->ID)
                {
                    case 25863:
                    case 26655:
                    case 26656:
                    case 31700:
                        if (m_caster->GetMapId() == 531)
                        {
                            isAQ40Mounted = true;
                        }
                        break;
                    case 25953:
                    case 26054:
                    case 26055:
                    case 26056:
                        if (m_caster->GetMapId() == 531)
                        {
                            isAQ40Mounted = true;
                            break;
                        }
                        else
                        {
                            return SPELL_FAILED_NOT_HERE;
                        }
                    default:
                        break;
                }

                if (!isAQ40Mounted &&IsPlayer(m_caster) && !sMapStore.LookupEntry(m_caster->GetMapId())->IsMountAllowed() && !m_IsTriggeredSpell)
                {
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;
                }

                if (m_caster->GetTerrain()->GetAreaId(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z()) == 35)
                {
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;
                }

                if (m_caster->IsInDisallowedMountForm())
                {
                    return SPELL_FAILED_NOT_SHAPESHIFT;
                }
                break;
            }
            case SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS:
            {
                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (IsFriendly(*m_caster, *expectedTarget))
                {
                    return SPELL_FAILED_TARGET_FRIENDLY;
                }
                break;
            }
            case SPELL_AURA_PERIODIC_MANA_LEECH:
            {
                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (!IsPlayer(m_caster) || m_CastItem)
                {
                    break;
                }

                if (expectedTarget->GetPowerType() != POWER_MANA)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
                break;
            }
            case SPELL_AURA_WATER_WALK:
            {
                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (IsPlayer(expectedTarget))
                {
                    Player const* player = static_cast<Player const*>(expectedTarget);

                    if (player->GetShapeshiftForm() != FORM_NONE || player->IsMounted())
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
            }
            default:
                break;
        }
    }

    return SPELL_CAST_OK;
}
