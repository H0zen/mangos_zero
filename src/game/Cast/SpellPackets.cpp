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
#include "Corpse.h"
#include "Cast/Recipe/RecipeBook.h"

void Spell::SendCastResult(SpellCastResult result)
{
    if (!IsPlayer(m_caster))
    {
        if (((Creature*)m_caster)->AI())
        {
            ((Creature*)m_caster)->AI()->OnSpellCastChange(m_spellInfo, result);
        }
        return;
    }

    if (((Player*)m_caster)->GetSession()->PlayerLoading())
    {
        return;
    }

    if (result == SPELL_FAILED_CHEST_IN_USE)
    {
        SendInterrupted(result);
    }

    SendCastResult((Player*)m_caster, m_spellInfo, result);
}

void Spell::SendCastResult(Player* caster, SpellEntry const* spellInfo, SpellCastResult result)
{
    WorldPacket data(SMSG_CAST_FAILED, (4 + 1 + 1));
    data << uint32(spellInfo->ID);

    if (result != SPELL_CAST_OK)
    {
        data << uint8(2);
        data << uint8(!(cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive) ? result : SPELL_FAILED_DONT_REPORT);
        switch (result)
        {
            case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
                data << uint32(spellInfo->RequiresSpellFocus);
                break;
            case SPELL_FAILED_REQUIRES_AREA:
                break;
            case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
                data << uint32(spellInfo->EquippedItemClass);
                data << uint32(spellInfo->EquippedItemSubclass);
                data << uint32(spellInfo->EquippedItemInvTypes);
                break;
            default:
                break;
        }
    }
    else
    {
        data << uint8(0);
    }

    caster->GetSession()->SendPacket(&data);
}

void Spell::SendSpellStart()
{
    if (!IsNeedSendToClient())
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Sending SMSG_SPELL_START id=%u", m_spellInfo->ID);

    uint32 castFlags = CAST_FLAG_UNKNOWN2;
    if (IsRangedSpell())
    {
        castFlags |= CAST_FLAG_AMMO;
    }

    WorldPacket data(SMSG_SPELL_START, (8 + 8 + 4 + 2 + 4));
    if (m_CastItem)
    {
        data << m_CastItem->GetPackGUID();
    }
    else
    {
        data << m_caster->GetPackGUID();
    }

    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->ID);
    data << uint16(castFlags);
    data << uint32(m_timer);

    data << m_targets;

    if (castFlags & CAST_FLAG_AMMO)
    {
        WriteAmmoToPacket(&data);
    }

    Deliver(Audience::Around(*m_caster).AndSubject(), &data);
}

void Spell::SendSpellGo()
{

    if (!IsNeedSendToClient())
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Sending SMSG_SPELL_GO id=%u", m_spellInfo->ID);

    uint32 castFlags = CAST_FLAG_UNKNOWN9;
    if (IsRangedSpell())
    {
        castFlags |= CAST_FLAG_AMMO;
    }

    WorldPacket data(SMSG_SPELL_GO, 53);

    if (m_CastItem)
    {
        data << m_CastItem->GetPackGUID();
    }
    else
    {
        data << m_caster->GetPackGUID();
    }

    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->ID);
    data << uint16(castFlags);

    WriteSpellGoTargets(&data);

    data << m_targets;

    if (castFlags & CAST_FLAG_AMMO)
    {
        WriteAmmoToPacket(&data);
    }

    Deliver(Audience::Around(*m_caster).AndSubject(), &data);
}

void Spell::WriteAmmoToPacket(WorldPacket* data)
{
    uint32 ammoInventoryType = 0;
    uint32 ammoDisplayID = 0;

    if (IsPlayer(m_caster))
    {
        Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(RANGED_ATTACK);
        if (pItem)
        {
            ammoInventoryType = pItem->GetProto()->InventoryType;
            if (ammoInventoryType == INVTYPE_THROWN)
            {
                ammoDisplayID = pItem->GetProto()->DisplayInfoID;
            }
            else
            {
                uint32 ammoID = ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID);
                if (ammoID)
                {
                    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(ammoID);
                    if (pProto)
                    {
                        ammoDisplayID = pProto->DisplayInfoID;
                        ammoInventoryType = pProto->InventoryType;
                    }
                }
            }
        }
    }
    else
    {
        for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
        {

            if (uint32 item_class = m_caster->GetVirtualItemInfo(i, VIRTUAL_ITEM_INFO_0_OFFSET_CLASS))
            {
                if (item_class == ITEM_CLASS_WEAPON)
                {
                    switch (m_caster->GetVirtualItemInfo(i, VIRTUAL_ITEM_INFO_0_OFFSET_SUBCLASS))
                    {
                        case ITEM_SUBCLASS_WEAPON_THROWN:
                            ammoDisplayID = m_caster->GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + i);
                            ammoInventoryType = m_caster->GetVirtualItemInfo(i, VIRTUAL_ITEM_INFO_0_OFFSET_INVENTORYTYPE);
                            break;
                        case ITEM_SUBCLASS_WEAPON_BOW:
                        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                            ammoDisplayID = 5996;
                            ammoInventoryType = INVTYPE_AMMO;
                            break;
                        case ITEM_SUBCLASS_WEAPON_GUN:
                            ammoDisplayID = 5998;
                            ammoInventoryType = INVTYPE_AMMO;
                            break;
                    }

                    if (ammoDisplayID)
                    {
                        break;
                    }
                }
            }
        }
    }

    *data << uint32(ammoDisplayID);
    *data << uint32(ammoInventoryType);
}

void Spell::WriteSpellGoTargets(WorldPacket* data)
{

    *data << (uint8)(m_roster.Units().size() + m_roster.Objects().size());

    for (auto& enrolled : m_roster.Units())
    {
        *data << enrolled.guid;

        if (enrolled.slots == 0)
        {

            enrolled.verdict = SPELL_MISS_IMMUNE2;
        }
        else if (enrolled.verdict == SPELL_MISS_NONE)
        {
            m_needAliveTargetMask |= enrolled.slots;
        }
    }

    for (const auto& enrolled : m_roster.Objects())
    {
        *data << enrolled.guid;
    }

    *data << uint8(0);

    if (!(Recipe().Starts() == cast::Start::Channelled))
    {
        m_needAliveTargetMask = 0;
    }
}

void Spell::SendLogExecute()
{

    WorldPacket data(SMSG_SPELLLOGEXECUTE, (8 + 4 + 4 + (4 + 4 + 8)));

    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->ID);

    size_t efcount_pos = data.wpos();
    int32 effectCount = 0;
    data << uint32(effectCount);

    size_t starteff_pos = data.wpos();
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        data << uint32(Recipe().At(static_cast<uint8>(i)).verb);
        data << uint32(1);

        bool hasSpecial = true;
        {
            switch (Recipe().At(static_cast<uint8>(i)).verb)
            {
                case SPELL_EFFECT_POWER_DRAIN:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);
                    data << uint32(0);
                    data << float(0);
                    break;
                case SPELL_EFFECT_ADD_EXTRA_ATTACKS:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);
                    break;
                case SPELL_EFFECT_INTERRUPT_CAST:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);
                    break;
                case SPELL_EFFECT_DURABILITY_DAMAGE:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);
                    data << uint32(0);
                    break;
                case SPELL_EFFECT_CREATE_ITEM:
                    data << uint32(Recipe().At(static_cast<uint8>(i)).itemType);
                    break;
                case SPELL_EFFECT_FEED_PET:
                    data << m_targets.getItemTargetEntry();
                    break;
                case SPELL_EFFECT_RESURRECT:
                case SPELL_EFFECT_DISPEL:
                case SPELL_EFFECT_THREAT:
                case SPELL_EFFECT_DISTRACT:
                case SPELL_EFFECT_SANCTUARY:
                case SPELL_EFFECT_THREAT_ALL:
                case SPELL_EFFECT_DISPEL_MECHANIC:
                case SPELL_EFFECT_RESURRECT_NEW:
                case SPELL_EFFECT_ATTACK_ME:
                case SPELL_EFFECT_SKIN_PLAYER_CORPSE:
                case SPELL_EFFECT_MODIFY_THREAT_PERCENT:
                case SPELL_EFFECT_126:
                case SPELL_EFFECT_DISMISS_PET:
                case SPELL_EFFECT_OPEN_LOCK:
                case SPELL_EFFECT_OPEN_LOCK_ITEM:
                case SPELL_EFFECT_INSTAKILL:
                    if (ObjectGuid guid = GetPrefilledOrUnitTargetGuid(SpellEffectIndex(i)))
                    {
                        data << guid;
                    }
                    else if (m_targets.getItemTargetGuid())
                    {
                        data << m_targets.getItemTargetGuid();
                    }
                    else if (m_targets.getGOTargetGuid())
                    {
                        data << m_targets.getGOTargetGuid();
                    }
                    break;
                case SPELL_EFFECT_DUMMY:
                    break;
                default:
                    hasSpecial = false;
                    break;
            }
        }
        if (hasSpecial)
        {
            ++effectCount;
            starteff_pos = data.wpos();
        }
        else
        {
            data.wpos(starteff_pos);
        }
    }
    if (!effectCount)
    {
        effectCount = 1;
        data << uint32(Recipe().At(EFFECT_INDEX_0).verb);
        data << uint32(1);
    }
    data.put<uint32>(efcount_pos, effectCount);

    Deliver(Audience::Around(*m_caster).AndSubject(), &data);
}

void Spell::SendInterrupted(SpellCastResult result)
{
    Player *casterPlayer = static_cast<Player*>(m_caster);

    if (casterPlayer)
    {
        WorldPacket data(SMSG_SPELL_FAILURE, (8 + 4 + 1));
        data << m_caster->GetObjectGuid();
        data << m_spellInfo->ID;
        data << uint8(result);
        casterPlayer->SendDirectMessage(&data);
    }

    WorldPacket data(SMSG_SPELL_FAILED_OTHER, (8 + 4));
    data << m_caster->GetObjectGuid();
    data << m_spellInfo->ID;
    if (casterPlayer)
    {
        Deliver(Audience::Around(*casterPlayer).Except(casterPlayer), &data);
    }
    else
    {
        Deliver(Audience::Around(*m_caster).AndSubject(), &data);
    }
}

void Spell::SendChannelUpdate(uint32 time)
{
    if (time == 0)
    {

        if (Recipe().Says().farsight &&IsPlayer(m_caster) && m_caster->GetCharmGuid() &&
            !IsSpellHaveAura(m_spellInfo, SPELL_AURA_MOD_POSSESS) && !IsSpellHaveAura(m_spellInfo, SPELL_AURA_MOD_POSSESS_PET))
        {
            Player* player = (Player*)m_caster;

            Unit* possessed = player->GetCharm();

            player->SetCharm(nullptr);
            if (possessed)
            {
                player->SetClientControl(possessed, 0);
            }
            player->SetMover(nullptr);
            player->GetCamera().ResetView();
            player->RemovePetActionBar();

            if (possessed)
            {
                possessed->clearUnitState(UNIT_STAT_CONTROLLED);
                possessed->RemoveUnitFlag(UNIT_FLAG_POSSESSED);
                possessed->SetCharmerGuid(0);

                if (possessed->GetUInt32Value(UNIT_CREATED_BY_SPELL) == m_spellInfo->ID &&IsCreature(possessed))
                {
                    ((Creature*)possessed)->ForcedDespawn();
                }
            }
        }

        m_caster->RemoveAurasCastBy(m_spellInfo->ID, m_caster->GetObjectGuid());

        ObjectGuid target_guid = m_caster->GetChannelObjectGuid();
        if (target_guid != m_caster->GetObjectGuid() && (GuidHigh(target_guid) == HIGHGUID_UNIT || GuidHigh(target_guid) == HIGHGUID_PET || GuidHigh(target_guid) == HIGHGUID_PLAYER))
        {
            if (Unit* target = ObjectLookup::GetUnit(*m_caster, target_guid))
            {
                target->RemoveAurasCastBy(m_spellInfo->ID, m_caster->GetObjectGuid());
            }
        }

        if (m_caster->GetUInt32Value(UNIT_CHANNEL_SPELL) != m_spellInfo->ID)
        {
            return;
        }

        m_caster->SetChannelObjectGuid(0);
        m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, 0);
    }

    if (IsPlayer(m_caster))
    {
        WorldPacket data(MSG_CHANNEL_UPDATE, 4);
        data << uint32(time);
        ((Player*)m_caster)->SendDirectMessage(&data);
    }
}

void Spell::SendChannelStart(uint32 duration)
{
    Occupant* target = nullptr;

    if (Recipe().At(EFFECT_INDEX_0).verb == SPELL_EFFECT_PERSISTENT_AREA_AURA)
    {
        target = m_caster->Conjured().AreaOf(m_spellInfo->ID, EFFECT_INDEX_0);
    }

    else if (!m_roster.Units().empty())
    {
        for (const auto& enrolled : m_roster.Units())
        {
            if ((enrolled.slots & (1 << EFFECT_INDEX_0)) && enrolled.reflectedVerdict == SPELL_MISS_NONE &&
                enrolled.guid != m_caster->GetObjectGuid())
            {
                target = ObjectLookup::GetUnit(*m_caster, enrolled.guid);
                break;
            }
        }
    }
    else if (!m_roster.Objects().empty())
    {
        for (const auto& enrolled : m_roster.Objects())
        {
            if (enrolled.slots & (1 << EFFECT_INDEX_0))
            {
                target = m_caster->GetMap()->GetGameObject(enrolled.guid);
                break;
            }
        }
    }

    if (IsPlayer(m_caster))
    {
        WorldPacket data(MSG_CHANNEL_START, (4 + 4));
        data << uint32(m_spellInfo->ID);
        data << uint32(duration);
        ((Player*)m_caster)->SendDirectMessage(&data);
    }

    m_timer = duration;

    if (target)
    {
        m_caster->SetChannelObjectGuid(target->GetObjectGuid());
    }

    m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, m_spellInfo->ID);
}

void Spell::SendResurrectRequest(Player* target)
{

    const char* sentName =IsPlayer(m_caster) ? "" : m_caster->GetNameForLocaleIdx(target->GetSession()->GetSessionDbLocaleIndex());

    WorldPacket data(SMSG_RESURRECT_REQUEST, (8 + 4 + strlen(sentName) + 1 + 1 + 1));
    data << m_caster->GetObjectGuid();
    data << uint32(strlen(sentName) + 1);

    data << sentName;
    data << uint8(m_caster->isSpiritHealer());

    data << uint8(!Recipe().Says().ignoresResurrectionTimer);
    target->GetSession()->SendPacket(&data);
}
