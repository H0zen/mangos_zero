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

enum ScriptExecutionParam
{
    SCRIPT_EXEC_PARAM_NONE                    = 0x00,
    SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE        = 0x01,
    SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET        = 0x02,
    SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE_TARGET = 0x03,
};

class ScriptSchedule
{
    public:
        explicit ScriptSchedule(Map& on) : m_on(on) {}
        ~ScriptSchedule();

        ScriptSchedule(ScriptSchedule const&) = delete;
        ScriptSchedule& operator=(ScriptSchedule const&) = delete;

        bool Start(DBScriptType type, uint32 id, Object* source, Object* target,
                   ScriptExecutionParam execParams = SCRIPT_EXEC_PARAM_NONE);

        void StartCommand(ScriptInfo const& script, uint32 delay, Object* source, Object* target);

        void RunDue();

        bool Empty() const { return m_due.empty(); }
        std::size_t Size() const { return m_due.size(); }

    private:
        typedef std::multimap<time_t, ScriptAction> Queue;

        Map& m_on;

        Queue m_due;
};
