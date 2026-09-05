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
    /// Where an utterance of this kind goes.
    enum class Audience
    {
        Around,     ///< everyone within Form::range
        Listener,   ///< the target alone
        Zone,       ///< everyone in the speaker's zone
    };

    struct Form
    {
        ChatMsg  message;
        Audience audience;
        uint32   range;     ///< world config key; unused unless Audience::Around
    };

    Form FormOf(ChatType kind)
    {
        switch (kind)
        {
            case CHAT_TYPE_YELL:
                return { CHAT_MSG_MONSTER_YELL, Audience::Around, CONFIG_FLOAT_LISTEN_RANGE_YELL };
            case CHAT_TYPE_TEXT_EMOTE:
                return { CHAT_MSG_MONSTER_EMOTE, Audience::Around, CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE };
            case CHAT_TYPE_BOSS_EMOTE:
                return { CHAT_MSG_RAID_BOSS_EMOTE, Audience::Around, CONFIG_FLOAT_LISTEN_RANGE_YELL };
            case CHAT_TYPE_WHISPER:
                return { CHAT_MSG_MONSTER_WHISPER, Audience::Listener, 0 };
            case CHAT_TYPE_BOSS_WHISPER:
                return { CHAT_MSG_RAID_BOSS_WHISPER, Audience::Listener, 0 };
            case CHAT_TYPE_ZONE_YELL:
                return { CHAT_MSG_MONSTER_YELL, Audience::Zone, 0 };
            case CHAT_TYPE_SAY:
            default:
                return { CHAT_MSG_MONSTER_SAY, Audience::Around, CONFIG_FLOAT_LISTEN_RANGE_SAY };
        }
    }

    /// Everyone in the speaker's zone, whatever map they are standing on.
    template<class Say>
    void ToZone(Occupant const& speaker, Say& say)
    {
        uint32 const zone = speaker.GetTerrain()->GetZoneId(speaker.Where().X(), speaker.Where().Y(),
                                                            speaker.Where().Z());

        Map::PlayerList const& players = speaker.GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* listener = itr->getSource();
            if (listener->GetTerrain()->GetZoneId(listener->Where().X(), listener->Where().Y(),
                                                  listener->Where().Z()) == zone)
            {
                say(listener);
            }
        }
    }
}

void Utter(Occupant const& speaker, ChatType kind, char const* text, Unit const* target, Language language)
{
    Form const form = FormOf(kind);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, form.message, text, language, CHAT_TAG_NONE,
                                 speaker.GetObjectGuid(), speaker.GetName(),
                                 target ? target->GetObjectGuid() : ObjectGuid(),
                                 target ? target->GetName() : "");

    switch (form.audience)
    {
        case Audience::Around:
            BroadcastWithin(speaker, &data, sWorld.getConfig(eConfigFloatValues(form.range)), true);
            break;

        case Audience::Listener:
            if (Player const* listener = ToPlayer(target))
            {
                listener->GetSession()->SendPacket(&data);
            }
            break;

        case Audience::Zone:
        {
            auto say = [&data](Player* listener) { listener->GetSession()->SendPacket(&data); };
            ToZone(speaker, say);
            break;
        }
    }
}

namespace MaNGOS
{
    /// Builds one chat packet per locale, so a line reaches each listener in
    /// the language their client asked for.
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
                                             i_target ? i_target->GetObjectGuid() : ObjectGuid(),
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

    // Only a spoken line carries a language; an emote and a whisper are read
    // whatever the listener speaks.
    Language const language = (form.message == CHAT_MSG_MONSTER_SAY || form.message == CHAT_MSG_MONSTER_YELL)
                            ? Language(line->LanguageId) : LANG_UNIVERSAL;

    MaNGOS::MonsterChatBuilder build(speaker, form.message, line, language, target);
    MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say(build);

    switch (form.audience)
    {
        case Audience::Around:
        {
            float const range = sWorld.getConfig(eConfigFloatValues(form.range));
            MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> > worker(&speaker, range, say);
            Cell::VisitWorldObjects(&speaker, worker, range);
            break;
        }

        case Audience::Listener:
            if (Player* listener = const_cast<Player*>(ToPlayer(target)))
            {
                say(listener);
            }
            break;

        case Audience::Zone:
            ToZone(speaker, say);
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
        Broadcast(source, &data, true);
    }
}

void SendDespawnAnimation(Occupant const& what)
{
    WorldPacket data(SMSG_GAMEOBJECT_DESPAWN_ANIM, 8);
    data << what.GetObjectGuid();
    Broadcast(what, &data, true);
}
