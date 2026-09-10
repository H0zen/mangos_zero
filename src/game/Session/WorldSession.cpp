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

#include <zlib.h>
#include "IClientLink.h"
#include <utility>
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <set>
#include <memory>
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "OpcodeTable.h"
#include "SessionMailbox.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ProtocolAnswers.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Group.h"
#include "CinematicFlyover.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "World.h"
#include "BattleGround/BattleGroundMgr.h"
#include "SocialMgr.h"

#include <cstdarg>

WorldSession::WorldSession(uint32 id, std::shared_ptr<proto::IClientLink> link,
                           std::shared_ptr<SessionMailbox> mailbox, AccountTypes sec,
                           time_t mute_time, LocaleConstant locale)
    : m_muteTime(mute_time),
    _player(nullptr), m_link(std::move(link)),
    m_mailbox(mailbox ? std::move(mailbox) : std::make_shared<SessionMailbox>()),
    _security(sec), _accountId(id), _logoutTime(0),
    m_inQueue(false), m_playerLoading(false), m_playerLogout(false), m_playerRecentlyLogout(false), m_playerSave(false),
    m_clientLocale(locale), m_sessionDbcLocale(sWorld.GetAvailableDbcLocale(locale)),
    m_sessionDbLocaleIndex(sObjectMgr.GetIndexForLocale(locale)),
    m_latency(0), m_clientTimeDelay(0), m_tutorialState(TUTORIALDATA_UNCHANGED), m_npcWatchLastGuid(),
    m_pingTracker()
{
    if (m_link)
    {
        m_Address = m_link->GetRemoteAddress();
    }
}

WorldSession::~WorldSession()
{
    m_mailbox->Close();

    if (_player)
    {
        LogoutPlayer(true);
    }

    if (m_link)
    {
        m_link->Close();
        m_link.reset();
    }

}

void WorldSession::SizeError(WorldPacket const& packet, uint32 size) const
{
    sLog.outError("Client (account %u) send packet %s (%u) with size %zu but expected %u (attempt crash server?), skipped",
        GetAccountId(), LookupOpcodeName(packet.GetOpcode()), packet.GetOpcode(), packet.size(), size);
}

char const* WorldSession::GetPlayerName() const
{
    return GetPlayer() ? GetPlayer()->GetName() : "<none>";
}

void WorldSession::SendPacket(WorldPacket const* packet)
{

    if (!m_link)
    {
        return;
    }

    if (opcodeTable[packet->GetOpcode()].status == STATUS_UNHANDLED)
    {
        sLog.outError("SESSION: tried to send an unhandled opcode 0x%.4X", packet->GetOpcode());
        return;
    }

#ifdef MANGOS_DEBUG

    static uint64 sendPacketCount = 0;
    static uint64 sendPacketBytes = 0;

    static time_t firstTime = time(nullptr);
    static time_t lastTime = firstTime;

    static uint64 sendLastPacketCount = 0;
    static uint64 sendLastPacketBytes = 0;

    time_t cur_time = time(nullptr);

    if ((cur_time - lastTime) < 60)
    {
        sendPacketCount += 1;
        sendPacketBytes += packet->size();

        sendLastPacketCount += 1;
        sendLastPacketBytes += packet->size();
    }
    else
    {
        uint64 minTime = uint64(cur_time - lastTime);
        uint64 fullTime = uint64(lastTime - firstTime);
        DETAIL_LOG("Send all time packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f time: %u", sendPacketCount, sendPacketBytes, float(sendPacketCount) / fullTime, float(sendPacketBytes) / fullTime, uint32(fullTime));
        DETAIL_LOG("Send last min packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f", sendLastPacketCount, sendLastPacketBytes, float(sendLastPacketCount) / minTime, float(sendLastPacketBytes) / minTime);

        lastTime = cur_time;
        sendLastPacketCount = 1;
        sendLastPacketBytes = packet->wpos();
    }

#endif

    m_link->SendPacket(*packet);
}

void WorldSession::SetPendingAddonInfo(std::unique_ptr<WorldPacket> packet)
{
    m_pendingAddonInfo = std::move(packet);
}

void WorldSession::SendPendingAddonInfo()
{
    if (!m_pendingAddonInfo)
    {
        return;
    }

    SendPacket(m_pendingAddonInfo.get());
    m_pendingAddonInfo.reset();
}

void WorldSession::QueuePacket(WorldPacket* new_packet)
{
    m_mailbox->Enqueue(std::unique_ptr<WorldPacket>(new_packet));
}

void WorldSession::LogUnexpectedOpcode(WorldPacket* packet, const char* reason)
{
    sLog.outError("SESSION: received unexpected opcode %s (0x%.4X) %s",
        LookupOpcodeName(packet->GetOpcode()),
        packet->GetOpcode(),
        reason);
}

void WorldSession::LogUnprocessedTail(WorldPacket* packet)
{
    sLog.outError("SESSION: opcode %s (0x%.4X) have unprocessed tail data (read stop at %zu from %zu)",
        LookupOpcodeName(packet->GetOpcode()),
        packet->GetOpcode(),
        packet->rpos(), packet->wpos());
}

Map* WorldSession::MapForPacket(const WorldPacket& packet) const
{
    const OpcodeHandler& opHandle = opcodeTable[packet.GetOpcode()];
    if (opHandle.packetProcessing != PROCESS_THREADSAFE)
    {
        return nullptr;
    }

    if (!_player || !_player->IsInWorld())
    {
        return nullptr;
    }

    return _player->GetMap();
}

void WorldSession::HandlePacket(WorldPacket& packetRef)
{
    WorldPacket* const packet = &packetRef;
    {

        OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
        try
        {
            switch (opHandle.status)
            {
                case STATUS_LOGGEDIN:
                    if (!_player)
                    {

                        if (!m_playerRecentlyLogout)
                        {
                            LogUnexpectedOpcode(packet, "the player has not logged in yet");
                        }
                    }
                    else if (_player->IsInWorld())
                    {
                        ExecuteOpcode(opHandle, packet);
                    }

                    break;
                case STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT:
                    if (!_player && !m_playerRecentlyLogout)
                    {
                        LogUnexpectedOpcode(packet, "the player has not logged in yet and not recently logout");
                    }
                    else

                    {
                        ExecuteOpcode(opHandle, packet);
                    }
                    break;
                case STATUS_TRANSFER:
                    if (!_player)
                    {
                        LogUnexpectedOpcode(packet, "the player has not logged in yet");
                    }
                    else if (_player->IsInWorld())
                    {
                        LogUnexpectedOpcode(packet, "the player is still in world");
                    }
                    else
                    {
                        ExecuteOpcode(opHandle, packet);
                    }
                    break;
                case STATUS_AUTHED:

                    if (m_inQueue && packet->GetOpcode() != CMSG_PING
                        && packet->GetOpcode() != CMSG_KEEP_ALIVE)
                    {
                        LogUnexpectedOpcode(packet, "the player not pass queue yet");
                        break;
                    }

                    m_playerRecentlyLogout = false;

                    ExecuteOpcode(opHandle, packet);
                    break;
                case STATUS_NEVER:
                    sLog.outError("SESSION: received not allowed opcode %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
                    break;
                case STATUS_UNHANDLED:
                    DEBUG_LOG("SESSION: received not handled opcode %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
                    break;
                default:
                    sLog.outError("SESSION: received wrong-status-req opcode %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
                    break;
            }
        }
        catch (ByteBufferException&)
        {
            sLog.outError("WorldSession::Update ByteBufferException occured while parsing a packet (opcode: %u) from client %s, accountid=%i.",
                packet->GetOpcode(), GetRemoteAddress().c_str(), GetAccountId());
            if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
            {
                DEBUG_LOG("Dumping error causing packet:");
                packet->hexlike();
            }

            if (sWorld.getConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
            {
                DETAIL_LOG("Disconnecting session [account id %u / address %s] for badly formatted packet.",
                    GetAccountId(), GetRemoteAddress().c_str());

                KickPlayer();
            }
        }

    }
}

bool WorldSession::Update()
{

    while (m_link && !m_link->IsClosed())
    {
        std::unique_ptr<WorldPacket> owned = m_mailbox->Next();
        if (!owned)
        {
            break;
        }

        if (Map* map = MapForPacket(*owned))
        {
            map->PostPacket(this, _player->GetObjectGuid(), std::move(owned));
            continue;
        }

        HandlePacket(*owned);
    }

    if (m_link && m_link->IsClosed())
    {
        m_link.reset();
    }

    time_t currTime = time(nullptr);
    if (!m_link || (ShouldLogOut(currTime) && !m_playerLoading))
    {
        LogoutPlayer(true);
    }

    if (!m_link)
    {
        return false;
    }

    return true;
}

void WorldSession::LogoutPlayer(bool Save)
{

    while (_player && _player->IsBeingTeleportedFar())
    {
        HandleMoveWorldportAckOpcode();
    }

    m_playerLogout = true;
    m_playerSave = Save;

    if (_player)
    {

        if (CinematicFlyover* flyover = _player->GetCinematicFlyover())
        {
            if (flyover->IsActive())
            {
                flyover->Stop();
            }
        }

        sLog.outChar("Account: %d (IP: %s) Logout Character:[%s] (guid: %u)", GetAccountId(), GetRemoteAddress().c_str(), _player->GetName() , _player->GetGUIDLow());

        if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
        {
            DoLootRelease(lootGuid);
        }

        if (_player->GetDeathTimer())
        {
            _player->GetHostileRefManager().deleteReferences();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        else if (!_player->getAttackers().empty())
        {
            _player->CombatStop();
            _player->GetHostileRefManager().setOnlineOfflineState(false);
            _player->RemoveAllAurasOnDeath();

            std::set<Player*> aset;
            for (Unit::AttackerSet::const_iterator itr = _player->getAttackers().begin(); itr != _player->getAttackers().end(); ++itr)
            {
                Unit* owner = (*itr)->GetOwner();
                if (owner)
                {
                    if (IsPlayer(owner))
                    {
                        aset.insert((Player*)owner);
                    }
                }
                else if (IsPlayer(*itr))
                {
                    aset.insert((Player*)(*itr));
                }
            }

            _player->SetPvPDeath(!aset.empty());
            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();

            for (std::set<Player*>::const_iterator itr = aset.begin(); itr != aset.end(); ++itr)
            {
                (*itr)->RewardHonor(_player, aset.size());
            }

            if (!aset.empty())
            {
                if (BattleGround* bg = _player->Battle().Ground())
                {
                    bg->HandleKillPlayer(_player, *aset.begin());
                }
            }
        }
        else if (_player->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {

            _player->RemoveAurasOfType(SPELL_AURA_MOD_SHAPESHIFT);

            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }

        if (BattleGround* bg = _player->Battle().Ground())
        {
            bg->EventPlayerLoggedOut(_player);
        }

        if (!_player->Binds().StillWelcome() && !_player->isGameMaster())
        {
            _player->TeleportToHomebind();

        }

        while (_player->IsBeingTeleportedFar())
        {
            HandleMoveWorldportAckOpcode();
        }

        for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            if (BattleGroundQueueTypeId bgQueueTypeId = _player->Queues().Kind(i))
            {
                _player->Queues().Give(bgQueueTypeId);
                sBattleGroundMgr.m_BattleGroundQueues[ bgQueueTypeId ].RemovePlayer(_player->GetObjectGuid(), true);
            }
        }

        static SqlStatementID id;

        SqlStatement stmt = LoginDatabase.CreateStatement(id, "UPDATE `account` SET `active_realm_id` = ? WHERE `id` = ?");
        stmt.PExecute(uint32(0), GetAccountId());

        if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
        {
            if (MemberSlot* slot = guild->GetMemberSlot(_player->GetObjectGuid()))
            {
                slot->SetMemberStats(_player);
                slot->UpdateLogoutTime();
            }

            guild->BroadcastEvent(GE_SIGNED_OFF, _player->GetObjectGuid(), _player->GetName());
        }

        _player->RemovePet(PET_SAVE_AS_CURRENT);

        if (Save)
        {
            _player->SaveToDB();
        }

        _player->CleanupChannels();

        _player->UninviteFromGroup();

        if (_player->GetGroup() && !_player->GetGroup()->isRaidGroup() && m_link)
        {
            _player->RemoveFromGroup();
        }

        if (_player->GetGroup())
        {
            _player->GetGroup()->SendUpdate();
        }

        sSocialMgr.SendFriendStatus(_player, FRIEND_OFFLINE, _player->GetObjectGuid(), true);
        sSocialMgr.RemovePlayerSocial(_player->GetGUIDLow());

        if (_player->IsInWorld())
        {
            Map* _map = _player->GetMap();
            _map->Remove(_player, true);
        }
        else
        {
            _player->CleanupsBeforeDelete();
            Map::DeleteFromWorld(_player);
        }

        ClearNpcWatchLastGuid();
        SetPlayer(nullptr);

        WorldPacket data(SMSG_LOGOUT_COMPLETE, 0);
        SendPacket(&data);

        static SqlStatementID updChars;
        stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 0 WHERE `account` = ?");
        stmt.PExecute(GetAccountId());

        DEBUG_LOG("SESSION: Sent SMSG_LOGOUT_COMPLETE Message");
    }

    m_playerLogout = false;
    m_playerSave = false;
    m_playerRecentlyLogout = true;
    LogoutRequest(0);
}

void WorldSession::KickPlayer()
{
    if (m_link)
    {
        m_link->Close();
    }
}

void protocol::Ping(WorldSession& session, WorldPacket& recvPacket)
{
    uint32 ping = 0;
    uint32 latency = 0;
    recvPacket >> ping;
    recvPacket >> latency;

    if (uint32 const fastRun = session.PingsTooFast())
    {
        sLog.outError(
            "WorldSession::HandlePingOpcode: account %u kicked for overspeeded "
            "pings (%u in a row), address = %s",
            session.GetAccountId(), fastRun, session.GetRemoteAddress().c_str());
        session.KickPlayer();
        return;
    }

    session.SetLatency(latency);
    session.SetClientTimeDelay(0);

    WorldPacket response(SMSG_PONG, 4);
    response << ping;
    session.SendPacket(&response);
}

void protocol::KeepAlive(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("CMSG_KEEP_ALIVE ,size: %zu ", recvPacket.size());
}

void WorldSession::SendAreaTriggerMessage(const char* Text, ...)
{
    va_list ap;
    char szStr [1024];
    szStr[0] = '\0';

    va_start(ap, Text);
    vsnprintf(szStr, 1024, Text, ap);
    va_end(ap);

    uint32 length = strlen(szStr) + 1;
    WorldPacket data(SMSG_AREA_TRIGGER_MESSAGE, 4 + length);
    data << length;
    data << szStr;
    SendPacket(&data);
}

void WorldSession::SendNotification(const char* format, ...)
{
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, format);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data << szStr;
        SendPacket(&data);
    }
}

void WorldSession::SendNotification(int32 string_id, ...)
{
    char const* format = GetMangosString(string_id);
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, string_id);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data << szStr;
        SendPacket(&data);
    }
}

const char* WorldSession::GetMangosString(int32 entry) const
{
    return sObjectMgr.GetMangosString(entry, GetSessionDbLocaleIndex());
}

void protocol::_NULL(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("SESSION: received unimplemented opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void protocol::_EarlyProccess(WorldSession& session, WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received opcode %s (0x%.4X) that must be processed by the protocol layer",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void protocol::_ServerSide(WorldSession& session, WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received server-side opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void protocol::_Deprecated(WorldSession& session, WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received deprecated opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void WorldSession::SendAuthWaitQue(uint32 position)
{
    if (position == 0)
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_OK);
        SendPacket(&packet);
    }
    else
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4);
        packet << uint8(AUTH_WAIT_QUEUE);
        packet << uint32(position);
        SendPacket(&packet);
    }
}

void WorldSession::LoadTutorialsData()
{
    for (int aX = 0 ; aX < 8 ; ++aX)
    {
        m_Tutorials[ aX ] = 0;
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7` FROM `character_tutorial` WHERE `account` = '%u'", GetAccountId());

    if (!result)
    {
        m_tutorialState = TUTORIALDATA_NEW;
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        for (int iI = 0; iI < 8; ++iI)
        {
            m_Tutorials[iI] = fields[iI].GetUInt32();
        }
    }
    while (result->NextRow());

    delete result;

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

void WorldSession::SendTutorialsData()
{
    WorldPacket data(SMSG_TUTORIAL_FLAGS, 4 * 8);
    for (uint32 i = 0; i < 8; ++i)
    {
        data << m_Tutorials[i];
    }
    SendPacket(&data);
}

void WorldSession::SaveTutorialsData()
{
    static SqlStatementID updTutorial ;
    static SqlStatementID insTutorial ;

    switch (m_tutorialState)
    {
        case TUTORIALDATA_CHANGED:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updTutorial, "UPDATE `character_tutorial` SET `tut0`=?, `tut1`=?, `tut2`=?, `tut3`=?, `tut4`=?, `tut5`=?, `tut6`=?, `tut7`=? WHERE `account` = ?");
            for (int i = 0; i < 8; ++i)
            {
                stmt.addUInt32(m_Tutorials[i]);
            }

            stmt.addUInt32(GetAccountId());
            stmt.Execute();
        }
        break;

        case TUTORIALDATA_NEW:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(insTutorial, "INSERT INTO `character_tutorial` (`account`,`tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

            stmt.addUInt32(GetAccountId());
            for (int i = 0; i < 8; ++i)
            {
                stmt.addUInt32(m_Tutorials[i]);
            }

            stmt.Execute();
        }
        break;
        case TUTORIALDATA_UNCHANGED:
            break;
    }

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

uint32 WorldSession::PingsTooFast()
{
    uint32 const fastRun = m_pingTracker.Record(SessionPingTracker::Clock::now());

    if (!m_pingTracker.ShouldKick(sWorld.getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS),
                                  GetSecurity() == SEC_PLAYER))
    {
        return 0;
    }

    return fastRun;
}

void WorldSession::SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg)
{
    WorldPacket data(SMSG_TRANSFER_ABORTED, 1);
    data << uint8(reason);
    SendPacket(&data);
}

void WorldSession::ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket* packet)
{

    if (_player)
    {
        _player->SetCanDelayTeleport(true);
    }

    if (opHandle.answer)
    {
        opHandle.answer(*this, *packet);
    }
    else
    {
        (this->*opHandle.handler)(*packet);
    }

    if (_player)
    {

        _player->SetCanDelayTeleport(false);

        if (_player->IsHasDelayedTeleport())
        {
            _player->TeleportTo(_player->GetTeleportDest(), _player->GetTeleportOptions());
        }
    }

    if (packet->rpos() < packet->wpos() && sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
    {
        LogUnprocessedTail(packet);
    }
}

void WorldSession::SendPlaySpellVisual(ObjectGuid guid, uint32 spellArtKit)
{
    WorldPacket data(SMSG_PLAY_SPELL_VISUAL, 8 + 4);
    data << guid;
    data << spellArtKit;
    SendPacket(&data);
}
