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

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <cstring>
#include <string>
#include <ctime>
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ChatAnswers.h"
#include "World.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "ChannelMgr.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Language.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

bool WorldSession::processChatmessageFurtherAfterSecurityChecks(std::string& msg, uint32 lang)
{
    if (lang != LANG_ADDON)
    {

        if (msg.size() > 255)
        {
            sLog.outError("Player %s (GUID: %u) tries send a chatmessage with more than 255 symbols", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
            return false;
        }

        if (sWorld.getConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
        {
            stripLineInvisibleChars(msg);
        }

        if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) && GetSecurity() < SEC_MODERATOR &&
            !ChatHandler(this).isValidChatMessage(msg.c_str()))
        {
            sLog.outError("Player %s (GUID: %u) sent a chatmessage with an invalid link: %s", GetPlayer()->GetName(),
                GetPlayer()->GetGUIDLow(), msg.c_str());
            if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
            {
                KickPlayer();
            }
            return false;
        }
    }

    return true;
}

void chat::Messagechat(WorldSession& session, WorldPacket& recv_data)
{
    uint32 type;
    uint32 lang;

    recv_data >> type;
    recv_data >> lang;

    if (type >= MAX_CHAT_MSG_TYPE)
    {
        sLog.outError("CHAT: Wrong message type received: %u", type);
        return;
    }

    DEBUG_LOG("CHAT: packet received. type %u, lang %u", type, lang);

    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if (!langDesc)
    {
        session.SendNotification(LANG_UNKNOWN_LANGUAGE);
        return;
    }

    if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
    {

        if ((langDesc->lang_id == LANG_UNIVERSAL &&
            !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) &&
            session.GetSecurity() == SEC_PLAYER) ||
            (langDesc->skill_id != 0 && !session.GetPlayer()->HasSkill(langDesc->skill_id)))
        {
            session.SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            return;
        }
    }

    if (lang == LANG_ADDON)
    {

        if (!sWorld.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
        {
            return;
        }
    }

    else
    {

        if (session.GetPlayer()->isGameMaster())
        {
            lang = LANG_UNIVERSAL;
        }
        else
        {

            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT))
            {
                lang = LANG_UNIVERSAL;
            }
            else
            {
                switch (type)
                {
                    case CHAT_MSG_PARTY:
                    case CHAT_MSG_RAID:
                    case CHAT_MSG_RAID_LEADER:
                    case CHAT_MSG_RAID_WARNING:

                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                        {
                            lang = LANG_UNIVERSAL;
                        }
                        break;
                    case CHAT_MSG_GUILD:
                    case CHAT_MSG_OFFICER:

                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                        {
                            lang = LANG_UNIVERSAL;
                        }
                        break;
                }
            }

            const auto ModLangAuras = session.GetPlayer()->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
            if (!ModLangAuras.empty())
            {
                lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
            }
        }

        if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
        {
            if (!session.GetPlayer()->CanSpeak())
            {
                std::string timeStr = secsToTimeString(session.m_muteTime - time(nullptr));
                session.SendNotification(session.GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
                return;
            }

            session.GetPlayer()->UpdateSpeakTime();
        }
    }

    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (type == CHAT_MSG_SAY)
            {
                session.GetPlayer()->Say(msg, lang);
            }
            else if (type == CHAT_MSG_EMOTE)
            {
                session.GetPlayer()->TextEmote(msg);
            }
            else if (type == CHAT_MSG_YELL)
            {
                session.GetPlayer()->Yell(msg, lang);
            }
        } break;

        case CHAT_MSG_WHISPER:
        {
            std::string to, msg;
            recv_data >> to;
            recv_data >> msg;

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (!normalizePlayerName(to))
            {
                session.SendPlayerNotFoundNotice(to);
                {
                    break;
                }
            }

            Player* player = sObjectMgr.GetPlayer(to.c_str());
            uint32 tSecurity = session.GetSecurity();
            uint32 pSecurity = player ? player->GetSession()->GetSecurity() : SEC_PLAYER;
            if (!player || (tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER && !player->isAcceptWhispers()))
            {
                session.SendPlayerNotFoundNotice(to);
                return;
            }

            if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER)
            {
                if (session.GetPlayer()->GetTeam() != player->GetTeam())
                {
                    session.SendWrongFactionNotice();
                    return;
                }
            }

            session.GetPlayer()->Whisper(msg, lang, player->GetObjectGuid());
        } break;

        case CHAT_MSG_PARTY:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = session.GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = session.GetPlayer()->GetGroup();
                if (!group || group->isBGGroup())
                {
                    return;
                }
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false, group->GetMemberGroup(session.GetPlayer()->GetObjectGuid()));

            break;
        }
        case CHAT_MSG_GUILD:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (session.GetPlayer()->GetGuildId())
            {
                if (Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId()))
                {

                    guild->BroadcastToGuild(&session, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }

            }
            break;
        }
        case CHAT_MSG_OFFICER:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (session.GetPlayer()->GetGuildId())
            {
                if (Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId()))
                {

                    guild->BroadcastToOfficers(&session, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }
            }
            break;
        }
        case CHAT_MSG_RAID:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = session.GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = session.GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup())
                {
                    return;
                }
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;
        case CHAT_MSG_RAID_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = session.GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = session.GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup() || !group->IsLeader(session.GetPlayer()->GetObjectGuid()))
                {
                    return;
                }
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_LEADER, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_RAID_WARNING:
        {
            std::string msg;
            recv_data >> msg;

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = session.GetPlayer()->GetGroup();
            if (!group || !group->isRaidGroup() ||
                !(group->IsLeader(session.GetPlayer()->GetObjectGuid()) || group->IsAssistant(session.GetPlayer()->GetObjectGuid())))
            {
                return;
            }

            WorldPacket data;

            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_WARNING, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND:
        {
            std::string msg;
            recv_data >> msg;

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = session.GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup())
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = session.GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup() || !group->IsLeader(session.GetPlayer()->GetObjectGuid()))
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND_LEADER, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_CHANNEL:
        {
            std::string channel, msg;
            recv_data >> channel;
            recv_data >> msg;

            if (!session.processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
            {
                if (Channel* chn = cMgr->GetChannel(channel, session.GetPlayer()))
                {
                    chn->Say(session.GetPlayer(), msg.c_str(), lang);
                }
            }
        } break;

        case CHAT_MSG_AFK:
        {
            std::string msg;
            recv_data >> msg;

            if (!session.GetPlayer()->IsInCombat())
            {
                if (session.GetPlayer()->isAFK())
                {
                    if (msg.empty())
                    {
                        session.GetPlayer()->ToggleAFK();
                    }
                    else
                    {
                        session.GetPlayer()->autoReplyMsg = msg;
                    }
                }
                else
                {
                    session.GetPlayer()->autoReplyMsg = msg.empty() ? session.GetMangosString(LANG_PLAYER_AFK_DEFAULT) : msg;

                    if (session.GetPlayer()->isDND())
                    {
                        session.GetPlayer()->ToggleDND();
                    }

                    session.GetPlayer()->ToggleAFK();
                }
            }
            break;
        }
        case CHAT_MSG_DND:
        {
            std::string msg;
            recv_data >> msg;

            if (session.GetPlayer()->isDND())
            {
                if (msg.empty())
                {
                    session.GetPlayer()->ToggleDND();
                }
                else
                {
                    session.GetPlayer()->autoReplyMsg = msg;
                }
            }
            else
            {
                session.GetPlayer()->autoReplyMsg = msg.empty() ? session.GetMangosString(LANG_PLAYER_DND_DEFAULT) : msg;

                if (session.GetPlayer()->isAFK())
                {
                    session.GetPlayer()->ToggleAFK();
                }

                session.GetPlayer()->ToggleDND();
            }

            break;
        }

        default:
            sLog.outError("CHAT: unknown message type %u, lang: %u", type, lang);
            break;
    }
}

void chat::Emote(Player& who, WorldPacket& recv_data)
{
    if (!who.IsAlive() || who.hasUnitState(UNIT_STAT_DIED))
    {
        return;
    }

    uint32 emote;
    recv_data >> emote;

    who.HandleEmoteCommand(emote);
}

namespace MaNGOS
{
    class EmoteChatBuilder
    {
        public:
            EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num, Unit const* target)
                : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* nam = i_target ? i_target->GetNameForLocaleIdx(loc_idx) : nullptr;
                uint32 namlen = (nam ? strlen(nam) : 0) + 1;

                data.Initialize(SMSG_TEXT_EMOTE, (20 + namlen));
                data << static_cast<ObjectGuid>(i_player.GetObjectGuid());
                data << uint32(i_text_emote);
                data << uint32(i_emote_num);
                data << uint32(namlen);
                if (namlen > 1)
                {
                    data.append(nam, namlen);
                }
                else
                {
                    data << uint8(0x00);
                }
            }

        private:
            Player const& i_player;
            uint32        i_text_emote;
            uint32        i_emote_num;
            Unit const*   i_target;
    };
}

void chat::TextEmote(WorldSession& session, WorldPacket& recv_data)
{
    if (!session.GetPlayer()->IsAlive())
    {
        return;
    }

    if (!session.GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(session.m_muteTime - time(nullptr));
        session.SendNotification(session.GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    uint32 text_emote, emoteNum;
    ObjectGuid guid = 0;

    recv_data >> text_emote;
    recv_data >> emoteNum;
    recv_data >> guid;

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
    {
        return;
    }

    uint32 emote_id = em->EmoteID;

    switch (emote_id)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        default:
        {

            if (session.GetPlayer()->hasUnitState(UNIT_STAT_DIED))
            {
                break;
            }

            session.GetPlayer()->HandleEmoteCommand(emote_id);
            break;
        }
    }

    Unit* unit = session.GetPlayer()->GetMap()->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(*session.GetPlayer(), text_emote, emoteNum, unit);
    MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > emote_do(emote_builder);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > > emote_worker(session.GetPlayer(), sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitWorldObjects(session.GetPlayer(), emote_worker,  sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));

    if (unit &&IsCreature(unit) && ((Creature*)unit)->AI())
    {
        ((Creature*)unit)->AI()->ReceiveEmote(session.GetPlayer(), text_emote);
    }
}

void chat::ChatIgnored(Player& who, WorldPacket& recv_data)
{
    ObjectGuid iguid = 0;

    recv_data >> iguid;

    Player* player = sObjectMgr.GetPlayer(iguid);
    if (!player || !player->GetSession())
    {
        return;
    }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_IGNORED, who.GetName(), LANG_UNIVERSAL, CHAT_TAG_NONE, who.GetObjectGuid());
    player->GetSession()->SendPacket(&data);
}

void WorldSession::SendPlayerNotFoundNotice(const std::string &name)
{
    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, name.size() + 1);
    data << name;
    SendPacket(&data);
}

void WorldSession::SendWrongFactionNotice()
{
    WorldPacket data(SMSG_CHAT_WRONG_FACTION, 0);
    SendPacket(&data);
}

void WorldSession::SendChatRestrictedNotice()
{
    WorldPacket data(SMSG_CHAT_RESTRICTED, 0);
    SendPacket(&data);
}
