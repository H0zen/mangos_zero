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

#include "Tenure.h"
#include "Creature.h"

namespace
{
    /// Spends what it can and says whether nothing was left to spend.
    bool Spend(uint32& left, uint32 elapsed)
    {
        if (left <= elapsed)
        {
            return true;
        }

        left -= elapsed;
        return false;
    }
}

tenure::Verdict tenure::Tick(TempSpawnType rule, uint32 left, uint32 granted, uint32 elapsed, Body const& body)
{
    Verdict said;
    said.left = left;

    switch (rule)
    {
        case TEMPSPAWN_MANUAL_DESPAWN:
            break;

        case TEMPSPAWN_TIMED_DESPAWN:
            said.vanish = Spend(said.left, elapsed);
            break;

        case TEMPSPAWN_TIMED_OOC_DESPAWN:
            // A fight puts the clock back to full, so the term is measured from
            // the end of the last fight rather than from the summoning.
            if (body.inCombat)
            {
                said.left = granted;
            }
            else
            {
                said.vanish = Spend(said.left, elapsed);
            }
            break;

        case TEMPSPAWN_CORPSE_TIMED_DESPAWN:
            if (body.corpse && Spend(said.left, elapsed))
            {
                said.vanish = true;
                break;
            }

            said.vanish = body.despawned;
            break;

        case TEMPSPAWN_CORPSE_DESPAWN:
            said.vanish = body.dead;
            break;

        case TEMPSPAWN_DEAD_DESPAWN:
            said.vanish = body.despawned;
            break;

        case TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN:
            if (body.dead)
            {
                said.vanish = true;
                break;
            }

            if (body.inCombat)
            {
                said.left = granted;
            }
            else
            {
                said.vanish = Spend(said.left, elapsed);
            }
            break;

        case TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN:
            if (body.despawned)
            {
                said.vanish = true;
                break;
            }

            if (body.inCombat || !body.alive)
            {
                said.left = granted;
            }
            else
            {
                said.vanish = Spend(said.left, elapsed);
            }
            break;

        case TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN:
            said.vanish = body.dead || Spend(said.left, elapsed);
            break;

        case TEMPSPAWN_TIMED_OR_DEAD_DESPAWN:
            said.vanish = body.despawned || Spend(said.left, elapsed);
            break;
    }

    return said;
}

tenure::Body tenure::BodyOf(Creature const& who)
{
    Body how;
    how.inCombat = who.IsInCombat();
    how.alive = who.IsAlive();
    how.dead = who.IsDead();
    how.corpse = who.IsCorpse();
    how.despawned = who.IsDespawned();
    return how;
}
