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

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#include "Cast/Recipe/RecipeBook.h"

void Player::_ApplyItemMods(Item* item, uint8 slot, bool apply)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !item)
    {
        return;
    }

    if (item->IsBroken())
    {
        return;
    }

    ItemPrototype const* proto = item->GetProto();

    if (!proto)
    {
        return;
    }

    DETAIL_LOG("applying mods for item %u ", item->GetGUIDLow());

    uint32 attacktype = Inventory::AttackFrom(slot);
    if (attacktype < MAX_ATTACK)
    {
        _ApplyWeaponDependentAuraMods(item, WeaponAttackType(attacktype), apply);
    }

    _ApplyItemBonuses(proto, slot, apply);

    if (slot == EQUIPMENT_SLOT_RANGED)
    {
        _ApplyAmmoBonuses();
    }

    ApplyItemEquipSpell(item, apply);
    ApplyEnchantment(item, apply);

    DEBUG_LOG("_ApplyItemMods complete.");
}

void Player::_ApplyItemBonuses(ItemPrototype const* proto, uint8 slot, bool apply)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !proto)
    {
        return;
    }

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        float val = float(proto->ItemStat[i].ItemStatValue);

        if (val == 0)
        {
            break;
        }

        switch (proto->ItemStat[i].ItemStatType)
        {
            case ITEM_MOD_MANA:
                stats::Apply(*this, UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_HEALTH:
                stats::Apply(*this, UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_AGILITY:
                stats::Apply(*this, UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
                break;
            case ITEM_MOD_STRENGTH:
                stats::Apply(*this, UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
                break;
            case ITEM_MOD_INTELLECT:
                stats::Apply(*this, UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
                break;
            case ITEM_MOD_SPIRIT:
                stats::Apply(*this, UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
                break;
            case ITEM_MOD_STAMINA:
                stats::Apply(*this, UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
                break;
        }
    }

    if (proto->Armor)
    {
        stats::Apply(*this, UNIT_MOD_ARMOR, BASE_VALUE, float(proto->Armor), apply);
    }

    if (proto->Block)
    {
        HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(proto->Block), apply);
    }

    if (proto->HolyRes)
    {
        stats::Apply(*this, UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);
    }

    if (proto->FireRes)
    {
        stats::Apply(*this, UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);
    }

    if (proto->NatureRes)
    {
        stats::Apply(*this, UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(proto->NatureRes), apply);
    }

    if (proto->FrostRes)
    {
        stats::Apply(*this, UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(proto->FrostRes), apply);
    }

    if (proto->ShadowRes)
    {
        stats::Apply(*this, UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(proto->ShadowRes), apply);
    }

    if (proto->ArcaneRes)
    {
        stats::Apply(*this, UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(proto->ArcaneRes), apply);
    }

    WeaponAttackType attType = BASE_ATTACK;
    float damage = 0.0f;

    if (slot == EQUIPMENT_SLOT_RANGED &&
        (proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_THROWN ||
        proto->InventoryType == INVTYPE_RANGEDRIGHT))
    {
        attType = RANGED_ATTACK;
    }
    else if (slot == EQUIPMENT_SLOT_OFFHAND)
    {
        attType = OFF_ATTACK;
    }

    if (proto->Damage[0].DamageMin > 0)
    {
        damage = apply ? proto->Damage[0].DamageMin : BASE_MINDAMAGE;
        SetBaseWeaponDamage(attType, MINDAMAGE, damage);

    }

    if (proto->Damage[0].DamageMax  > 0)
    {
        damage = apply ? proto->Damage[0].DamageMax : BASE_MAXDAMAGE;
        SetBaseWeaponDamage(attType, MAXDAMAGE, damage);
    }

    if (!CanUseEquippedWeapon(attType))
    {
        return;
    }

    if (proto->Delay)
    {
        if (slot == EQUIPMENT_SLOT_RANGED)
        {
            SetAttackTime(RANGED_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        }
        else if (slot == EQUIPMENT_SLOT_MAINHAND)
        {
            SetAttackTime(BASE_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        }
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
        {
            SetAttackTime(OFF_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        }
    }

    if (Tallied().Ready() && (damage || proto->Delay))
    {
        Sheet().Swing(attType);
    }
}

void Player::_ApplyWeaponDependentAuraMods(Item* item, WeaponAttackType attackType, bool apply)
{
    const auto auraCritList = GetAurasByType(SPELL_AURA_MOD_CRIT_PERCENT);
    for (auto* aura : auraCritList)
    {
        _ApplyWeaponDependentAuraCritMod(item, attackType, aura, apply);
    }

    const auto auraDamageFlatList = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (auto* aura : auraDamageFlatList)
    {
        _ApplyWeaponDependentAuraDamageMod(item, attackType, aura, apply);
    }

    const auto auraDamagePCTList = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (auto* aura : auraDamagePCTList)
    {
        _ApplyWeaponDependentAuraDamageMod(item, attackType, aura, apply);
    }
}

void Player::_ApplyWeaponDependentAuraCritMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{

    if (aura->GetSpellProto()->EquippedItemClass == -1)
    {
        return;
    }

    BaseModGroup mod = BASEMOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   mod = CRIT_PERCENTAGE;        break;
        case OFF_ATTACK:    mod = OFFHAND_CRIT_PERCENTAGE; break;
        case RANGED_ATTACK: mod = RANGED_CRIT_PERCENTAGE; break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        HandleBaseModValue(mod, FLAT_MOD, float(aura->GetModifier()->m_amount), apply);
    }
}

void Player::_ApplyWeaponDependentAuraDamageMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{

    Modifier const* modifier = aura->GetModifier();
    if ((modifier->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) == 0 && (getClassMask() & CLASSMASK_WAND_USERS) == 0)
    {
        return;
    }

    if (aura->GetSpellProto()->EquippedItemClass == -1)
    {
        return;
    }

    UnitMods unitMod = UNIT_MOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   unitMod = UNIT_MOD_DAMAGE_MAINHAND; break;
        case OFF_ATTACK:    unitMod = UNIT_MOD_DAMAGE_OFFHAND;  break;
        case RANGED_ATTACK: unitMod = UNIT_MOD_DAMAGE_RANGED;   break;
        default: return;
    }

    UnitModifierType unitModType = TOTAL_VALUE;
    switch (modifier->m_auraname)
    {
        case SPELL_AURA_MOD_DAMAGE_DONE:         unitModType = TOTAL_VALUE; break;
        case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE: unitModType = TOTAL_PCT;   break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        stats::Apply(*this, unitMod, unitModType, float(modifier->m_amount), apply);
    }
}

void Player::ApplyItemEquipSpell(Item* item, bool apply, bool form_change)
{
    if (!item)
    {
        return;
    }

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
    {
        return;
    }

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        if (!spellData.SpellId)
        {
            continue;
        }

        if (apply)
        {

            if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
            {
                continue;
            }
        }
        else
        {

            if (spellData.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE && (form_change || spellData.SpellCharges < 0))
            {
                continue;
            }
        }

        SpellEntry const* spellproto = sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellproto)
        {
            continue;
        }

        ApplyEquipSpell(spellproto, item, apply, form_change);
    }
}

void Player::ApplyEquipSpell(SpellEntry const* spellInfo, Item* item, bool apply, bool form_change)
{
    if (apply)
    {

        if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) != SPELL_CAST_OK)
        {
            return;
        }

        if (form_change)
        {
            bool found = false;
            for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
            {
                SpellAuraHolderBounds spair = GetSpellAuraHolderBounds(spellInfo->ID);
                for (SpellAuraHolderMap::const_iterator iter = spair.first; iter != spair.second; ++iter)
                {
                    if (!item || iter->second->GetCastItemGuid() == item->GetObjectGuid())
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    break;
                }
            }

            if (found)
            {
                return;
            }
        }

        DEBUG_LOG("WORLD: cast %s Equip spellId - %i", (item ? "item" : "itemset"), spellInfo->ID);

        CastSpell(this, spellInfo, true, item);
    }
    else
    {
        if (form_change)
        {

            if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) == SPELL_CAST_OK)
            {
                return;
            }
        }

        if (item)
        {
            RemoveAurasFromItem(item, spellInfo->ID);
        }
        else
        {
            RemoveAuras(spellInfo->ID);
        }
    }
}

void Player::UpdateEquipSpellsAtFormChange()
{
    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_inventory.Own(i) && !m_inventory.Own(i)->IsBroken())
        {
            ApplyItemEquipSpell(m_inventory.Own(i), false, true);
            ApplyItemEquipSpell(m_inventory.Own(i), true, true);
        }
    }

    for (size_t setindex = 0; setindex < ItemSetEff.size(); ++setindex)
    {
        ItemSetEffect* eff = ItemSetEff[setindex];
        if (!eff)
        {
            continue;
        }

        for (uint32 y = 0; y < 8; ++y)
        {
            SpellEntry const* spellInfo = eff->spells[y];
            if (!spellInfo)
            {
                continue;
            }

            ApplyEquipSpell(spellInfo, nullptr, false, true);
            ApplyEquipSpell(spellInfo, nullptr, true, true);
        }
    }
}

void Player::CastItemCombatSpell(Unit* Target, WeaponAttackType attType)
{
    Item* item = GetWeaponForAttack(attType, true, true);
    if (!item)
    {
        return;
    }

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
    {
        return;
    }

    if (!Target || Target == this)
    {
        return;
    }

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        if (!spellData.SpellId)
        {
            continue;
        }

        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellInfo)
        {
            sLog.outError("WORLD: unknown Item spellid %i", spellData.SpellId);
            continue;
        }

        if (m_extraAttacks && spellInfo->HasSpellEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS))
        {
            return;
        }

        float chance = (float)spellInfo->ProcChance;

        if (spellData.SpellPPMRate)
        {
            uint32 WeaponSpeed = proto->Delay;
            chance = GetPPMProcChance(WeaponSpeed, spellData.SpellPPMRate);
        }
        else if (chance > 100.0f)
        {
            chance = GetWeaponProcChance();
        }

        if (roll_chance_f(chance))
        {
            CastSpell(Target, spellInfo->ID, true, item);
        }
    }

    for (int e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
        {
            continue;
        }
        for (int s = 0; s < 3; ++s)
        {
            uint32 proc_spell_id = pEnchant->EffectArg[s];

            if (pEnchant->Effect[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
            {
                continue;
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(proc_spell_id);
            if (!spellInfo)
            {
                sLog.outError("Player::CastItemCombatSpell Enchant %i, cast unknown spell %i", pEnchant->ID, proc_spell_id);
                continue;
            }

            float ppmRate = sSpellMgr.GetItemEnchantProcChance(spellInfo->ID);

            float chance = ppmRate
                ? GetPPMProcChance(proto->Delay, ppmRate)
                : pEnchant->EffectPointsMin[s] != 0 ? float(pEnchant->EffectPointsMin[s]) : GetWeaponProcChance();

            SpellMods().Apply(spellInfo->ID, SPELLMOD_CHANCE_OF_SUCCESS, chance);

            if (roll_chance_f(chance))
            {
                if (cast::RecipeOf(*spellInfo).IsPositive())
                {
                    CastSpell(this, spellInfo->ID, true, item);
                }
                else
                {
                    CastSpell(Target, spellInfo->ID, true, item);
                }
            }
        }
    }
}

void Player::CastItemUseSpell(Item* item, SpellCastTargets const& targets)
{
    ItemPrototype const* proto = item->GetProto();

    int count = 0;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        if (!spellData.SpellId)
        {
            continue;
        }

        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE && spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_NO_DELAY_USE)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellInfo)
        {
            sLog.outError("Player::CastItemUseSpell: Item (Entry: %u) in have wrong spell id %u, ignoring", proto->ItemId, spellData.SpellId);
            continue;
        }

        Spell* spell = new Spell(this, spellInfo, (count > 0));
        spell->m_CastItem = item;
        spell->prepare(&targets);

        ++count;
    }
}

void Player::_RemoveAllItemMods()
{
    DEBUG_LOG("_RemoveAllItemMods start.");

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_inventory.Own(i))
        {
            ItemPrototype const* proto = m_inventory.Own(i)->GetProto();
            if (!proto)
            {
                continue;
            }

            if (proto->ItemSet)
            {
                RemoveItemsSetItem(this, proto);
            }

            if (m_inventory.Own(i)->IsBroken())
            {
                continue;
            }

            ApplyItemEquipSpell(m_inventory.Own(i), false);
            ApplyEnchantment(m_inventory.Own(i), false);
        }
    }

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_inventory.Own(i))
        {
            if (m_inventory.Own(i)->IsBroken())
            {
                continue;
            }
            ItemPrototype const* proto = m_inventory.Own(i)->GetProto();
            if (!proto)
            {
                continue;
            }

            uint32 attacktype = Inventory::AttackFrom(i);
            if (attacktype < MAX_ATTACK)
            {
                _ApplyWeaponDependentAuraMods(m_inventory.Own(i), WeaponAttackType(attacktype), false);
            }

            _ApplyItemBonuses(proto, i, false);

            if (i == EQUIPMENT_SLOT_RANGED)
            {
                _ApplyAmmoBonuses();
            }
        }
    }

    DEBUG_LOG("_RemoveAllItemMods complete.");
}

void Player::_ApplyAllItemMods()
{
    DEBUG_LOG("_ApplyAllItemMods start.");

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_inventory.Own(i))
        {
            if (m_inventory.Own(i)->IsBroken())
            {
                continue;
            }

            ItemPrototype const* proto = m_inventory.Own(i)->GetProto();
            if (!proto)
            {
                continue;
            }

            uint32 attacktype = Inventory::AttackFrom(i);
            if (attacktype < MAX_ATTACK)
            {
                _ApplyWeaponDependentAuraMods(m_inventory.Own(i), WeaponAttackType(attacktype), true);
            }

            _ApplyItemBonuses(proto, i, true);

            if (i == EQUIPMENT_SLOT_RANGED)
            {
                _ApplyAmmoBonuses();
            }
        }
    }

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_inventory.Own(i))
        {
            ItemPrototype const* proto = m_inventory.Own(i)->GetProto();
            if (!proto)
            {
                continue;
            }

            if (proto->ItemSet)
            {
                AddItemsSetItem(this, m_inventory.Own(i));
            }

            if (m_inventory.Own(i)->IsBroken())
            {
                continue;
            }

            ApplyItemEquipSpell(m_inventory.Own(i), true);
            ApplyEnchantment(m_inventory.Own(i), true);
        }
    }

    DEBUG_LOG("_ApplyAllItemMods complete.");
}

void Player::_ApplyAmmoBonuses()
{

    uint32 ammo_id = GetUInt32Value(PLAYER_AMMO_ID);
    if (!ammo_id)
    {
        return;
    }

    float currentAmmoDPSMin;
    float currentAmmoDPSMax;

    ItemPrototype const* ammo_proto = ObjectMgr::GetItemPrototype(ammo_id);
    if (!ammo_proto || ammo_proto->Class != ITEM_CLASS_PROJECTILE || !CheckAmmoCompatibility(ammo_proto))
    {
        currentAmmoDPSMin = 0.f;
        currentAmmoDPSMax = 0.f;
    }
    else
    {
        currentAmmoDPSMin = ammo_proto->Damage[0].DamageMin;
        currentAmmoDPSMax = ammo_proto->Damage[0].DamageMax;
    }

    if (std::make_pair(currentAmmoDPSMin, currentAmmoDPSMax) == Arms().Ammo())
    {
        return;
    }

    Arms().Ammo(currentAmmoDPSMin, currentAmmoDPSMax);

    if (Tallied().Ready())
    {
        Sheet().Swing(RANGED_ATTACK);
    }
}

bool Player::CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const
{
    if (!ammo_proto)
    {
        return false;
    }

    Item* weapon = GetWeaponForAttack(RANGED_ATTACK, true, false);
    if (!weapon)
    {
        return false;
    }

    ItemPrototype const* weapon_proto = weapon->GetProto();
    if (!weapon_proto || weapon_proto->Class != ITEM_CLASS_WEAPON)
    {
        return false;
    }

    switch (weapon_proto->SubClass)
    {
        case ITEM_SUBCLASS_WEAPON_BOW:
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_ARROW)
            {
                return false;
            }
            break;
        case ITEM_SUBCLASS_WEAPON_GUN:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_BULLET)
            {
                return false;
            }
            break;
        default:
            return false;
    }

    return true;
}
