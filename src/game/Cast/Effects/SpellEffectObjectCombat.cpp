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

#include <cmath>
#include <random>
#include "Summoning.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
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
#include "CreatureRecord.h"
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
#include "Cast/Recipe/RecipeBook.h"

void Spell::EffectThreat(const cast::Operation& )
{
    if (!unitTarget || !unitTarget->IsAlive() || !m_caster->IsAlive())
    {
        return;
    }

    if (!unitTarget->CanHaveThreatList())
    {
        return;
    }

    unitTarget->AddThreat(m_caster, float(damage), false, GetSpellSchoolMask(m_spellInfo), m_spellInfo);
}

void Spell::EffectHealMaxHealth(const cast::Operation& )
{
    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    uint32 heal = m_caster->GetMaxHealth();

    m_healing += heal;
}

void Spell::EffectInterruptCast(const cast::Operation& )
{
    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
    {
        if (Spell* spell = unitTarget->GetCurrentSpell(CurrentSpellTypes(i)))
        {
            SpellEntry const* curSpellInfo = spell->m_spellInfo;

            if ((curSpellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) && curSpellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
            {
                unitTarget->ProhibitSpellSchool(GetSpellSchoolMask(curSpellInfo), Recipe().DurationMs());
                unitTarget->InterruptSpell(CurrentSpellTypes(i), false);
            }
        }
    }
}

void Spell::EffectSummonObjectWild(const cast::Operation& operation)
{
    uint32 gameobject_id = operation.miscValue;

    GameObject* pGameObj = new GameObject;

    Occupant* target = focusObject;
    if (!target)
    {
        target = m_caster;
    }

    float x, y, z;
    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        m_targets.getDestination(x, y, z);
    }
    else
    {
        ClosePointNear(*m_caster, x, y, z, DEFAULT_WORLD_OBJECT_SIZE);
    }

    Map* map = target->GetMap();

    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), gameobject_id, map,
        x, y, z, target->Where().Facing()))
    {
        delete pGameObj;
        return;
    }

    int32 duration = Recipe().DurationMs();

    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->ID);

    map->Add(pGameObj);
    pGameObj->AIM_Initialize();

    if (pGameObj->GetGoType() == GAMEOBJECT_TYPE_FLAGDROP &&IsPlayer(m_caster))
    {
        Player* pl = (Player*)m_caster;
        BattleGround* bg = ((Player*)m_caster)->Battle().Ground();

        switch (pGameObj->GetMapId())
        {
            case 489:
            {
                if (bg && bg->GetTypeID() == BATTLEGROUND_WS && bg->GetStatus() == STATUS_IN_PROGRESS)
                {
                    Team team = pl->GetTeam() == ALLIANCE ? HORDE : ALLIANCE;

                    ((BattleGroundWS*)bg)->SetDroppedFlagGuid(pGameObj->GetObjectGuid(), team);
                }
                break;
            }
        }
    }

    pGameObj->SummonLinkedTrapIfAny();

    if (IsCreature(m_caster) && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->JustSummoned(pGameObj);
    }
    if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned(pGameObj);
    }
}

void Spell::EffectSanctuary(const cast::Operation& )
{
    if (!unitTarget)
    {
        return;
    }

    unitTarget->CombatStop();
    unitTarget->GetHostileRefManager().deleteReferences();

    if (m_spellInfo->IsFitToFamily(SPELLFAMILY_ROGUE, UI64LIT(0x0000000000000800)))
    {
        ((Player*)m_caster)->RemoveAurasOfType(SPELL_AURA_MOD_ROOT);
    }

    if (m_triggeredByAuraSpell && m_spellInfo->ID == 14093 &&IsPlayer(unitTarget))
    {

        uint32 stealth_id = 0;
        SpellCooldowns const scm = ((Player*)unitTarget)->GetSpellCooldownMap();
        for (SpellCooldowns::const_reverse_iterator it = scm.rbegin(); it != scm.rend(); ++it)
        {
            if (it->first >= 1784 && it->first <= 1787)
            {
                stealth_id = it->first;
                break;
            }
        }
        if (!stealth_id)
        {
            return;
        }

        ((Player*)unitTarget)->RemoveSpellCooldown(stealth_id);
        unitTarget->CastSpell(unitTarget, stealth_id, true);
    }
}

void Spell::EffectAddComboPoints(const cast::Operation& )
{
    if (!unitTarget)
    {
        return;
    }

    if (!IsPlayer(m_caster))
    {
        return;
    }

    if (damage <= 0)
    {
        return;
    }

    ((Player*)m_caster)->AddComboPoints(unitTarget, damage);
}

void Spell::EffectDuel(const cast::Operation& operation)
{
    if (!m_caster || !unitTarget || !IsPlayer(m_caster) || !IsPlayer(unitTarget))
    {
        return;
    }

    Player* caster = (Player*)m_caster;
    Player* target = (Player*)unitTarget;

    if (caster->Duelling().Stands() || target->Duelling().Stands() || !target->GetSocial() || target->GetSocial()->HasIgnore(caster->GetObjectGuid()))
    {
        return;
    }

    AreaTableEntry const* casterAreaEntry = GetAreaEntryByAreaID(caster->GetTerrain()->GetAreaId(caster->Where().X(), caster->Where().Y(), caster->Where().Z()));
    if (casterAreaEntry && !(casterAreaEntry->Flags & AREA_FLAG_DUEL))
    {
        SendCastResult(SPELL_FAILED_NO_DUELING);
        return;
    }

    AreaTableEntry const* targetAreaEntry = GetAreaEntryByAreaID(target->GetTerrain()->GetAreaId(target->Where().X(), target->Where().Y(), target->Where().Z()));
    if (targetAreaEntry && !(targetAreaEntry->Flags & AREA_FLAG_DUEL))
    {
        SendCastResult(SPELL_FAILED_NO_DUELING);
        return;
    }

    GameObject* pGameObj = new GameObject;

    uint32 gameobject_id = operation.miscValue;

    Map* map = m_caster->GetMap();
    float x = (m_caster->Where().X() + unitTarget->Where().X()) * 0.5f;
    float y = (m_caster->Where().Y() + unitTarget->Where().Y()) * 0.5f;
    float z = m_caster->Where().Z();
    ClampToAllowedZ(*m_caster, x, y, z);
    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), gameobject_id, map, x, y, z, m_caster->Where().Facing()))
    {
        delete pGameObj;
        return;
    }

    pGameObj->SetUInt32Value(GAMEOBJECT_FACTION, m_caster->getFaction());
    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel() + 1);
    int32 duration = Recipe().DurationMs();
    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->ID);

    m_caster->Conjured().AddObject(pGameObj);
    map->Add(pGameObj);
    pGameObj->AIM_Initialize();

    WorldPacket data(SMSG_DUEL_REQUESTED, 8 + 8);
    data << pGameObj->GetObjectGuid();
    data << caster->GetObjectGuid();
    caster->GetSession()->SendPacket(&data);
    target->GetSession()->SendPacket(&data);

    caster->Duelling().Offered(caster, target);
    target->Duelling().Offered(caster, caster);

    caster->SetDuelArbiterGuid(pGameObj->GetObjectGuid());
    target->SetDuelArbiterGuid(pGameObj->GetObjectGuid());

}

void Spell::EffectStuck(const cast::Operation& )
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_CAST_UNSTUCK))
    {
        return;
    }

    Player* pTarget = (Player*)unitTarget;

    DEBUG_LOG("Spell Effect: Stuck");
    DETAIL_LOG("Player %s (guid %u) used auto-unstuck future at map %u (%f, %f, %f)", pTarget->GetName(), pTarget->GetGUIDLow(), m_caster->GetMapId(), m_caster->Where().X(), pTarget->Where().Y(), pTarget->Where().Z());

    if (pTarget->IsTaxiFlying())
    {
        return;
    }

    pTarget->TeleportToHomebind(unitTarget == m_caster ? TELE_TO_SPELL : 0);

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(8690);
    if (!spellInfo)
    {
        return;
    }
    Spell spell(pTarget, spellInfo, true);
    spell.SendSpellCooldown();
}

void Spell::EffectSummonPlayer(const cast::Operation& )
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    if (unitTarget->GetDummyAura(23445))
    {
        return;
    }

    float x, y, z;
    ClosePointNear(*m_caster, x, y, z, unitTarget->Where().Extent());

    ((Player*)unitTarget)->SetSummonPoint(m_caster->GetMapId(), x, y, z);

    WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
    data << m_caster->GetObjectGuid();
    data << uint32(m_caster->GetTerrain()->GetZoneId(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z()));
    data << uint32(MAX_PLAYER_SUMMON_DELAY * IN_MILLISECONDS);
    ((Player*)unitTarget)->GetSession()->SendPacket(&data);
}

static ScriptInfo generateActivateCommand()
{
    ScriptInfo si;
    si.command = SCRIPT_COMMAND_ACTIVATE_OBJECT;
    si.id = 0;
    si.buddyEntry = 0;
    si.searchRadiusOrGuid = 0;
    si.data_flags = 0x00;
    return si;
}

void Spell::EffectActivateObject(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!gameObjTarget)
    {
        return;
    }

    uint32 misc_value = operation.miscValue;

    switch (misc_value)
    {
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
        case 8:
        case 10:
        {
            static ScriptInfo activateCommand = generateActivateCommand();

            int32 delay_secs = m_spellInfo->CalculateSimpleValue(eff_idx);

            gameObjTarget->GetMap()->Scripts().StartCommand(activateCommand, delay_secs, m_caster, gameObjTarget);
            break;
        }
        case 3:
            gameObjTarget->SendGameObjectCustomAnim();
            break;
        case 12:
            gameObjTarget->UseDoorOrButton(0, true);
            break;
        case 15:
            gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
            break;
        case 16:
        {
            switch (m_spellInfo->ID)
            {
                case 24734:
                case 24744:
                case 24756:
                case 24758:
                case 24760:
                case 24763:
                case 24765:
                case 24768:
                case 24770:
                case 24772:
                case 24784:
                case 24786:
                case 24788:
                case 24789:
                case 24790:
                {
                    uint32 npcEntry = 0;
                    uint32 templars[] = {15209, 15211, 15212, 15307};
                    uint32 dukes[] = {15206, 15207, 15208, 15220};
                    uint32 royals[] = {15203, 15204, 15205, 15305};

                    switch (m_spellInfo->ID)
                    {
                        case 24734: npcEntry = templars[urand(0, 3)]; break;
                        case 24763: npcEntry = dukes[urand(0, 3)];    break;
                        case 24784: npcEntry = royals[urand(0, 3)];   break;
                        case 24744: npcEntry = 15209;                 break;
                        case 24756: npcEntry = 15212;                 break;
                        case 24758: npcEntry = 15307;                 break;
                        case 24760: npcEntry = 15211;                 break;
                        case 24765: npcEntry = 15206;                 break;
                        case 24768: npcEntry = 15220;                 break;
                        case 24770: npcEntry = 15208;                 break;
                        case 24772: npcEntry = 15207;                 break;
                        case 24786: npcEntry = 15203;                 break;
                        case 24788: npcEntry = 15204;                 break;
                        case 24789: npcEntry = 15205;                 break;
                        case 24790: npcEntry = 15305;                 break;
                    }

                    SummonCreature(*gameObjTarget, npcEntry, gameObjTarget->Where().X(), gameObjTarget->Where().Y(), gameObjTarget->Where().Z(), gameObjTarget->Where().BearingTo(m_caster->Where()), TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, MINUTE * IN_MILLISECONDS);
                    gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
                    break;
                }
            }
            break;
        }
        default:
            sLog.outError("Spell::EffectActivateObject called with unknown misc value. Spell Id %u", m_spellInfo->ID);
            break;
    }
}

void Spell::EffectEnchantHeldItem(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    Player* item_owner = (Player*)unitTarget;
    Item* item = item_owner->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

    if (!item)
    {
        return;
    }

    if (!item ->IsEquipped())
    {
        return;
    }

    if (operation.miscValue)
    {
        uint32 enchant_id = operation.miscValue;
        int32 duration = Recipe().DurationMs();
        if (!duration)
        {
            duration = m_currentBasePoints[eff_idx];
        }
        if (!duration)
        {
            duration = 10 * IN_MILLISECONDS;
        }

        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
        {
            return;
        }

        EnchantmentSlot slot = TEMP_ENCHANTMENT_SLOT;

        if (item->GetEnchantmentId(slot) && item->GetEnchantmentId(slot) != enchant_id)
        {
            return;
        }

        item->SetEnchantment(slot, enchant_id, duration, 0);
        item_owner->ApplyEnchantment(item, slot, true);
    }
}

void Spell::EffectDisEnchant(const cast::Operation& )
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    Player* p_caster = (Player*)m_caster;
    if (!itemTarget || !itemTarget->GetProto()->DisenchantID)
    {
        return;
    }

    p_caster->UpdateCraftSkill(m_spellInfo->ID);

    ((Player*)m_caster)->SendLoot(itemTarget->GetObjectGuid(), LOOT_DISENCHANTING);

}

void Spell::EffectInebriate(const cast::Operation& )
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    Player* player = (Player*)unitTarget;
    uint16 currentDrunk = player->Drinking().Amount();
    uint16 drunkMod = damage * 256;
    if (currentDrunk + drunkMod > 0xFFFF)
    {
        currentDrunk = 0xFFFF;
    }
    else
    {
        currentDrunk += drunkMod;
    }
    player->Drinking().Amount(currentDrunk);
}

void Spell::EffectFeedPet(const cast::Operation& operation)
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    Player* _player = (Player*)m_caster;

    Item* foodItem = itemTarget;
    if (!foodItem)
    {
        return;
    }

    Pet* pet = _player->GetPet();
    if (!pet)
    {
        return;
    }

    if (!pet->IsAlive())
    {
        return;
    }

    int32 benefit = pet->GetCurrentFoodBenefitLevel(foodItem->GetProto()->ItemLevel);
    if (benefit <= 0)
    {
        return;
    }

    uint32 count = 1;
    _player->DestroyItemCount(foodItem, count, true);

    m_caster->CastCustomSpell(m_caster, operation.triggerSpell, &benefit, nullptr, nullptr, true);
}

void Spell::EffectDismissPet(const cast::Operation& )
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    Pet* pet = m_caster->GetPet();

    if (!pet || !pet->IsAlive())
    {
        return;
    }

    pet->Unsummon(PET_SAVE_NOT_IN_SLOT, m_caster);
}

void Spell::EffectSummonObject(const cast::Operation& operation)
{
    uint32 go_id = operation.miscValue;

    uint8 slot = 0;
    switch (operation.verb)
    {
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT1: slot = 0; break;
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT2: slot = 1; break;
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT3: slot = 2; break;
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT4: slot = 3; break;
        default: return;
    }

    if (ObjectGuid guid = m_caster->m_ObjectSlotGuid[slot])
    {
        if (GameObject* obj = m_caster->GetMap()->GetGameObject(guid))
        {
            obj->SetLootState(GO_JUST_DEACTIVATED);
        }
        m_caster->m_ObjectSlotGuid[slot] = 0;
    }

    GameObject* pGameObj = new GameObject;

    float x, y, z;

    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        m_targets.getDestination(x, y, z);
    }

    else
    {
        ClosePointNear(*m_caster, x, y, z, DEFAULT_WORLD_OBJECT_SIZE);
    }

    Map* map = m_caster->GetMap();
    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), go_id, map,
        x, y, z, m_caster->Where().Facing(), 0.0f, 0.0f, 0.0f, 0.0f, GO_ANIMPROGRESS_DEFAULT, GO_STATE_READY))
    {
        delete pGameObj;
        return;
    }

    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel());
    int32 duration = Recipe().DurationMs();
    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->ID);
    m_caster->Conjured().AddObject(pGameObj);

    map->Add(pGameObj);
    pGameObj->AIM_Initialize();
    WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
    data << static_cast<ObjectGuid>(pGameObj->GetObjectGuid());
    Deliver(Audience::Around(*m_caster).AndSubject(), &data);

    m_caster->m_ObjectSlotGuid[slot] = pGameObj->GetObjectGuid();

    pGameObj->SummonLinkedTrapIfAny();

    if (IsCreature(m_caster) && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->JustSummoned(pGameObj);
    }
    if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned(pGameObj);
    }
}

void Spell::EffectResurrect(const cast::Operation& )
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    if (unitTarget->IsAlive() || !unitTarget->IsInWorld())
    {
        return;
    }

    switch (m_spellInfo->ID)
    {
        case 8342:
        case 22999:
        {
            uint32 failChance = 0;
            uint32 failSpellId = 0;
            switch (m_spellInfo->ID)
            {
                case 8342:  failChance = 67; failSpellId = 8338;  break;
                case 22999: failChance = 50; failSpellId = 23055; break;
            }

            if (roll_chance_i(failChance))
            {
                if (failSpellId)
                {
                    m_caster->CastSpell(m_caster, failSpellId, true, m_CastItem);
                }
                return;
            }
            break;
        }
        default:
            break;
    }

    Player* pTarget = ((Player*)unitTarget);

    if (pTarget->isRessurectRequested())
    {
        return;
    }

    uint32 health = pTarget->GetMaxHealth() * damage / 100;
    uint32 mana   = pTarget->GetMaxPower(POWER_MANA) * damage / 100;

    pTarget->setResurrectRequestData(m_caster->GetObjectGuid(), m_caster->GetMapId(), m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z(), health, mana);
    SendResurrectRequest(pTarget);
}

void Spell::EffectAddExtraAttacks(const cast::Operation& )
{
    if (!unitTarget || !unitTarget->IsAlive())
    {
        return;
    }

    if (unitTarget->m_extraAttacks)
    {
        if (m_spellInfo->ID == 20178 && unitTarget->m_extraAttacks < 4)
        {
            ++unitTarget->m_extraAttacks;
        }
    }
    else
    {
        unitTarget->m_extraAttacks = damage;
    }
}

void Spell::EffectParry(const cast::Operation& )
{
    if (unitTarget &&IsPlayer(unitTarget))
    {
        ((Player*)unitTarget)->Arms().CanParry(true);
    }
}

void Spell::EffectBlock(const cast::Operation& )
{
    if (unitTarget &&IsPlayer(unitTarget))
    {
        ((Player*)unitTarget)->Arms().CanBlock(true);
    }
}

void Spell::EffectLeapForward(const cast::Operation& operation)
{
    float dist = GetSpellRadius(sSpellRadiusStore.LookupEntry(operation.radiusIndex));
    const float IN_OR_UNDER_LIQUID_RANGE = 0.8f;

    Geometry::Vector3 prevPos, nextPos;
    float orientation = unitTarget->Where().Facing();

    prevPos.x = unitTarget->Where().X();
    prevPos.y = unitTarget->Where().Y();
    prevPos.z = unitTarget->Where().Z();

    float groundZ = prevPos.z;
    bool isPrevInLiquid = false;

    if (!unitTarget->GetMap()->GetHeightInRange(prevPos.x, prevPos.y, groundZ, 3.0f) && unitTarget->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING))
    {
        nextPos.x = prevPos.x + dist * cos(orientation);
        nextPos.y = prevPos.y + dist * sin(orientation);
        nextPos.z = prevPos.z - 2.0f;

        GridMapLiquidData liquidData;
        if (unitTarget->GetMap()->GetTerrain()->IsInWater(nextPos.x, nextPos.y, nextPos.z, &liquidData))
        {
            if (fabs(nextPos.z - liquidData.level) < 10.0f)
            {
                nextPos.z = liquidData.level - IN_OR_UNDER_LIQUID_RANGE;
            }
        }
        else
        {

            unitTarget->GetMap()->GetHeightInRange(nextPos.x, nextPos.y, nextPos.z, 10.0f);
        }

        unitTarget->GetMap()->GetHitPosition(prevPos.x, prevPos.y, prevPos.z + 0.5f, nextPos.x, nextPos.y, nextPos.z, -0.5f);

        unitTarget->NearTeleportTo(nextPos.x, nextPos.y, nextPos.z, orientation, unitTarget == m_caster);

        return;
    }

    if (fabs(prevPos.z - groundZ) > 0.5f)
    {
        prevPos.z = groundZ;
    }

    isPrevInLiquid = unitTarget->GetMap()->GetTerrain()->IsInWater(prevPos.x, prevPos.y, prevPos.z);

    const float step = 2.0f;
    const float maxSlope = 50.0f;
    const float MAX_SLOPE_IN_RADIAN = maxSlope / 180.0f * M_PI_F;
    float nextZPointEstimation = 1.0f;
    float destx = prevPos.x + dist * cos(orientation);
    float desty = prevPos.y + dist * sin(orientation);
    const uint32 numChecks = ceil(fabs(dist / step));
    const float DELTA_X = (destx - prevPos.x) / numChecks;
    const float DELTA_Y = (desty - prevPos.y) / numChecks;

    for (uint32 i = 1; i < numChecks + 1; ++i)
    {

        nextPos.x = prevPos.x + DELTA_X;
        nextPos.y = prevPos.y + DELTA_Y;
        nextPos.z = prevPos.z + nextZPointEstimation;

        bool isInLiquid = false;
        bool isInLiquidTested = false;
        bool isOnGround = false;
        GridMapLiquidData liquidData;

        if (!unitTarget->GetMap()->GetHeightInRange(nextPos.x, nextPos.y, nextPos.z))
        {

            if (!unitTarget->GetMap()->GetTerrain()->IsInWater(nextPos.x, nextPos.y, nextPos.z, &liquidData))
            {

                nextPos = prevPos;
                break;
            }
            else
            {
                isInLiquid = true;
                isInLiquidTested = true;
            }
        }
        else
        {
            isOnGround = true;
        }

        if (isInLiquid || (!isInLiquidTested && unitTarget->GetMap()->GetTerrain()->IsInWater(nextPos.x, nextPos.y, nextPos.z, &liquidData)))
        {
            if (!isPrevInLiquid && fabs(liquidData.level - prevPos.z) > 2.0f)
            {

                nextPos = prevPos;
                break;
            }

            if ((liquidData.level - IN_OR_UNDER_LIQUID_RANGE) > nextPos.z)
            {
                nextPos.z = prevPos.z;
            }
            else
            {
                nextPos.z = liquidData.level - IN_OR_UNDER_LIQUID_RANGE;
            }

            isInLiquid = true;

            float ground = nextPos.z;
            if (unitTarget->GetMap()->GetHeightInRange(nextPos.x, nextPos.y, ground))
            {
                if (nextPos.z < ground)
                {
                    nextPos.z = ground;
                    isOnGround = true;
                }
            }
        }

        float hitZ = nextPos.z + 1.5f;
        if (unitTarget->GetMap()->GetHitPosition(prevPos.x, prevPos.y, prevPos.z + 1.5f, nextPos.x, nextPos.y, hitZ, -1.0f))
        {

            nextPos = prevPos;
            break;
        }

        if (isOnGround)
        {

            float ac = fabs(prevPos.z - nextPos.z);

            float slope = atan(ac / step);

            if (slope > MAX_SLOPE_IN_RADIAN)
            {

                nextPos = prevPos;
                break;
            }

        }

        nextZPointEstimation = (nextPos.z - prevPos.z) / 2.0f;
        isPrevInLiquid = isInLiquid;
        prevPos = nextPos;
    }

    unitTarget->NearTeleportTo(nextPos.x, nextPos.y, nextPos.z, orientation, unitTarget == m_caster);
}

void Spell::EffectReputation(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    Player* _player = (Player*)unitTarget;

    int32  rep_change = m_currentBasePoints[eff_idx];
    uint32 faction_id = operation.miscValue;

    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

    if (!factionEntry)
    {
        return;
    }

    rep_change = _player->CalculateReputationGain(REPUTATION_SOURCE_SPELL, rep_change, faction_id);

    _player->GetReputationMgr().ModifyReputation(factionEntry, rep_change);
}

void Spell::EffectQuestComplete(const cast::Operation& operation)
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    uint32 quest_id = operation.miscValue;
    ((Player*)unitTarget)->Journal().Explored(quest_id);
}

void Spell::EffectSelfResurrect(const cast::Operation& operation)
{
    if (!unitTarget || unitTarget->IsAlive())
    {
        return;
    }
    if (!IsPlayer(unitTarget))
    {
        return;
    }
    if (!unitTarget->IsInWorld())
    {
        return;
    }

    uint32 health = 0;
    uint32 mana = 0;

    if (damage < 0)
    {
        health = uint32(-damage);
        mana = operation.miscValue;
    }

    else
    {
        health = uint32(damage / 100.0f * unitTarget->GetMaxHealth());
        if (unitTarget->GetMaxPower(POWER_MANA) > 0)
        {
            mana = uint32(damage / 100.0f * unitTarget->GetMaxPower(POWER_MANA));
        }
    }

    Player* plr = ((Player*)unitTarget);
    plr->ResurrectPlayer(0.0f);

    plr->SetHealth(health);
    plr->SetPower(POWER_MANA, mana);
    plr->SetPower(POWER_RAGE, 0);
    plr->SetPower(POWER_ENERGY, plr->GetMaxPower(POWER_ENERGY));

    plr->SpawnCorpseBones();
}

void Spell::EffectSkinning(const cast::Operation& )
{
    if (!IsCreature(unitTarget))
    {
        return;
    }
    if (!m_caster || !IsPlayer(m_caster))
    {
        return;
    }

    Creature* creature = (Creature*) unitTarget;
    int32 targetLevel = creature->getLevel();

    uint32 skill = creature->Record().RequiredLootSkill();

    ((Player*)m_caster)->SendLoot(creature->GetObjectGuid(), LOOT_SKINNING);
    creature->RemoveUnitFlag(UNIT_FLAG_SKINNABLE);
    creature->TappedBy(m_caster);

    int32 reqValue = targetLevel < 10 ? 0 : targetLevel < 20 ? (targetLevel - 10) * 10 : targetLevel * 5;

    int32 skillValue = ((Player*)m_caster)->GetPureSkillValue(skill);

    ((Player*)m_caster)->UpdateGatherSkill(skill, skillValue, reqValue, creature->IsElite() ? 2 : 1);
}

void Spell::EffectCharge(const cast::Operation& )
{
    if (!unitTarget)
    {
        return;
    }

    float x, y, z;
    ContactPointNear(*unitTarget, m_caster, x, y, z, 3.666666f);

    if (!IsPlayer(unitTarget))
    {
        ((Creature*)unitTarget)->StopMoving();
    }

    m_caster->MonsterMoveWithSpeed(x, y, z, 24.f, true, true);

    if (unitTarget != m_caster && !Recipe().IsPositive())
    {
        m_caster->Attack(unitTarget, true);
    }
}

void Spell::EffectKnockBack(const cast::Operation& operation)
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    ((Player*)unitTarget)->KnockBackFrom(m_caster, float(operation.miscValue) / 10, float(damage) / 10);
}

void Spell::EffectSendTaxi(const cast::Operation& operation)
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    ((Player*)unitTarget)->ActivateTaxiPathTo(operation.miscValue, m_spellInfo->ID);
}

void Spell::EffectPlayerPull(const cast::Operation& operation)
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    float dist = unitTarget->Where().DistanceTo(m_caster->Where(), false);
    if (damage && dist > damage)
    {
        dist = float(damage);
    }

    ((Player*)unitTarget)->KnockBackFrom(m_caster, -dist, float(operation.miscValue) / 10);
}
