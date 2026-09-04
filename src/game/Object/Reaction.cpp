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
    /// Whoever is actually behind a unit: a pet answers for its master.
    Unit const& MasterOrSelf(Unit const& unit)
    {
        Unit const* owner = unit.GetCharmerOrOwner();
        return owner ? *owner : unit;
    }

    bool FightingEachOther(Unit const& a, Unit const& b)
    {
        return a.getVictim() == &b || b.getVictim() == &a;
    }

    /// The standing a player has earned settles the question when either side
    /// is a player; which way round decides whether the war declaration or the
    /// reputation rank is read.
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

    /// Two players, once ownership has been followed and neither is fighting
    /// the other.
    Reaction AsPvpStateStands(Player const& who, Player const& whom)
    {
        if (who.IsInDuelWith(&whom))
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

        // Across the two sides. An unflagged whom is nobody's enemy, and a
        // flagged one is an enemy only to someone flagged as well. What is left
        // in between is the yellow name: attackable by choice, not by state.
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

    Player const* watcher = ToPlayer(&whom);
    if (watcher && watcher->isGameMaster())
    {
        return Reaction::Friendly;
    }

    // Whoever is already swinging at whoever, through either one's master.
    Unit const& whoCounts = MasterOrSelf(who);
    Unit const& whomCounts = MasterOrSelf(whom);
    if (FightingEachOther(who, whom) || FightingEachOther(whoCounts, whom)
        || FightingEachOther(who, whomCounts) || FightingEachOther(whoCounts, whomCounts))
    {
        return Reaction::Hostile;
    }

    // A master and its own pet, or two pets of one master.
    if (&whoCounts == &whomCounts)
    {
        return Reaction::Friendly;
    }

    Player const* playerWho = ToPlayer(&whoCounts);
    Player const* playerWhom = ToPlayer(&whomCounts);
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
    Player const* watcher = ToPlayer(&whom);
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

    // A wild object has no side of its own, so it opposes whoever a player
    // drives and ignores everything else.
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
    if (Unit const* unit = ToUnit(&who))
    {
        return ReactionOf(*unit, whom);
    }

    if (GameObject const* go = ToGameObject(&who))
    {
        return ReactionOf(*go, whom);
    }

    if (Corpse const* corpse = ToCorpse(&who))
    {
        return ReactionOf(*corpse, whom);
    }

    if (DynamicObject const* dynObject = ToDynObject(&who))
    {
        return ReactionOf(*dynObject, whom);
    }

    return Reaction::Neither;
}

namespace
{
    /// A faction the player can hold a standing with answers through that
    /// standing, so the template is not asked.
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
