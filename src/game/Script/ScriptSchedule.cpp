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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "ScriptSchedule.h"

#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "Object.h"
#include "World.h"

namespace
{
    /// The three guids a step is filed under: who started it, at whom, and whose
    /// item it was if an item started it.
    struct Actors
    {
        ObjectGuid source;
        ObjectGuid target;
        ObjectGuid owner;
    };

    Actors ActorsOf(Object* source, Object* target)
    {
        Actors who;
        who.source = source->GetObjectGuid();
        who.target = target ? target->GetObjectGuid() : ObjectGuid();
        who.owner = source->isType(TYPEMASK_ITEM) ? ((Item*)source)->GetOwnerGuid() : ObjectGuid();
        return who;
    }
}

ScriptSchedule::~ScriptSchedule()
{
    if (!m_due.empty())
    {
        sScriptMgr.DecreaseScheduledScriptCount(m_due.size());
    }
}

bool ScriptSchedule::Start(DBScriptType type, uint32 id, Object* source, Object* target,
                           ScriptExecutionParam execParams)
{
    MANGOS_ASSERT(source);

    ScriptChainMap const* scm = sScriptMgr.GetScriptChainMap(type);
    if (!scm)
    {
        return false;
    }

    ScriptChainMap::const_iterator s = scm->find(id);
    if (s == scm->end())
    {
        return false;
    }

    Actors const who = ActorsOf(source, target);

    if (execParams)                                         // Check if the execution should be uniquely
    {
        for (Queue::const_iterator searchItr = m_due.begin(); searchItr != m_due.end(); ++searchItr)
        {
            if (searchItr->second.IsSameScript(type, id,
                (execParams & SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE) ? who.source : ObjectGuid(),
                (execParams & SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET) ? who.target : ObjectGuid(), who.owner))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_DB_SCRIPTS, "DB-SCRIPTS: Process table `dbscripts [type=%d]` id %u. Skip script as script already started for source %s, target %s - ScriptsStartParams %u", type, id, who.source.GetString().c_str(), who.target.GetString().c_str(), execParams);
                return true;
            }
        }
    }

    ///- Schedule script execution for all scripts in the script map
    ScriptChain const* chain = &(s->second);
    for (ScriptChain::const_iterator iter = chain->begin(); iter != chain->end(); ++iter)
    {
        ScriptAction sa(type, &m_on, who.source, who.target, who.owner, &(*iter));

        m_due.insert(Queue::value_type(time_t(sWorld.GetGameTime() + iter->delay), sa));

        sScriptMgr.IncreaseScheduledScriptsCount();
    }

    return true;
}

void ScriptSchedule::StartCommand(ScriptInfo const& script, uint32 delay, Object* source, Object* target)
{
    // NOTE: script record _must_ exist until command executed

    Actors const who = ActorsOf(source, target);

    ScriptAction sa(DBS_INTERNAL, &m_on, who.source, who.target, who.owner, &script);

    m_due.insert(Queue::value_type(time_t(sWorld.GetGameTime() + delay), sa));

    sScriptMgr.IncreaseScheduledScriptsCount();
}

void ScriptSchedule::RunDue()
{
    if (m_due.empty())
    {
        return;
    }

    ///- Process overdue queued scripts
    Queue::iterator iter = m_due.begin();
    // ok as multimap is a *sorted* associative container
    while (!m_due.empty() && (iter->first <= sWorld.GetGameTime()))
    {
        if (iter->second.HandleScriptStep())
        {
            // Terminate following script steps of this script
            DBScriptType type = iter->second.GetType();
            uint32 id = iter->second.GetId();
            ObjectGuid sourceGuid = iter->second.GetSourceGuid();
            ObjectGuid targetGuid = iter->second.GetTargetGuid();
            ObjectGuid ownerGuid = iter->second.GetOwnerGuid();

            for (Queue::iterator rmItr = m_due.begin(); rmItr != m_due.end();)
            {
                if (rmItr->second.IsSameScript(type, id, sourceGuid, targetGuid, ownerGuid))
                {
                    m_due.erase(rmItr++);
                    sScriptMgr.DecreaseScheduledScriptCount();
                }
                else
                {
                    ++rmItr;
                }
            }
        }
        else
        {
            m_due.erase(iter);

            sScriptMgr.DecreaseScheduledScriptCount();
        }
        iter = m_due.begin();
    }
}
