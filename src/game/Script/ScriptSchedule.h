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

#pragma once

#include "Platform/Define.h"
#include "ScriptMgr.h"

#include <ctime>
#include <map>

class Map;
class Object;

/**
 * @brief Whether a chain already running may be started again.
 */
enum ScriptExecutionParam
{
    SCRIPT_EXEC_PARAM_NONE                    = 0x00,   // Start regardless if already started
    SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE        = 0x01,   // Start Script only if not yet started (uniqueness identified by id and source)
    SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET        = 0x02,   // Start Script only if not yet started (uniqueness identified by id and target)
    SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE_TARGET = 0x03,   // Start Script only if not yet started (uniqueness identified by id, source and target)
};

/**
 * @brief The script steps a map has queued, and the hour each falls due.
 *
 * A timed queue, sorted by the hour. It is kept per map because a step names
 * objects that live on one and is handed that map to run against -- not because
 * scheduling is any part of what a map is. A map is a region of space, what is
 * in it, and the tick that advances them; this is a tenant.
 */
class ScriptSchedule
{
    public:
        explicit ScriptSchedule(Map& on) : m_on(on) {}
        ~ScriptSchedule();

        ScriptSchedule(ScriptSchedule const&) = delete;
        ScriptSchedule& operator=(ScriptSchedule const&) = delete;

        /**
         * @brief Queues every step of a script chain.
         *
         * @param execParams When set, a chain already running for the same source
         *                   and target is left alone rather than started twice.
         * @return false when the data names no such chain.
         */
        bool Start(DBScriptType type, uint32 id, Object* source, Object* target,
                   ScriptExecutionParam execParams = SCRIPT_EXEC_PARAM_NONE);

        /// Queues one step the core made up itself, rather than one read from a row.
        void StartCommand(ScriptInfo const& script, uint32 delay, Object* source, Object* target);

        /// Runs every step whose hour has come, and drops what it runs.
        void RunDue();

        bool Empty() const { return m_due.empty(); }
        std::size_t Size() const { return m_due.size(); }

    private:
        typedef std::multimap<time_t, ScriptAction> Queue;

        /// The map a step is handed to run against.
        Map& m_on;

        Queue m_due;
};
