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

#include "Combat/Reading.h"

#include "Creature.h"
#include "SpellAuras.h"
#include "Unit.h"

namespace combat
{
    namespace
    {
        /// The core keeps chances in hundredths of a percent; the accessors hand
        /// back percentages.
        int32 ToBasisPoints(float percent)
        {
            return int32(percent * 100.f);
        }

        const Creature* AsCreature(const Unit& unit)
        {
            return unit.GetTypeId() == TYPEID_UNIT
                       ? static_cast<const Creature*>(&unit)
                       : nullptr;
        }
    }

    Combatant ReadAttacker(const Unit& attacker, const Unit& victim,
                           WeaponAttackType attackType)
    {
        Combatant c;
        c.guid = attacker.GetObjectGuid();
        c.level = attacker.getLevel();
        c.isPlayer = attacker.GetTypeId() == TYPEID_PLAYER;
        c.classId = uint8(attacker.getClass());
        c.health = int32(attacker.GetHealth());

        c.missChance = ToBasisPoints(attacker.MeleeMissChanceCalc(&victim, attackType));
        c.critChance = ToBasisPoints(attacker.GetUnitCriticalChance(attackType, &victim));

        c.weaponSkill = int32(attacker.GetWeaponSkillValue(attackType, &victim));
        c.maxSkillForLevel = int32(attacker.GetMaxSkillValueForLevel(&victim));

        if (const auto* creature = AsCreature(attacker))
        {
            c.isPet = creature->IsPet();
            c.isEvading = creature->IsInEvadeMode();

            // A creature crushes unless its template forbids it. A player never
            // does, whatever the flag says.
            const auto* info = creature->GetCreatureInfo();
            c.canCrush = info && !(info->ExtraFlags & CREATURE_FLAG_EXTRA_NO_CRUSH);
        }

        return c;
    }

    Combatant ReadObjectCaster(ObjectGuid caster, uint32 level)
    {
        Combatant c;
        c.guid = caster;
        c.level = level ? level : 1;

        // Everything else stays at its default: no weapon, no class, no auras,
        // and no melee. A gameobject casts and nothing more, so the melee table
        // is never reached with one of these on the attacking side.
        return c;
    }

    Combatant ReadVictim(const Unit& victim, const Unit& attacker)
    {
        Combatant c;
        c.guid = victim.GetObjectGuid();
        c.level = victim.getLevel();
        c.isPlayer = victim.GetTypeId() == TYPEID_PLAYER;
        c.classId = uint8(victim.getClass());
        c.health = int32(victim.GetHealth());
        c.isSitting = !victim.IsStandState();

        c.dodgeChance = ToBasisPoints(victim.GetUnitDodgeChance());
        c.parryChance = ToBasisPoints(victim.GetUnitParryChance());
        c.blockChance = ToBasisPoints(victim.GetUnitBlockChance());

        c.defenceSkill = int32(victim.GetDefenseSkillValue(&attacker));
        c.maxDefenceForLevel = int32(victim.GetMaxSkillValueForLevel(&attacker));

        if (const auto* creature = AsCreature(victim))
        {
            c.isPet = creature->IsPet();
            c.isEvading = creature->IsInEvadeMode();

            const auto* info = creature->GetCreatureInfo();
            c.canParry = !info || !(info->ExtraFlags & CREATURE_FLAG_EXTRA_NO_PARRY);
            c.canBlock = !info || !(info->ExtraFlags & CREATURE_FLAG_EXTRA_NO_BLOCK);
        }

        return c;
    }

    Defences ReadDefences(const Unit& victim, const Unit& attacker,
                          SpellSchoolMask school)
    {
        auto& mutableVictim = const_cast<Unit&>(victim);
        auto& mutableAttacker = const_cast<Unit&>(attacker);

        Defences d;

        d.immune = mutableVictim.IsImmuneToDamage(school);

        // An attacker's target-resistance aura reduces what the victim has,
        // which is why both sides are read here rather than in the victim alone.
        d.armour = int32(victim.GetArmor()) +
                   mutableAttacker.GetTotalAuraModifierByMiscMask(
                       SPELL_AURA_MOD_TARGET_RESISTANCE, SPELL_SCHOOL_MASK_NORMAL);
        if (d.armour < 0)
        {
            d.armour = 0;
        }

        if ((school & SPELL_SCHOOL_MASK_NORMAL) == 0)
        {
            d.resistance = int32(victim.GetResistance(GetFirstSchoolInMask(school))) +
                           mutableAttacker.GetTotalAuraModifierByMiscMask(
                               SPELL_AURA_MOD_TARGET_RESISTANCE, school);
            if (d.resistance < 0)
            {
                d.resistance = 0;
            }
        }

        d.blockValue = int32(victim.GetShieldBlockValue());

        for (const Aura* aura : victim.GetAurasByType(SPELL_AURA_SCHOOL_ABSORB))
        {
            const auto* mod = aura->GetModifier();
            if (!mod || mod->m_amount <= 0)
            {
                continue;
            }

            Absorber shield;
            shield.caster = aura->GetCasterGuid();
            shield.spellId = aura->GetId();
            shield.remaining = mod->m_amount;
            shield.schoolMask = uint32(mod->m_miscvalue);
            d.absorbers.push_back(shield);
        }

        // After the free shields, and in that order: a mage's mana is spent only
        // once whatever costs nothing has already been used up.
        for (const Aura* aura : victim.GetAurasByType(SPELL_AURA_MANA_SHIELD))
        {
            const auto* mod = aura->GetModifier();
            if (!mod || mod->m_amount <= 0)
            {
                continue;
            }

            Absorber shield;
            shield.caster = aura->GetCasterGuid();
            shield.spellId = aura->GetId();
            shield.remaining = mod->m_amount;
            shield.schoolMask = uint32(mod->m_miscvalue);

            auto multiplier = aura->GetSpellProto()->EffectAmplitude[aura->GetEffIndex()];
            if (multiplier > 0.f)
            {
                if (auto* modOwner = mutableVictim.GetSpellModOwner())
                {
                    modOwner->ApplySpellMod(aura->GetId(), SPELLMOD_MULTIPLE_VALUE, multiplier);
                }
            }
            shield.manaMultiplier = multiplier;

            d.absorbers.push_back(shield);
        }

        d.mana = int32(victim.GetPower(POWER_MANA));

        for (const Aura* aura : victim.GetAurasByType(SPELL_AURA_SPLIT_DAMAGE_FLAT))
        {
            const auto* mod = aura->GetModifier();
            if (!mod || mod->m_amount <= 0)
            {
                continue;
            }

            Splitter splitter;
            splitter.target = aura->GetCasterGuid();
            splitter.spellId = aura->GetId();
            splitter.flat = mod->m_amount;
            d.splitters.push_back(splitter);
        }

        for (const Aura* aura : victim.GetAurasByType(SPELL_AURA_SPLIT_DAMAGE_PCT))
        {
            const auto* mod = aura->GetModifier();
            if (!mod || mod->m_amount <= 0)
            {
                continue;
            }

            Splitter splitter;
            splitter.target = aura->GetCasterGuid();
            splitter.spellId = aura->GetId();
            splitter.fraction = float(mod->m_amount) / 100.f;
            d.splitters.push_back(splitter);
        }

        return d;
    }
}
