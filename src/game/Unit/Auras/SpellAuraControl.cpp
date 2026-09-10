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
#include <ctime>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Mint.h"
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
#include "Cast/Recipe/RecipeBook.h"

static AuraType const frozenAuraTypes[] = { SPELL_AURA_MOD_ROOT, SPELL_AURA_MOD_STUN, SPELL_AURA_NONE };

void Aura::HandleModPossess(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (GetCasterGuid() == target->GetObjectGuid())
    {
        return;
    }

    Unit* caster = GetCaster();
    if (!caster || !IsPlayer(caster))
    {
        return;
    }

    Player* p_caster = (Player*)caster;
    Camera& camera = p_caster->GetCamera();

    if (apply)
    {
        target->addUnitState(UNIT_STAT_CONTROLLED);

        target->SetUnitFlag(UNIT_FLAG_POSSESSED);
        target->SetCharmerGuid(p_caster->GetObjectGuid());
        target->setFaction(p_caster->getFaction());

        camera.SetView(target);

        p_caster->SetCharm(target);
        p_caster->SetClientControl(target, 1);
        p_caster->SetMover(target);

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();

        CharmInfo& charmInfo = target->InitCharmInfo();
        charmInfo.InitPossessCreateSpells();
        charmInfo.SetReactState(REACT_PASSIVE);
        charmInfo.SetCommandState(COMMAND_STAY);

        p_caster->PossessSpellInitialize();

        if (IsCreature(target))
        {
            ((Creature*)target)->AIM_Initialize();
        }
        else if (IsPlayer(target))
        {
            ((Player*)target)->SetClientControl(target, 0);
        }
    }
    else
    {
        p_caster->SetCharm(nullptr);

        p_caster->SetClientControl(target, 0);
        p_caster->SetMover(nullptr);

        camera.ResetView();

        p_caster->RemovePetActionBar();

        if (m_removeMode == AURA_REMOVE_BY_DELETE)
        {
            return;
        }

        target->clearUnitState(UNIT_STAT_CONTROLLED);

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();

        target->RemoveUnitFlag(UNIT_FLAG_POSSESSED);

        target->SetCharmerGuid(0);

        if (IsPlayer(target))
        {
            ((Player*)target)->setFactionForRace(target->getRace());
            ((Player*)target)->SetClientControl(target, 1);
        }
        else if (IsCreature(target))
        {
            CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();
            target->setFaction(cinfo->FactionAlliance);
        }

        if (IsCreature(target))
        {
            ((Creature*)target)->AIM_Initialize();
            target->AttackedBy(caster);
        }
    }
}

void Aura::HandleModPossessPet(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* caster = GetCaster();
    if (!caster || !IsPlayer(caster))
    {
        return;
    }

    Unit* target = GetTarget();
    if (!IsCreature(target) || !((Creature*)target)->IsPet())
    {
        return;
    }

    Pet* pet = (Pet*)target;

    Player* p_caster = (Player*)caster;
    Camera& camera = p_caster->GetCamera();

    if (apply)
    {
        pet->addUnitState(UNIT_STAT_CONTROLLED);

        camera.SetView(pet);

        p_caster->SetCharm(pet);
        p_caster->SetClientControl(pet, 1);
        ((Player*)caster)->SetMover(pet);

        pet->SetUnitFlag(UNIT_FLAG_POSSESSED);

        pet->StopMoving();
        pet->GetMotionMaster()->Clear(false);
        pet->GetMotionMaster()->MoveIdle();
    }
    else
    {
        p_caster->SetCharm(nullptr);
        p_caster->SetClientControl(pet, 0);
        p_caster->SetMover(nullptr);

        camera.ResetView();

        if (m_removeMode == AURA_REMOVE_BY_DELETE)
        {
            return;
        }

        pet->clearUnitState(UNIT_STAT_CONTROLLED);

        pet->RemoveUnitFlag(UNIT_FLAG_POSSESSED);

        pet->AttackStop();

        if (!InReach(*pet, *p_caster, pet->GetMap()->GetVisibilityDistance()))
        {
            p_caster->RemovePet(PET_SAVE_REAGENTS);
        }
        else
        {
            pet->GetMotionMaster()->MoveFollow(caster, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
        }
    }
}

void Aura::HandleModCharm(bool apply, bool Real)
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

    if (GetCasterGuid() == target->GetObjectGuid())
    {
        return;
    }

    Unit* caster = GetCaster();
    if (!caster)
    {
        return;
    }

    if (apply)
    {

        target->RemoveAurasOfType(SPELL_AURA_MOD_CHARM, GetHolder());
        target->RemoveAurasOfType(SPELL_AURA_MOD_POSSESS, GetHolder());

        target->SetCharmerGuid(GetCasterGuid());
        target->setFaction(caster->getFaction());
        target->CastStop(target == caster ? GetId() : 0);
        caster->SetCharm(target);

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();
        target->GetMotionMaster()->MovementExpired(true);

        if (IsCreature(target))
        {
            ((Creature*)target)->AIM_Initialize();
            CharmInfo& charmInfo = target->InitCharmInfo();
            charmInfo.InitCharmCreateSpells();
            charmInfo.SetReactState(REACT_DEFENSIVE);

            if (IsPlayer(caster) && caster->getClass() == CLASS_WARLOCK)
            {
                CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();
                if (cinfo && cinfo->CreatureType == CREATURE_TYPE_DEMON)
                {

                    if (target->getClass() == 0)
                    {
                        if (cinfo->UnitClass == 0)
                        {
                            sLog.outErrorDb("Creature (Entry: %u) have UnitClass = 0 but used in charmed spell, that will be result client crash.", cinfo->Entry);
                        }
                        else
                        {
                            sLog.outError("Creature (Entry: %u) have UnitClass = %u but at charming have class 0!!! that will be result client crash.", cinfo->Entry, cinfo->UnitClass);
                        }

                        target->SetClass(CLASS_MAGE);
                    }

                    charmInfo.SetPetNumber(sMint.PetNumbers().Next(), true);

                    target->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(nullptr)));
                }
            }
        }
        else if (Player *plTarget = static_cast<Player*>(target))
        {
            plTarget->SetClientControl(plTarget, 0);
        }

        if (IsPlayer(caster))
        {
            ((Player*)caster)->CharmSpellInitialize();
        }
    }
    else
    {
        target->SetCharmerGuid(0);

        if (Player *plTarget = static_cast<Player*>(target))
        {
            plTarget->SetClientControl(plTarget, 1);
            plTarget->setFactionForRace(target->getRace());
        }
        else
        {
            CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();

            if (((Creature*)target)->IsPet())
            {
                if (Unit* owner = target->GetOwner())
                {
                    target->setFaction(owner->getFaction());
                }
                else if (cinfo)
                {
                    target->setFaction(cinfo->FactionAlliance);
                }
            }
            else if (cinfo)
            {
                target->setFaction(cinfo->FactionAlliance);
            }

            if (cinfo &&IsPlayer(caster) && caster->getClass() == CLASS_WARLOCK && cinfo->CreatureType == CREATURE_TYPE_DEMON)
            {

                if (target->GetCharmInfo())
                {
                    target->GetCharmInfo()->SetPetNumber(0, true);
                }
                else
                {
                    sLog.outError("Aura::HandleModCharm: target (GUID: %u TypeId: %u) has a charm aura but no charm info!", target->GetGUIDLow(), target->GetTypeId());
                }
            }
        }

        caster->SetCharm(nullptr);

        if (IsPlayer(caster))
        {
            ((Player*)caster)->RemovePetActionBar();
        }

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();
        target->GetMotionMaster()->MovementExpired(true);

        if (IsCreature(target))
        {
            ((Creature*)target)->AIM_Initialize();

        }
    }
}

void Aura::HandleModConfuse(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    if (!apply && GetTarget()->HasAuraType(SPELL_AURA_MOD_CONFUSE))
    {
        return;
    }

    GetTarget()->SetConfused(apply, GetCasterGuid(), GetId());
}

void Aura::HandleModFear(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    if (!apply && GetTarget()->HasAuraType(SPELL_AURA_MOD_FEAR))
    {
        return;
    }

    GetTarget()->SetFeared(apply, GetCasterGuid(), GetId());
}

void Aura::HandleFeignDeath(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    GetTarget()->SetFeignDeath(apply, GetCasterGuid());
}

void Aura::HandleAuraModDisarm(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!apply && target->HasAuraType(GetModifier()->m_auraname))
    {
        return;
    }

    target->ApplyUnitFlag(UNIT_FLAG_DISARMED, apply);

    if (!IsPlayer(target))
    {
        return;
    }

    if (target->IsInFeralForm())
    {
        return;
    }

    if (apply)
    {
        target->SetAttackTime(BASE_ATTACK, BASE_ATTACK_TIME);
    }
    else
    {
        ((Player*)target)->SetRegularAttackTime();
    }

    target->Sheet().Swing(BASE_ATTACK);
}

void Aura::HandleAuraModStun(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {

        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        target->SetStunned(true);
    }
    else
    {

        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0]; *itr != SPELL_AURA_NONE; ++itr)
            {
                const auto auras = target->GetAurasByType(*itr);
                for (auto* aura : auras)
                {
                    if (GetSpellSchoolMask(aura->GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                {
                    break;
                }
            }

            if (!found_another)
            {
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
            }
        }

        if (target->HasAuraType(SPELL_AURA_MOD_STUN))
        {
            return;
        }

        target->SetStunned(false);

        if (GetSpellProto()->SpellClassSet == SPELLFAMILY_HUNTER && GetSpellProto()->SpellClassMask & UI64LIT(0x00010000))
        {
            Unit* caster = GetCaster();
            if (!caster || !IsPlayer(caster))
            {
                return;
            }

            uint32 spell_id;
            switch (GetId())
            {
                case 19386: spell_id = 24131; break;
                case 24132: spell_id = 24134; break;
                case 24133: spell_id = 24135; break;
                default:
                    sLog.outError("Spell selection called for unexpected original spell %u, new spell for this spell family?", GetId());
                    return;
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);

            if (!spellInfo)
            {
                return;
            }

            caster->CastSpell(target, spellInfo, true, nullptr, this);
            return;
        }
    }

    if (GetSpellProto()->IsFitToFamily(SPELLFAMILY_ROGUE, UI64LIT(0x80)))
    {
        target->SetInDummyCombatState(apply);
        if (!apply)
        {
            target->GetThreatManager().setDirty(true);
            GetCaster()->GetThreatManager().setDirty(true);
        }
    }
}

void Aura::HandleModStealth(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {

        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        if (Real)
        {
            target->SetCreeping(true);

            if (Player* player = static_cast<Player*>(target))
            {
                player->ApplyAuraVision(AURA_VISION_STEALTH, true);
            }

            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
                target->SetVisibility(VISIBILITY_GROUP_STEALTH);
            }

            if (IsPlayer(target) && GetId() == 20580)
            {
                target->CastSpell(target, 21009, true, nullptr, this);
            }
        }
    }
    else
    {

        if (Real &&IsPlayer(target) && GetId() == 20580)
        {
            target->RemoveAuras(21009);
        }

        if (m_removeMode == AURA_REMOVE_BY_CANCEL)
        {
            target->RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
        }

        if (Real && !target->HasAuraType(SPELL_AURA_MOD_STEALTH))
        {

            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->SetCreeping(false);

                if (Player* player = static_cast<Player*>(target))
                {
                    player->ApplyAuraVision(AURA_VISION_STEALTH, false);
                }

                if (target->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                {
                    target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
                    target->SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
                }
                else
                {
                    target->SetVisibility(VISIBILITY_ON);
                }
            }
        }
    }
}

void Aura::HandleInvisibility(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        target->m_invisibilityMask |= (1 << m_modifier.m_miscvalue);

        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        if (Real)
        {
            if (Player* player = static_cast<Player*>(target))
            {
                player->ApplyAuraVision(AURA_VISION_INVISIBILITY, true);
            }
        }

        if (target->GetVisibility() == VISIBILITY_ON)
        {

            target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
            target->SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
        }
    }
    else
    {

        target->m_invisibilityMask = 0;
        const auto auras = target->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY);
        for (auto* aura : auras)
        {
            target->m_invisibilityMask |= (1 << aura->GetModifier()->m_miscvalue);
        }

        if (Real && target->m_invisibilityMask == 0)
        {
            if (Player* player = static_cast<Player*>(target))
            {
                player->ApplyAuraVision(AURA_VISION_INVISIBILITY, false);
            }

            if (target->GetVisibility() != VISIBILITY_OFF)
            {

                if (!target->HasAuraType(SPELL_AURA_MOD_STEALTH))
                {
                    target->SetVisibility(VISIBILITY_ON);
                }
            }
        }
    }
}

void Aura::HandleInvisibilityDetect(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        target->m_detectInvisibilityMask |= (1 << m_modifier.m_miscvalue);
    }
    else
    {

        target->m_detectInvisibilityMask = 0;
        const auto auras = target->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY_DETECTION);
        for (auto* aura : auras)
        {
            target->m_detectInvisibilityMask |= (1 << aura->GetModifier()->m_miscvalue);
        }
    }
    if (Real &&IsPlayer(target))
    {
        ((Player*)target)->GetCamera().UpdateVisibilityForOwner();
    }
}

void Aura::HandleDetectAmore(bool apply, bool )
{
    if (Player* player = static_cast<Player*>(GetTarget()))
    {
        player->ApplyAuraVision(uint8(AURA_VISION_AMORE_0 << m_modifier.m_amount), apply);
    }
}

void Aura::HandleAuraModRoot(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {

        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        target->addUnitState(UNIT_STAT_ROOT);

        if (IsPlayer(target))
        {
            target->SetRoot(true);

            ((Player*)target)->m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
        }
        else
        {
            target->StopMoving();
        }
    }
    else
    {

        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0]; *itr != SPELL_AURA_NONE; ++itr)
            {
                const auto auras = target->GetAurasByType(*itr);
                for (auto* aura : auras)
                {
                    if (GetSpellSchoolMask(aura->GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                {
                    break;
                }
            }

            if (!found_another)
            {
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
            }
        }

        if (target->HasAuraType(SPELL_AURA_MOD_ROOT))
        {
            return;
        }

    }

    target->SetImmobilizedState(apply);
}

void Aura::HandleAuraModSilence(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        target->SetUnitFlag(UNIT_FLAG_SILENCED);

        for (uint32 i = CURRENT_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
        {
            if (Spell* spell = target->GetCurrentSpell(CurrentSpellTypes(i)))
            {
                if (spell->m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)

                {
                    target->InterruptSpell(CurrentSpellTypes(i), false);
                }
            }
        }
    }
    else
    {

        if (target->HasAuraType(SPELL_AURA_MOD_SILENCE))
        {
            return;
        }

        target->RemoveUnitFlag(UNIT_FLAG_SILENCED);
    }
}

void Aura::HandleModThreat(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target->IsAlive())
    {
        return;
    }

    int level_diff = 0;
    int multiplier = 0;
    switch (GetId())
    {

        case 26400:
            level_diff = target->getLevel() - 60;
            multiplier = 2;
            break;

        case 28862:
            level_diff = target->getLevel() - 60;
            multiplier = 1;
            break;
    }

    if (level_diff > 0)
    {
        m_modifier.m_amount += multiplier * level_diff;
    }

    if (IsPlayer(target))
    {
        for (int8 x = 0; x < MAX_SPELL_SCHOOL; ++x)
        {
            if (m_modifier.m_miscvalue & int32(1 << x))
            {
                ApplyPercentModFloatVar(target->m_threatModifier[x], float(m_modifier.m_amount), apply);
            }
        }
    }
}

void Aura::HandleAuraModTotalThreat(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target->IsAlive() || !IsPlayer(target))
    {
        return;
    }

    Unit* caster = GetCaster();

    if (!caster || !caster->IsAlive())
    {
        return;
    }

    float threatMod = apply ? float(m_modifier.m_amount) : float(-m_modifier.m_amount);

    target->GetHostileRefManager().threatAssist(caster, threatMod, GetSpellProto());
}

void Aura::HandleModTaunt(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target->IsAlive() || !target->CanHaveThreatList())
    {
        return;
    }

    Unit* caster = GetCaster();

    if (!caster || !caster->IsAlive())
    {
        return;
    }

    if (apply)
    {
        target->TauntApply(caster);
    }
    else
    {

        target->TauntFadeOut(caster);
    }
}

void Aura::HandleAuraModIncreaseSpeed(bool , bool Real)
{

    if (!Real)
    {
        return;
    }

    if (Unit* caster = GetCaster())
    {
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_SPEED, m_modifier.m_amount);
        }
    }

    GetTarget()->Pacing().Reckon(MOVE_RUN, true);
}

void Aura::HandleAuraModIncreaseMountedSpeed(bool , bool Real)
{

    if (!Real)
    {
        return;
    }

    GetTarget()->Pacing().Reckon(MOVE_RUN, true);
}

void Aura::HandleAuraModIncreaseSwimSpeed(bool , bool Real)
{

    if (!Real)
    {
        return;
    }

    GetTarget()->Pacing().Reckon(MOVE_SWIM, true);
}

void Aura::HandleAuraModDecreaseSpeed(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    if (Unit* caster = GetCaster())
    {
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_SPEED, m_modifier.m_amount);
        }
    }

    Unit* target = GetTarget();

    target->Pacing().Reckon(MOVE_RUN, true);
    target->Pacing().Reckon(MOVE_SWIM, true);
}

void Aura::HandleAuraModUseNormalSpeed(bool , bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    target->Pacing().Reckon(MOVE_RUN, true);
    target->Pacing().Reckon(MOVE_SWIM, true);
}

void Aura::HandleModMechanicImmunity(bool apply, bool )
{
    uint32 misc  = m_modifier.m_miscvalue;
    Unit* target = GetTarget();

    if (apply && Recipe().Says().dispelsOnImmunity)
    {
        uint32 mechanic = 1 << (misc - 1);

        target->RemoveAurasAtMechanicImmunity(mechanic, GetId());
    }

    target->ApplySpellImmune(GetId(), IMMUNITY_MECHANIC, misc, apply);
}

void Aura::HandleModMechanicImmunityMask(bool apply, bool )
{
    uint32 mechanic  = m_modifier.m_miscvalue;

    if (apply && Recipe().Says().dispelsOnImmunity)
    {
        GetTarget()->RemoveAurasAtMechanicImmunity(mechanic, GetId());
    }

}

void Aura::HandleAuraModEffectImmunity(bool apply, bool )
{
    Unit* target = GetTarget();

    if (!apply &&IsPlayer(target) &&
        (GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION))
    {
        Player* player = (Player*)target;
        if (BattleGround* bg = player->Battle().Ground())
        {
            bg->EventPlayerDroppedFlag(player);
        }
        else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
        {
            outdoorPvP->HandleDropFlag(player, GetSpellProto()->ID);
        }
    }

    target->ApplySpellImmune(GetId(), IMMUNITY_EFFECT, m_modifier.m_miscvalue, apply);
}

void Aura::HandleAuraModStateImmunity(bool apply, bool Real)
{
    if (apply && Real && Recipe().Says().dispelsOnImmunity)
    {
        for (const auto* aura : GetTarget()->GetAurasByType(static_cast<AuraType>(m_modifier.m_miscvalue)))
        {
            if (aura != this)
            {
                GetTarget()->RemoveAuras(aura->GetId());
            }
        }
    }

    GetTarget()->ApplySpellImmune(GetId(), IMMUNITY_STATE, m_modifier.m_miscvalue, apply);
}

void Aura::HandleAuraModSchoolImmunity(bool apply, bool Real)
{
    Unit* target = GetTarget();
    target->ApplySpellImmune(GetId(), IMMUNITY_SCHOOL, m_modifier.m_miscvalue, apply);

    if (Recipe().Says().dispelsOnImmunity && Recipe().Says().shieldReducesDamage)
    {
        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);
    }

    if (Real && apply &&
        Recipe().Says().dispelsOnImmunity &&
        cast::Recipes().IsPositive(GetId()))
    {
        uint32 school_mask = m_modifier.m_miscvalue;
        Unit::SpellAuraHolderMap& Auras = target->GetSpellAuraHolderMap();
        for (Unit::SpellAuraHolderMap::iterator iter = Auras.begin(), next; iter != Auras.end(); iter = next)
        {
            next = iter;
            ++next;
            SpellEntry const* spell = iter->second->GetSpellProto();
            if ((GetSpellSchoolMask(spell) & school_mask) &&
                !cast::RecipeOf(*spell).Says().ignoresInvulnerability &&
                !iter->second->IsPositive() &&
                spell->ID != GetId())
            {
                target->RemoveAuras(spell->ID);
                if (Auras.empty())
                {
                    break;
                }
                else
                {
                    next = Auras.begin();
                }
            }
        }
    }
    if (Real && GetSpellProto()->Mechanic == MECHANIC_BANISH)
    {
        if (apply)
        {
            target->addUnitState(UNIT_STAT_ISOLATED);
        }
        else
        {
            target->clearUnitState(UNIT_STAT_ISOLATED);
        }
    }
}

void Aura::HandleAuraModDmgImmunity(bool apply, bool )
{
    GetTarget()->ApplySpellImmune(GetId(), IMMUNITY_DAMAGE, m_modifier.m_miscvalue, apply);
}

void Aura::HandleAuraModDispelImmunity(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    GetTarget()->ApplySpellDispelImmunity(GetSpellProto(), DispelType(m_modifier.m_miscvalue), apply);
}
