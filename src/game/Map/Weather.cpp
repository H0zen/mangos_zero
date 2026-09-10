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

#include "Weather.h"
#include "WorldSession.h"
#include "Player.h"
#include "Map.h"
#include "World.h"
#include "WorldPacket.h"
#include "Log.h"
#include "Util.h"
#include "ProgressBar.h"

enum WeatherSounds
{
    WEATHER_NOSOUND                = 0,
    WEATHER_RAINLIGHT              = 8533,
    WEATHER_RAINMEDIUM             = 8534,
    WEATHER_RAINHEAVY              = 8535,
    WEATHER_SNOWLIGHT              = 8536,
    WEATHER_SNOWMEDIUM             = 8537,
    WEATHER_SNOWHEAVY              = 8538,
    WEATHER_SANDSTORMLIGHT         = 8556,
    WEATHER_SANDSTORMMEDIUM        = 8557,
    WEATHER_SANDSTORMHEAVY         = 8558
};

Weather::Weather(uint32 zone, WeatherZoneChances const* weatherChances)
    : m_zone(zone),
    m_type(WEATHER_TYPE_FINE),
    m_grade(0.0f),
    m_weatherChances(weatherChances),
    m_isPermanentWeather(false)
{
    m_timer.SetInterval(sWorld.getConfig(CONFIG_UINT32_INTERVAL_CHANGEWEATHER));
    DETAIL_FILTER_LOG(LOG_FILTER_WEATHER, "WORLD: Starting weather system for zone %u (change every %u minutes).", m_zone, uint32(m_timer.GetInterval() / (MINUTE * IN_MILLISECONDS)));
}

bool Weather::Update(uint32 diff, Map* _map)
{
    m_timer.Update(diff);

    if (m_timer.Passed())
    {
        m_timer.Reset();

        if (ReGenerate())
        {

            if (!SendWeatherForPlayersInZone(_map))
            {
                return false;
            }
        }
    }
    return true;
}

bool Weather::ReGenerate()
{
    if (m_isPermanentWeather)
    {
        return false;
    }

    WeatherType old_type = m_type;
    float old_grade = m_grade;

    if (!m_weatherChances)
    {
        m_type = WEATHER_TYPE_FINE;
        m_grade = 0.0f;

        return old_type != m_type || old_grade != m_grade;
    }

    uint32 u = urand(0, 99);

    if (u < 30)
    {
        return false;
    }

    time_t gtime = sWorld.GetGameTime();
    std::tm ltime = safe_localtime(gtime);
    uint32 season = ((ltime.tm_yday - 78 + 365) / 91) % 4;

    static char const* seasonName[WEATHER_SEASONS] = { "spring", "summer", "fall", "winter" };

    DEBUG_FILTER_LOG(LOG_FILTER_WEATHER, "Generating a change in %s weather for zone %u.", seasonName[season], m_zone);

    if ((u < 60) && (m_grade < 0.33333334f))
    {
        m_type = WEATHER_TYPE_FINE;
        m_grade = 0.0f;
    }

    if ((u < 60) && (m_type != WEATHER_TYPE_FINE))
    {
        m_grade -= 0.33333334f;
        return true;
    }

    if ((u < 90) && (m_type != WEATHER_TYPE_FINE))
    {
        m_grade += 0.33333334f;
        return true;
    }

    if (m_type != WEATHER_TYPE_FINE)
    {

        if (m_grade < 0.33333334f)
        {
            m_grade = 0.9999f;
            return true;
        }
        else
        {
            if (m_grade > 0.6666667f)
            {

                uint32 rnd = urand(0, 99);
                if (rnd < 50)
                {
                    m_grade -= 0.6666667f;
                    return true;
                }
            }
            m_type = WEATHER_TYPE_FINE;
            m_grade = 0;
        }
    }

    uint32 chance1 =          m_weatherChances->data[season].rainChance;
    uint32 chance2 = chance1 + m_weatherChances->data[season].snowChance;
    uint32 chance3 = chance2 + m_weatherChances->data[season].stormChance;

    uint32 rnd = urand(1, 100);
    if (rnd <= chance1)
    {
        m_type = WEATHER_TYPE_RAIN;
    }
    else if (rnd <= chance2)
    {
        m_type = WEATHER_TYPE_SNOW;
    }
    else if (rnd <= chance3)
    {
        m_type = WEATHER_TYPE_STORM;
    }
    else
    {
        m_type = WEATHER_TYPE_FINE;
    }

    if (m_type == WEATHER_TYPE_FINE)
    {
        m_grade = 0.0f;
    }
    else if (u < 90)
    {
        m_grade = rand_norm_f() * 0.3333f;
    }
    else
    {

        rnd = urand(0, 99);
        if (rnd < 50)
        {
            m_grade = rand_norm_f() * 0.3333f + 0.3334f;
        }
        else
        {
            m_grade = rand_norm_f() * 0.3333f + 0.6667f;
        }
    }

    NormalizeGrade();

    return m_type != old_type || m_grade != old_grade;
}

void Weather::SendWeatherUpdateToPlayer(Player* player)
{
    NormalizeGrade();

    WorldPacket data(SMSG_WEATHER, 4 + 4 + 4 + 1);
    data << uint32(m_type);
    data << float(m_grade);
    data << uint32(GetSound());
    data << uint8(0);

    player->GetSession()->SendPacket(&data);
}

bool Weather::SendWeatherForPlayersInZone(Map* _map)
{
    NormalizeGrade();

    WorldPacket data(SMSG_WEATHER, 4 + 4 + 4 + 1);
    data << uint32(m_type);
    data << float(m_grade);
    data << uint32(GetSound());
    data << uint8(0);

    if (!Deliver(Audience::InZone(*_map, m_zone), &data))
    {
        return false;
    }

    LogWeatherState(GetWeatherState());

    return true;
}

void Weather::SetWeather(WeatherType type, float grade, Map* _map, bool isPermanent)
{
    m_isPermanentWeather = isPermanent;

    if (m_type == type && m_grade == grade)
    {
        return;
    }

    m_type = type;
    m_grade = grade;
    SendWeatherForPlayersInZone(_map);
}

WeatherState Weather::GetWeatherState() const
{
    if (m_grade < 0.27f)
    {
        return WEATHER_STATE_FINE;
    }

    switch (m_type)
    {
        case WEATHER_TYPE_RAIN:
            if (m_grade < 0.40f)
            {
                return WEATHER_STATE_LIGHT_RAIN;
            }
            else if (m_grade < 0.70f)
            {
                return WEATHER_STATE_MEDIUM_RAIN;
            }
            else
            {
                return WEATHER_STATE_HEAVY_RAIN;
            }
        case WEATHER_TYPE_SNOW:
            if (m_grade < 0.40f)
            {
                return WEATHER_STATE_LIGHT_SNOW;
            }
            else if (m_grade < 0.70f)
            {
                return WEATHER_STATE_MEDIUM_SNOW;
            }
            else
            {
                return WEATHER_STATE_HEAVY_SNOW;
            }
        case WEATHER_TYPE_STORM:
            if (m_grade < 0.40f)
            {
                return WEATHER_STATE_LIGHT_SANDSTORM;
            }
            else if (m_grade < 0.70f)
            {
                return WEATHER_STATE_MEDIUM_SANDSTORM;
            }
            else
            {
                return WEATHER_STATE_HEAVY_SANDSTORM;
            }
        case WEATHER_TYPE_FINE:
        default:
            return WEATHER_STATE_FINE;
    }
}

void Weather::NormalizeGrade()
{
    if (m_grade >= 1)
    {
        m_grade = 0.9999f;
    }
    else if (m_grade < 0)
    {
        m_grade = 0.0001f;
    }
}

void Weather::LogWeatherState(WeatherState state) const
{
    char const* wthstr;
    switch (state)
    {
        case WEATHER_STATE_LIGHT_RAIN:
            wthstr = "light rain";
            break;
        case WEATHER_STATE_MEDIUM_RAIN:
            wthstr = "medium rain";
            break;
        case WEATHER_STATE_HEAVY_RAIN:
            wthstr = "heavy rain";
            break;
        case WEATHER_STATE_LIGHT_SNOW:
            wthstr = "light snow";
            break;
        case WEATHER_STATE_MEDIUM_SNOW:
            wthstr = "medium snow";
            break;
        case WEATHER_STATE_HEAVY_SNOW:
            wthstr = "heavy snow";
            break;
        case WEATHER_STATE_LIGHT_SANDSTORM:
            wthstr = "light sandstorm";
            break;
        case WEATHER_STATE_MEDIUM_SANDSTORM:
            wthstr = "medium sandstorm";
            break;
        case WEATHER_STATE_HEAVY_SANDSTORM:
            wthstr = "heavy sandstorm";
            break;
        case WEATHER_STATE_FINE:
        default:
            wthstr = "fine";
            break;
    }

    DETAIL_FILTER_LOG(LOG_FILTER_WEATHER, "Change the weather of zone %u (type %u, grade %f) to state %s.", m_zone, m_type, m_grade, wthstr);
}

WeatherSystem::WeatherSystem(Map* _map) : m_map(_map)
{}

WeatherSystem::~WeatherSystem()
{

    for (WeatherMap::const_iterator itr = m_weathers.begin(); itr != m_weathers.end(); ++itr)
    {
        delete itr->second;
    }

    m_weathers.clear();
}

Weather* WeatherSystem::FindOrCreateWeather(uint32 zoneId)
{
    WeatherMap::const_iterator itr = m_weathers.find(zoneId);

    if (itr != m_weathers.end())
    {
        return itr->second;
    }

    Weather* w = new Weather(zoneId, sWeatherMgr.GetWeatherChances(zoneId));
    m_weathers[zoneId] = w;
    return w;
}

void WeatherSystem::UpdateWeathers(uint32 diff)
{

    for (WeatherMap::iterator itr = m_weathers.begin(); itr != m_weathers.end();)
    {

        if (!itr->second->Update(diff, m_map))
        {
            delete itr->second;
            m_weathers.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }
}

uint32 Weather::GetSound()
{
    uint32 sound;
    switch (m_type)
    {
        case WEATHER_TYPE_RAIN:
            if (m_grade < 0.40f)
            {
                sound = WEATHER_RAINLIGHT;
            }
            else if (m_grade < 0.70f)
            {
                sound = WEATHER_RAINMEDIUM;
            }
            else
            {
                sound = WEATHER_RAINHEAVY;
            }
            break;
        case WEATHER_TYPE_SNOW:
            if (m_grade < 0.40f)
            {
                sound = WEATHER_SNOWLIGHT;
            }
            else if (m_grade < 0.70f)
            {
                sound = WEATHER_SNOWMEDIUM;
            }
            else
            {
                sound = WEATHER_SNOWHEAVY;
            }
            break;
        case WEATHER_TYPE_STORM:
            if (m_grade < 0.40f)
            {
                sound = WEATHER_SANDSTORMLIGHT;
            }
            else if (m_grade < 0.70f)
            {
                sound = WEATHER_SANDSTORMMEDIUM;
            }
            else
            {
                sound = WEATHER_SANDSTORMHEAVY;
            }
            break;
        case WEATHER_TYPE_FINE:
        default:
            sound = WEATHER_NOSOUND;
            break;
    }
    return sound;
}

void WeatherMgr::LoadWeatherZoneChances()
{
    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `zone`, `spring_rain_chance`, `spring_snow_chance`, `spring_storm_chance`, `summer_rain_chance`, `summer_snow_chance`, `summer_storm_chance`, `fall_rain_chance`, `fall_snow_chance`, `fall_storm_chance`, `winter_rain_chance`, `winter_snow_chance`, `winter_storm_chance` FROM `game_weather`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 weather definitions. DB table `game_weather` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 zone_id = fields[0].GetUInt32();

        WeatherZoneChances& wzc = mWeatherZoneMap[zone_id];

        for (int season = 0; season < WEATHER_SEASONS; ++season)
        {
            wzc.data[season].rainChance  = fields[season * (MAX_WEATHER_TYPE - 1) + 1].GetUInt32();
            wzc.data[season].snowChance  = fields[season * (MAX_WEATHER_TYPE - 1) + 2].GetUInt32();
            wzc.data[season].stormChance = fields[season * (MAX_WEATHER_TYPE - 1) + 3].GetUInt32();

            if (wzc.data[season].rainChance > 100)
            {
                wzc.data[season].rainChance = 25;
                sLog.outErrorDb("Weather for zone %u season %u has wrong rain chance > 100%%", zone_id, season);
            }

            if (wzc.data[season].snowChance > 100)
            {
                wzc.data[season].snowChance = 25;
                sLog.outErrorDb("Weather for zone %u season %u has wrong snow chance > 100%%", zone_id, season);
            }

            if (wzc.data[season].stormChance > 100)
            {
                wzc.data[season].stormChance = 25;
                sLog.outErrorDb("Weather for zone %u season %u has wrong storm chance > 100%%", zone_id, season);
            }
        }

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u weather definitions", count);
    sLog.outString();
}
