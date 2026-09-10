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

#include <random>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
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
#include "Kinds.h"
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

void Spell::SendLoot(ObjectGuid guid, LootType loottype, LockType lockType)
{
    if (gameObjTarget)
    {
        switch (gameObjTarget->GetGoType())
        {
            case GAMEOBJECT_TYPE_DOOR:
            case GAMEOBJECT_TYPE_BUTTON:
            case GAMEOBJECT_TYPE_QUESTGIVER:
            case GAMEOBJECT_TYPE_SPELL_FOCUS:
            case GAMEOBJECT_TYPE_GOOBER:
                gameObjTarget->Use(m_caster);
                return;

            case GAMEOBJECT_TYPE_CHEST:
                gameObjTarget->Use(m_caster);

                break;

            case GAMEOBJECT_TYPE_TRAP:
                if (lockType == LOCKTYPE_DISARM_TRAP)
                {
                    gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
                    return;
                }
                sLog.outError("Spell::SendLoot unhandled locktype %u for GameObject trap (entry %u) for spell %u.", lockType, gameObjTarget->GetEntry(), m_spellInfo->ID);
                return;
            default:
                sLog.outError("Spell::SendLoot unhandled GameObject type %u (entry %u) for spell %u.", gameObjTarget->GetGoType(), gameObjTarget->GetEntry(), m_spellInfo->ID);
                return;
        }
    }

    if (!IsPlayer(m_caster))
    {
        return;
    }

    ((Player*)m_caster)->SendLoot(guid, loottype);
}

void Spell::EffectOpenLock(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!m_caster || !IsPlayer(m_caster))
    {
        DEBUG_LOG("WORLD: Open Lock - No Player Caster!");
        return;
    }

    Player* player = (Player*)m_caster;

    uint32 lockId = 0;
    ObjectGuid guid = 0;

    if (gameObjTarget)
    {
        GameObjectInfo const* goInfo = gameObjTarget->GetGOInfo();

        if ((goInfo->type == GAMEOBJECT_TYPE_BUTTON && goInfo->button.noDamageImmune) ||
            (goInfo->type == GAMEOBJECT_TYPE_GOOBER && goInfo->goober.losOK))
        {

            if (BattleGround* bg = player->Battle().Ground())
            {

                if (bg->GetTypeID() == BATTLEGROUND_AB || bg->GetTypeID() == BATTLEGROUND_AV)
                {
                    bg->EventPlayerClickedOnFlag(player, gameObjTarget);
                }
                return;
            }
        }
        else if (goInfo->type == GAMEOBJECT_TYPE_FLAGSTAND)
        {

            if (player->Battle().Ground())
            {
                return;
            }
        }
        lockId = goInfo->GetLockId();
        guid = gameObjTarget->GetObjectGuid();
    }
    else if (itemTarget)
    {
        lockId = itemTarget->GetProto()->LockID;
        guid = itemTarget->GetObjectGuid();
    }
    else
    {
        DEBUG_LOG("WORLD: Open Lock - No GameObject/Item Target!");
        return;
    }

    SkillType skillId = SKILL_NONE;
    int32 reqSkillValue = 0;
    int32 skillValue;

    SpellCastResult res = CanOpenLock(eff_idx, lockId, skillId, reqSkillValue, skillValue);
    if (res != SPELL_CAST_OK)
    {
        SendCastResult(res);
        return;
    }

    if (itemTarget)
    {
        itemTarget->SetItemFlag(ITEM_DYNFLAG_UNLOCKED);
    }

    if (gameObjTarget)
    {
        gameObjTarget->RemoveGoFlag(GO_FLAG_LOCKED);
    }

    SendLoot(guid, LOOT_SKINNING, LockType(operation.miscValue));

    if (!m_CastItem && skillId != SKILL_NONE)
    {

        if (uint32 pureSkillValue = player->GetPureSkillValue(skillId))
        {
            if (gameObjTarget)
            {

                Chest& lock = gameObjTarget->Behaves<ChestBehaviour>()->Lock();

                if (!lock.HasTaught(player->GetObjectGuid()) &&
                    player->UpdateGatherSkill(skillId, pureSkillValue, reqSkillValue))
                {
                    lock.Taught(player->GetObjectGuid());
                }
            }
            else if (itemTarget)
            {

                player->UpdateGatherSkill(skillId, pureSkillValue, reqSkillValue);
            }
        }
    }
}

void Spell::EffectSummonChangeItem(const cast::Operation& operation)
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    Player* player = (Player*)m_caster;

    if (!m_CastItem)
    {
        return;
    }

    if (m_CastItem->GetOwnerGuid() != player->GetObjectGuid())
    {
        return;
    }

    uint32 newitemid = operation.itemType;
    if (!newitemid)
    {
        return;
    }

    Item* oldItem = m_CastItem;

    ClearCastItem();

    player->ConvertItem(oldItem, newitemid);
}

void Spell::EffectProficiency(const cast::Operation& )
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }
    Player* p_target = (Player*)unitTarget;

    uint32 subClassMask = m_spellInfo->EquippedItemSubclass;
    if (m_spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON && !(p_target->Arms().WeaponTraining() & subClassMask))
    {
        p_target->Arms().LearnWeapon(subClassMask);
        p_target->SendProficiency(ITEM_CLASS_WEAPON, p_target->Arms().WeaponTraining());
    }
    if (m_spellInfo->EquippedItemClass == ITEM_CLASS_ARMOR && !(p_target->Arms().ArmourTraining() & subClassMask))
    {
        p_target->Arms().LearnArmour(subClassMask);
        p_target->SendProficiency(ITEM_CLASS_ARMOR, p_target->Arms().ArmourTraining());
    }
}

void Spell::EffectApplyAreaAura(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    AreaAura* Aur = new AreaAura(m_spellInfo, eff_idx, &m_currentBasePoints[eff_idx], m_spellAuraHolder, unitTarget, m_caster, m_CastItem);
    m_spellAuraHolder->AddAura(Aur, eff_idx);
}
