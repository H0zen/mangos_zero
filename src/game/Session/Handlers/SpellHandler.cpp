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

#include "Platform/Define.h"
#include "DBCStores.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "SpellAnswers.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "Totem.h"
#include "SpellAuras.h"
#include "Cast/Recipe/RecipeBook.h"

void spells::UseItem(Player& who, WorldPacket& recvPacket)
{
    uint8 bagIndex, slot;
    uint8 spell_count;

    recvPacket >> bagIndex >> slot >> spell_count;

    Player* pUser = &who;

    if (!pUser->IsSelfMover())
    {
        recvPacket.rpos(recvPacket.wpos());
        return;
    }

    Item* pItem = pUser->GetItemByPos(bagIndex, slot);
    if (!pItem)
    {
        recvPacket.rpos(recvPacket.wpos());
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    DETAIL_LOG("WORLD: CMSG_USE_ITEM packet, bagIndex: %u, slot: %u, spell_count: %u , Item: %u, data length = %u", bagIndex, slot, spell_count, pItem->GetEntry(), (uint32)recvPacket.size());

    ItemPrototype const* proto = pItem->GetProto();
    if (!proto)
    {
        recvPacket.rpos(recvPacket.wpos());
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    if (proto->InventoryType != INVTYPE_NON_EQUIP && !pItem->IsEquipped())
    {
        recvPacket.rpos(recvPacket.wpos());
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    InventoryResult msg = pUser->CanUseItem(pItem);
    if (msg != EQUIP_ERR_OK)
    {
        recvPacket.rpos(recvPacket.wpos());
        pUser->SendEquipError(msg, pItem, nullptr);
        return;
    }

    if (pItem->IsInTrade())
    {
        recvPacket.rpos(recvPacket.wpos());
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    if (pUser->IsInCombat())
    {
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (SpellEntry const* spellInfo = sSpellStore.LookupEntry(proto->Spells[i].SpellId))
            {
                if (IsNonCombatSpell(spellInfo))
                {
                    recvPacket.rpos(recvPacket.wpos());
                    pUser->SendEquipError(EQUIP_ERR_NOT_IN_COMBAT, pItem, nullptr);
                    return;
                }
            }
        }
    }

    if (pItem->GetProto()->Bonding == BIND_WHEN_USE || pItem->GetProto()->Bonding == BIND_WHEN_PICKED_UP || pItem->GetProto()->Bonding == BIND_QUEST_ITEM)
    {
        if (!pItem->IsSoulBound())
        {
            pItem->SetState(ITEM_CHANGED, pUser);
            pItem->SetBinding(true);
        }
    }

    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(pUser);

    targets.Update(pUser);

    if (!pItem->IsTargetValidForItemUse(targets.getUnitTarget()))
    {

        pUser->SendEquipError(EQUIP_ERR_NONE, pItem, nullptr);

        uint32 spellid = 0;
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE || proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_NO_DELAY_USE)
            {
                spellid = proto->Spells[i].SpellId;
                break;
            }
        }

        if (SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid))
        {
            Spell::SendCastResult(&who, spellInfo, SPELL_FAILED_BAD_TARGETS);
        }
        return;
    }

    if (!sScriptMgr.OnItemUse(pUser, pItem, targets))
    {

        pUser->CastItemUseSpell(pItem, targets);
    }
}

#define OPEN_CHEST 11437
#define OPEN_SAFE 11535
#define OPEN_CAGE 11792
#define OPEN_BOOTY_CHEST 5107
#define OPEN_STRONGBOX 8517

void spells::OpenItem(Player& who, WorldPacket& recvPacket)
{
    DETAIL_LOG("WORLD: CMSG_OPEN_ITEM packet, data length = %zu", recvPacket.size());

    uint8 bagIndex, slot;

    recvPacket >> bagIndex >> slot;

    DETAIL_LOG("bagIndex: %u, slot: %u", bagIndex, slot);

    Player* pUser = &who;

    if (!pUser->IsSelfMover())
    {
        return;
    }

    Item* pItem = pUser->GetItemByPos(bagIndex, slot);
    if (!pItem)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    ItemPrototype const* proto = pItem->GetProto();
    if (!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    uint32 lockId = proto->LockID;
    if (lockId && !pItem->HasItemFlag(ITEM_DYNFLAG_UNLOCKED))
    {
        LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

        if (!lockInfo)
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, pItem, nullptr);
            sLog.outError("WORLD::OpenItem: item [guid = %u] has an unknown lockId: %u!", pItem->GetGUIDLow() , lockId);
            return;
        }

        if (lockInfo->Skill[1] || lockInfo->Skill[0])
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, pItem, nullptr);
            return;
        }
    }

    if (pItem->HasItemFlag(ITEM_DYNFLAG_WRAPPED))
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `entry`, `flags` FROM `character_gifts` WHERE `item_guid` = '%u'", pItem->GetGUIDLow());
        if (result)
        {
            Field* fields = result->Fetch();
            uint32 entry = fields[0].GetUInt32();
            uint32 flags = fields[1].GetUInt32();

            pItem->SetGiftCreatorGuid(0);
            pItem->SetEntry(entry);
            pItem->SetAllItemFlags(flags);
            pItem->SetState(ITEM_CHANGED, pUser);
            delete result;
        }
        else
        {
            sLog.outError("Wrapped item %u don't have record in character_gifts table and will deleted", pItem->GetGUIDLow());
            pUser->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
            return;
        }

        static SqlStatementID delGifts ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
        stmt.PExecute(pItem->GetGUIDLow());
    }
    else
    {
        pUser->SendLoot(pItem->GetObjectGuid(), LOOT_CORPSE);
    }
}

void spells::GameObjectUse(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;

    recv_data >> guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_GAMEOBJ_USE guid: %s", GuidString(guid).c_str());

    if (!who.IsSelfMover())
    {
        return;
    }

    GameObject* obj = who.GetMap()->GetGameObject(guid);
    if (!obj)
    {
        return;
    }

    if (!InReach(*obj, who, obj->GetInteractionDistance()))
    {
        return;
    }

    if (!obj->isSpawned())
    {
        sLog.outError("HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for despawned GameObject (Entry %u), didn't expect this to happen.", obj->GetEntry());
        return;
    }

    if (obj->GetGoType() == GAMEOBJECT_TYPE_GENERIC)
    {
        sLog.outError("HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for not allowed GameObject type %u (Entry %u), didn't expect this to happen.", obj->GetGoType(), obj->GetEntry());
        return;
    }

    if (obj->HasGoFlag(GO_FLAG_NO_INTERACT))
    {
        sLog.outError("HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for GameObject (Entry %u) with non intractable flag (Flags %u), didn't expect this to happen.", obj->GetEntry(), obj->GetGoFlags());
        return;
    }

    obj->Use(&who);
}

void spells::CastSpell(Player& who, WorldPacket& recvPacket)
{
    uint32 spellId;
    recvPacket >> spellId;

    Unit* mover = who.GetMover();
    if (mover != &who &&IsPlayer(mover))
    {
        recvPacket.rpos(recvPacket.wpos());
        return;
    }

    DEBUG_LOG("WORLD: got cast spell packet, spellId - %u, data length = %zu",
        spellId, recvPacket.size());

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u", spellId);
        recvPacket.rpos(recvPacket.wpos());
        return;
    }

    if (IsPlayer(mover))
    {

        if (!((Player*)mover)->HasActiveSpell(spellId) || (cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
        {
            sLog.outError("World: Player %u casts spell %u which he shouldn't have", mover->GetGUIDLow(), spellId);

            recvPacket.rpos(recvPacket.wpos());
            return;
        }
    }
    else
    {

        if (!((Creature*)mover)->HasSpell(spellId) || (cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
        {

            recvPacket.rpos(recvPacket.wpos());
            return;
        }
    }

    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(&who);

    if (Unit* target = targets.getUnitTarget())
    {

        if (SpellEntry const* actualSpellInfo = sSpellMgr.SelectAuraRankForLevel(spellInfo, target->getLevel()))
        {
            spellInfo = actualSpellInfo;
        }
    }

    Spell* spell = new Spell(&who, spellInfo, false);
    spell->prepare(&targets);
}

void spells::CancelCast(Player& who, WorldPacket& recvPacket)
{
    uint32 spellId;

    recvPacket >> spellId;

    Unit* mover = who.GetMover();
    if (mover != &who &&IsPlayer(mover))
    {
        return;
    }

    if (who.IsNonMeleeSpellCasted(false))
    {
        who.InterruptNonMeleeSpells(false, spellId);
    }
}

void spells::CancelAura(Player& who, WorldPacket& recvPacket)
{
    uint32 spellId;
    recvPacket >> spellId;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return;
    }

    if (cast::RecipeOf(*spellInfo).Says().cannotBeCancelled)
    {
        return;
    }

    if ((cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
    {
        return;
    }

    if (!cast::Recipes().IsPositive(spellId))
    {

        if (!who.IsSelfMover())
        {

            bool allow = false;
            for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
            {
                if (spellInfo->EffectAura[k] == SPELL_AURA_MOD_POSSESS ||
                    spellInfo->EffectAura[k] == SPELL_AURA_MOD_POSSESS_PET)
                {
                    allow = true;
                    break;
                }
            }

            if (!allow)
            {
                return;
            }
        }
        else
        {
            return;
        }
    }

    if ((cast::RecipeOf(*spellInfo).Starts() == cast::Start::Channelled))
    {
        if (Spell* curSpell = who.GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            if (curSpell->m_spellInfo->ID == spellId)
            {
                who.InterruptSpell(CURRENT_CHANNELED_SPELL);
            }
        }
        return;
    }

    SpellAuraHolder* holder = who.GetSpellAuraHolder(spellId);

    if (holder && holder->GetCasterGuid() != who.GetObjectGuid() && HasAreaAuraEffect(holder->GetSpellProto()))
    {
        return;
    }

    who.CancelAuras(spellId);
}

void spells::PetCancelAura(Player& who, WorldPacket& recvPacket)
{
    ObjectGuid guid = 0;
    uint32 spellId;

    recvPacket >> guid;
    recvPacket >> spellId;

    if (!who.IsSelfMover())
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown PET spell id %u", spellId);
        return;
    }

    Creature* pet = who.GetMap()->GetAnyTypeCreature(guid);

    if (!pet)
    {
        sLog.outError("HandlePetCancelAuraOpcode - %s not exist.", GuidString(guid).c_str());
        return;
    }

    if (guid != who.GetPetGuid() && guid != who.GetCharmGuid())
    {
        sLog.outError("HandlePetCancelAura. %s isn't pet of %s", GuidString(guid).c_str(), who.GetGuidStr().c_str());
        return;
    }

    if (!pet->IsAlive())
    {
        pet->SendPetActionFeedback(FEEDBACK_PET_DEAD);
        return;
    }

    pet->RemoveAuras(spellId);

    pet->AddCreatureSpellCooldown(spellId);
}

void spells::CancelGrowthAura(Player& who, WorldPacket& )
{

}

void spells::CancelAutoRepeatSpell(Player& who, WorldPacket& )
{

    who.GetMover()->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
}

void spells::CancelChanneling(Player& who, WorldPacket& recv_data)
{
    recv_data.read_skip<uint32>();

    Unit* mover = who.GetMover();
    if (mover != &who &&IsPlayer(mover))
    {
        return;
    }

    who.InterruptSpell(CURRENT_CHANNELED_SPELL);
}

void spells::TotemDestroyed(Player& who, WorldPacket& recvPacket)
{
    uint8 slotId;

    recvPacket >> slotId;

    if (!who.IsSelfMover())
    {
        return;
    }

    if (int(slotId) >= MAX_TOTEM_SLOT)
    {
        return;
    }

    if (Totem* totem = who.Retainers().TotemIn(TotemSlot(slotId)))
    {
        totem->UnSummon();
    }
}

void spells::SelfRes(Player& who, WorldPacket& )
{
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: CMSG_SELF_RES");

    if (who.GetUInt32Value(PLAYER_SELF_RES_SPELL))
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(who.GetUInt32Value(PLAYER_SELF_RES_SPELL));
        if (spellInfo)
        {
            who.CastSpell(&who, spellInfo, false);
        }

        who.SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
    }
}
