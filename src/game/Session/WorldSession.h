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

#pragma once

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include <ctime>
#include <string>
#include <vector>
#include <list>
#include "CharacterEnumMapSnapshot.h"
#include "SessionProtocolPolicy.h"
#include "Auth/BigNumber.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "AuctionHouseMgr.h"
#include "Item.h"
#include <chrono>
#include <memory>

struct ItemPrototype;
struct AuctionEntry;
struct AuctionHouseEntry;
struct TradeStatusInfo;

class Creature;
class Item;
class Object;
class Player;
class Unit;
class WorldPacket;
class SessionMailbox;
class QueryResult;
class LoginQueryHolder;
class CharacterHandler;
class GMTicket;
class MovementInfo;
class WorldSession;

namespace proto
{
class IClientLink;
}

struct OpcodeHandler;

enum PartyOperation
{
    PARTY_OP_INVITE = 0,
    PARTY_OP_LEAVE = 2
};

enum PartyResult
{
    ERR_PARTY_RESULT_OK = 0,
    ERR_BAD_PLAYER_NAME_S = 1,
    ERR_TARGET_NOT_IN_GROUP_S = 2,
    ERR_GROUP_FULL = 3,
    ERR_ALREADY_IN_GROUP_S = 4,
    ERR_NOT_IN_GROUP = 5,
    ERR_NOT_LEADER = 6,
    ERR_PLAYER_WRONG_FACTION = 7,
    ERR_IGNORING_YOU_S = 8
};

enum TutorialDataState
{
    TUTORIALDATA_UNCHANGED = 0,
    TUTORIALDATA_CHANGED = 1,
    TUTORIALDATA_NEW = 2
};

class WorldSession
{
    friend class CharacterHandler;

    public:

        WorldSession(uint32 id, std::shared_ptr<proto::IClientLink> link,
                     std::shared_ptr<SessionMailbox> mailbox, AccountTypes sec,
                     time_t mute_time, LocaleConstant locale);

        ~WorldSession();

        void SetPlayerLoading(bool loading) { m_playerLoading = loading; }

        uint32 PingsTooFast();

        bool PlayerLoading() const
        {
            return m_playerLoading;
        }

        bool PlayerLogout() const
        {
            return m_playerLogout;
        }

        bool PlayerLogoutWithSave() const
        {
            return m_playerLogout && m_playerSave;
        }

        void SizeError(WorldPacket const& packet, uint32 size) const;

        void SendPacket(WorldPacket const* packet);
        void SetPendingAddonInfo(std::unique_ptr<WorldPacket> packet);
        void SendPendingAddonInfo();
        void SendNotification(const char* format, ...) ATTR_PRINTF(2, 3);
        void SendNotification(int32 string_id, ...);
        void SendPetNameInvalid(uint32 error, const std::string& name);
        void SendPartyResult(PartyOperation operation, const std::string& member, PartyResult res);
        void SendGuildInvite(Player* player, bool alreadyInGuild = false);
        void SendAreaTriggerMessage(const char* Text, ...) ATTR_PRINTF(2, 3);
        void SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg = 0);
        void SendQueryTimeResponse();

        AccountTypes GetSecurity() const
        {
            return _security;
        }
        uint32 GetAccountId() const
        {
            return _accountId;
        }
        Player* GetPlayer() const
        {
            return _player;
        }
        ObjectGuid GetNpcWatchLastGuid() const
        {
            return m_npcWatchLastGuid;
        }
        void SetNpcWatchLastGuid(ObjectGuid guid)
        {
            m_npcWatchLastGuid = guid;
        }
        void ClearNpcWatchLastGuid()
        {
            m_npcWatchLastGuid = 0;
        }
        char const* GetPlayerName() const;
        void SetSecurity(AccountTypes security)
        {
            _security = security;
        }
        std::string const& GetRemoteAddress()
        {
            return m_Address;
        }
        void SetPlayer(Player* plr)
        {
            _player = plr;
        }

        bool HasMatchingCharacterEnumMap(ObjectGuid guid, uint32 mapId) const
        {
            return m_characterEnumMaps.Matches(guid, mapId);
        }

        void SetInQueue(bool state)
        {
            m_inQueue = state;
        }

        bool isLogingOut() const
        {
            return _logoutTime || m_playerLogout;
        }

        void LogoutRequest(time_t requestTime)
        {
            _logoutTime = requestTime;
        }

        bool ShouldLogOut(time_t currTime) const
        {
            return (_logoutTime > 0 && currTime >= _logoutTime + 20);
        }

        void LogoutPlayer(bool Save);
        void KickPlayer();

        void QueuePacket(WorldPacket* new_packet);

        void HandlePacket(WorldPacket& packet);

        Map* MapForPacket(const WorldPacket& packet) const;

        bool Update();

        void SendAuthWaitQue(uint32 position);

        void SendNameQueryOpcode(Player* p);
        void SendNameQueryOpcodeFromDB(ObjectGuid guid);
        static void SendNameQueryOpcodeFromDBCallBack(QueryResult* result, uint32 accountId);

        void SendTrainerList(ObjectGuid guid);
        void SendTrainerList(ObjectGuid guid, const std::string& strTitle);

        void SendListInventory(ObjectGuid guid);
        bool CheckBanker(ObjectGuid guid);
        void SendShowBank(ObjectGuid guid);
        bool CheckMailBox(ObjectGuid guid);
        void SendTabardVendorActivate(ObjectGuid guid);
        void SendSpiritResurrect();
        void SendBindPoint(Creature* npc);
        void SendGMTicketGetTicket(uint32 status, GMTicket* ticket = nullptr);

        void SendAttackStop(Unit const* enemy);

        void SendBattlegGroundList(ObjectGuid guid, BattleGroundTypeId bgTypeId);

        void SendTradeStatus(const TradeStatusInfo& status);
        void SendUpdateTrade(bool trader_state = true);
        void SendCancelTrade();

        void SendPetitionQueryOpcode(ObjectGuid petitionguid);

        void SendPetNameQuery(ObjectGuid guid, uint32 petnumber);
        void SendStablePet(ObjectGuid guid);
        void SendStableResult(uint8 res);
        bool CheckStableMaster(ObjectGuid guid);

        void LoadTutorialsData();
        void SendTutorialsData();
        void SaveTutorialsData();
        uint32 GetTutorialInt(uint32 intId)
        {
            return m_Tutorials[intId];
        }

        void SetTutorialInt(uint32 intId, uint32 value)
        {
            if (m_Tutorials[intId] != value)
            {
                m_Tutorials[intId] = value;
                if (m_tutorialState == TUTORIALDATA_UNCHANGED)
                {
                    m_tutorialState = TUTORIALDATA_CHANGED;
                }
            }
        }

        void SendAuctionHello(Unit* unit);
        void SendAuctionCommandResult(AuctionEntry* auc, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError = EQUIP_ERR_OK, uint32 newOutbid = 0);

        void SendAuctionCommandResultData(uint32 aucId, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError, uint32 newOutbid);
        void SendAuctionBidderNotification(AuctionEntry* auction, bool won);

        void SendAuctionBidderNotificationData(uint32 houseId, uint32 id, uint32 bidder, uint32 bid, uint32 outbid, uint32 itemTemplate, int32 itemRand, bool won);
        void SendAuctionOwnerNotification(AuctionEntry* auction, bool sold);

        void SendAuctionOwnerNotificationData(uint32 houseId, uint32 id, uint32 bid, uint32 outbid, uint32 bidderGuidLow, uint32 itemTemplate, int32 itemRand, bool sold);
        void SendAuctionRemovedNotification(AuctionEntry* auction);

        void SendAuctionRemovedNotificationData(uint32 id, uint32 itemTemplate, int32 itemRand);
        static void SendAuctionOutbiddedMail(AuctionEntry* auction);
        void SendAuctionCancelledToBidderMail(AuctionEntry* auction);
        AuctionHouseEntry const* GetCheckedAuctionHouseForAuctioneer(ObjectGuid guid);

        void SendEnchantmentLog(ObjectGuid targetGuid, ObjectGuid casterGuid, uint32 itemId, uint32 spellId);
        void SendItemEnchantTimeUpdate(ObjectGuid playerGuid, ObjectGuid itemGuid, uint32 slot, uint32 duration);

        void SendTaxiStatus(ObjectGuid guid);
        void SendTaxiMenu(Creature* unit);
        void SendDoFlight(uint32 mountDisplayId, uint32 path, uint32 pathNode = 0);
        bool SendLearnNewTaxiNode(Creature* unit);
        void SendActivateTaxiReply(ActivateTaxiReply reply);

        void SendGuildCommandResult(uint32 typecmd, const std::string& str, uint32 cmdresult);
        void SendPetitionShowList(ObjectGuid guid);
        void SendSaveGuildEmblem(uint32 msg);
        void SendBattleGroundJoinError(uint8 err);

        void SendMeetingstoneFailed(uint8 status);
        void SendMeetingstoneSetqueue(uint32 areaid, uint8 status);

        void BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket* data);

        void DoLootRelease(ObjectGuid lguid);

        time_t m_muteTime;

        LocaleConstant GetClientLocale() const
        {
            return m_clientLocale;
        }
        LocaleConstant GetSessionDbcLocale() const
        {
            return m_sessionDbcLocale;
        }
        int GetSessionDbLocaleIndex() const
        {
            return m_sessionDbLocaleIndex;
        }
        const char* GetMangosString(int32 entry) const;

        uint32 GetLatency() const
        {
            return m_latency;
        }
        void SetLatency(uint32 latency)
        {
            m_latency = latency;
        }
        void SetClientTimeDelay(uint32 delay) { m_clientTimeDelay = delay; }
        uint32 getDialogStatus(Player* pPlayer, Object* questgiver, uint32 defstatus);

        void SendKnockBack(float angle, float horizontalSpeed, float verticalSpeed);
        void SendPlaySpellVisual(ObjectGuid guid, uint32 spellArtKit);

        void HandleCharEnumOpcode(WorldPacket& recvPacket);
        void HandleCharEnum(QueryResult* result);
        void HandlePlayerLogin(LoginQueryHolder* holder);

        void HandlePlayedTime(WorldPacket& recvPacket);

        void HandleMoveUnRootAck(WorldPacket& recvPacket);
        void HandleMoveRootAck(WorldPacket& recvPacket);

        void HandleInspectOpcode(WorldPacket& recvPacket);

        void HandleInspectHonorStatsOpcode(WorldPacket& recvPacket);

        void HandleFeatherFallAck(WorldPacket& recv_data);

        void HandleShowingHelmOpcode(WorldPacket& recv_data);
        void HandleShowingCloakOpcode(WorldPacket& recv_data);

        void HandleRepopRequestOpcode(WorldPacket& recvPacket);

        void HandleWhoOpcode(WorldPacket& recvPacket);
        void HandleLogoutRequestOpcode(WorldPacket& recvPacket);
        void HandlePlayerLogoutOpcode(WorldPacket& recvPacket);
        void HandleLogoutCancelOpcode(WorldPacket& recvPacket);

        void SendGMTicketStatusUpdate(GMTicketStatus statusCode);

        void HandleTogglePvP(WorldPacket& recvPacket);

        void HandleZoneUpdateOpcode(WorldPacket& recvPacket);
        void HandleSetTargetOpcode(WorldPacket& recvPacket);
        void HandleSetSelectionOpcode(WorldPacket& recvPacket);
        void HandleStandStateChangeOpcode(WorldPacket& recvPacket);
        void HandleFriendListOpcode(WorldPacket& recvPacket);
        static void HandleAddFriendOpcodeCallBack(QueryResult* result, uint32 accountId);
        static void HandleAddIgnoreOpcodeCallBack(QueryResult* result, uint32 accountId);
        void HandleBugOpcode(WorldPacket& recvPacket);

        void HandleAreaTriggerOpcode(WorldPacket& recvPacket);

        void HandleUpdateAccountData(WorldPacket& recvPacket);
        void HandleRequestAccountData(WorldPacket& recvPacket);
        void HandleSetActionButtonOpcode(WorldPacket& recvPacket);

        void HandleMeetingStoneInfoOpcode(WorldPacket& recPacket);

        void HandleMoveWorldportAckOpcode();

        bool CanInteractWithQuestGiver(ObjectGuid guid, char const* descr);

        bool processChatmessageFurtherAfterSecurityChecks(std::string&, uint32);
        void SendPlayerNotFoundNotice(const std::string &name);
        void SendWrongFactionNotice();
        void SendChatRestrictedNotice();

        void HandleReclaimCorpseOpcode(WorldPacket& recvPacket);
        void HandleResurrectResponseOpcode(WorldPacket& recvPacket);

        void HandleCompleteCinematic(WorldPacket& recvPacket);
        void HandleNextCinematicCamera(WorldPacket& recvPacket);

        void HandlePageQuerySkippedOpcode(WorldPacket& recvPacket);

        void HandleTutorialClearOpcode(WorldPacket& recv_data);
        void HandleTutorialResetOpcode(WorldPacket& recv_data);

        void HandleSetActionBarTogglesOpcode(WorldPacket& recv_data);

        static void HandleChangePlayerNameOpcodeCallBack(QueryResult* result, uint32 accountId, std::string newname);

        void HandleBattleGroundPlayerPositionsOpcode(WorldPacket& recv_data);
        void HandlePVPLogDataOpcode(WorldPacket& recv_data);
        void HandleBattlefieldStatusOpcode(WorldPacket& recv_data);

        void HandleWorldTeleportOpcode(WorldPacket& recv_data);
        void HandleMoveSetRawPosition(WorldPacket& recv_data);
        void HandleFarSightOpcode(WorldPacket& recv_data);
        void HandleWhoisOpcode(WorldPacket& recv_data);

        void HandleCancelMountAuraOpcode(WorldPacket& recv_data);
        void HandleRequestPetInfoOpcode(WorldPacket& recv_data);

        void HandleSetTaxiBenchmarkOpcode(WorldPacket& recv_data);

    private:

        void ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket* packet);

        void LogUnexpectedOpcode(WorldPacket* packet, const char* reason);
        void LogUnprocessedTail(WorldPacket* packet);

        Player* _player;
        CharacterEnumMapSnapshot m_characterEnumMaps;
        std::shared_ptr<proto::IClientLink> m_link;
        std::shared_ptr<SessionMailbox> m_mailbox;
        std::unique_ptr<WorldPacket> m_pendingAddonInfo;
        std::string m_Address;

        AccountTypes _security;
        uint32 _accountId;

        time_t _logoutTime;
        bool m_inQueue;
        bool m_playerLoading;
        bool m_playerLogout;
        bool m_playerRecentlyLogout;
        bool m_playerSave;
        LocaleConstant m_clientLocale;
        LocaleConstant m_sessionDbcLocale;
        int m_sessionDbLocaleIndex;
        uint32 m_latency;
        uint32 m_Tutorials[8];
        TutorialDataState m_tutorialState;
        uint32 m_clientTimeDelay;
        ObjectGuid m_npcWatchLastGuid = 0;
        SessionPingTracker m_pingTracker;
};
