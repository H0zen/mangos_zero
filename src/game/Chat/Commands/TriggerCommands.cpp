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

#include "Chat.h"
#include "ObjectMgr.h"

void ChatHandler::ShowTriggerTargetListHelper(uint32 id, AreaTrigger const* at, bool subpart )
{
    if (m_session)
    {
        char dist_buf[50];
        if (!subpart)
        {
            float dist = m_session->GetPlayer()->Where().DistanceTo(Geometry::Vector2(at->target_X, at->target_Y));
            snprintf(dist_buf, 50, GetMangosString(LANG_TRIGGER_DIST), dist);
        }
        else
        {
            dist_buf[0] = '\0';
        }

        PSendSysMessage(LANG_TRIGGER_TARGET_LIST_CHAT,
            subpart ? " -> " : "", id, id, at->target_mapId, at->target_X, at->target_Y, at->target_Z, dist_buf);
    }
    else
    {
        PSendSysMessage(LANG_TRIGGER_TARGET_LIST_CONSOLE,
            subpart ? " -> " : "", id, at->target_mapId, at->target_X, at->target_Y, at->target_Z);
    }
}

void ChatHandler::ShowTriggerListHelper(AreaTriggerEntry const* atEntry)
{
    char const* tavern = sObjectMgr.IsTavernAreaTrigger(atEntry->ID) ? GetMangosString(LANG_TRIGGER_TAVERN) : "";
    char const* quest = sObjectMgr.GetQuestForAreaTrigger(atEntry->ID) ? GetMangosString(LANG_TRIGGER_QUEST) : "";

    if (m_session)
    {
        float dist = m_session->GetPlayer()->Where().DistanceTo(Geometry::Vector2(atEntry->x, atEntry->y));
        char dist_buf[50];
        snprintf(dist_buf, 50, GetMangosString(LANG_TRIGGER_DIST), dist);

        PSendSysMessage(LANG_TRIGGER_LIST_CHAT,
            atEntry->ID, atEntry->ID, atEntry->mapid, atEntry->x, atEntry->y, atEntry->z, dist_buf, tavern, quest);
    }
    else
    {
        PSendSysMessage(LANG_TRIGGER_LIST_CONSOLE,
            atEntry->ID, atEntry->mapid, atEntry->x, atEntry->y, atEntry->z, tavern, quest);
    }

    if (AreaTrigger const* at = sObjectMgr.GetAreaTrigger(atEntry->ID))
    {
        ShowTriggerTargetListHelper(atEntry->ID, at, true);
    }
}

bool ChatHandler::HandleTriggerCommand(char* args)
{
    AreaTriggerEntry const* atEntry = nullptr;

    Player* pl = m_session ? m_session->GetPlayer() : nullptr;

    if (*args)
    {
        uint32 atId;
        if (!ExtractUint32KeyFromLink(&args, "Hareatrigger", atId))
        {
            return false;
        }

        if (!atId)
        {
            return false;
        }

        atEntry = sAreaTriggerStore.LookupEntry(atId);

        if (!atEntry)
        {
            PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, atId);
            SetSentErrorMessage(true);
            return false;
        }
    }

    else
    {
        if (!m_session)
        {
            return false;
        }

        float dist2 = MAP_SIZE * MAP_SIZE;

        for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
        {
            AreaTriggerEntry const* atTestEntry = sAreaTriggerStore.LookupEntry(id);
            if (!atTestEntry)
            {
                continue;
            }

            if (atTestEntry->mapid != m_session->GetPlayer()->GetMapId())
            {
                continue;
            }

            float dx = atTestEntry->x - pl->Where().X();
            float dy = atTestEntry->y - pl->Where().Y();

            float test_dist2 = dx * dx + dy * dy;

            if (test_dist2 >= dist2)
            {
                continue;
            }

            dist2 = test_dist2;
            atEntry = atTestEntry;
        }

        if (!atEntry)
        {
            SendSysMessage(LANG_COMMAND_NOTRIGGERFOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }

    ShowTriggerListHelper(atEntry);

    int loc_idx = GetSessionDbLocaleIndex();

    AreaTrigger const* at = sObjectMgr.GetAreaTrigger(atEntry->ID);
    if (at)
    {
        PSendSysMessage(LANG_TRIGGER_CONDITION, at->condition);
    }

    if (uint32 quest_id = sObjectMgr.GetQuestForAreaTrigger(atEntry->ID))
    {
        SendSysMessage(LANG_TRIGGER_EXPLORE_QUEST);
        ShowQuestListHelper(quest_id, loc_idx, pl);
    }

    return true;
}

bool ChatHandler::HandleTriggerActiveCommand(char* )
{
    uint32 counter = 0;

    Player* pl = m_session->GetPlayer();

    for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
    {
        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
        if (!atEntry)
        {
            continue;
        }

        if (!IsPointInAreaTriggerZone(atEntry, pl->GetMapId(), pl->Where().X(), pl->Where().Y(), pl->Where().Z()))
        {
            continue;
        }

        ShowTriggerListHelper(atEntry);

        ++counter;
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_COMMAND_NOTRIGGERFOUND);
    }

    return true;
}

bool ChatHandler::HandleTriggerNearCommand(char* args)
{
    float distance = (!*args) ? 10.0f : (float)atof(args);
    float dist2 = distance * distance;
    uint32 counter = 0;

    Player* pl = m_session->GetPlayer();

    for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
    {
        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
        if (!atEntry)
        {
            continue;
        }

        if (atEntry->mapid != m_session->GetPlayer()->GetMapId())
        {
            continue;
        }

        float dx = atEntry->x - pl->Where().X();
        float dy = atEntry->y - pl->Where().Y();

        if (dx * dx + dy * dy > dist2)
        {
            continue;
        }

        ShowTriggerListHelper(atEntry);

        ++counter;
    }

    for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
    {
        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
        if (!atEntry)
        {
            continue;
        }

        AreaTrigger const* at = sObjectMgr.GetAreaTrigger(atEntry->ID);
        if (!at)
        {
            continue;
        }

        if (at->target_mapId != m_session->GetPlayer()->GetMapId())
        {
            continue;
        }

        float dx = at->target_X - pl->Where().X();
        float dy = at->target_Y - pl->Where().Y();

        if (dx * dx + dy * dy > dist2)
        {
            continue;
        }

        ShowTriggerTargetListHelper(atEntry->ID, at);

        ++counter;
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_COMMAND_NOTRIGGERFOUND);
    }

    return true;
}
