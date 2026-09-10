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

#include "Reaction.h"

#include "Corpse.h"
#include "DBCStores.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "Group.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "Unit.h"

namespace
{
    Reaction FromRank(ReputationRank rank)
    {
        if (rank <= REP_HOSTILE)
        {
            return Reaction::Hostile;
        }

        if (rank >= REP_FRIENDLY)
        {
            return Reaction::Friendly;
        }

        return Reaction::Neither;
    }
}

Reaction OpinionOf(Player const& player, FactionTemplateEntry const* faction, bool byWarState)
{
    if (!faction || !faction->Faction)
    {
        return Reaction::NoOpinion;
    }

    ReputationMgr const& standing = player.GetReputationMgr();

    if (ReputationRank const* forced = standing.GetForcedRankIfAny(faction))
    {
        return FromRank(*forced);
    }

    FactionEntry const* known = sFactionStore.LookupEntry(faction->Faction);
    if (!known)
    {
        return Reaction::NoOpinion;
    }

    if (byWarState)
    {
        FactionState const* state = standing.GetState(known);
        if (!state)
        {
            return Reaction::NoOpinion;
        }

        return (state->Flags & FACTION_FLAG_AT_WAR) ? Reaction::Hostile : Reaction::Friendly;
    }

    if (known->ReputationIndex < 0)
    {
        return Reaction::NoOpinion;
    }

    return FromRank(standing.GetRank(known));
}

Reaction AsFactionsDeclare(FactionTemplateEntry const& who, FactionTemplateEntry const& whom)
{
    if (who.IsHostileTo(whom))
    {
        return Reaction::Hostile;
    }

    if (who.IsFriendlyTo(whom))
    {
        return Reaction::Friendly;
    }

    return Reaction::Neither;
}

namespace
{

    Unit const& MasterOrSelf(Unit const& unit)
    {
        Unit const* owner = unit.GetCharmerOrOwner();
        return owner ? *owner : unit;
    }

    bool FightingEachOther(Unit const& a, Unit const& b)
    {
        return a.getVictim() == &b || b.getVictim() == &a;
    }

    Reaction FromStanding(Player const* playerWho, Player const* playerWhom,
                          FactionTemplateEntry const* whoFaction,
                          FactionTemplateEntry const* whomFaction)
    {
        if (playerWho)
        {
            return OpinionOf(*playerWho, whomFaction, true);
        }

        if (playerWhom)
        {
            return OpinionOf(*playerWhom, whoFaction, false);
        }

        return Reaction::NoOpinion;
    }

    Reaction AsPvpStateStands(Player const& who, Player const& whom)
    {
        if (who.Duelling().With(&whom))
        {
            return Reaction::Hostile;
        }

        if (who.GetGroup() && who.GetGroup() == whom.GetGroup())
        {
            return Reaction::Friendly;
        }

        if (whom.HasPlayerFlag(PLAYER_FLAGS_SANCTUARY) && who.HasPlayerFlag(PLAYER_FLAGS_SANCTUARY))
        {
            return Reaction::Friendly;
        }

        if (who.IsFFAPvP() && whom.IsFFAPvP())
        {
            return Reaction::Hostile;
        }

        if (who.GetTeam() == whom.GetTeam())
        {
            return Reaction::Friendly;
        }

        if (!whom.IsPvP())
        {
            return Reaction::Friendly;
        }

        return who.IsPvP() ? Reaction::Hostile : Reaction::Neither;
    }
}

Reaction ReactionOf(Unit const& who, Unit const& whom)
{
    if (&who == &whom)
    {
        return Reaction::Friendly;
    }

    Player const* watcher = static_cast<Player const*>(&whom);
    if (watcher && watcher->isGameMaster())
    {
        return Reaction::Friendly;
    }

    Unit const& whoCounts = MasterOrSelf(who);
    Unit const& whomCounts = MasterOrSelf(whom);
    if (FightingEachOther(who, whom) || FightingEachOther(whoCounts, whom)
        || FightingEachOther(who, whomCounts) || FightingEachOther(whoCounts, whomCounts))
    {
        return Reaction::Hostile;
    }

    if (&whoCounts == &whomCounts)
    {
        return Reaction::Friendly;
    }

    Player const* playerWho = static_cast<Player const*>(&whoCounts);
    Player const* playerWhom = static_cast<Player const*>(&whomCounts);
    if (playerWho && playerWhom)
    {
        return AsPvpStateStands(*playerWho, *playerWhom);
    }

    FactionTemplateEntry const* whoFaction = whoCounts.getFactionTemplateEntry();
    FactionTemplateEntry const* whomFaction = whomCounts.getFactionTemplateEntry();
    if (!whoFaction || !whomFaction)
    {
        return Reaction::Neither;
    }

    if (whomCounts.isAttackingPlayer() && whoCounts.IsContestedGuard())
    {
        return Reaction::Hostile;
    }

    Reaction const standing = FromStanding(playerWho, playerWhom, whoFaction, whomFaction);
    if (standing != Reaction::NoOpinion)
    {
        return standing;
    }

    return AsFactionsDeclare(*whoFaction, *whomFaction);
}

Reaction ReactionOf(GameObject const& who, Unit const& whom)
{
    Player const* watcher = static_cast<Player const*>(&whom);
    if (watcher && watcher->isGameMaster())
    {
        return Reaction::Friendly;
    }

    if (Unit const* owner = who.GetOwner())
    {
        return ReactionOf(*owner, whom);
    }

    if (Unit const* master = whom.GetCharmerOrOwner())
    {
        return ReactionOf(who, *master);
    }

    if (!who.GetGOInfo()->faction)
    {
        return whom.IsControlledByPlayer() ? Reaction::Hostile : Reaction::Neither;
    }

    FactionTemplateEntry const* whoFaction = sFactionTemplateStore.LookupEntry(who.GetGOInfo()->faction);
    FactionTemplateEntry const* whomFaction = whom.getFactionTemplateEntry();
    if (!whoFaction || !whomFaction)
    {
        return Reaction::Neither;
    }

    if (watcher)
    {
        Reaction const standing = OpinionOf(*watcher, whoFaction, false);
        if (standing != Reaction::NoOpinion)
        {
            return standing;
        }
    }

    return AsFactionsDeclare(*whoFaction, *whomFaction);
}

Reaction ReactionOf(Corpse const& who, Unit const& whom)
{
    Player const* owner = sObjectMgr.GetPlayer(who.GetOwnerGuid());
    return owner ? ReactionOf(*owner, whom) : Reaction::Friendly;
}

Reaction ReactionOf(DynamicObject const& who, Unit const& whom)
{
    Unit const* caster = who.GetCaster();
    return caster ? ReactionOf(*caster, whom) : Reaction::Friendly;
}

Reaction ReactionOf(Object const& who, Unit const& whom)
{
    if (Unit const* unit = static_cast<Unit const*>(&who))
    {
        return ReactionOf(*unit, whom);
    }

    if (GameObject const* go = static_cast<GameObject const*>(&who))
    {
        return ReactionOf(*go, whom);
    }

    if (Corpse const* corpse = static_cast<Corpse const*>(&who))
    {
        return ReactionOf(*corpse, whom);
    }

    if (DynamicObject const* dynObject = static_cast<DynamicObject const*>(&who))
    {
        return ReactionOf(*dynObject, whom);
    }

    return Reaction::Neither;
}

namespace
{

    bool StandingDecides(FactionTemplateEntry const* faction)
    {
        FactionEntry const* known = sFactionStore.LookupEntry(faction->Faction);
        return known && known->ReputationIndex >= 0;
    }
}

bool HostileToPlayers(Unit const& who)
{
    FactionTemplateEntry const* faction = who.getFactionTemplateEntry();
    if (!faction || !faction->Faction)
    {
        return false;
    }

    return !StandingDecides(faction) && faction->IsHostileToPlayers();
}

bool NeutralToAll(Unit const& who)
{
    FactionTemplateEntry const* faction = who.getFactionTemplateEntry();
    if (!faction || !faction->Faction)
    {
        return true;
    }

    return !StandingDecides(faction) && faction->IsNeutralToAll();
}
