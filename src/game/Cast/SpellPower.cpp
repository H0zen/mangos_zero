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

#include <list>
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
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "ObjectLookup.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"

void Spell::TakeCastItem()
{
    if (!m_CastItem || !IsPlayer(m_caster))
    {
        return;
    }

    if (m_IsTriggeredSpell && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM))
    {
        return;
    }

    ItemPrototype const* proto = m_CastItem->GetProto();

    if (!proto)
    {

        sLog.outError("Cast item (%s) has no item prototype", m_CastItem->GetGuidStr().c_str());
        return;
    }

    bool expendable = false;
    bool withoutCharges = false;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId)
        {

            if (proto->Spells[i].SpellCharges)
            {
                if (proto->Spells[i].SpellCharges < 0 && !(proto->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE))
                {
                    expendable = true;
                }

                int32 charges = m_CastItem->GetSpellCharges(i);

                if (charges)
                {
                    (charges > 0) ? --charges : ++charges;
                    if (proto->Stackable == 1)
                    {
                        m_CastItem->SetSpellCharges(i, charges);
                    }
                    m_CastItem->SetState(ITEM_CHANGED, (Player*)m_caster);
                }

                withoutCharges = (charges == 0);
            }
        }
    }

    if (expendable && withoutCharges)
    {
        uint32 count = 1;
        ((Player*)m_caster)->DestroyItemCount(m_CastItem, count, true);

        ClearCastItem();
    }
}

void Spell::TakePower()
{
    if (m_CastItem || m_IsTriggeredSpell)
    {
        return;
    }

    if (m_spellInfo->PowerType == POWER_HEALTH)
    {
        m_caster->ModifyHealth(-(int32)m_powerCost);
        return;
    }

    Powers powerType = Powers(m_spellInfo->PowerType);

    bool hit = true;
    for (uint8 j = 0; j < 3; ++j)
    {

        if (Recipe().At(static_cast<uint8>(j)).targetA == TARGET_CHAIN_DAMAGE ||
            Recipe().At(static_cast<uint8>(j)).targetA == TARGET_CURRENT_ENEMY_COORDINATES)
        {
            if (IsPlayer(m_caster))
            {
                if (powerType == POWER_ENERGY || powerType == POWER_RAGE)
                {
                    for (const auto& enrolled : m_roster.Units())
                    {
                        if (enrolled.verdict != SPELL_MISS_NONE)
                        {
                            hit = false;
                        }
                        break;
                    }
                }
            }
        }
    }
    if (hit || m_spellInfo->AttributesEx & SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS || m_spellInfo->AttributesEx & SPELL_ATTR_EX_REQ_COMBO_POINTS)
    {
        m_caster->ModifyPower(powerType, -(int32)m_powerCost);
    }
    else
    {
        m_caster->ModifyPower(powerType, -(int32)m_powerCost / 5);
    }

    if (powerType == POWER_MANA && m_powerCost > 0)
    {
        m_caster->SetLastManaUse();
    }
}

void Spell::TakeReagents()
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    if (IgnoreItemRequirements())
    {
        return;
    }

    Player* p_caster = (Player*)m_caster;
    if (p_caster->CanNoReagentCast(m_spellInfo))
    {
        return;
    }

    for (uint32 x = 0; x < MAX_SPELL_REAGENTS; ++x)
    {
        if (m_spellInfo->Reagent[x] <= 0)
        {
            continue;
        }

        uint32 itemid = m_spellInfo->Reagent[x];
        uint32 itemcount = m_spellInfo->ReagentCount[x];

        if (m_CastItem)
        {
            ItemPrototype const* proto = m_CastItem->GetProto();
            if (proto && proto->ItemId == itemid)
            {
                for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
                {

                    int32 charges = m_CastItem->GetSpellCharges(s);
                    if (proto->Spells[s].SpellCharges < 0 && abs(charges) < 2)
                    {
                        ++itemcount;
                        break;
                    }
                }

                m_CastItem = nullptr;
            }
        }

        if (m_targets.getItemTargetEntry() == itemid)
        {
            m_targets.setItemTarget(nullptr);
        }

        p_caster->DestroyItemCount(itemid, itemcount, true);
    }
}

void Spell::HandleThreatSpells()
{
    if (m_roster.Units().empty())
    {
        return;
    }

    SpellThreatEntry const* threatEntry = Recipe().Threat();

    if (!threatEntry || (!threatEntry->threat && threatEntry->ap_bonus == 0.0f))
    {
        return;
    }

    float threat = threatEntry->threat;
    if (threatEntry->ap_bonus != 0.0f)
    {
        threat += threatEntry->ap_bonus * m_caster->GetTotalAttackPowerValue(Recipe().Swings());
    }

    bool positive = true;
    uint8 effectMask = 0;
    for (const auto& operation : Recipe().Does())
    {
        effectMask |= (1 << operation.slot);
    }

    if (Recipe().UnwantedSlots() & effectMask)
    {

        if ((Recipe().UnwantedSlots() & effectMask) != effectMask)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u, rank %u, is not clearly positive or negative, ignoring bonus threat", m_spellInfo->ID, sSpellMgr.GetSpellRank(m_spellInfo->ID));
            return;
        }
        positive = false;
    }

    if (!positive)
    {
        threat /= m_roster.Units().size();
    }

    for (const auto& enrolled : m_roster.Units())
    {
        if (enrolled.verdict != SPELL_MISS_NONE)
        {
            continue;
        }

        Unit* target = m_caster->GetObjectGuid() == enrolled.guid ? m_caster : ObjectLookup::GetUnit(*m_caster, enrolled.guid);
        if (!target)
        {
            continue;
        }

        if (positive)
        {
            target->GetHostileRefManager().threatAssist(m_caster , threat, m_spellInfo);
        }

        else
        {
            if (!target->CanHaveThreatList())
            {
                continue;
            }

            target->AddThreat(m_caster, threat, false, GetSpellSchoolMask(m_spellInfo), m_spellInfo);
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u added an additional %f threat for %s %zu target(s)", m_spellInfo->ID, threat, positive ? "assisting" : "harming", m_roster.Units().size());
}
