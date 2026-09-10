/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2015-2026 MaNGOS <https://www.getmangos.eu>
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Utilities/Errors.h"
#include <map>
#include <set>
#include "DisableMgr.h"
#include "ObjectMgr.h"
#include "OutdoorPvPMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "ProgressBar.h"
#include "World.h"
#include "BattleGroundMgr.h"

namespace DisableMgr
{

    namespace
    {

        struct DisableData
        {
            uint8 flags;
            std::set<uint32> params[2];
        };

        typedef std::map<uint32, DisableData> DisableTypeMap;

        typedef std::map<DisableType, DisableTypeMap> DisableMap;

        DisableMap m_DisableMap;
    }

#define CONTINUE if (newData) delete data; continue

    void LoadDisables()
    {

        for (DisableMap::iterator itr = m_DisableMap.begin(); itr != m_DisableMap.end(); ++itr)
        {
            itr->second.clear();
        }

        m_DisableMap.clear();

        QueryResult* result = WorldDatabase.Query("SELECT `sourceType`, `entry`, `flags`, `data` FROM `disables`");

        uint32 total_count = 0;

        if (!result)
        {
            sLog.outString(">> Loaded 0 disables. DB table `disables` is empty!");
            return;
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();
            DisableType type = DisableType(fields[0].GetUInt32());
            if (type >= MAX_DISABLE_TYPES)
            {
                sLog.outErrorDb("Invalid type %u specified in `disables` table, skipped.", type);
                continue;
            }

            uint32 entry = fields[1].GetUInt32();
            uint8 flags = fields[2].GetUInt8();
            uint32 data0 = fields[3].GetUInt32();

            DisableData* data;
            bool newData = false;
            if (m_DisableMap[type].find(entry) != m_DisableMap[type].end())
            {
                data = &m_DisableMap[type][entry];
            }
            else
            {
                data = new DisableData();
                newData = true;
            }

            data->flags = flags;

            switch (type)
            {
                case DISABLE_TYPE_SPELL:
                    if (!(sSpellStore.LookupEntry(entry) || flags & SPELL_DISABLE_DEPRECATED_SPELL))
                    {
                        ERROR_DB_STRICT_LOG("Spell entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        CONTINUE;
                    }

                    if (!flags || flags > MAX_SPELL_DISABLE_TYPE)
                    {
                        ERROR_DB_STRICT_LOG("Disable flags for spell %u are invalid, skipped.", entry);
                        CONTINUE;
                    }

                    if (flags & SPELL_DISABLE_MAP)
                    {
                        data->params[0].insert(data0 & 0xFFFF);
                    }

                    if (flags & SPELL_DISABLE_AREA)
                    {
                        data->params[1].insert(data0 >> 16);
                    }

                    break;

                case DISABLE_TYPE_QUEST:
                    break;
                case DISABLE_TYPE_MAP:
                {
                    MapEntry const* mapEntry = sMapStore.LookupEntry(entry);
                    if (!mapEntry)
                    {
                        ERROR_DB_STRICT_LOG("Map entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        CONTINUE;
                    }
                    bool isFlagInvalid = false;
                    switch (mapEntry->InstanceType)
                    {
                        case MAP_COMMON:
                        case MAP_INSTANCE:
                        case MAP_RAID:
                            if (flags)
                            {
                                isFlagInvalid = true;
                            }
                            break;
                        case MAP_BATTLEGROUND:

                            ERROR_DB_STRICT_LOG("Battleground map %u specified to be disabled in map case, skipped.", entry);
                            CONTINUE;
                    }
                    if (isFlagInvalid)
                    {
                        ERROR_DB_STRICT_LOG("Disable flags for map %u are invalid, skipped.", entry);
                        CONTINUE;
                    }
                    break;
                }
                case DISABLE_TYPE_BATTLEGROUND:
                    if (!sBattleGroundMgr.GetBattleGroundTemplate(BattleGroundTypeId(entry)))
                    {
                        ERROR_DB_STRICT_LOG("Battleground entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        CONTINUE;
                    }
                    if (flags)
                    {
                        ERROR_DB_STRICT_LOG("Disable flags specified for battleground %u, useless data.", entry);
                    }
                    break;
                case DISABLE_TYPE_OUTDOORPVP:
                    if (entry > MAX_OPVP_ID)
                    {
                        ERROR_DB_STRICT_LOG("OutdoorPvPTypes value %u from `disables` is invalid, skipped.", entry);
                        CONTINUE;
                    }
                    if (flags)
                    {
                        ERROR_DB_STRICT_LOG("Disable flags specified for outdoor PvP %u, useless data.", entry);
                    }
                    break;

                case DISABLE_TYPE_VMAP:
                {
                    MapEntry const* mapEntry = sMapStore.LookupEntry(entry);
                    if (!mapEntry)
                    {
                        ERROR_DB_STRICT_LOG("Map entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        CONTINUE;
                    }
                    switch (mapEntry->InstanceType)
                    {
                        case MAP_COMMON:
                            if (flags & COLLISION_DISABLE_AREAFLAG)
                            {
                                sLog.outDebug("Areaflag disabled for world map %u.", entry);
                            }
                            if (flags & COLLISION_DISABLE_LIQUIDSTATUS)
                            {
                                sLog.outDebug("Liquid status disabled for world map %u.", entry);
                            }
                            break;
                        case MAP_INSTANCE:
                        case MAP_RAID:
                            if (flags & COLLISION_DISABLE_HEIGHT)
                            {
                                sLog.outDebug("Height disabled for instance map %u.", entry);
                            }
                            if (flags & COLLISION_DISABLE_LOS)
                            {
                                sLog.outDebug("LoS disabled for instance map %u.", entry);
                            }
                            break;
                        case MAP_BATTLEGROUND:
                            if (flags & COLLISION_DISABLE_HEIGHT)
                            {
                                sLog.outDebug("Height disabled for battleground map %u.", entry);
                            }
                            if (flags & COLLISION_DISABLE_LOS)
                            {
                                sLog.outDebug("LoS disabled for battleground map %u.", entry);
                            }
                            break;

                        default:
                            break;
                    }
                    break;
                }
                case DISABLE_TYPE_MMAP:
                {
                    MapEntry const* mapEntry = sMapStore.LookupEntry(entry);
                    if (!mapEntry)
                    {
                        ERROR_DB_STRICT_LOG("Map entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        CONTINUE;
                    }
                    switch (mapEntry->InstanceType)
                    {
                        case MAP_COMMON:
                            sLog.outDebug("Pathfinding disabled for world map %u.", entry);
                            break;
                        case MAP_INSTANCE:
                        case MAP_RAID:
                            sLog.outDebug("Pathfinding disabled for instance map %u.", entry);
                            break;
                        case MAP_BATTLEGROUND:
                            sLog.outDebug("Pathfinding disabled for battleground map %u.", entry);
                            break;

                        default:
                            break;
                    }
                    break;
                }
                case DISABLE_TYPE_CREATURE_SPAWN:
                case DISABLE_TYPE_GAMEOBJECT_SPAWN:
                    if ((flags & SPAWN_DISABLE_CHECK_GUID) != 0)
                    {
                        if (data0)
                        {
                            data->params[0].insert(data0);
                        }
                        else
                        {
                            ERROR_DB_STRICT_LOG("Disables type %u: required GUID is missing for entry %u, ignoring disable entry.", type, entry);
                            CONTINUE;
                        }
                    }
                    break;
                case DISABLE_TYPE_ITEM_DROP:
                    break;
                default:
                    break;
            }

            m_DisableMap[type].insert(DisableTypeMap::value_type(entry, *data));
            ++total_count;
        }
        while (result->NextRow());
        delete result;

        sLog.outString(">> Loaded %u disables", total_count);
    }

    void CheckQuestDisables()
    {
        uint32 count = m_DisableMap[DISABLE_TYPE_QUEST].size();
        if (!count)
        {
            sLog.outString(">> Checked 0 quest disables.");
            return;
        }

        for (DisableTypeMap::iterator itr = m_DisableMap[DISABLE_TYPE_QUEST].begin(); itr != m_DisableMap[DISABLE_TYPE_QUEST].end();)
        {
            const uint32 entry = itr->first;
            if (!sObjectMgr.GetQuestTemplate(entry))
            {
                ERROR_DB_STRICT_LOG("Quest entry %u from `disables` doesn't exist, skipped.", entry);
                m_DisableMap[DISABLE_TYPE_QUEST].erase(itr++);
                continue;
            }
            if (itr->second.flags)
            {
                ERROR_DB_STRICT_LOG("Disable flags specified for quest %u, useless data.", entry);
            }
            ++itr;
        }

        sLog.outString(">> Checked %u quest disables", count);
    }

    bool IsDisabledFor(DisableType type, uint32 entry, Unit const* unit, uint8 flags, uint32 adData)
    {
        MANGOS_ASSERT(type < MAX_DISABLE_TYPES);
        if (m_DisableMap[type].empty())
        {
            return false;
        }

        DisableTypeMap::iterator itr = m_DisableMap[type].find(entry);
        if (itr == m_DisableMap[type].end())
        {
            return false;
        }

        switch (type)
        {
            case DISABLE_TYPE_SPELL:
            {
                uint8 spellFlags = itr->second.flags;
                if (unit)
                {
                    if ((spellFlags & SPELL_DISABLE_PLAYER &&IsPlayer(unit)) || (IsCreature(unit) && ((static_cast<Creature const*>(unit)->IsPet() && spellFlags & SPELL_DISABLE_PET) || spellFlags & SPELL_DISABLE_CREATURE)))
                    {
                        if (spellFlags & SPELL_DISABLE_MAP)
                        {
                            std::set<uint32> const& mapIds = itr->second.params[0];
                            if (mapIds.find(unit->GetMapId()) != mapIds.end())
                            {
                                return true;
                            }

                            if (!(spellFlags & SPELL_DISABLE_AREA))
                            {
                                return false;
                            }

                        }

                        if (spellFlags & SPELL_DISABLE_AREA)
                        {
                            std::set<uint32> const& areaIds = itr->second.params[1];
                            if (areaIds.find(unit->GetTerrain()->GetAreaId(unit->Where().X(), unit->Where().Y(), unit->Where().Z())) != areaIds.end())
                            {
                                return true;
                            }
                            return false;
                        }
                        else
                        {
                            return true;
                        }
                    }

                    return false;
                }
                else if (spellFlags & SPELL_DISABLE_DEPRECATED_SPELL)
                {
                    return true;
                }
                else if (flags & SPELL_DISABLE_LOS)
                {
                    return (spellFlags & SPELL_DISABLE_LOS) != 0;
                }

                break;
            }
            case DISABLE_TYPE_MAP:
                if (static_cast<Player const*>(unit))
                {

                    return true;
                }
                return false;
            case DISABLE_TYPE_QUEST:
                if (!unit)
                {
                    return true;
                }
                if (Player const* player = static_cast<Player const*>(unit))
                {
                    return !player->isGameMaster();
                }
                return true;
            case DISABLE_TYPE_BATTLEGROUND:
            case DISABLE_TYPE_OUTDOORPVP:
            case DISABLE_TYPE_ACHIEVEMENT_CRITERIA:
            case DISABLE_TYPE_MMAP:
            case DISABLE_TYPE_ITEM_DROP:
                return true;
            case DISABLE_TYPE_VMAP:
                return (flags & itr->second.flags) != 0;
            case DISABLE_TYPE_CREATURE_SPAWN:
            case DISABLE_TYPE_GAMEOBJECT_SPAWN:
                return (itr->second.flags & SPAWN_DISABLE_CHECK_GUID) == 0 || itr->second.params[0].count(adData) > 0;
            default:
                break;
        }

        return false;
    }

    bool IsVMAPDisabledFor(uint32 entry, uint8 flags)
    {
        return IsDisabledFor(DISABLE_TYPE_VMAP, entry, nullptr, flags);
    }

    bool IsPathfindingEnabled(uint32 mapId)
    {
        return sWorld.getConfig(CONFIG_BOOL_MMAP_ENABLED) &&
            !IsDisabledFor(DISABLE_TYPE_MMAP, mapId);
    }
}
