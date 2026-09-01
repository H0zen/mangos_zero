/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 1.12.x client.
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "ClientBuildPolicy.h"

namespace
{
RealmBuildInfo const SupportedClientBuilds[] =
{
    {6141, 1, 12, 3, ' '},
    {6005, 1, 12, 2, ' '},
    {5875, 1, 12, 1, ' '},
};
}

RealmBuildInfo const* FindBuildInfo(uint16 build)
{
    for (RealmBuildInfo const& info : SupportedClientBuilds)
    {
        if (static_cast<uint32>(info.build) == build)
        {
            return &info;
        }
    }

    return nullptr;
}
