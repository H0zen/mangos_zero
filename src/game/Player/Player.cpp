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
#include "Utilities/Errors.h"
#include <cassert>
#include <sstream>
#include <string>
#include <vector>
#include "Utilities/MathDefines.h"
#include "Utilities/PackedValues.h"
#include "Player.h"
#include "LoginEffectPackets.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapCoords.h"
#include "MapFoundry.h"
#include "MapRoster.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "PlayerRegistry.h"
#include "CorpseManager.h"
#include "ObjectLookup.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "TransportMap.h"
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
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"

#include <cmath>
#include "Cast/Recipe/RecipeBook.h"

namespace
{

    class LoginEffectEvent final : public BasicEvent
    {
        public:
            explicit LoginEffectEvent(Player& player) : m_player(player)
            {
            }

            bool Execute(uint64 eTime, uint32) override
            {
                std::optional<LoginEffectPhase> phase =
                    m_state.TakeNext(m_player.IsInWorld());
                if (!phase)
                {
                    return true;
                }

                WorldPacket packet = *phase == LoginEffectPhase::Start ?
                    LoginEffectPackets::BuildStart(
                        m_player.GetObjectGuid()) :
                    LoginEffectPackets::BuildGo(
                        m_player.GetObjectGuid());
                Deliver(Audience::Around(m_player).AndSubject(), &packet);

                if (*phase == LoginEffectPhase::Start)
                {
                    m_player.m_Events.AddEvent(this,
                        eTime + LoginEffectDelayBefore(LoginEffectPhase::Go),
                        false);
                    return false;
                }
                return true;
            }

        private:
            Player& m_player;
            LoginEffectSequenceState m_state;
    };

    class LoginCinematicRootTimeoutEvent final : public BasicEvent
    {
        public:
            explicit LoginCinematicRootTimeoutEvent(Player& player)
                : m_player(player)
            {
            }

            bool Execute(uint64, uint32) override
            {
                m_player.ReleaseLoginCinematicRoot();
                return true;
            }

        private:
            Player& m_player;
    };
}

#define ZONE_UPDATE_INTERVAL (1*IN_MILLISECONDS)

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))
#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)
#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v,m)

PlayerTaxi::PlayerTaxi()
{

    memset(m_taximask, 0, sizeof(m_taximask));
}

void PlayerTaxi::InitTaxiNodes(uint32 race, uint32 )
{
    memset(m_taximask, 0, sizeof(m_taximask));

    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    m_taximask[0] = rEntry->StartingTaxiNodes;
}

std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi)
{
    for (int i = 0; i < TaxiMaskSize; ++i)
    {
        ss << taxi.m_taximask[i] << " ";
    }
    return ss;
}

SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, SpellEntry const* spellEntry, SpellEffectIndex eff, int16 _charges ) : op(_op), type(_type), charges(_charges), value(_value), spellId(spellEntry->ID), lastAffected(nullptr)
{
    mask = sSpellMgr.GetSpellAffectMask(spellEntry->ID, eff);
}

SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, Aura const* aura, int16 _charges ) : op(_op), type(_type), charges(_charges), value(_value), spellId(aura->GetId()), lastAffected(nullptr)
{
    mask = sSpellMgr.GetSpellAffectMask(aura->GetId(), aura->GetEffIndex());
}

bool SpellModifier::isAffectedOnSpell(SpellEntry const* spell) const
{
    SpellEntry const* affect_spell = sSpellStore.LookupEntry(spellId);

    if (!affect_spell || affect_spell->SpellClassSet != spell->SpellClassSet)
    {
        return false;
    }
    return spell->IsFitToFamilyMask(mask);
}

TradeData* TradeData::GetTraderData() const
{
    return m_trader->GetTradeData();
}

Item* TradeData::GetItem(TradeSlots slot) const
{
    return m_items[slot] ? m_player->GetItemByGuid(m_items[slot]) : nullptr;
}

bool TradeData::HasItem(ObjectGuid item_guid) const
{
    for (int i = 0; i < TRADE_SLOT_COUNT; ++i)
    {
        if (m_items[i] == item_guid)
        {
            return true;
        }
    }
    return false;
}

Item* TradeData::GetSpellCastItem() const
{
    return m_spellCastItem ?  m_player->GetItemByGuid(m_spellCastItem) : nullptr;
}

void TradeData::SetItem(TradeSlots slot, Item* item)
{
    ObjectGuid itemGuid = item ? item->GetObjectGuid() : 0;

    if (m_items[slot] == itemGuid)
    {
        return;
    }

    m_items[slot] = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update();

    if (slot == TRADE_SLOT_NONTRADED)
    {
        GetTraderData()->SetSpell(0);
    }

    SetSpell(0);
}

void TradeData::SetSpell(uint32 spell_id, Item* castItem )
{
    ObjectGuid itemGuid = castItem ? castItem->GetObjectGuid() : 0;

    if (m_spell == spell_id && m_spellCastItem == itemGuid)
    {
        return;
    }

    m_spell = spell_id;
    m_spellCastItem = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update(true);
    Update(false);
}

void TradeData::SetMoney(uint32 money)
{
    if (m_money == money)
    {
        return;
    }

    if (money > m_player->GetMoney())
    {
        TradeStatusInfo info;
        info.Status = TRADE_STATUS_CLOSE_WINDOW;
        info.Result = EQUIP_ERR_NOT_ENOUGH_MONEY;
        m_player->GetSession()->SendTradeStatus(info);
        return;
    }

    m_money = money;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update();
}

void TradeData::Update(bool for_trader )
{
    if (for_trader)
    {
        m_trader->GetSession()->SendUpdateTrade(true);
    }
    else
    {
        m_player->GetSession()->SendUpdateTrade(false);
    }
}

void TradeData::SetAccepted(bool state, bool crosssend )
{
    m_accepted = state;

    if (!state)
    {
        TradeStatusInfo info;
        info.Status = TRADE_STATUS_BACK_TO_TRADE;
        if (crosssend)
        {
            m_trader->GetSession()->SendTradeStatus(info);
        }
        else
        {
            m_player->GetSession()->SendTradeStatus(info);
        }
    }
}

Player::Player(WorldSession* session): Unit(), m_inventory(*this), m_honor(*this), m_journal(*this), m_perils(*this), m_drink(*this), m_rest(*this), m_post(*this), m_arms(*this), m_spellMods(*this), m_duel(*this), m_battle(*this), m_binds(*this), m_sheet(*this), m_pace(*this), m_mover(this), m_camera(this), m_reputationMgr(this), m_spellCooldownMgr(this), m_petMgr(this)
{

    m_transport = 0;

    m_speakTime = 0;
    m_speakCount = 0;

    m_visibilityObserverSweepTimer = World::GetVisibilityObserverSweepInterval();

    m_objectTypeId = TYPEID_PLAYER;

    SetActiveObjectState(true);

    m_session = session;

    m_ExtraFlags = 0;
    if (GetSession()->GetSecurity() >= SEC_GAMEMASTER)
    {
        SetAcceptTicket(true);
    }

    if (GetSession()->GetSecurity() == SEC_PLAYER)
    {
        SetAcceptWhispers(true);
    }

    m_comboPoints = 0;

    m_usedTalentCount = 0;

    m_rageDecayRate = 1.25f;
    m_rageDecayMultiplier = 19.50f;

    m_zoneUpdateId = 0;
    m_zoneUpdateTimer = 0;
    m_positionStatusUpdateTimer = 0;

    m_areaUpdateId = 0;

    m_nextSave = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    m_nextSave = urand(m_nextSave / 2, m_nextSave * 3 / 2);

    clearResurrectRequestData();

    m_social = nullptr;

    Invites().ToParty(nullptr);
    m_groupUpdateMask = 0;
    m_auraUpdateMask = 0;

    ClearHonorInfo();

    m_atLoginFlags = AT_LOGIN_NONE;

    m_trade = nullptr;

    m_cinematic = 0;

    PlayerTalkClass = new PlayerMenu(GetSession());

    m_deathTimer = 0;

    m_deathExpireTime = 0;

    m_DetectInvTimer = 1 * IN_MILLISECONDS;

    m_played.StartAt(time(nullptr));

    m_resetTalentsCost = 0;

    m_resetTalentsTime = 0;

    for (int i = 0; i < MAX_MOVE_TYPE; ++i)
    {
        m_forced_speed_changes[i] = 0;
    }

    for (int i = 0; i < BASEMOD_END; ++i)
    {
        m_auraBaseMod[i][FLAT_MOD] = 0.0f;
        m_auraBaseMod[i][PCT_MOD] = 1.0f;
    }

    m_contestedPvPTimer = 0;

    m_lastFallTime = 0;

    m_lastFallZ = 0;

}

Player::~Player()
{

    CleanupsBeforeDelete();

    for (uint8 i = 0; i < PLAYER_SLOTS_COUNT; ++i)
    {
        delete m_inventory.Own(i);
    }

    CleanupChannels();

    delete PlayerTalkClass;

    for (size_t x = 0; x < ItemSetEff.size(); ++x)
    {
        delete ItemSetEff[x];
    }

}

void Player::CleanupsBeforeDelete()
{

    m_loginCinematicRootOwnership.Clear();

    if (m_cinematicFlyover && m_cinematicFlyover->IsActive())
    {
        m_cinematicFlyover->Stop();
    }
    m_cinematicFlyover.reset();

    if (m_mirror.IsOpen())
    {

        TradeCancel(false);

        Duelling().Complete(DUEL_FLED);
    }

    sOutdoorPvPMgr.HandlePlayerLeaveZone(this, m_zoneUpdateId);

    Unit::CleanupsBeforeDelete();
}

bool Player::Create(uint32 guidlow, const std::string& name, uint8 race, uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle, uint8 hairColor, uint8 facialHair, uint8 )
{

    Object::_Create(guidlow, 0, HIGHGUID_PLAYER);
    m_inventory.Saves().Belongs(GetObjectGuid());

    m_name = name;

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, class_);
    if (!info)
    {
        sLog.outError("Player has incorrect race/class pair. Can't be loaded.");
        return false;
    }

    ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(class_);
    if (!cEntry)
    {
        sLog.outError("Class %u not found in DBC (Wrong DBC files?)", class_);
        return false;
    }

    if (gender != uint8(GENDER_MALE) && gender != uint8(GENDER_FEMALE))
    {
        sLog.outError("Invalid gender %u at player creation", uint32(gender));
        return false;
    }

    for (uint8 i = 0; i < PLAYER_SLOTS_COUNT; ++i)
    {
        m_inventory.Own(i, nullptr);
    }

    SetLocationMapId(info->mapId);
    Place().MoveTo(info->positionX, info->positionY, info->positionZ, info->orientation);
    m_movementInfo.ChangePosition(info->positionX, info->positionY,
                                  info->positionZ, info->orientation);

    SetMap(sMapFoundry.OpenFor(*this, info->mapId));

    uint8 powertype = cEntry->DisplayPower;

    setFactionForRace(race);

    SetRace(race);
    SetClass(class_);
    SetGender(gender);
    SetPowerKind(Powers(powertype));

    InitDisplayIds();

    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    SetCastSpeedMod(1.0f);

    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, -1);

    SetByteValue(PLAYER_BYTES, 0, skin);
    SetByteValue(PLAYER_BYTES, 1, face);
    SetByteValue(PLAYER_BYTES, 2, hairStyle);
    SetByteValue(PLAYER_BYTES, 3, hairColor);
    SetByteValue(PLAYER_BYTES_2, 0, facialHair);
    SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_NORMAL);

    SetDrunkAndGender(0, gender);
    SetShownHonorRank(0);

    SetUInt32Value(PLAYER_GUILDID, 0);
    SetUInt32Value(PLAYER_GUILDRANK, 0);
    SetUInt32Value(PLAYER_GUILD_TIMESTAMP, 0);

    if (GetSession()->GetSecurity() >= SEC_MODERATOR)
    {
        SetUInt32Value(UNIT_FIELD_LEVEL, sWorld.getConfig(CONFIG_UINT32_START_GM_LEVEL));
    }
    else
    {
        SetUInt32Value(UNIT_FIELD_LEVEL, sWorld.getConfig(CONFIG_UINT32_START_PLAYER_LEVEL));
    }

    SetUInt32Value(PLAYER_FIELD_COINAGE, sWorld.getConfig(CONFIG_UINT32_START_PLAYER_MONEY));

    m_played.Fresh(time(nullptr));

    InitStatsForLevel();
    InitTaxiNodes();
    InitTalentForLevel();
    InitPrimaryProfessions();

    Sheet().MaxHealth();
    SetHealth(GetMaxHealth());

    if (GetPowerType() == POWER_MANA)
    {
        Sheet().MaxPower(POWER_MANA);
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }

    learnDefaultSpells();

    for (PlayerCreateInfoActions::const_iterator action_itr = info->action.begin(); action_itr != info->action.end(); ++action_itr)
    {
        addActionButton(action_itr->button, action_itr->action, action_itr->type);
    }

    CharStartOutfitEntry const* oEntry = nullptr;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        if (CharStartOutfitEntry const* entry = sCharStartOutfitStore.LookupEntry(i))
        {
            if (entry->RaceID == race && entry->ClassID == class_ && entry->SexID == gender)
            {
                oEntry = entry;
                break;
            }
        }
    }

    if (oEntry)
    {
        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (oEntry->ItemID[j] <= 0)
            {
                continue;
            }

            uint32 item_id = oEntry->ItemID[j];

            ItemPrototype const* iProto = ObjectMgr::GetItemPrototype(item_id);
            if (!iProto)
            {
                continue;
            }

            int32 count = iProto->BuyCount;

            if (iProto->Class == ITEM_CLASS_CONSUMABLE && iProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                switch (iProto->Spells[0].SpellCategory)
                {
                    case 11:
                        if (iProto->Stackable > 4)
                        {
                            count = 4;
                        }
                        break;
                    case 59:
                        if (iProto->Stackable > 2)
                        {
                            count = 2;
                        }
                        break;
                }
            }

            StoreNewItemInBestSlots(item_id, count);
        }
    }

    for (PlayerCreateInfoItems::const_iterator item_id_itr = info->item.begin(); item_id_itr != info->item.end(); ++item_id_itr)
    {
        StoreNewItemInBestSlots(item_id_itr->item_id, item_id_itr->item_amount);
    }

    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            uint16 eDest;

            InventoryResult msg = CanEquipItem(NULL_SLOT, eDest, pItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                EquipItem(eDest, pItem, true);
            }

            else
            {
                ItemPosCountVec sDest;
                msg = CanStoreItem(NULL_BAG, NULL_SLOT, sDest, pItem, false);
                if (msg == EQUIP_ERR_OK)
                {
                    RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                    pItem = StoreItem(sDest, pItem, true);
                }

                msg = CanUseAmmo(pItem->GetEntry());
                if (msg == EQUIP_ERR_OK)
                {
                    SetAmmo(pItem->GetEntry());
                }
            }
        }
    }

    return true;
}

bool Player::StoreNewItemInBestSlots(uint32 titem_id, uint32 titem_amount)
{
    DEBUG_LOG("STORAGE: Creating initial item, itemId = %u, count = %u", titem_id, titem_amount);

    while (titem_amount > 0)
    {
        uint16 eDest;
        uint8 msg = CanEquipNewItem(NULL_SLOT, eDest, titem_id, false);
        if (msg != EQUIP_ERR_OK)
        {
            break;
        }

        EquipNewItem(eDest, titem_id, true);
        AutoUnequipOffhandIfNeed();
        --titem_amount;
    }

    if (titem_amount == 0)
    {
        return true;
    }

    ItemPosCountVec sDest;

    uint8 msg = CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, sDest, titem_id, titem_amount);
    if (msg == EQUIP_ERR_OK)
    {
        StoreNewItem(sDest, titem_id, true, Item::GenerateItemRandomPropertyId(titem_id));
        return true;
    }

    sLog.outError("STORAGE: Can't equip or store initial item %u for race %u class %u , error msg = %u", titem_id, getRace(), getClass(), msg);
    return false;
}

Item* Player::StoreNewItemInInventorySlot(uint32 itemEntry, uint32 amount)
{
    ItemPosCountVec vDest;

    uint8 msg = CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, vDest, itemEntry, amount);

    if (msg == EQUIP_ERR_OK)
    {
        if (Item* pItem = StoreNewItem(vDest, itemEntry, true, Item::GenerateItemRandomPropertyId(itemEntry)))
        {
            return pItem;
        }
    }

    return nullptr;
}

void Player::Update(uint32 update_diff, uint32 p_time)
{

    if (!IsInWorld())
    {
        return;
    }

    if (Post().NextDelivery() && Post().NextDelivery() <= time(nullptr))
    {
        Post().Expecting(time(nullptr));

    }

    SetCanDelayTeleport(true);
    Unit::Update(update_diff, p_time);
    SetCanDelayTeleport(false);

    if (World::GetVisibilityObserverSweepEnabled() && !IsBeingTeleported())
    {
        if (m_visibilityObserverSweepTimer <= update_diff)
        {
            m_visibilityObserverSweepTimer = World::GetVisibilityObserverSweepInterval();
            GetCamera().UpdateVisibilityForOwner();
        }
        else
        {
            m_visibilityObserverSweepTimer -= update_diff;
        }
    }

    if (m_cinematicFlyover && m_cinematicFlyover->IsActive())
    {
        m_cinematicFlyover->Update(update_diff);
    }

    if (uint32 ranged_att = getAttackTimer(RANGED_ATTACK))
    {
        setAttackTimer(RANGED_ATTACK, (update_diff >= ranged_att ? 0 : ranged_att - update_diff));
    }

    time_t now = time(nullptr);

    UpdatePvPFlag(now);

    UpdateContestedPvP(update_diff);

    Duelling().CountdownRunsOut(now);

    Duelling().WatchTheFlag(now);

    if (uint32 const since = m_played.Since(now))
    {
        m_inventory.RunClocks(since, false);
    }

    if (!m_journal.Timed().empty())
    {
        auto iter = m_journal.Timed().begin();
        while (iter != m_journal.Timed().end())
        {
            QuestStatusData& q_status = m_journal.Of(*iter);
            if (q_status.m_timer <= update_diff)
            {
                uint32 quest_id  = *iter;
                ++iter;
                FailQuest(quest_id);
            }
            else
            {
                q_status.m_timer -= update_diff;
                if (q_status.uState != QUEST_NEW)
                {
                    q_status.uState = QUEST_CHANGED;
                }
                ++iter;
            }
        }
    }

    if (hasUnitState(UNIT_STAT_MELEE_ATTACKING))
    {
        UpdateMeleeAttackingState();

        Unit* pVictim = getVictim();
        if (pVictim && !IsNonMeleeSpellCasted(false))
        {
            Player* vOwner = pVictim->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (vOwner && vOwner->IsPvP() && !Duelling().With(vOwner))
            {
                UpdatePvP(true);
                RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
            }
        }
    }

    if (HasPlayerFlag(PLAYER_FLAGS_RESTING))
    {
        if (Resting().EnteredInn() > 0)
        {
            time_t time_inn = now - Resting().EnteredInn();
            if (time_inn >= 10)
            {
                Resting().Bonus(Resting().Bonus() + Resting().Over(time_inn));
                m_rest.EnteredInn(now);
            }
        }
    }

    m_recovery.Run(update_diff);

    if (m_positionStatusUpdateTimer)
    {
        if (update_diff >= m_positionStatusUpdateTimer)
        {
            m_positionStatusUpdateTimer = 0;
        }
        else
        {
            m_positionStatusUpdateTimer -= update_diff;
        }
    }

    if (Arms().ChangeTimer() > 0)
    {
        if (update_diff >= Arms().ChangeTimer())
        {
        }
        else
        {
            Arms().ChangeTimer(Arms().ChangeTimer() - update_diff);
        }
    }

    if (m_zoneUpdateTimer > 0)
    {
        if (update_diff >= m_zoneUpdateTimer)
        {
            uint32 newzone, newarea;
            GetZoneAndAreaAboardOrHere(newzone, newarea);

            if (m_zoneUpdateId != newzone)
            {
                UpdateZone(newzone, newarea);
            }
            else
            {

                if (m_areaUpdateId != newarea)
                {
                    UpdateArea(newarea);
                }

                m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;
            }
        }
        else
        {
            m_zoneUpdateTimer -= update_diff;
        }
    }

    if (IsAlive())
    {
        RegenerateAll();
    }

    if (m_deathState == JUST_DIED)
    {
        KillPlayer();
    }

    if (m_nextSave > 0)
    {
        if (update_diff >= m_nextSave)
        {

            SaveToDB();
            DETAIL_LOG("Player '%s' (GUID: %u) saved", GetName(), GetGUIDLow());
        }
        else
        {
            m_nextSave -= update_diff;
        }
    }

    m_perils.Run(update_diff);

    if (m_DetectInvTimer > 0)
    {
        if (update_diff >= m_DetectInvTimer)
        {
            HandleStealthedUnitsDetection();
            m_DetectInvTimer = 3000;
        }
        else
        {
            m_DetectInvTimer -= update_diff;
        }
    }

    if (uint32 const elapsed = m_played.Since(now))
    {
        m_played.Advance(now);
    }

    m_drink.Run(update_diff);

    if (m_deathTimer > 0 && !GetMap()->Instanceable())
    {
        if (p_time >= m_deathTimer)
        {
            m_deathTimer = 0;
            BuildPlayerRepop();
            RepopAtGraveyard();
        }
        else
        {
            m_deathTimer -= p_time;
        }
    }

    m_inventory.RunEnchantClocks(update_diff);

    UpdateHomebindTime(update_diff);

    SendUpdateToOutOfRangeGroupMembers();
    if (IsHasDelayedTeleport())
    {
        TeleportTo(m_teleport.To(), m_teleport.Options());
    }

}

void Player::SetDeathState(DeathState s)
{
    uint32 ressSpellId = 0;

    bool cur = IsAlive();

    if (s == JUST_DIED && cur)
    {

        Drinking().Amount(0);

        ClearComboPoints();

        clearResurrectRequestData();

        RemoveAurasOfType(SPELL_AURA_MOD_SHAPESHIFT);

        RemovePet(PET_SAVE_REAGENTS);

        RemoveMiniPet();

        ressSpellId = GetUInt32Value(PLAYER_SELF_RES_SPELL);

        if (!ressSpellId)
        {
            ressSpellId = GetResurrectionSpellId();
        }

        if (InstanceData* mapInstance = GetInstanceData())
        {
            mapInstance->OnPlayerDeath(this);
        }
    }

    Unit::SetDeathState(s);

    if (s == JUST_DIED && cur && ressSpellId)
    {
        SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);
    }

    if (IsAlive() && !cur)
    {

        SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);

        if (getClass() == CLASS_WARRIOR)
        {
            CastSpell(this, SPELL_ID_PASSIVE_BATTLE_STANCE, true);
        }
    }
}

void Player::ToggleAFK()
{
    TogglePlayerFlag(PLAYER_FLAGS_AFK);

    if (isAFK() && Battle().InOne())
    {
        Battle().Leave();
    }
}

void Player::ToggleDND()
{
    TogglePlayerFlag(PLAYER_FLAGS_DND);
}

ChatTagFlags Player::GetChatTag() const
{
    if (isGMChat())
    {
        return CHAT_TAG_GM;
    }

    if (isAFK())
    {
        return CHAT_TAG_AFK;
    }
    if (isDND())
    {
        return CHAT_TAG_DND;
    }

    return CHAT_TAG_NONE;
}

bool Player::TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options , bool allowNoDelay )
{

    if (m_cinematicFlyover && m_cinematicFlyover->IsActive())
    {
        m_cinematicFlyover->Stop();
    }

    if (Transport::IsVesselMapId(mapid))
    {
        Map* deck = sMapRoster.Find(mapid);
        TransportMap* hull = deck ? deck->AsTransport() : nullptr;

        if (!hull)
        {
            sLog.outError("TeleportTo: vessel map %u has no hull; %s not moved.",
                          mapid, GetGuidStr().c_str());
            return false;
        }

        return hull->Board(this, x, y, z, orientation, options);
    }

    if (!MapCoords::Valid(mapid, x, y, z, orientation))
    {
        sLog.outError("TeleportTo: invalid map %d or absent instance template.", mapid);
        return false;
    }

    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);

    if (!isGameMaster() && DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, mapid, this))
    {
        sLog.outDebug("Player (GUID: %u, name: %s) tried to enter a forbidden map %u", GetGUIDLow(), GetName(), mapid);
        SendTransferAbortedByLockStatus(mEntry,nullptr, AREA_LOCKSTATUS_NOT_ALLOWED);
        return false;
    }

    Pet* pet = GetPet();

    if (!Battle().InOne() && mEntry->IsBattleGround())
    {
        return false;
    }

    if (!IsAlive() && mEntry->IsDungeon())
    {
        ResurrectPlayer(0.5f);
        SpawnCorpseBones();
    }

    if (!(options & TELE_TO_NOT_LEAVE_TRANSPORT) && m_transport)
    {

        if (TransportMap* hull = m_transport->AsMap())
        {
            hull->Disembark(this, m_transport->Where().X(), m_transport->Where().Y(),
                            m_transport->Where().Z(), m_transport->Where().Facing());
        }

        m_transport = nullptr;
        m_movementInfo.ClearTransportData();
    }

    if (m_duel.Stands() && GetMapId() != mapid)
    {
        if (GetMap()->GetGameObject(GetDuelArbiterGuid()))
        {
            Duelling().Complete(DUEL_FLED);
        }
    }

    m_movementInfo.SetMovementFlags(m_transport ? MOVEFLAG_ONTRANSPORT : MOVEFLAG_NONE);
    DisableSpline();

    if ((GetMapId() == mapid) && (!m_transport))
    {

        SetSemaphoreTeleportFar(false);

        if (!allowNoDelay)
        {
            if (SetDelayedTeleportFlagIfCan())
            {
                SetSemaphoreTeleportNear(true);

                m_teleport.Aim(Geometry::Placement::Somewhere(mapid, Geometry::Vector3(x, y, z), orientation), options);
                return true;
            }
        }

        if (!(options & TELE_TO_NOT_UNSUMMON_PET))
        {

            if (pet)
            {
                if (!pet->Where().WithinDist(Geometry::Vector3(x, y, z), GetMap()->GetVisibilityDistance()))
                {
                    if (pet->IsAlive())
                    {
                        UnsummonPetTemporaryIfAny();
                    }
                    else
                    {
                        pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
                        pet = GetPet();
                    }
                }
            }
        }

        if (!(options & TELE_TO_NOT_LEAVE_COMBAT))
        {
            CombatStop();
        }

        m_teleport.To() = Geometry::Placement::Somewhere(mapid, Geometry::Vector3(x, y, z), orientation);
        SetFallInformation(0, z);

        SetSemaphoreTeleportNear(true);

        if (!GetSession()->PlayerLogout())
        {
            WorldPacket data;
            BuildTeleportAckMsg(data, x, y, z, orientation);
            GetSession()->SendPacket(&data);
        }
    }
    else
    {

        Map* oldmap = IsInWorld() ? GetMap() : nullptr;

        DungeonPersistentState* state = Binds().CopyForHimOrHisGroup(mapid);
        Map* map = sMapRoster.Find(mapid, state ? state->GetInstanceId() : 0);
        if (!map || map->CanEnter(this))
        {

            SetSemaphoreTeleportNear(false);

            if (!allowNoDelay)
            {
                if (SetDelayedTeleportFlagIfCan())
                {
                    SetSemaphoreTeleportFar(true);

                    m_teleport.Aim(Geometry::Placement::Somewhere(mapid, Geometry::Vector3(x, y, z), orientation), options);
                    return true;
                }
            }

            SetSelectionGuid(0);

            CombatStop();

            ResetContestedPvP();

            if (BattleGround const* bg = Battle().Ground())
            {

                if (bg->GetMapId() != mapid)
                {
                    Battle().Leave(false);
                }
            }

            if (pet)
            {
                if (pet->IsAlive())
                {
                    UnsummonPetTemporaryIfAny();
                }
                else
                {
                    pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
                    pet = GetPet();
                }
            }

            Conjured().RemoveAllAreas();

            if (!(options & TELE_TO_SPELL))
            {
                if (IsNonMeleeSpellCasted(true))
                {
                    InterruptNonMeleeSpells(true);
                }
            }

            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CHANGE_MAP | AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);

            if (!GetSession()->PlayerLogout())
            {

                WorldPacket data(SMSG_TRANSFER_PENDING, (4 + 4 + 4));
                data << uint32(mapid);
                if (m_transport)
                {

                    data << uint32(m_transport->GetEntry());
                    data << uint32(m_transport->GetMapId());
                }
                GetSession()->SendPacket(&data);
            }

            if (oldmap)
            {
                oldmap->Remove(this, false);
            }

            float final_x = x;
            float final_y = y;
            float final_z = z;
            float final_o = orientation;

            Position const* transportPosition = m_movementInfo.GetTransportPos();

            if (m_transport)
            {
                final_x += transportPosition->x;
                final_y += transportPosition->y;
                final_z += transportPosition->z;
                final_o += transportPosition->o;
            }

            m_teleport.To() = Geometry::Placement::Somewhere(mapid, Geometry::Vector3(final_x, final_y, final_z), final_o);
            SetFallInformation(0, final_z);

            SetSemaphoreTeleportFar(true);

            if (!GetSession()->PlayerLogout())
            {

                WorldPacket data(SMSG_NEW_WORLD, (20));
                data << uint32(mapid);
                if (m_transport)
                {
                    data << float(transportPosition->x);
                    data << float(transportPosition->y);
                    data << float(transportPosition->z);
                    data << float(transportPosition->o);
                }
                else
                {
                    data << float(final_x);
                    data << float(final_y);
                    data << float(final_z);
                    data << float(final_o);
                }

                GetSession()->SendPacket(&data);
                Binds().TellSaved();
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

void Player::ProcessDelayedOperations()
{
    if (m_teleport.Owed() == 0)
    {
        return;
    }

    if (m_teleport.Owes(DELAYED_RESURRECT_PLAYER))
    {
        RaiseOnOffer();
    }

    if (m_teleport.Owes(DELAYED_SAVE_PLAYER))
    {
        SaveToDB();
    }

    if (m_teleport.Owes(DELAYED_SPELL_CAST_DESERTER))
    {
        CastSpell(this, 26013, true);
    }

    m_teleport.Settled();
}

void Player::AddToWorld()
{

    Unit::AddToWorld();

    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_inventory.Own(i))
        {
            m_inventory.Own(i)->AddToWorld();
        }
    }

    if (GetTransport() && GetGroup() && GetGroup()->isRaidGroup())
    {
        SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        GetGroup()->SendUpdateToPlayer(this);
    }
}

void Player::RemoveFromWorld()
{

    if (IsInWorld())
    {

        Retainers().UnsummonAllTotems();
        RemoveMiniPet();
    }

    if (GetTransport() && GetGroup() && GetGroup()->isRaidGroup())
    {
        WorldPacket data;

        data.Initialize(SMSG_GROUP_LIST, 24);
        data << uint64(0) << uint64(0) << uint64(0);
        m_session->SendPacket(&data);
    }

    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_inventory.Own(i))
        {
            m_inventory.Own(i)->RemoveFromWorld();
        }
    }

    Duelling().Complete(DUEL_INTERRUPTED);

    if (IsInWorld())
    {
        GetCamera().ResetView();
    }

    Unit::RemoveFromWorld();
}

Creature* Player::GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask)
{

    if (!guid || !IsInWorld() || IsTaxiFlying())
    {
        return nullptr;
    }

    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
    {
        return nullptr;
    }

    Creature* unit = GetMap()->GetAnyTypeCreature(guid);
    if (!unit)
    {
        return nullptr;
    }

    if (npcflagmask && !unit->HasNpcFlag(npcflagmask))
    {
        return nullptr;
    }

    if (npcflagmask == UNIT_NPC_FLAG_STABLEMASTER)
    {
        if (getClass() != CLASS_HUNTER)
        {
            return nullptr;
        }
    }

    if (!unit->IsAlive())
    {
        return nullptr;
    }

    if (IsAlive() && unit->IsInvisibleForAlive())
    {
        return nullptr;
    }

    if (unit->GetCharmerGuid())
    {
        return nullptr;
    }

    if (IsHostile(*unit, *this))
    {
        return nullptr;
    }

    if (!InReach(*unit, *this, INTERACTION_DISTANCE))
    {
        return nullptr;
    }

    return unit;
}

GameObject* Player::GetGameObjectIfCanInteractWith(ObjectGuid guid, uint32 gameobject_type) const
{

    if (!guid || !IsInWorld() || IsTaxiFlying())
    {
        return nullptr;
    }

    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
    {
        return nullptr;
    }

    if (GameObject* go = GetMap()->GetGameObject(guid))
    {
        if (uint32(go->GetGoType()) == gameobject_type || gameobject_type == MAX_GAMEOBJECT_TYPE)
        {
            float maxdist = go->GetInteractionDistance();
            if (InReach(*go, *this, maxdist) && go->isSpawned())
            {
                return go;
            }

            sLog.outError("GetGameObjectIfCanInteractWith: GameObject '%s' [GUID: %u] is too far away from player %s [GUID: %u] to be used by him (distance=%f, maximal %f is allowed)",
                go->GetGOInfo()->name,  go->GetGUIDLow(), GetName(), GetGUIDLow(), go->Where().DistanceTo(this->Where()), maxdist);
        }
    }
    return nullptr;
}

bool Player::IsUnderWater() const
{
    return GetMap()->GetTerrain()->IsUnderWater(Where().X(), Where().Y(), Where().Z() + 2);
}
struct SetGameMasterOnHelper
{
    explicit SetGameMasterOnHelper() {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(35);
        unit->GetHostileRefManager().setOnlineOfflineState(false);
    }
};

struct SetGameMasterOffHelper
{
    explicit SetGameMasterOffHelper(uint32 _faction) : faction(_faction) {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(faction);
        unit->GetHostileRefManager().setOnlineOfflineState(true);
    }
    uint32 faction;
};

void Player::SetGameMaster(bool on)
{
    if (on)
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_ON;

        SetUnitFlag(UNIT_FLAG_UNK_0);
        SetPlayerFlag(PLAYER_FLAGS_GM);
        CallForAllControlledUnits(SetGameMasterOnHelper(), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);

        SetFFAPvP(false);
        ResetContestedPvP();

        GetHostileRefManager().setOnlineOfflineState(false);
        CombatStopWithPets();

        if (Pet* pet = GetPet())
        {
            pet->setFaction(35);
            pet->GetHostileRefManager().setOnlineOfflineState(false);
        }
    }
    else
    {
        m_ExtraFlags &= ~ PLAYER_EXTRA_GM_ON;

        RemoveUnitFlag(UNIT_FLAG_UNK_0);
        RemovePlayerFlag(PLAYER_FLAGS_GM);

        if (Pet* pet = GetPet())
        {
            pet->setFaction(getFaction());
            pet->GetHostileRefManager().setOnlineOfflineState(true);
        }

        CallForAllControlledUnits(SetGameMasterOffHelper(getFaction()), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);

        if (sWorld.IsFFAPvPRealm())
        {
            SetFFAPvP(true);
        }

        UpdateArea(m_areaUpdateId);

        GetHostileRefManager().setOnlineOfflineState(true);
    }

    m_camera.UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    UpdateForQuestObjects();
}

void Player::SetGMVisible(bool on)
{
    if (on)
    {
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_INVISIBLE;

        if (HasAuraType(SPELL_AURA_MOD_STEALTH))
        {
            SetVisibility(VISIBILITY_GROUP_STEALTH);
        }
        else if (HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
        {
            SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
        }
        else
        {
            SetVisibility(VISIBILITY_ON);
        }
    }
    else
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_INVISIBLE;

        SetAcceptWhispers(false);
        SetGameMaster(true);

        SetVisibility(VISIBILITY_OFF);
    }
}

void Player::SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 RestXP)
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21);
    data << (victim ? victim->GetObjectGuid() : 0);
    data << uint32(GivenXP + RestXP);
    data << uint8(victim ? 0 : 1);
    if (victim)
    {
        data << uint32(GivenXP);
        data << float(1);
    }
    GetSession()->SendPacket(&data);
}

void Player::GiveXP(uint32 xp, Unit* victim)
{
    if (xp < 1)
    {
        return;
    }

    if (!IsAlive())
    {
        return;
    }

    uint32 level = getLevel();

    if (level >= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        return;
    }

    uint32 rested_bonus_xp = victim ? Resting().SpendOn(xp) : 0;

    SendLogXPGain(xp, victim, rested_bonus_xp);

    uint32 curXP = GetUInt32Value(PLAYER_XP);
    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = curXP + xp + rested_bonus_xp;

    while (newXP >= nextLvlXP && level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        newXP -= nextLvlXP;

        if (level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            GiveLevel(level + 1);
        }

        level = getLevel();
        nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    }

    SetUInt32Value(PLAYER_XP, newXP);
}

void Player::GiveLevel(uint32 level)
{
    uint8 oldLevel = getLevel();
    if (level == getLevel())
    {
        return;
    }

    PlayerLevelInfo info;
    sObjectMgr.GetPlayerLevelInfo(getRace(), getClass(), level, &info);

    PlayerClassLevelInfo classInfo;
    sObjectMgr.GetPlayerClassLevelInfo(getClass(), level, &classInfo);

    WorldPacket data(SMSG_LEVELUP_INFO, (4 + 4 + MAX_POWERS * 4 + MAX_STATS * 4));
    data << uint32(level);
    data << uint32((int32(classInfo.basehealth) - int32(GetCreateHealth())) +
        ((int32(info.stats[STAT_STAMINA]) - Tallied().Made(STAT_STAMINA)) * 10));

    data << uint32(int32(classInfo.basemana)   - int32(GetCreateMana()));
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        data << uint32(int32(info.stats[i]) - Tallied().Made(Stats(i)));
    }

    GetSession()->SendPacket(&data);

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr.GetXPForLevel(level));

    if (getLevel() != level)
    {
        m_played.NewLevel();
    }
    SetLevel(level);
    UpdateSkillsForLevel();

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        Tallied().Made(Stats(i), info.stats[i]);
    }

    SetCreateHealth(classInfo.basehealth);
    SetCreateMana(classInfo.basemana);

    InitTalentForLevel();

    Sheet().Everything();

    if (IsAlive())
    {
        SetHealth(GetMaxHealth());
    }
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
    {
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    }
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);

    if (Pet* pet = GetPet())
    {
        pet->SynchronizeLevelWithOwner();
    }

}

void Player::SetFreeTalentPoints(uint32 points)
{

    SetUInt32Value(PLAYER_CHARACTER_POINTS1, points);
}

void Player::UpdateFreeTalentPoints(bool resetIfNeed)
{
    uint32 level = getLevel();

    if (level < 10)
    {

        if (m_usedTalentCount > 0)
        {
            if (resetIfNeed)
            {
                resetTalents(true);
            }
            SetFreeTalentPoints(0);
        }
    }
    else
    {
        uint32 talentPointsForLevel = CalculateTalentsPoints();

        if (m_usedTalentCount > talentPointsForLevel)
        {
            if (resetIfNeed && GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            {
                resetTalents(true);
            }
            else
            {
                SetFreeTalentPoints(0);
            }
        }

        else
        {
            SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
        }
    }
}

void Player::InitTalentForLevel()
{
    UpdateFreeTalentPoints();
}

void Player::InitStatsForLevel(bool reapplyMods)
{
    if (reapplyMods)
    {
        _RemoveAllStatBonuses();
    }

    PlayerClassLevelInfo classInfo;
    sObjectMgr.GetPlayerClassLevelInfo(getClass(), getLevel(), &classInfo);

    PlayerLevelInfo info;
    sObjectMgr.GetPlayerLevelInfo(getRace(), getClass(), getLevel(), &info);

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr.GetXPForLevel(getLevel()));

    SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    UpdateSkillsForLevel();

    SetCastSpeedMod(1.0f);

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        Tallied().Made(Stats(i), info.stats[i]);
    }

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        SetStat(Stats(i), info.stats[i]);
    }

    SetCreateHealth(classInfo.basehealth);

    SetCreateMana(classInfo.basemana);

    SetArmor(int32(Tallied().Made(STAT_AGILITY) * 2));

    InitStatBuffMods();

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, 0);
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, 0);
        SetDamageDonePercent(i, 1.00f);
    }

    SetFloatValue(UNIT_FIELD_BASEATTACKTIME, 2000.0f);
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME + 1, 2000.0f);
    SetFloatValue(UNIT_FIELD_RANGEDATTACKTIME, 2000.0f);

    SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

    SetAttackPower(false, 0, 0, 0.0f);
    SetAttackPower(true, 0, 0, 0.0f);

    SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

    SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);

    SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);

    SetArmor(int32(Tallied().Made(STAT_AGILITY) * 2));
    SetResistanceBuffMods(SpellSchools(0), true, 0.0f);
    SetResistanceBuffMods(SpellSchools(0), false, 0.0f);

    for (int i = 1; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetResistance(SpellSchools(i), 0);
        SetResistanceBuffMods(SpellSchools(i), true, 0.0f);
        SetResistanceBuffMods(SpellSchools(i), false, 0.0f);
    }

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, 0);
        SetPowerCostMultiplier(i, 0.0f);
    }

    InitDataForForm(reapplyMods);

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
    {
        SetMaxPower(Powers(i),  GetCreatePowers(Powers(i)));
    }

    SetMaxHealth(classInfo.basehealth);

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);

    RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_NOT_ATTACKABLE_1 |
        UNIT_FLAG_OOC_NOT_ATTACKABLE | UNIT_FLAG_PASSIVE  | UNIT_FLAG_LOOTING          |
        UNIT_FLAG_PET_IN_COMBAT  | UNIT_FLAG_SILENCED     | UNIT_FLAG_PACIFIED         |
        UNIT_FLAG_STUNNED        | UNIT_FLAG_IN_COMBAT    | UNIT_FLAG_DISARMED         |
        UNIT_FLAG_CONFUSED       | UNIT_FLAG_FLEEING      | UNIT_FLAG_NOT_SELECTABLE   |
        UNIT_FLAG_SKINNABLE      | UNIT_FLAG_TAXI_FLIGHT);
    SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);

    RemovePlayerFlag(PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND | PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST | PLAYER_FLAGS_FFA_PVP);

    SetBearing(0);

    SetUInt32Value(PLAYER_FIELD_BYTES2, 0);

    if (reapplyMods)
    {
        _ApplyAllStatBonuses();
    }

    SetHealth(GetMaxHealth());
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
    {
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    }
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);

    if (Pet* pet = GetPet())
    {
        pet->SynchronizeLevelWithOwner();
    }
}

struct spell_data
{
    uint16 spell_id;
    uint16 on_cooldown;
};

struct spell_cooldown_data
{
    uint16 spell_id;
    uint16 item_id;
    uint16 spell_category;
    uint32 spell_cd_ms;
    uint32 cat_cd_ms;
};

void Player::SendInitialSpells()
{
    time_t curTime = time(nullptr);
    time_t infTime = curTime + infinityCooldownDelayCheck;

    uint16 spellCount = 0;

    WorldPacket data(SMSG_INITIAL_SPELLS, (1 + 2 + 4 * m_spells.size() + 2 + GetSpellCooldownMap().size() * (2 + 2 + 2 + 4 + 4)));
    data << uint8(0);

    size_t countPos = data.wpos();
    data << uint16(spellCount);

    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {

        PlayerSpell const& playerSpell = itr->second;

        if (playerSpell.state == PLAYERSPELL_REMOVED)
        {
            continue;
        }

        if (!playerSpell.active || playerSpell.disabled)
        {
            continue;
        }

        data << uint16(itr->first);
        data << uint16(0);

        spellCount += 1;
    }

    data.put<uint16>(countPos, spellCount);

    uint16 spellCooldowns = GetSpellCooldownMap().size();
    data << uint16(spellCooldowns);
    for (SpellCooldowns::const_iterator itr = GetSpellCooldownMap().begin(); itr != GetSpellCooldownMap().end(); ++itr)
    {

        SpellEntry const* sEntry = sSpellStore.LookupEntry(itr->first);
        if (!sEntry)
        {
            continue;
        }

        SpellCooldown const& spellCooldown = itr->second;

        data << uint16(itr->first);

        data << uint16(spellCooldown.itemid);
        data << uint16(sEntry->Category);

        if (spellCooldown.end >= infTime)
        {
            data << uint32(1);
            data << uint32(0x80000000);
            continue;
        }

        time_t cooldown = spellCooldown.end > curTime ? (spellCooldown.end - curTime) * IN_MILLISECONDS : 0;

        if (sEntry->Category)
        {
            data << uint32(0);
            data << uint32(cooldown);
        }
        else
        {
            data << uint32(cooldown);
            data << uint32(0);
        }
    }

    GetSession()->SendPacket(&data);

    DETAIL_LOG("CHARACTER: Sent Initial Spells");
}

void Player::UpdateDefense()
{
    uint32 defense_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_DEFENSE);

    if (UpdateSkill(SKILL_DEFENSE, defense_skill_gain))
    {

        Sheet().Defences();
    }
}

bool Player::SetPosition(float x, float y, float z, float orientation, bool teleport)
{

    if (!MaNGOS::IsValidMapCoord(x, y, z, orientation))
    {
        DEBUG_LOG("Player::SetPosition(%f, %f, %f, %f, %d) .. bad coordinates for player %d!", x, y, z, orientation, teleport, GetGUIDLow());
        return false;
    }

    Map* m = GetMap();

    const float old_x = Where().X();
    const float old_y = Where().Y();
    const float old_z = Where().Z();
    const float old_r = Where().Facing();

    if (teleport || old_x != x || old_y != y || old_z != z || old_r != orientation)
    {
        if (teleport || old_x != x || old_y != y || old_z != z)
        {
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);
        }
        else
        {
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TURNING);
        }

        RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);

        m->PlayerRelocation(this, x, y, z, orientation);

        m_movementInfo.ChangePosition(x, y, z, orientation);

        m = GetMap();
        x = Where().X();
        y = Where().Y();
        z = Where().Z();

        if (GetGroup() && (old_x != x || old_y != y))
        {
            SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POSITION);
        }
        if (GetTrader() && !InReach(*this, *(GetTrader()), INTERACTION_DISTANCE))
        {
            GetSession()->SendCancelTrade();
        }
    }

    if (m_positionStatusUpdateTimer)
    {
        return true;
    }
    m_positionStatusUpdateTimer = 100;

    m_perils.Look(m, x, y, z);

    CheckAreaExploreAndOutdoor();

    return true;
}

void Player::SaveRecallPosition()
{
    m_recall = Geometry::Placement::Somewhere(GetMapId(), Where().Pos(), Where().Facing());
}

void Player::SendDirectMessage(WorldPacket* data) const
{
    GetSession()->SendPacket(data);
}

void Player::SendCinematicStart(uint32 CinematicSequenceId)
{
    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
    data << uint32(CinematicSequenceId);
    SendDirectMessage(&data);
}

Team Player::TeamForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        sLog.outError("Race %u not found in DBC: wrong DBC files?", uint32(race));
        return ALLIANCE;
    }

    switch (rEntry->BaseLanguage)
    {
        case 7: return ALLIANCE;
        case 1: return HORDE;
    }

    sLog.outError("Race %u have wrong teamid %u in DBC: wrong DBC files?", uint32(race), rEntry->BaseLanguage);
    return TEAM_NONE;
}

uint32 Player::getFactionForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        sLog.outError("Race %u not found in DBC: wrong DBC files?", uint32(race));
        return 0;
    }

    return rEntry->FactionID;
}

void Player::setFactionForRace(uint8 race)
{
    m_team = TeamForRace(race);
    setFaction(getFactionForRace(race));
}

bool Player::RewardHonor(Unit* uVictim, uint32 groupsize)
{
    float honor_points = 0;

    DETAIL_LOG("PLAYER: RewardHonor");

    if (!uVictim)
    {
        return false;
    }

    if (uVictim->GetAura(2479, EFFECT_INDEX_0))
    {
        return false;
    }

    if (IsCreature(uVictim))
    {
        Creature* cVictim = (Creature*)uVictim;
        if (cVictim->IsCivilian())
        {
            AddHonorCP(MaNGOS::Honor::DishonorableKillPoints(getLevel()), DISHONORABLE, cVictim->GetEntry(), TYPEID_UNIT);
            return true;
        }

        if (cVictim->IsRacialLeader())
        {

            AddHonorCP(398.0, HONORABLE, cVictim->GetEntry(), TYPEID_UNIT);

            SendPvPCredit(cVictim->GetObjectGuid(), 19, 398);
            return true;
        }
    }
    else if (IsPlayer(uVictim))
    {
        Player* pVictim = (Player*)uVictim;

        if (GetTeam() == pVictim->GetTeam())
        {
            return false;
        }

        if (getLevel() < (pVictim->getLevel() + 5))
        {
            float hp = MaNGOS::Honor::HonorableKillPoints(this, pVictim, groupsize);
            AddHonorCP(hp, HONORABLE, pVictim->GetGUIDLow(), TYPEID_PLAYER);

            SendPvPCredit(pVictim->GetObjectGuid(), uint32(pVictim->GetHonorRankInfo().rank), uint32(hp));
            return true;
        }
    }

    return false;
}

void Player::SendPvPCredit(ObjectGuid guid, uint32 rank, uint32 points)
{
    WorldPacket data(SMSG_PVP_CREDIT, 4 + 8 + 4);
    data << points;
    data << guid;
    data << rank;
    GetSession()->SendPacket(&data);
}

bool Player::CanUseCapturePoint()
{
    return IsAlive() &&
        !HasStealthAura() &&
        !HasInvisibilityAura() &&
        (IsPvP() || sWorld.IsPvPRealm()) &&
        !HasMovementFlag(MOVEFLAG_FLYING) &&
        !IsTaxiFlying() &&
        !isGameMaster();
}

void Player::SendUpdateWorldState(uint32 Field, uint32 Value)
{
    WorldPacket data(SMSG_UPDATE_WORLD_STATE, 8);
    data << Field;
    data << Value;
    GetSession()->SendPacket(&data);
}

void Player::SendInitWorldStates(uint32 zoneid)
{
    SendInitWorldStates(GetMapId(), zoneid);
}

void Player::SendInitWorldStates(uint32 mapid, uint32 zoneid)
{

    BattleGround* bg = Battle().Ground();

    DEBUG_LOG("Sending SMSG_INIT_WORLD_STATES to Map:%u, Zone: %u", mapid, zoneid);

    uint32 count = 0;

    WorldPacket data(SMSG_INIT_WORLD_STATES, (4 + 4 + 2 + 6));
    data << uint32(mapid);
    data << uint32(zoneid);
    size_t count_pos = data.wpos();
    data << uint16(0);

    switch (zoneid)
    {
        case 139:
        case 1377:
            if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(zoneid))
            {
                outdoorPvP->FillInitialWorldStates(data, count);
            }
            break;
        case 2597:
            if (bg && bg->GetTypeID() == BATTLEGROUND_AV)
            {
                bg->FillInitialWorldStates(data, count);
            }
            break;
        case 3277:
            if (bg && bg->GetTypeID() == BATTLEGROUND_WS)
            {
                bg->FillInitialWorldStates(data, count);
            }
            break;
        case 3358:
            if (bg && bg->GetTypeID() == BATTLEGROUND_AB)
            {
                bg->FillInitialWorldStates(data, count);
            }
            break;
    }

    data.put<uint16>(count_pos, count);

    GetSession()->SendPacket(&data);
}

void Player::SetBindPoint(ObjectGuid guid)
{
    WorldPacket data(SMSG_BINDER_CONFIRM, 8);
    data << static_cast<ObjectGuid>(guid);
    GetSession()->SendPacket(&data);
}

void Player::SendTalentWipeConfirm(ObjectGuid guid)
{
    WorldPacket data(MSG_TALENT_WIPE_CONFIRM, (8 + 4));
    data << static_cast<ObjectGuid>(guid);
    data << uint32(resetTalentsCost());
    GetSession()->SendPacket(&data);
}

void Player::SendPetSkillWipeConfirm()
{
    Pet* pet = GetPet();
    if (!pet)
    {
        return;
    }
    WorldPacket data(SMSG_PET_UNLEARN_CONFIRM, (8 + 4));
    data << static_cast<ObjectGuid>(pet->GetObjectGuid());
    data << uint32(pet->resetTalentsCost());
    GetSession()->SendPacket(&data);
}

bool Player::ViableEquipSlots(ItemPrototype const* proto, uint8 *viable_slots) const
{
    uint8 pClass;

    if (!viable_slots)
    {

        return false;
    }

    viable_slots[0] = NULL_SLOT;
    viable_slots[1] = NULL_SLOT;
    viable_slots[2] = NULL_SLOT;
    viable_slots[3] = NULL_SLOT;

    if (CanUseItem(proto) == EQUIP_ERR_OK)
    {

        switch (proto->InventoryType)
        {
            case INVTYPE_HEAD:

                viable_slots[0] = EQUIPMENT_SLOT_HEAD;
                break;
            case INVTYPE_NECK:

                viable_slots[0] = EQUIPMENT_SLOT_NECK;
                break;
            case INVTYPE_SHOULDERS:

                viable_slots[0] = EQUIPMENT_SLOT_SHOULDERS;
                break;
            case INVTYPE_BODY:

                viable_slots[0] = EQUIPMENT_SLOT_BODY;
                break;
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:

                viable_slots[0] = EQUIPMENT_SLOT_CHEST;
                break;
            case INVTYPE_WAIST:

                viable_slots[0] = EQUIPMENT_SLOT_WAIST;
                break;
            case INVTYPE_LEGS:

                viable_slots[0] = EQUIPMENT_SLOT_LEGS;
                break;
            case INVTYPE_FEET:

                viable_slots[0] = EQUIPMENT_SLOT_FEET;
                break;
            case INVTYPE_WRISTS:

                viable_slots[0] = EQUIPMENT_SLOT_WRISTS;
                break;
            case INVTYPE_HANDS:

                viable_slots[0] = EQUIPMENT_SLOT_HANDS;
                break;
            case INVTYPE_FINGER:

                viable_slots[0] = EQUIPMENT_SLOT_FINGER1;
                viable_slots[1] = EQUIPMENT_SLOT_FINGER2;
                break;
            case INVTYPE_TRINKET:

                viable_slots[0] = EQUIPMENT_SLOT_TRINKET1;
                viable_slots[1] = EQUIPMENT_SLOT_TRINKET2;
                break;
            case INVTYPE_CLOAK:

                viable_slots[0] = EQUIPMENT_SLOT_BACK;
                break;
            case INVTYPE_WEAPON:

                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;

                if (Arms().CanDualWield())
                {

                    viable_slots[1] = EQUIPMENT_SLOT_OFFHAND;
                }
                break;
            case INVTYPE_SHIELD:

                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_RANGED:

                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_2HWEAPON:

                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;

                if (Arms().CanDualWield())
                {

                    viable_slots[1] = EQUIPMENT_SLOT_OFFHAND;
                }
                break;
            case INVTYPE_TABARD:

                viable_slots[0] = EQUIPMENT_SLOT_TABARD;
                break;
            case INVTYPE_WEAPONMAINHAND:

                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;
                break;
            case INVTYPE_WEAPONOFFHAND:

                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_HOLDABLE:

                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_THROWN:

                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_RANGEDRIGHT:

                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_BAG:

                viable_slots[0] = INVENTORY_SLOT_BAG_START + 0;
                viable_slots[1] = INVENTORY_SLOT_BAG_START + 1;
                viable_slots[2] = INVENTORY_SLOT_BAG_START + 2;
                viable_slots[3] = INVENTORY_SLOT_BAG_START + 3;
                break;
            case INVTYPE_RELIC:

                pClass = getClass();

                if (pClass)
                {

                    switch (proto->SubClass)
                    {
                        case ITEM_SUBCLASS_ARMOR_LIBRAM:

                            if (pClass == CLASS_PALADIN)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_IDOL:

                            if (pClass == CLASS_DRUID)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_TOTEM:

                            if (pClass == CLASS_SHAMAN)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_MISC:

                            if (pClass == CLASS_WARLOCK)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        default:

                            break;
                    }
                }
                break;
            default:

                break;
        }
    }
    return (viable_slots[0] != NULL_SLOT);
}

bool Player::HasItemWithIdEquipped(uint32 item, uint32 count, uint8 except_slot) const
{
    uint32 tempcount = 0;
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (i == int(except_slot))
        {
            continue;
        }

        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
            {
                return true;
            }
        }
    }

    return false;
}

InventoryResult Player::CanUseItemEluna(uint32 itemEntry) const
{
    (void)itemEntry;
    return EQUIP_ERR_OK;
}

void Player::SendOpenContainer()
{
    DEBUG_LOG("WORLD: Sent SMSG_OPEN_CONTAINER");
    WorldPacket data(SMSG_OPEN_CONTAINER, 8);
    data << GetObjectGuid();
    GetSession()->SendPacket(&data);
}

Quest const* Player::GetQuestTemplate(uint32 quest_id)
{
    return sObjectMgr.GetQuestTemplate(quest_id);
}

void Player::SetQuestRewarded(uint32 quest_id, bool rewarded)
{
    if (sObjectMgr.GetQuestTemplate(quest_id))
    {
        QuestStatusData& q_status = m_journal.Of(quest_id);

        q_status.m_rewarded = rewarded;
    }

    UpdateForQuestObjects();
}

void Player::SendQuestFailedAtTaker(uint32 quest_id, uint32 reason)
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTGIVER_QUEST_FAILED, 8);
        data << uint32(quest_id);
        data << uint32(reason);
        GetSession()->SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_FAILED");
    }
}

bool Player::IsTappedByMeOrMyGroup(Creature* creature)
{

    if (!creature->HasDynFlag(UNIT_DYNFLAG_TAPPED))
    {
        return false;
    }

    if (Player* recipient = creature->Claim().Entitled())
    {

        if (Group* plr_group = recipient->GetGroup())
        {

            if (Group* my_group = GetGroup())
            {

                if (plr_group != my_group)
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            return true;
        }

        else if (recipient == this)
        {
            return true;
        }
    }
    else

    {
        return false;
    }

    return false;
}

void Player::SaveMail()
{
    static SqlStatementID updateMail ;
    static SqlStatementID deleteMailItems ;

    static SqlStatementID deleteItem ;
    static SqlStatementID deleteMail ;
    static SqlStatementID deleteItems ;

    for (PlayerMails::iterator itr = Post().Letters().begin(); itr != Post().Letters().end(); ++itr)
    {
        Mail* m = (*itr);
        if (m->state == MAIL_STATE_CHANGED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updateMail, "UPDATE `mail` SET `body` = ?,`has_items` = ?, `expire_time` = ?, `deliver_time` = ?, `money` = ?, `cod` = ?, `checked` = ? WHERE `id` = ?");
            stmt.addString(m->body.c_str());
            stmt.addUInt32(m->HasItems() ? 1 : 0);
            stmt.addUInt64(uint64(m->expire_time));
            stmt.addUInt64(uint64(m->deliver_time));
            stmt.addUInt32(m->money);
            stmt.addUInt32(m->COD);
            stmt.addUInt32(m->checked);
            stmt.addUInt32(m->messageID);
            stmt.Execute();

            if (!m->removedItems.empty())
            {
                stmt = CharacterDatabase.CreateStatement(deleteMailItems, "DELETE FROM `mail_items` WHERE `item_guid` = ?");

                for (std::vector<uint32>::const_iterator itr2 = m->removedItems.begin(); itr2 != m->removedItems.end(); ++itr2)
                {
                    stmt.PExecute(*itr2);
                }

                m->removedItems.clear();
            }
            m->state = MAIL_STATE_UNCHANGED;
        }
        else if (m->state == MAIL_STATE_DELETED)
        {
            if (m->HasItems())
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteItem, "DELETE FROM `item_instance` WHERE `guid` = ?");
                for (MailItemInfoVec::const_iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    stmt.PExecute(itr2->item_guid);
                }
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(deleteMail, "DELETE FROM `mail` WHERE `id` = ?");
            stmt.PExecute(m->messageID);

            stmt = CharacterDatabase.CreateStatement(deleteItems, "DELETE FROM `mail_items` WHERE `mail_id` = ?");
            stmt.PExecute(m->messageID);
        }
    }

    for (PlayerMails::iterator itr = Post().Letters().begin(); itr != Post().Letters().end();)
    {
        if ((*itr)->state == MAIL_STATE_DELETED)
        {
            Mail* m = *itr;
            Post().Letters().erase(itr);
            delete m;
            itr = Post().Letters().begin();
        }
        else
        {
            ++itr;
        }
    }

}

void Player::SendAttackSwingNotStanding()
{
    WorldPacket data(SMSG_ATTACKSWING_NOTSTANDING, 0);
    GetSession()->SendPacket(&data);
}

void Player::SetFFAPvP(bool state)
{
    if (state)
    {
        SetPlayerFlag(PLAYER_FLAGS_FFA_PVP);
    }
    else
    {
        RemovePlayerFlag(PLAYER_FLAGS_FFA_PVP);
    }
}

void Player::RemoveMiniPet()
{
    if (Pet* pet = GetMiniPet())
    {
        pet->Unsummon(PET_SAVE_AS_DELETED);
    }
}

Pet* Player::GetMiniPet() const
{
    if ((m_miniPetGuid == 0))
    {
        return nullptr;
    }

    return GetMap()->GetPet(m_miniPetGuid);
}

void Player::Mount(uint32 mount, uint32 spellId)
{
    if (!mount)
    {
        return;
    }

    Unit::Mount(mount, spellId);

    if (!spellId)
    {
        UnsummonPetTemporaryIfAny();
    }

    else
    {

        if (Pet* pet = GetPet())
        {
            if (pet->IsPermanentPetFor((Player*)this) &&
                sWorld.getConfig(CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT))
            {
                UnsummonPetTemporaryIfAny();
            }
            else
            {
                pet->ApplyModeFlags(PET_MODE_DISABLE_ACTIONS, true);
            }
        }
    }
}

void Player::Unmount(bool from_aura)
{
    if (!IsMounted())
    {
        return;
    }

    Unit::Unmount(from_aura);

    if (Pet* pet = GetPet())
    {
        pet->ApplyModeFlags(PET_MODE_DISABLE_ACTIONS, false);
    }
    else
    {
        ResummonPetTemporaryUnSummonedIfAny();
    }
}

void Player::SendMountResult(PlayerMountResult result)
{
    WorldPacket data(SMSG_MOUNTRESULT, 4);
    data << uint32(result);
    GetSession()->SendPacket(&data);
}

void Player::SendDismountResult(PlayerDismountResult result)
{
    WorldPacket data(SMSG_DISMOUNTRESULT, 4);
    data << uint32(result);
    GetSession()->SendPacket(&data);
}

void Player::ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs)
{

    WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + m_spells.size() * 8);
    data << GetObjectGuid();
    time_t curTime = time(nullptr);
    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED)
        {
            continue;
        }
        uint32 unSpellId = itr->first;
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(unSpellId);
        MANGOS_ASSERT(spellInfo);

        if (cast::RecipeOf(*spellInfo).Says().spentWhileActive)
        {
            continue;
        }

        if ((idSchoolMask & GetSpellSchoolMask(spellInfo)) && GetSpellCooldownDelay(unSpellId) < unTimeMs)
        {
            data << uint32(unSpellId);
            data << uint32(unTimeMs);
            AddSpellCooldown(unSpellId, 0, curTime + unTimeMs / IN_MILLISECONDS);
        }
    }
    GetSession()->SendPacket(&data);
}

void Player::InitDataForForm(bool reapplyMods)
{
    ShapeshiftForm form = GetShapeshiftForm();

    switch (form)
    {
        case FORM_CAT:
        {
            SetAttackTime(BASE_ATTACK, 1000);
            SetAttackTime(OFF_ATTACK, 1000);

            if (GetPowerType() != POWER_ENERGY)
            {
                SetPowerType(POWER_ENERGY);
            }
            break;
        }
        case FORM_BEAR:
        case FORM_DIREBEAR:
        {
            SetAttackTime(BASE_ATTACK, 2500);
            SetAttackTime(OFF_ATTACK, 2500);

            if (GetPowerType() != POWER_RAGE)
            {
                SetPowerType(POWER_RAGE);
            }
            break;
        }
        default:
        {
            SetRegularAttackTime();

            ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(getClass());
            if (cEntry && cEntry->DisplayPower < MAX_POWERS && uint32(GetPowerType()) != cEntry->DisplayPower)
            {
                SetPowerType(Powers(cEntry->DisplayPower));
            }

            break;
        }
    }

    if (!reapplyMods)
    {
        UpdateEquipSpellsAtFormChange();
    }

    Sheet().AttackPower(false);
    Sheet().AttackPower(true);
}

void Player::InitDisplayIds()
{
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair. Can't init display ids.", GetGUIDLow());
        return;
    }

    uint8 gender = getGender();
    switch (gender)
    {
        case GENDER_FEMALE:

            if (getRace() == RACE_TAUREN)
            {
                SetObjectScale(DEFAULT_TAUREN_FEMALE_SCALE);
            }
            else
            {
                SetObjectScale(DEFAULT_OBJECT_SCALE);
            }

            SetDisplayId(info->displayId_f);
            SetNativeDisplayId(info->displayId_f);
            break;
        case GENDER_MALE:

            if (getRace() == RACE_TAUREN)
            {
                SetObjectScale(DEFAULT_TAUREN_MALE_SCALE);
            }
            else
            {
                SetObjectScale(DEFAULT_OBJECT_SCALE);
            }

            SetDisplayId(info->displayId_m);
            SetNativeDisplayId(info->displayId_m);
            break;
        default:
            sLog.outError("Invalid gender %u for player", gender);
            return;
    }
}

bool Player::BuyItemFromVendor(ObjectGuid vendorGuid, uint32 item, uint8 count, uint8 bag, uint8 slot)
{

    if (count < 1)
    {
        count = 1;
    }

    if (bag != NULL_BAG && bag != INVENTORY_SLOT_BAG_0 && slot > MAX_BAG_SIZE && slot != NULL_SLOT)
    {
        return false;
    }

    if (!IsAlive())
    {
        return false;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (!pProto)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, item, 0);
        return false;
    }

    Creature* pCreature = GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: BuyItemFromVendor - %s not found or you can't interact with him.", GuidString(vendorGuid).c_str());
        SendBuyError(BUY_ERR_DISTANCE_TOO_FAR, nullptr, item, 0);
        return false;
    }

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();
    if ((!vItems || vItems->Empty()) && (!tItems || tItems->Empty()))
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 vCount = vItems ? vItems->GetItemCount() : 0;
    uint32 tCount = tItems ? tItems->GetItemCount() : 0;

    size_t vendorslot = vItems ? vItems->FindItemSlot(item) : vCount;
    if (vendorslot >= vCount)
    {
        vendorslot = vCount + (tItems ? tItems->FindItemSlot(item) : tCount);
    }

    if (vendorslot >= vCount + tCount)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    VendorItem const* crItem = vendorslot < vCount ? vItems->GetItem(vendorslot) : tItems->GetItem(vendorslot - vCount);
    if (!crItem || crItem->item != item)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 totalCount = pProto->BuyCount * count;

    if (crItem->maxcount != 0)
    {
        if (pCreature->GetVendorItemCurrentCount(crItem) < totalCount)
        {
            SendBuyError(BUY_ERR_ITEM_ALREADY_SOLD, pCreature, item, 0);
            return false;
        }
    }

    uint32 reqFaction = pProto->RequiredReputationFaction;
    if (!reqFaction && pProto->RequiredReputationRank > 0)
    {
        reqFaction = pCreature->getFactionTemplateEntry()->Faction;
    }

    if (uint32(GetReputationRank(reqFaction)) < pProto->RequiredReputationRank)
    {
        SendBuyError(BUY_ERR_REPUTATION_REQUIRE, pCreature, item, 0);
        return false;
    }

    if (pProto->RequiredHonorRank && (GetHonorHighestRankInfo().rank < (uint8)pProto->RequiredHonorRank || getLevel() < pProto->RequiredLevel))
    {
        SendBuyError(BUY_ERR_RANK_REQUIRE, pCreature, item, 0);
        return false;
    }

    if (crItem->conditionId && !isGameMaster() && !sObjectMgr.IsPlayerMeetToCondition(crItem->conditionId, this, pCreature->GetMap(), pCreature, CONDITION_FROM_VENDOR))
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 price = pProto->BuyPrice * count;

    price = uint32(floor(price * GetReputationPriceDiscount(pCreature)));

    if (GetMoney() < price)
    {
        SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, item, 0);
        return false;
    }

    Item* pItem = nullptr;

    if ((bag == NULL_BAG && slot == NULL_SLOT) || Inventory::IsCarried(bag, slot))
    {
        ItemPosCountVec dest;
        InventoryResult msg = CanStoreNewItem(bag, slot, dest, item, totalCount);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, item);
            return false;
        }

        ModifyMoney(-int32(price));

        pItem = StoreNewItem(dest, item, true);
    }
    else if (Inventory::IsWorn(bag, slot))
    {
        if (totalCount != 1)
        {
            SendEquipError(EQUIP_ERR_ITEM_CANT_BE_EQUIPPED, nullptr, nullptr);
            return false;
        }

        uint16 dest;
        InventoryResult msg = CanEquipNewItem(slot, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, item);
            return false;
        }

        ModifyMoney(-int32(price));

        pItem = EquipNewItem(dest, item, true);

        if (pItem)
        {
            AutoUnequipOffhandIfNeed();
        }
    }
    else
    {
        SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, nullptr, nullptr);
        return false;
    }

    if (!pItem)
    {
        return false;
    }

    uint32 new_count = pCreature->UpdateVendorItemCurrentCount(crItem, totalCount);

    WorldPacket data(SMSG_BUY_ITEM, 8 + 4 + 4 + 4);
    data << pCreature->GetObjectGuid();
    data << uint32(vendorslot + 1);
    data << uint32(crItem->maxcount > 0 ? new_count : 0xFFFFFFFF);
    data << uint32(count);
    GetSession()->SendPacket(&data);

    SendNewItem(pItem, totalCount, true, false, false);

    return crItem->maxcount != 0;
}

void Player::InitPrimaryProfessions()
{
    uint32 maxProfs = GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT))
        ? sWorld.getConfig(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL) : 10;
    SetFreePrimaryProfessions(maxProfs);
}

void Player::SetComboPoints()
{
    Unit* combotarget = ObjectLookup::GetUnit(*this, m_comboTargetGuid);
    if (combotarget)
    {
        SetGuidValue(PLAYER_FIELD_COMBO_TARGET, combotarget->GetObjectGuid());
        SetShownComboPoints(m_comboPoints);
    }

}

void Player::SendInitialPacketsBeforeAddToMap(bool deferLoginTimeSpeed)
{

    WorldPacket data(SMSG_SET_REST_START, 4);
    data << uint32(0);
    GetSession()->SendPacket(&data);

    data.Initialize(SMSG_BINDPOINTUPDATE, 5 * 4);
    data << Home().X() << Home().Y() << Home().Z();
    data << (uint32) Home().MapId();
    data << (uint32) Home().AreaId();
    GetSession()->SendPacket(&data);

    GetSession()->SendTutorialsData();
    SendInitialSpells();
    SendInitialActionButtons();

    m_reputationMgr.SendInitialReputations();

    UpdateHonor();

    if (!deferLoginTimeSpeed)
    {
        SendLoginTimeSpeed();
    }

    if (IsTaxiFlying())
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_FLYING);
    }

    SetMover(this);
}

void Player::SendLoginTimeSpeed()
{
    WorldPacket data(SMSG_LOGIN_SETTIMESPEED, 8);
    data << uint32(secsToTimeBitFields(sWorld.GetGameTime()));
    data << float(0.01666667f);
    GetSession()->SendPacket(&data);
}

void Player::ScheduleLoginEffect()
{
    m_Events.AddEvent(new LoginEffectEvent(*this),
        m_Events.CalculateTime(
            LoginEffectDelayBefore(LoginEffectPhase::Start)));
}

void Player::BeginLoginCinematicRoot()
{
    if (!m_loginCinematicRootOwnership.Claim())
    {
        return;
    }

    m_Events.AddEvent(new LoginCinematicRootTimeoutEvent(*this),
        m_Events.CalculateTime(LOGIN_CINEMATIC_ROOT_TIMEOUT_MS));
    SetRoot(true);
}

void Player::ReleaseLoginCinematicRoot()
{

    if (!m_loginCinematicRootOwnership.ReleaseOnce(IsInWorld()))
    {
        return;
    }

    if (!HasAuraType(SPELL_AURA_MOD_STUN) &&
        !HasAuraType(SPELL_AURA_MOD_ROOT))
    {

        SetRoot(false);
    }
}

void Player::GetWorldAnchor(uint32& mapId, float& x, float& y, float& z) const
{
    if (Transport* vessel = Transport::VesselOf(*this))
    {
        if (Map* sailing = vessel->GetMap())
        {
            mapId = sailing->GetId();
            x = vessel->Where().X();
            y = vessel->Where().Y();
            z = vessel->Where().Z();
            return;
        }
    }

    mapId = GetMapId();
    x = Where().X();
    y = Where().Y();
    z = Where().Z();
}

Map* Player::BoardingMap() const
{
    TransportMap* hull = m_transport ? m_transport->AsMap() : nullptr;

    return (hull && hull->IsCommissioned()) ? hull : GetMap();
}

TerrainInfo const* Player::AnchorTerrain() const
{
    if (Transport* vessel = Transport::VesselOf(*this))
    {
        if (Map* sailing = vessel->GetMap())
        {
            return sailing->GetTerrain();
        }
    }

    return GetTerrain();
}

void Player::GetZoneAndAreaAboardOrHere(uint32& zone, uint32& area) const
{
    if (Transport* vessel = Transport::VesselOf(*this))
    {
        if (Map* sailing = vessel->GetMap())
        {
            sailing->GetTerrain()->GetZoneAndAreaId(zone, area, vessel->Where().X(),
                                                    vessel->Where().Y(), vessel->Where().Z());
            return;
        }
    }

    GetTerrain()->GetZoneAndAreaId(zone, area, Where().X(), Where().Y(), Where().Z());
}

void Player::UpdateLiftMinions()
{

    GameObject* lift = nullptr;
    if (m_movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        lift = GetMap()->GetGameObject(m_movementInfo.GetTransportGuid());
        if (lift && !lift->IsLift())
        {
            lift = nullptr;
        }
    }

    const Position masterLocal = *m_movementInfo.GetTransportPos();
    const float liftO = lift ? lift->Where().Facing() : 0.0f;
    Player* master = this;

    CallForAllControlledUnits(
        [lift, masterLocal, liftO, master](Unit* minion)
        {
            if (!minion || !minion->IsAlive())
            {
                return;
            }

            const bool riding = minion->m_movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT);

            if (lift)
            {

                const float ox = 0.0f;
                const float oy = 1.5f;
                const float c = std::cos(liftO);
                const float s = std::sin(liftO);
                const float wx = master->Where().X() + (ox * c - oy * s);
                const float wy = master->Where().Y() + (ox * s + oy * c);
                const float wz = master->Where().Z();

                minion->m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
                minion->m_movementInfo.SetTransportData(lift->GetObjectGuid(),
                                                        masterLocal.x + ox, masterLocal.y + oy,
                                                        masterLocal.z, masterLocal.o, 0);
                minion->m_movementInfo.ChangePosition(wx, wy, wz, master->Where().Facing());

                minion->GetMotionMaster()->MoveIdle();
                minion->GetMap()->CreatureRelocation((Creature*)minion, wx, wy, wz, master->Where().Facing());

                minion->SendHeartBeat();
            }
            else if (riding)
            {

                minion->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);
                minion->m_movementInfo.ClearTransportData();

                float x, y, z;
                ClosePointNear(*master, x, y, z, minion->Where().Extent(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
                minion->m_movementInfo.ChangePosition(x, y, z, master->Where().Facing());
                minion->GetMap()->CreatureRelocation((Creature*)minion, x, y, z, master->Where().Facing());
                minion->SendHeartBeat();
                minion->GetMotionMaster()->Initialize();
            }
        },
        CONTROLLED_PET | CONTROLLED_MINIPET | CONTROLLED_GUARDIANS);
}

void Player::SendInitialPacketsAfterAddToMap(InitialWorldEntryContext const* initialEntry)
{
    if (initialEntry)
    {
        UpdateZone(initialEntry->zoneId, initialEntry->areaId,
            !initialEntry->initialWorldStatesSent);
        if (initialEntry->cinematicStarted)
        {
            BeginLoginCinematicRoot();
        }

        ScheduleLoginEffect();
    }
    else
    {

        uint32 newzone, newarea;
        GetTerrain()->GetZoneAndAreaId(newzone, newarea, Where().X(), Where().Y(), Where().Z());
        UpdateZone(newzone, newarea);

        CastSpell(this, 836, true);
    }

    static const AuraType auratypes[] =
    {
        SPELL_AURA_MOD_FEAR,     SPELL_AURA_TRANSFORM,                 SPELL_AURA_WATER_WALK,
        SPELL_AURA_FEATHER_FALL, SPELL_AURA_HOVER,                     SPELL_AURA_SAFE_FALL,
        SPELL_AURA_NONE
    };

    for (AuraType const* itr = &auratypes[0]; itr && itr[0] != SPELL_AURA_NONE; ++itr)
    {

        const auto auraList = GetAurasByType(*itr);

        if (!auraList.empty())
        {
            auraList.front()->ApplyModifier(true, true);
        }
    }

    if (HasAuraType(SPELL_AURA_MOD_STUN) || HasAuraType(SPELL_AURA_MOD_ROOT))
    {
        SetRoot(true);
    }

    m_inventory.SendClocks();
}

void Player::ApplyEquipCooldown(Item* pItem)
{
    if (pItem->GetProto()->Flags & ITEM_FLAG_NO_EQUIP_COOLDOWN)
    {
        return;
    }

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = pItem->GetProto()->Spells[i];

        if (!spellData.SpellId)
        {
            continue;
        }

        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
        {
            continue;
        }

        SpellCooldowns::const_iterator itr = GetSpellCooldownMap().find(spellData.SpellId);
        if (itr != GetSpellCooldownMap().end() && itr->second.itemid == pItem->GetEntry() && itr->second.end > time(nullptr) + 30)
        {
            break;
        }

        AddSpellCooldown(spellData.SpellId, pItem->GetEntry(), time(nullptr) + 30);

        WorldPacket data(SMSG_ITEM_COOLDOWN, 12);
        data << static_cast<ObjectGuid>(pItem->GetObjectGuid());
        data << uint32(spellData.SpellId);
        GetSession()->SendPacket(&data);
    }
}

void Player::SendAuraDurationsForTarget(Unit* target)
{
    SpellAuraHolderMap const& auraHolders = target->GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        if (holder->GetAuraSlot() >= MAX_AURAS || holder->IsPassive() || holder->GetCasterGuid() != GetObjectGuid())
        {
            continue;
        }

        holder->SendAuraDurationForCaster(this);
    }
}

uint32 Player::GetMinLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId)
{
    if (bracket_id < 1)
    {
        return 0;
    }

    if (bracket_id > BG_BRACKET_ID_LAST)
    {
        bracket_id = BG_BRACKET_ID_LAST;
    }

    BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    assert(bg);
    return 10 * bracket_id + bg->GetMinLevel();
}

uint32 Player::GetMaxLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId)
{
    if (bracket_id >= BG_BRACKET_ID_LAST)
    {
        return 255;
    }

    return GetMinLevelForBattleGroundBracketId(bracket_id, bgTypeId) + 10;
}

BattleGroundBracketId Player::GetBattleGroundBracketIdFromLevel(BattleGroundTypeId bgTypeId) const
{
    BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    assert(bg);
    if (getLevel() < bg->GetMinLevel())
    {
        return BG_BRACKET_ID_FIRST;
    }

    uint32 bracket_id = (getLevel() - bg->GetMinLevel()) / 10;
    if (bracket_id > MAX_BATTLEGROUND_BRACKETS)
    {
        return BG_BRACKET_ID_LAST;
    }

    return BattleGroundBracketId(bracket_id);
}

float Player::GetReputationPriceDiscount(Creature const* pCreature) const
{
    FactionTemplateEntry const* vendor_faction = pCreature->getFactionTemplateEntry();
    if (!vendor_faction || !vendor_faction->Faction)
    {
        return 1.0f;
    }

    uint32 discount = 100;
    ReputationRank rank = GetReputationRank(vendor_faction->Faction);
    if (rank >= REP_HONORED)
    {
        discount -= 10;
    }

    if (GetHonorRankInfo().visualRank >= 3)
    {
        if (FactionTemplateEntry const* player_faction = getFactionTemplateEntry())
        {
            if (AsFactionsDeclare(*player_faction, *vendor_faction) == Reaction::Friendly)
            {
                discount -=10;
            }
        }
    }
    return float (discount / 100.0f);
}

bool Player::IsSpellFitByClassAndRace(uint32 spell_id, uint32* pReqlevel ) const
{
    uint32 racemask  = getRaceMask();
    uint32 classmask = getClassMask();

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spell_id);
    if (bounds.first == bounds.second)
    {
        return true;
    }

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* abilityEntry = _spell_idx->second;

        if (abilityEntry->RaceMask && (abilityEntry->RaceMask & racemask) == 0)
        {
            continue;
        }

        if (abilityEntry->ClassMask && (abilityEntry->ClassMask & classmask) == 0)
        {
            continue;
        }

        SkillRaceClassInfoMapBounds raceBounds = sSpellMgr.GetSkillRaceClassInfoMapBounds(abilityEntry->SkillLine);
        for (SkillRaceClassInfoMap::const_iterator itr = raceBounds.first; itr != raceBounds.second; ++itr)
        {
            SkillRaceClassInfoEntry const* skillRCEntry = itr->second;
            if ((skillRCEntry->RaceMask & racemask) && (skillRCEntry->ClassMask & classmask))
            {
                if (skillRCEntry->Flags & ABILITY_SKILL_NONTRAINABLE)
                {
                    return false;
                }

                if (pReqlevel)
                {
                    if (skillRCEntry->MinLevel)
                    {
                        *pReqlevel = skillRCEntry->MinLevel;
                        return true;
                    }
                }
                else
                {

                    switch (spell_id)
                    {
                        case 33388:
                        case 33389:
                            if (getLevel() < uint32(sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL)))
                            {
                                return false;
                            }
                            break;
                        case 33391:
                        case 33392:
                            if (getLevel() < uint32(sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL)))
                            {
                                return false;
                            }
                            break;
                        default:
                            if (skillRCEntry->MinLevel && getLevel() < skillRCEntry->MinLevel)
                            {
                                return false;
                            }
                            break;
                    }
                }
            }
        }

        return true;
    }

    return false;
}

void Player::SummonIfPossible(bool agree)
{
    if (!agree)
    {
        m_summon.Withdraw();
        return;
    }

    if (!m_summon.Stands(time(nullptr)))
    {
        return;
    }

    if (IsTaxiFlying())
    {
        GetMotionMaster()->MovementExpired();
        m_taxi.ClearTaxiDestinations();
    }

    if (BattleGround* bg = Battle().Ground())
    {
        bg->EventPlayerDroppedFlag(this);
    }

    Geometry::Placement const to = m_summon.at;
    m_summon.Withdraw();

    TeleportTo(to.MapId(), to.X(), to.Y(), to.Z(), Where().Facing());
}

void Player::AutoUnequipOffhandIfNeed()
{
    Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!offItem)
    {
        return;
    }

    if (!IsTwoHandUsed())
    {
        return;
    }

    ItemPosCountVec off_dest;
    uint8 off_msg = CanStoreItem(NULL_BAG, NULL_SLOT, off_dest, offItem, false);
    if (off_msg == EQUIP_ERR_OK)
    {
        RemoveItem(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
        StoreItem(off_dest, offItem, true);
    }
    else
    {
        MoveItemFromInventory(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
        CharacterDatabase.BeginTransaction();
        offItem->DeleteFromInventoryDB();
        offItem->SaveToDB();
        CharacterDatabase.CommitTransaction();

        std::string subject = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);
        MailDraft(subject, "There's were problems with equipping this item.").AddItem(offItem).SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
    }
}

bool Player::HasItemFitToSpellReqirements(SpellEntry const* spellInfo, Item const* ignoreItem)
{
    if (spellInfo->EquippedItemClass < 0)
    {
        return true;
    }

    switch (spellInfo->EquippedItemClass)
    {
        case ITEM_CLASS_WEAPON:
        {
            for (int i = EQUIPMENT_SLOT_MAINHAND; i < EQUIPMENT_SLOT_TABARD; ++i)
            {
                if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                    {
                        return true;
                    }
                }
            }
            break;
        }
        case ITEM_CLASS_ARMOR:
        {

            for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_MAINHAND; ++i)
            {
                if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                    {
                        return true;
                    }
                }
            }

            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
            {
                if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                {
                    return true;
                }
            }

            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
            {
                if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                {
                    return true;
                }
            }

            break;
        }
        default:
            sLog.outError("HasItemFitToSpellReqirements: Not handled spell requirement for item class %u", spellInfo->EquippedItemClass);
            break;
    }

    return false;
}

bool Player::CanNoReagentCast(SpellEntry const* ) const
{

    return false;
}

void Player::RemoveItemDependentAurasAndCasts(Item* pItem)
{
    SpellAuraHolderMap& auras = GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end();)
    {
        SpellAuraHolder* holder = itr->second;

        SpellEntry const* spellInfo = holder->GetSpellProto();
        if (holder->IsPassive() ||  holder->GetCasterGuid() != GetObjectGuid())
        {
            ++itr;
            continue;
        }

        if (HasItemFitToSpellReqirements(spellInfo, pItem))
        {
            ++itr;
            continue;
        }

        RemoveAuras(holder->GetId());
        itr = auras.begin();
    }

    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
        {
            if (spell->getState() != SPELL_STATE_DELAYED && !HasItemFitToSpellReqirements(spell->m_spellInfo, pItem))
            {
                InterruptSpell(CurrentSpellTypes(i));
            }
        }
    }
}

uint32 Player::GetResurrectionSpellId()
{

    uint32 prio = 0;
    uint32 spell_id = 0;
    const auto dummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
    for (auto* aura : dummyAuras)
    {

        if (prio < 2 && aura->GetSpellProto()->SpellVisualID == 99 && aura->GetSpellProto()->SpellIconID == 92)
        {
            switch (aura->GetId())
            {
                case 20707: spell_id =  3026; break;
                case 20762: spell_id = 20758; break;
                case 20763: spell_id = 20759; break;
                case 20764: spell_id = 20760; break;
                case 20765: spell_id = 20761; break;
                case 27239: spell_id = 27240; break;
                default:
                    sLog.outError("Unhandled spell %u: S.Resurrection", aura->GetId());
                    continue;
            }

            prio = 3;
        }

        else if (aura->GetId() == 23701 && roll_chance_i(10))
        {
            prio = 2;
            spell_id = 23700;
        }
    }

    if (prio < 1 && HasSpell(20608) && !HasSpellCooldown(21169) && HasItemCount(17030, EFFECT_INDEX_1))
    {
        spell_id = 21169;
    }

    return spell_id;
}

uint32 Player::GetBaseWeaponSkillValue(WeaponAttackType attType) const
{
    Item* item = GetWeaponForAttack(attType, true, true);

    if (attType != BASE_ATTACK && !item)
    {
        return 0;
    }

    uint32  skill = item ? item->GetSkill() : uint32(SKILL_UNARMED);
    return GetPureSkillValue(skill);
}

void Player::ResurectUsingRequestData()
{

    if (m_resurrect.MovesHim())
    {
        TeleportTo(m_resurrect.at.MapId(), m_resurrect.at.X(), m_resurrect.at.Y(),
                   m_resurrect.at.Z(), Where().Facing());
    }

    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_RESURRECT_PLAYER);
        return;
    }

    RaiseOnOffer();
}

void Player::RaiseOnOffer()
{
    ResurrectPlayer(0.0f, false);

    SetHealth(std::min(m_resurrect.health, GetMaxHealth()));
    SetPower(POWER_MANA, std::min(m_resurrect.mana, GetMaxPower(POWER_MANA)));
    SetPower(POWER_RAGE, 0);
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));

    SpawnCorpseBones();
}

bool Player::IsClientControl(Unit* target) const
{
    return (target && !target->IsFleeing() && !target->IsConfused() && !target->IsTaxiFlying() &&
        (!IsPlayer(target) ||
        !((Player*)target)->Battle().InOne() || ((Player*)target)->Battle().Ground()->GetStatus() != STATUS_WAIT_LEAVE) &&
        target->GetCharmerOrOwnerOrOwnGuid() == GetObjectGuid());
}

void Player::SetClientControl(Unit* target, uint8 allowMove)
{
    WorldPacket data(SMSG_CLIENT_CONTROL_UPDATE, target->GetPackGUID().size() + 1);
    data << target->GetPackGUID();
    data << uint8(allowMove);
    GetSession()->SendPacket(&data);
}

bool ItemPosCount::isContainedIn(ItemPosCountVec const& vec) const
{
    for (ItemPosCountVec::const_iterator itr = vec.begin(); itr != vec.end(); ++itr)
    {
        if (itr->pos == pos)
        {
            return true;
        }
    }

    return false;
}

bool Player::isTotalImmune()
{
    const auto immune = GetAurasByType(SPELL_AURA_SCHOOL_IMMUNITY);

    uint32 immuneMask = 0;
    for (auto* aura : immune)
    {
        immuneMask |= aura->GetModifier()->m_miscvalue;
        if (immuneMask & SPELL_SCHOOL_MASK_ALL)
        {
            return true;
        }
    }
    return false;
}

void Player::AutoStoreLoot(Occupant const* lootTarget, uint32 loot_id, LootStore const& store, bool broadcast, uint8 bag, uint8 slot)
{
    Loot loot(lootTarget);
    loot.FillLoot(loot_id, store, this, true);

    AutoStoreLoot(loot, broadcast, bag, slot);
}

void Player::AutoStoreLoot(Loot& loot, bool broadcast, uint8 bag, uint8 slot)
{
    uint32 max_slot = loot.GetMaxSlotInLootFor(this);
    for (uint32 i = 0; i < max_slot; ++i)
    {
        LootItem* lootItem = loot.LootItemInSlot(i, this);

        ItemPosCountVec dest;
        InventoryResult msg = CanStoreNewItem(bag, slot, dest, lootItem->itemid, lootItem->count);
        if (msg != EQUIP_ERR_OK && slot != NULL_SLOT)
        {
            msg = CanStoreNewItem(bag, NULL_SLOT, dest, lootItem->itemid, lootItem->count);
        }
        if (msg != EQUIP_ERR_OK && bag != NULL_BAG)
        {
            msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, lootItem->itemid, lootItem->count);
        }
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, lootItem->itemid);
            continue;
        }

        Item* pItem = StoreNewItem(dest, lootItem->itemid, true, lootItem->randomPropertyId);
        SendNewItem(pItem, lootItem->count, false, false, broadcast);
    }
}

Item* Player::ConvertItem(Item* item, uint32 newItemId)
{
    uint16 pos = item->GetPos();

    Item* pNewItem = Item::CreateItem(newItemId, 1, this);
    if (!pNewItem)
    {
        return nullptr;
    }

    for (uint8 j = PERM_ENCHANTMENT_SLOT; j <= TEMP_ENCHANTMENT_SLOT; ++j)
    {
        if (item->GetEnchantmentId(EnchantmentSlot(j)))
        {
            pNewItem->SetEnchantment(EnchantmentSlot(j), item->GetEnchantmentId(EnchantmentSlot(j)),
                item->GetEnchantmentDuration(EnchantmentSlot(j)), item->GetEnchantmentCharges(EnchantmentSlot(j)));
        }
    }

    if (item->GetUInt32Value(ITEM_FIELD_DURABILITY) < item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
    {
        double loosePercent = 1 - item->GetUInt32Value(ITEM_FIELD_DURABILITY) / double(item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY));
        DurabilityLoss(pNewItem, loosePercent);
    }

    if (Inventory::IsCarried(pos))
    {
        ItemPosCountVec dest;
        InventoryResult msg = CanStoreItem(item->GetBagSlot(), item->GetSlot(), dest, pNewItem, true);

        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return StoreItem(dest, pNewItem, true);
        }
    }
    else if (Inventory::IsBanked(pos))
    {
        ItemPosCountVec dest;
        InventoryResult msg = CanBankItem(item->GetBagSlot(), item->GetSlot(), dest, pNewItem, true);

        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return BankItem(dest, pNewItem, true);
        }
    }
    else if (Inventory::IsWorn(pos))
    {
        uint16 dest;
        InventoryResult msg = CanEquipItem(item->GetSlot(), dest, pNewItem, true, false);

        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            pNewItem = EquipItem(dest, pNewItem, true);
            AutoUnequipOffhandIfNeed();
            return pNewItem;
        }
    }

    delete pNewItem;
    return nullptr;
}

uint32 Player::CalculateTalentsPoints() const
{
    uint32 talentPointsForLevel = getLevel() < 10 ? 0 : getLevel() - 9;
    return uint32(talentPointsForLevel * sWorld.getConfig(CONFIG_FLOAT_RATE_TALENT));
}

struct DoPlayerLearnSpell
{
    DoPlayerLearnSpell(Player& _player) : player(_player) {}
    void operator()(uint32 spell_id) { player.learnSpell(spell_id, false); }
    Player& player;
};

void Player::learnSpellHighRank(uint32 spellid)
{
    learnSpell(spellid, false);

    DoPlayerLearnSpell worker(*this);
    sSpellMgr.doForHighRanks(spellid, worker);
}

void Player::_LoadSkills(QueryResult* result)
{

    uint32 count = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint16 skill    = fields[0].GetUInt16();
            uint16 value    = fields[1].GetUInt16();
            uint16 max      = fields[2].GetUInt16();

            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
            if (!pSkill)
            {
                sLog.outError("Character %u has skill %u that does not exist.", GetGUIDLow(), skill);
                continue;
            }

            switch (GetSkillRangeType(pSkill, false))
            {
                case SKILL_RANGE_LANGUAGE:
                    value = max = 300;
                    break;
                case SKILL_RANGE_MONO:
                    value = max = 1;
                    break;
                case SKILL_RANGE_LEVEL:
                    max = GetMaxSkillValueForLevel();
                    break;
                default:
                    break;
            }

            if (value == 0)
            {
                sLog.outError("Character %u has skill %u with value 0. Will be deleted.", GetGUIDLow(), skill);
                CharacterDatabase.PExecute("DELETE FROM `character_skills` WHERE `guid` = '%u' AND `skill` = '%u' ", GetGUIDLow(), skill);
                continue;
            }

            SetUInt32Value(PLAYER_SKILL_INDEX(count), MAKE_PAIR32(skill, 0));
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), MAKE_SKILL_VALUE(value, max));
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);

            mSkillStatus.insert(SkillStatusMap::value_type(skill, SkillStatusData(count, SKILL_UNCHANGED)));

            learnSkillRewardedSpells(skill, value);

            ++count;

            if (count >= PLAYER_MAX_SKILLS)
            {
                sLog.outError("Character %u has more than %u skills.", GetGUIDLow(), PLAYER_MAX_SKILLS);
                break;
            }
        }
        while (result->NextRow());
        delete result;
    }

    for (; count < PLAYER_MAX_SKILLS; ++count)
    {
        SetUInt32Value(PLAYER_SKILL_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);
    }
}

InventoryResult Player::CanEquipUniqueItem(Item* pItem, uint8 eslot) const
{
    ItemPrototype const* pProto = pItem->GetProto();

    if (InventoryResult res = CanEquipUniqueItem(pProto, eslot))
    {
        return res;
    }

    return EQUIP_ERR_OK;
}

InventoryResult Player::CanEquipUniqueItem(ItemPrototype const* itemProto, uint8 except_slot) const
{

    if (itemProto->Flags & ITEM_FLAG_UNIQUE_EQUIPPED)
    {

        if (HasItemWithIdEquipped(itemProto->ItemId, 1, except_slot))
        {
            return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
        }
    }

    return EQUIP_ERR_OK;
}

void Player::HandleFall(MovementInfo const& movementInfo)
{

    Position const* position = movementInfo.GetPos();
    float z_diff = m_lastFallZ - position->z;
    DEBUG_LOG("zDiff = %f", z_diff);

    if (z_diff >= 14.57f && !IsDead() && !isGameMaster() && !HasMovementFlag(MOVEFLAG_ONTRANSPORT) &&
        !HasAuraType(SPELL_AURA_HOVER) && !HasAuraType(SPELL_AURA_FEATHER_FALL) &&
        !IsImmuneToDamage(SPELL_SCHOOL_MASK_NORMAL))
    {

        int32 safe_fall = GetTotalAuraModifier(SPELL_AURA_SAFE_FALL);

        if (stats::FallShare(z_diff, float(safe_fall)) > 0.0f)
        {
            uint32 damage = stats::FallDamage(z_diff, float(safe_fall), GetMaxHealth(),
                                              sWorld.getConfig(CONFIG_FLOAT_RATE_DAMAGE_FALL));

            float height = position->z;
            ClampToAllowedZ(*this, position->x, position->y, height);

            if (damage > 0)
            {

                if (damage > GetMaxHealth())
                {
                    damage = GetMaxHealth();
                }

                if (GetDummyAura(43621))
                {
                    damage = GetMaxHealth() / 2;
                }

                Dangers().Harm(DAMAGE_FALL, damage);
            }

            DEBUG_LOG("FALLDAMAGE z=%f sz=%f pZ=%f FallTime=%d mZ=%f damage=%d SF=%d" , position->z, height, Where().Z(), movementInfo.GetFallTime(), height, damage, safe_fall);
        }
    }
}

void Player::ModifyMoney(int32 d)
{

    if (d < 0)
    {
        SetMoney(GetMoney() > uint32(-d) ? GetMoney() + d : 0);
    }
    else
    {
        SetMoney(GetMoney() < uint32(MAX_MONEY_AMOUNT - d) ? GetMoney() + d : MAX_MONEY_AMOUNT);
    }

}

void Player::RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also )
{
    m_atLoginFlags &= ~f;

    if (in_db_also)
    {
        CharacterDatabase.PExecute("UPDATE `characters` set `at_login` = `at_login` & ~ %u WHERE `guid` ='%u'", uint32(f), GetGUIDLow());
    }
}

void Player::SendClearCooldown(uint32 spell_id, Unit* target)
{
    WorldPacket data(SMSG_CLEAR_COOLDOWN, 4 + 8);
    data << uint32(spell_id);
    data << target->GetObjectGuid();
    SendDirectMessage(&data);
}

void Player::BuildTeleportAckMsg(WorldPacket& data, float x, float y, float z, float ang) const
{
    MovementInfo mi = m_movementInfo;
    mi.ChangePosition(x, y, z, ang);

    data.Initialize(MSG_MOVE_TELEPORT_ACK, 41);
    data << GetPackGUID();
    data << uint32(0);
    data << mi;
}

bool Player::HasMovementFlag(MovementFlags f) const
{
    return m_movementInfo.HasMovementFlag(f);
}

void Player::SetHomebindToLocation(Geometry::Placement const& loc, uint32 area_id)
{
    m_hearth.SetTo(loc.MapId(), uint16(area_id), loc.X(), loc.Y(), loc.Z());

    CharacterDatabase.PExecute("UPDATE `character_homebind` SET `map` = '%u', `zone` = '%u', `position_x` = '%f', `position_y` = '%f', `position_z` = '%f' WHERE `guid` = '%u'",
        Home().MapId(), Home().AreaId(), Home().X(), Home().Y(), Home().Z(), GetGUIDLow());
}

Object* Player::GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask)
{
    switch (GuidHigh(guid))
    {
        case HIGHGUID_ITEM:
            if (typemask & TYPEMASK_ITEM)
            {
                return GetItemByGuid(guid);
            }
            break;
        case HIGHGUID_PLAYER:
            if (GetObjectGuid() == guid)
            {
                return this;
            }
            if ((typemask & TYPEMASK_PLAYER) && IsInWorld())
            {
                return sPlayerRegistry.Find(guid);
            }
            break;
        case HIGHGUID_GAMEOBJECT:
            if ((typemask & TYPEMASK_GAMEOBJECT) && IsInWorld())
            {
                return GetMap()->GetGameObject(guid);
            }
            break;
        case HIGHGUID_UNIT:
            if ((typemask & TYPEMASK_UNIT) && IsInWorld())
            {
                return GetMap()->GetCreature(guid);
            }
            break;
        case HIGHGUID_PET:
            if ((typemask & TYPEMASK_UNIT) && IsInWorld())
            {
                return GetMap()->GetPet(guid);
            }
            break;
        case HIGHGUID_DYNAMICOBJECT:
            if ((typemask & TYPEMASK_DYNAMICOBJECT) && IsInWorld())
            {
                return GetMap()->GetDynamicObject(guid);
            }
            break;
        case HIGHGUID_TRANSPORT:
        case HIGHGUID_CORPSE:
        case HIGHGUID_MO_TRANSPORT:
            break;
    }

    return nullptr;
}

bool Player::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const
{
    switch (spellInfo->Effect[index])
    {
        case SPELL_EFFECT_ATTACK_ME:
            return true;
        default:
            break;
    }
    switch (spellInfo->EffectAura[index])
    {
        case SPELL_AURA_MOD_TAUNT:
            return true;
        default:
            break;
    }
    return Unit::IsImmuneToSpellEffect(spellInfo, index, castOnSelf);
}

void Player::KnockBackFrom(Unit* target, float horizontalSpeed, float verticalSpeed)
{
    float angle = this == target ? Where().Facing() + M_PI_F : target->Where().BearingTo(this->Where());
    GetSession()->SendKnockBack(angle, horizontalSpeed, verticalSpeed);
}
