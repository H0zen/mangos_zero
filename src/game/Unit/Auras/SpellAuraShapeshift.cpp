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
#include "Cast/Recipe/RecipeBook.h"

enum SpellCreatedItems {
    ITEM_SOUL_SHARD = 6265
};

void Aura::HandleAuraMounted(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
        if (!ci)
        {
            sLog.outErrorDb("AuraMounted: `creature_template`='%u' not found in database (only need it modelid)", m_modifier.m_miscvalue);
            return;
        }

        uint32 display_id = Creature::ChooseDisplayId(ci);
        CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelRandomGender(display_id);
        if (minfo)
        {
            display_id = minfo->modelid;
        }

        target->Mount(display_id, GetId());
    }
    else
    {
        target->Unmount(true);
    }
}

void Aura::HandleAuraWaterWalk(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    GetTarget()->SetWaterWalk(apply);
}

void Aura::HandleAuraFeatherFall(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    GetTarget()->SetFeatherFall(apply);
}

void Aura::HandleAuraHover(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    GetTarget()->SetHover(apply);
}

void Aura::HandleWaterBreathing(bool , bool )
{

    if (IsPlayer(GetTarget()))
    {
        ((Player*)GetTarget())->Dangers().Redraw();
    }
}

void Aura::HandleAuraModShapeshift(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    ShapeshiftForm form = ShapeshiftForm(m_modifier.m_miscvalue);

    SpellShapeshiftFormEntry const* ssEntry = sSpellShapeshiftFormStore.LookupEntry(form);
    if (!ssEntry)
    {
        sLog.outError("Unknown shapeshift form %u in spell %u", form, GetId());
        return;
    }

    uint32 modelid = 0;
    Powers PowerType = POWER_MANA;
    Unit* target = GetTarget();

    target->RemoveAurasOfType(SPELL_AURA_EMPATHY);

    switch (form)
    {
        case FORM_CAT:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 892;
            }
            else
            {
                modelid = 8571;
            }
            PowerType = POWER_ENERGY;
            break;
        case FORM_TRAVEL:
            modelid = 632;
            break;
        case FORM_AQUA:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 2428;
            }
            else
            {
                modelid = 2428;
            }
            break;
        case FORM_BEAR:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 2281;
            }
            else
            {
                modelid = 2289;
            }
            PowerType = POWER_RAGE;
            break;
        case FORM_GHOUL:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 10045;
            }
            break;
        case FORM_DIREBEAR:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 2281;
            }
            else
            {
                modelid = 2289;
            }
            PowerType = POWER_RAGE;
            break;
        case FORM_CREATUREBEAR:
            modelid = 902;
            break;
        case FORM_GHOSTWOLF:
            modelid = 4613;
            break;
        case FORM_MOONKIN:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 15374;
            }
            else
            {
                modelid = 15375;
            }
            break;
        case FORM_AMBIENT:
        case FORM_SHADOW:
        case FORM_STEALTH:
            break;
        case FORM_TREE:
            modelid = 864;
            break;
        case FORM_BATTLESTANCE:
        case FORM_BERSERKERSTANCE:
        case FORM_DEFENSIVESTANCE:
            PowerType = POWER_RAGE;
            break;
        case FORM_SPIRITOFREDEMPTION:
            modelid = 16031;
            break;
        default:
            break;
    }

    switch (form)
    {
        case FORM_CAT:
        case FORM_TREE:
        case FORM_TRAVEL:
        case FORM_AQUA:
        case FORM_BEAR:
        case FORM_DIREBEAR:
        case FORM_MOONKIN:
        {

            target->RemoveAurasOfType(SPELL_AURA_MOD_ROOT, GetHolder());
            const auto slowingAuras = target->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
            for (const auto* slowing : slowingAuras)
            {
                const SpellEntry* aurSpellInfo = slowing->GetSpellProto();

                const uint32 aurMechMask = GetAllSpellMechanicMask(aurSpellInfo);

                if ((aurMechMask & MECHANIC_NOT_REMOVED_BY_SHAPESHIFT) ||

                    (aurSpellInfo->SpellIconID == 15 && aurSpellInfo->DispelType == 0 &&
                    (aurMechMask & (1 << (MECHANIC_SNARE - 1))) == 0))
                {
                    continue;
                }

                target->CancelAuras(aurSpellInfo->ID);
            }

            if (target->IsPolymorphed())
            {
                target->RemoveAuras(target->GetTransform());
            }

        }
        case FORM_GHOSTWOLF:
        {

            target->RemoveAurasOfType(SPELL_AURA_WATER_WALK);

            break;
        }
        default:
            break;
    }

    if (apply)
    {

        target->RemoveAurasOfType(SPELL_AURA_MOD_SHAPESHIFT, GetHolder());

        if (modelid > 0)
        {
            target->SetObjectScale(DEFAULT_OBJECT_SCALE * target->GetObjectScaleMod());
            target->SetDisplayId(modelid);
        }

        if (PowerType != POWER_MANA)
        {

            if (target->GetPowerType() != PowerType)
            {
                target->SetPowerType(PowerType);
            }

            switch (form)
            {
                case FORM_CAT:
                case FORM_BEAR:
                case FORM_DIREBEAR:
                {

                    int32 furorChance = 0;
                    const auto mDummy = target->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto* aura : mDummy)
                    {
                        if (aura->GetSpellProto()->SpellIconID == 238)
                        {
                            furorChance = aura->GetModifier()->m_amount;
                            break;
                        }
                    }

                    if (m_modifier.m_miscvalue == FORM_CAT)
                    {
                        target->SetPower(POWER_ENERGY, 0);
                        if (irand(1, 100) <= furorChance)
                        {
                            target->CastSpell(target, 17099, true, nullptr, this);
                        }
                    }
                    else
                    {
                        target->SetPower(POWER_RAGE, 0);
                        if (irand(1, 100) <= furorChance)
                        {
                            target->CastSpell(target, 17057, true, nullptr, this);
                        }
                    }
                    break;
                }
                case FORM_BATTLESTANCE:
                case FORM_DEFENSIVESTANCE:
                case FORM_BERSERKERSTANCE:
                {
                    uint32 Rage_val = 0;

                    if (IsPlayer(target))
                    {
                        const auto aurasOverrideClassScripts = target->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                        for (auto* aura : aurasOverrideClassScripts)
                        {

                            switch (aura->GetModifier()->m_miscvalue)
                            {
                                case 831: Rage_val =  50; break;
                                case 832: Rage_val = 100; break;
                                case 833: Rage_val = 150; break;
                                case 834: Rage_val = 200; break;
                                case 835: Rage_val = 250; break;
                            }
                            if (Rage_val != 0)
                            {
                                break;
                            }
                        }
                    }
                    if (target->GetPower(POWER_RAGE) > Rage_val)
                    {
                        target->SetPower(POWER_RAGE, Rage_val);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        target->SetShapeshiftForm(form);
    }
    else
    {
        if (modelid > 0)
        {

            if (target->getRace() == RACE_TAUREN)
            {
                if (target->getGender() == GENDER_MALE)
                {
                    target->SetObjectScale(DEFAULT_TAUREN_MALE_SCALE * target->GetObjectScaleMod());
                }
                else
                {
                    target->SetObjectScale(DEFAULT_TAUREN_FEMALE_SCALE * target->GetObjectScaleMod());
                }
            }

            target->SetDisplayId(target->GetNativeDisplayId());
        }

        if (target->getClass() == CLASS_DRUID)
        {
            target->SetPowerType(POWER_MANA);
        }

        target->SetShapeshiftForm(FORM_NONE);
    }

    HandleShapeshiftBoosts(apply);

    if (IsPlayer(target))
    {
        ((Player*)target)->InitDataForForm();
    }
}

void Aura::HandleAuraTransform(bool apply, bool Real)
{
    Unit* target = GetTarget();
    if (apply)
    {

        if (m_modifier.m_miscvalue == 0)
        {
            switch (GetId())
            {
                case 16739:
                {
                    uint32 orb_model = target->GetNativeDisplayId();
                    switch (orb_model)
                    {

                        case 1479: target->SetDisplayId(10134); break;

                        case 1478: target->SetDisplayId(10135); break;

                        case 59:   target->SetDisplayId(10136); break;

                        case 49:   target->SetDisplayId(10137); break;

                        case 50:   target->SetDisplayId(10138); break;

                        case 51:   target->SetDisplayId(10139); break;

                        case 52:   target->SetDisplayId(10140); break;

                        case 53:   target->SetDisplayId(10141); break;

                        case 54:   target->SetDisplayId(10142); break;

                        case 55:   target->SetDisplayId(10143); break;

                        case 56:   target->SetDisplayId(10144); break;

                        case 58:   target->SetDisplayId(10145); break;

                        case 57:   target->SetDisplayId(10146); break;

                        case 60:   target->SetDisplayId(10147); break;

                        case 1563: target->SetDisplayId(10148); break;

                        case 1564: target->SetDisplayId(10149); break;
                        default: break;
                    }
                    break;
                }
                default:
                    sLog.outError("Aura::HandleAuraTransform, spell %u does not have creature entry defined, need custom defined model.", GetId());
                    break;
            }
        }
        else
        {
            uint32 model_id;

            CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
            if (!ci)
            {
                model_id = 16358;
                sLog.outError("Auras: unknown creature id = %d (only need its modelid) Form Spell Aura Transform in Spell ID = %d", m_modifier.m_miscvalue, GetId());
            }
            else
            {
                model_id = Creature::ChooseDisplayId(ci);
            }

            target->SetDisplayId(model_id);

            if (ci &&IsCreature(target))
            {
                ((Creature*)target)->LoadEquipment(ci->EquipmentTemplateId, false);
            }
        }

        if (!target->GetTransform() || !cast::Recipes().IsPositive(GetId()) || cast::Recipes().IsPositive(target->GetTransform()))
        {
            target->SetTransform(GetId());
        }
    }
    else
    {

        target->SetTransform(0);
        target->SetDisplayId(target->GetNativeDisplayId());

        if (IsCreature(target))
        {
            ((Creature*)target)->LoadEquipment(((Creature*)target)->GetCreatureInfo()->EquipmentTemplateId, true);
        }

        const auto otherTransforms = target->GetAurasByType(SPELL_AURA_TRANSFORM);
        if (!otherTransforms.empty())
        {

            Aura* handledAura = otherTransforms.front();
            for (auto* transform : otherTransforms)
            {

                if (!cast::Recipes().IsPositive(transform->GetSpellProto()->ID))
                {
                    handledAura = transform;
                    break;
                }
            }
            handledAura->ApplyModifier(true);
        }
    }
}

void Aura::HandleForceReaction(bool apply, bool Real)
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    if (!Real)
    {
        return;
    }

    Player* player = (Player*)GetTarget();

    uint32 faction_id = m_modifier.m_miscvalue;
    ReputationRank faction_rank = ReputationRank(m_modifier.m_amount);

    player->GetReputationMgr().ApplyForceReaction(faction_id, faction_rank, apply);
    player->GetReputationMgr().SendForceReactions();

    if ((apply && faction_rank >= REP_FRIENDLY) || (!apply && player->GetReputationRank(faction_id) >= REP_FRIENDLY))
    {
        player->StopAttackFaction(faction_id);
    }
}

void Aura::HandleAuraModSkill(bool apply, bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    uint32 prot = Operation().miscValue;
    int32 points = GetModifier()->m_amount;

    ((Player*)GetTarget())->ModifySkillBonus(prot, (apply ? points : -points), m_modifier.m_auraname == SPELL_AURA_MOD_SKILL_TALENT);
    if (prot == SKILL_DEFENSE)
    {
        ((Player*)GetTarget())->Sheet().Defences();
    }
}

void Aura::HandleChannelDeathItem(bool apply, bool Real)
{
    if (Real && !apply)
    {
        if (m_removeMode != AURA_REMOVE_BY_DEATH)
        {
            return;
        }

        if (m_modifier.m_amount <= 0)
        {
            return;
        }

        SpellEntry const* spellInfo = GetSpellProto();
        if (Operation().itemType == 0)
        {
            return;
        }

        Unit* caster = GetCaster();
        if (!caster || !IsPlayer(caster))
        {
            return;
        }

        uint32 createdItemId = Operation().itemType;

        if (createdItemId == ITEM_SOUL_SHARD)
        {
            Unit* victim = GetTarget();

            if (!((Player*)caster)->isHonorOrXPTarget(victim) || (IsCreature(victim) && !((Creature*)victim)->IsTappedBy((Player*)caster)))
            {
                return;
            }

            for (auto const& aura : victim->GetAurasByType(SPELL_AURA_CHANNEL_DEATH_ITEM))
            {
                if (aura != this && caster->GetObjectGuid() == aura->GetCasterGuid() && aura->GetSpellProto()->EffectItemType[aura->GetEffIndex()] == ITEM_SOUL_SHARD)
                {
                    return;
                }
            }

        }

        uint32 noSpaceForCount = 0;
        uint32 count = m_modifier.m_amount;

        ItemPosCountVec dest;
        InventoryResult msg = ((Player*)caster)->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, createdItemId, count, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK)
        {
            count -= noSpaceForCount;
            ((Player*)caster)->SendEquipError(msg, nullptr, nullptr, createdItemId);
            if (count == 0)
            {
                return;
            }
        }

        Item* newitem = ((Player*)caster)->StoreNewItem(dest, createdItemId, true);
        ((Player*)caster)->SendNewItem(newitem, count, true, true);
    }
}

void Aura::HandleBindSight(bool apply, bool )
{
    Unit* caster = GetCaster();
    if (!caster || !IsPlayer(caster))
    {
        return;
    }

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
    {
        camera.SetView(GetTarget());
    }
    else
    {
        camera.ResetView();
    }
}

void Aura::HandleFarSight(bool apply, bool )
{
    Unit* caster = GetCaster();
    if (!caster || !IsPlayer(caster))
    {
        return;
    }

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
    {
        camera.SetView(GetTarget());
    }
    else
    {
        camera.ResetView();
    }
}

void Aura::HandleAuraTrackCreatures(bool apply, bool )
{
    Player* player = static_cast<Player*>(GetTarget());
    if (!player)
    {
        return;
    }

    if (apply)
    {
        player->RemoveConflictingAuras(GetHolder());
    }

    player->ApplyTracking(Player::Tracked::Creatures, uint32(1) << (m_modifier.m_miscvalue - 1), apply);
}

void Aura::HandleAuraTrackResources(bool apply, bool )
{
    Player* player = static_cast<Player*>(GetTarget());
    if (!player)
    {
        return;
    }

    if (apply)
    {
        player->RemoveConflictingAuras(GetHolder());
    }

    player->ApplyTracking(Player::Tracked::Resources, uint32(1) << (m_modifier.m_miscvalue - 1), apply);
}

void Aura::HandleAuraTrackStealthed(bool apply, bool )
{
    Player* player = static_cast<Player*>(GetTarget());
    if (!player)
    {
        return;
    }

    if (apply)
    {
        player->RemoveConflictingAuras(GetHolder());
    }

    player->TrackStealthed(apply);
}

void Aura::HandleAuraModScale(bool apply, bool )
{
    GetTarget()->ApplyScalePercent(float(m_modifier.m_amount), apply);
    GetTarget()->UpdateModelData();
}
