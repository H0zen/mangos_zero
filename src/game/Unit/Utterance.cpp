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

#include "Utterance.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "Occupant.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Unit.h"
#include "World.h"
#include "WorldPacket.h"

namespace
{

    enum class Earshot
    {
        Around,
        Listener,
        Zone,
    };

    struct Form
    {
        ChatMsg  message;
        Earshot  earshot;
        uint32   range;
    };

    Form FormOf(ChatType kind)
    {
        switch (kind)
        {
            case CHAT_TYPE_YELL:
                return { CHAT_MSG_MONSTER_YELL, Earshot::Around, CONFIG_FLOAT_LISTEN_RANGE_YELL };
            case CHAT_TYPE_TEXT_EMOTE:
                return { CHAT_MSG_MONSTER_EMOTE, Earshot::Around, CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE };
            case CHAT_TYPE_BOSS_EMOTE:
                return { CHAT_MSG_RAID_BOSS_EMOTE, Earshot::Around, CONFIG_FLOAT_LISTEN_RANGE_YELL };
            case CHAT_TYPE_WHISPER:
                return { CHAT_MSG_MONSTER_WHISPER, Earshot::Listener, 0 };
            case CHAT_TYPE_BOSS_WHISPER:
                return { CHAT_MSG_RAID_BOSS_WHISPER, Earshot::Listener, 0 };
            case CHAT_TYPE_ZONE_YELL:
                return { CHAT_MSG_MONSTER_YELL, Earshot::Zone, 0 };
            case CHAT_TYPE_SAY:
            default:
                return { CHAT_MSG_MONSTER_SAY, Earshot::Around, CONFIG_FLOAT_LISTEN_RANGE_SAY };
        }
    }

    uint32 ZoneOf(Occupant const& speaker)
    {
        return speaker.GetTerrain()->GetZoneId(speaker.Where().X(), speaker.Where().Y(),
                                               speaker.Where().Z());
    }
}

void Utter(Occupant const& speaker, ChatType kind, char const* text, Unit const* target, Language language)
{
    Form const form = FormOf(kind);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, form.message, text, language, CHAT_TAG_NONE,
                                 speaker.GetObjectGuid(), speaker.GetName(),
                                 target ? target->GetObjectGuid() : 0,
                                 target ? target->GetName() : "");

    switch (form.earshot)
    {
        case Earshot::Around:
            Deliver(Audience::Within(speaker, sWorld.getConfig(eConfigFloatValues(form.range))).AndSubject(),
                    &data);
            break;

        case Earshot::Listener:
            if (Player const* listener = static_cast<Player const*>(target))
            {
                listener->GetSession()->SendPacket(&data);
            }
            break;

        case Earshot::Zone:
            Deliver(Audience::InZone(*speaker.GetMap(), ZoneOf(speaker)), &data);
            break;
    }
}

namespace MaNGOS
{

    class MonsterChatBuilder
    {
        public:
            MonsterChatBuilder(Occupant const& obj, ChatMsg msgtype, MangosStringLocale const* textData,
                               Language language, Unit const* target)
                : i_object(obj), i_msgtype(msgtype), i_textData(textData), i_language(language), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = nullptr;
                if (int32(i_textData->Content.size()) > loc_idx + 1 && !i_textData->Content[loc_idx + 1].empty())
                {
                    text = i_textData->Content[loc_idx + 1].c_str();
                }
                else
                {
                    text = i_textData->Content[0].c_str();
                }

                ChatHandler::BuildChatPacket(data, i_msgtype, text, i_language, CHAT_TAG_NONE,
                                             i_object.GetObjectGuid(), i_object.GetNameForLocaleIdx(loc_idx),
                                             i_target ? i_target->GetObjectGuid() : 0,
                                             i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
            }

        private:
            Occupant const& i_object;
            ChatMsg i_msgtype;
            MangosStringLocale const* i_textData;
            Language i_language;
            Unit const* i_target;
    };
}

void Utter(Occupant const& speaker, MangosStringLocale const* line, Unit const* target)
{
    MANGOS_ASSERT(line);

    Form const form = FormOf(ChatType(line->Type));

    Language const language = (form.message == CHAT_MSG_MONSTER_SAY || form.message == CHAT_MSG_MONSTER_YELL)
                            ? Language(line->LanguageId) : LANG_UNIVERSAL;

    MaNGOS::MonsterChatBuilder build(speaker, form.message, line, language, target);
    MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say(build);

    switch (form.earshot)
    {
        case Earshot::Around:
            Deliver(Audience::Within(speaker, sWorld.getConfig(eConfigFloatValues(form.range))).AndSubject(),
                    say);
            break;

        case Earshot::Listener:
            if (Player* listener = const_cast<Player*>(static_cast<Player const*>(target)))
            {
                say(listener);
            }
            break;

        case Earshot::Zone:
            Deliver(Audience::InZone(*speaker.GetMap(), ZoneOf(speaker)), say);
            break;
    }
}

void PlaySound(Occupant const& source, SoundKind kind, uint32 soundId, Player const* target)
{
    WorldPacket data(kind == SoundKind::AtObject ? SMSG_PLAY_OBJECT_SOUND
                   : kind == SoundKind::Music    ? SMSG_PLAY_MUSIC
                                                 : SMSG_PLAY_SOUND,
                     kind == SoundKind::AtObject ? 4 + 8 : 4);
    data << uint32(soundId);

    if (kind == SoundKind::AtObject)
    {
        data << source.GetObjectGuid();
    }

    if (target)
    {
        target->SendDirectMessage(&data);
    }
    else
    {
        Deliver(Audience::Around(source).AndSubject(), &data);
    }
}

void SendDespawnAnimation(Occupant const& what)
{
    WorldPacket data(SMSG_GAMEOBJECT_DESPAWN_ANIM, 8);
    data << what.GetObjectGuid();
    Deliver(Audience::Around(what).AndSubject(), &data);
}

namespace
{

    class StaticMonsterChatBuilder
    {
        public:
            StaticMonsterChatBuilder(CreatureInfo const* cInfo, ChatMsg msgtype, int32 textId,
                                     Language language, Unit const* target, uint32 senderLowGuid = 0)
                : i_cInfo(cInfo), i_msgtype(msgtype), i_textId(textId), i_language(language), i_target(target)
            {
                i_senderGuid = i_cInfo->GetObjectGuid(senderLowGuid);
            }

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                char const* nameForLocale = i_cInfo->Name;
                sObjectMgr.GetCreatureLocaleStrings(i_cInfo->Entry, loc_idx, &nameForLocale);

                ChatHandler::BuildChatPacket(data, i_msgtype, text, i_language, CHAT_TAG_NONE,
                                             i_senderGuid, nameForLocale,
                                             i_target ? i_target->GetObjectGuid() : 0,
                                             i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
            }

        private:
            ObjectGuid i_senderGuid = 0;
            CreatureInfo const* i_cInfo;
            ChatMsg i_msgtype;
            int32 i_textId;
            Language i_language;
            Unit const* i_target;
    };
}

void YellToMap(Map& map, CreatureInfo const* speaker, int32 textId, Language language,
               Unit const* target, uint32 senderLowGuid)
{
    StaticMonsterChatBuilder build(speaker, CHAT_MSG_MONSTER_YELL, textId, language, target, senderLowGuid);
    MaNGOS::LocalizedPacketDo<StaticMonsterChatBuilder> say(build);

    Deliver(Audience::Everyone(map), say);
}

void YellToMap(Map& map, ObjectGuid speaker, int32 textId, Language language, Unit const* target)
{
    if (!(GuidHigh(speaker) == HIGHGUID_UNIT || GuidHigh(speaker) == HIGHGUID_PET))
    {
        sLog.outError("YellToMap: %s is not a creature.", GuidString(speaker).c_str());
        return;
    }

    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(GuidEntry(speaker));
    if (!cInfo)
    {
        sLog.outError("YellToMap: no creature template for %s", GuidString(speaker).c_str());
        return;
    }

    YellToMap(map, cInfo, textId, language, target, GuidCounter(speaker));
}

void PlaySoundToMap(Map& map, uint32 soundId, uint32 zoneId)
{
    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << uint32(soundId);

    Deliver(zoneId ? Audience::InZone(map, zoneId) : Audience::Everyone(map), &data);
}
