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

#include <unordered_map>
#include "Platform/Define.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "Util.h"

class Player;
class Map;

enum WeatherState
{
    WEATHER_STATE_FINE              = 0,
    WEATHER_STATE_LIGHT_RAIN        = 3,
    WEATHER_STATE_MEDIUM_RAIN       = 4,
    WEATHER_STATE_HEAVY_RAIN        = 5,
    WEATHER_STATE_LIGHT_SNOW        = 6,
    WEATHER_STATE_MEDIUM_SNOW       = 7,
    WEATHER_STATE_HEAVY_SNOW        = 8,
    WEATHER_STATE_LIGHT_SANDSTORM   = 22,
    WEATHER_STATE_MEDIUM_SANDSTORM  = 41,
    WEATHER_STATE_HEAVY_SANDSTORM   = 42
};

struct WeatherZoneChances;

class Weather
{
    public:
        Weather(uint32 zone, WeatherZoneChances const* weatherChances);
        ~Weather() {};

        void SendWeatherUpdateToPlayer(Player* player);

        void SetWeather(WeatherType type, float grade, Map* _map, bool isPermanent);

        bool Update(uint32 diff, Map* _map);

        static bool IsValidWeatherType(uint32 type)
        {
            switch (type)
            {
                case WEATHER_TYPE_FINE:
                case WEATHER_TYPE_RAIN:
                case WEATHER_TYPE_SNOW:
                case WEATHER_TYPE_STORM:
                    return true;
                default:
                    return false;
            }
        }

    private:
        uint32 GetSound();

        bool SendWeatherForPlayersInZone(Map* _map);

        bool ReGenerate();

        WeatherState GetWeatherState() const;

        void NormalizeGrade();

        void LogWeatherState(WeatherState state) const;

        uint32 m_zone;
        WeatherType m_type;
        float m_grade;
        IntervalTimer m_timer;
        WeatherZoneChances const* m_weatherChances;
        bool m_isPermanentWeather;
};

class WeatherSystem
{
    public:
        WeatherSystem(Map* _map);
        ~WeatherSystem();

        Weather* FindOrCreateWeather(uint32 zoneId);
        void UpdateWeathers(uint32 diff);

    private:
        Map* const m_map;

        typedef std::unordered_map<uint32 , Weather*> WeatherMap;
        WeatherMap m_weathers;
};

#define WEATHER_SEASONS 4
struct WeatherSeasonChances
{
    uint32 rainChance;
    uint32 snowChance;
    uint32 stormChance;
};

struct WeatherZoneChances
{
    WeatherSeasonChances data[WEATHER_SEASONS];
};

class WeatherMgr
{
    public:
        WeatherMgr() {};
        ~WeatherMgr() {};

        void LoadWeatherZoneChances();

        WeatherZoneChances const* GetWeatherChances(uint32 zone_id) const
        {
            WeatherZoneMap::const_iterator itr = mWeatherZoneMap.find(zone_id);
            if (itr != mWeatherZoneMap.end())
            {
                return &itr->second;
            }
            else
            {
                return nullptr;
            }
        }

    private:
        typedef std::unordered_map<uint32 , WeatherZoneChances> WeatherZoneMap;
        WeatherZoneMap      mWeatherZoneMap;
};

#define sWeatherMgr MaNGOS::Singleton<WeatherMgr>::Instance()
