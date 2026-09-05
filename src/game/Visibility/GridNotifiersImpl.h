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

#include "Reaction.h"
#include "GridNotifiers.h"
#include "WorldPacket.h"
#include "Player.h"
#include "CreatureAI.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "DBCEnums.h"
#include "DBCStores.h"

template<class T>
    inline void MaNGOS::VisibleNotifier::Visit(GridRefManager<T>& m)
{
    for (typename GridRefManager<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        i_camera.UpdateVisibilityOf(iter->getSource(), Data(), i_visibleNow);
        i_clientGUIDs.erase(iter->getSource()->GetObjectGuid());
    }
}

inline void MaNGOS::ObjectUpdater::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Occupant::UpdateHelper helper(iter->getSource());
        helper.Update(i_timeDiff);
    }
}

inline void PlayerCreatureRelocationWorker(Player* pl, Creature* c)
{
    // Creature AI reaction
    if (!c->hasUnitState(UNIT_STAT_LOST_CONTROL))
    {
        if (c->AI() && c->AI()->IsVisible(pl) && !c->IsInEvadeMode())
        {
            c->AI()->MoveInLineOfSight(pl);
        }
    }
}

inline void CreatureCreatureRelocationWorker(Creature* c1, Creature* c2)
{
    if (!c1->hasUnitState(UNIT_STAT_LOST_CONTROL))
    {
        if (c1->AI() && c1->AI()->IsVisible(c2) && !c1->IsInEvadeMode())
        {
            c1->AI()->MoveInLineOfSight(c2);
        }
    }

    if (!c2->hasUnitState(UNIT_STAT_LOST_CONTROL))
    {
        if (c2->AI() && c2->AI()->IsVisible(c1) && !c2->IsInEvadeMode())
        {
            c2->AI()->MoveInLineOfSight(c1);
        }
    }
}

inline void MaNGOS::PlayerRelocationNotifier::Visit(CreatureMapType& m)
{
    if (!i_player.IsAlive() || i_player.IsTaxiFlying())
    {
        return;
    }

    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Creature* c = iter->getSource();
        if (c->IsAlive())
        {
            PlayerCreatureRelocationWorker(&i_player, c);
        }
    }
}

template<>
    inline void MaNGOS::CreatureRelocationNotifier::Visit(PlayerMapType& m)
{
    if (!i_creature.IsAlive())
    {
        return;
    }

    for (PlayerMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Player* player = iter->getSource();
        if (player->IsAlive() && !player->IsTaxiFlying())
        {
            PlayerCreatureRelocationWorker(player, &i_creature);
        }
    }
}

template<>
    inline void MaNGOS::CreatureRelocationNotifier::Visit(CreatureMapType& m)
{
    if (!i_creature.IsAlive())
    {
        return;
    }

    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Creature* c = iter->getSource();
        if (c != &i_creature && c->IsAlive())
        {
            CreatureCreatureRelocationWorker(c, &i_creature);
        }
    }
}

inline void MaNGOS::DynamicObjectUpdater::VisitHelper(Unit* target)
{
    if (!target->IsAlive() || target->IsTaxiFlying())
    {
        return;
    }

    if (target->IsCreature() && ((Creature*)target)->IsTotem())
    {
        return;
    }

    // Deck-aware: for an effect on a transport this measures the local separation, so the
    // world position the server is only guessing at never enters the test.
    if (!i_dynobject.IsInEffectRange(target))
    {
        return;
    }

    // Check targets for not_selectable unit flag and remove
    if (target->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_OOC_NOT_ATTACKABLE))
    {
        return;
    }

    // Evade target
    if (target->IsCreature() && ((Creature*)target)->IsInEvadeMode())
    {
        return;
    }

    // Check player targets and remove if in GM mode or GM invisibility (for not self casting case)
    if (target->IsPlayer() && target != i_check && (((Player*)target)->isGameMaster() || ((Player*)target)->GetVisibility() == VISIBILITY_OFF))
    {
        return;
    }

    // for player casts use less strict negative and more stricted positive targeting
    if (i_check->IsPlayer())
    {
        if (IsFriendly(*i_check, *target) != i_positive)
        {
            return;
        }
    }
    else
    {
        if (IsHostile(*i_check, *target) == i_positive)
        {
            return;
        }
    }

    if (i_dynobject.IsAffecting(target))
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(i_dynobject.GetSpellId());
    SpellEffectIndex eff_index  = i_dynobject.GetEffIndex();

    // Check target immune to spell or aura
    if (target->IsImmuneToSpell(spellInfo, false) || target->IsImmuneToSpellEffect(spellInfo, eff_index, false))
    {
        return;
    }

    // Apply PersistentAreaAura on target
    // in case 2 dynobject overlap areas for same spell, same holder is selected, so dynobjects share holder
    SpellAuraHolder* holder = target->GetSpellAuraHolder(spellInfo->ID, i_dynobject.GetCasterGuid());

    if (holder)
    {
        if (!holder->GetAuraByEffectIndex(eff_index))
        {
            PersistentAreaAura* Aur = new PersistentAreaAura(spellInfo, eff_index, nullptr, holder, target, i_dynobject.GetCaster());
            holder->AddAura(Aur, eff_index);
            target->AddAuraToModList(Aur);
            holder->SetInUse(true);
            Aur->ApplyModifier(true, true);
            holder->SetInUse(false);
        }
        else if (holder->GetAuraDuration() >= 0 && uint32(holder->GetAuraDuration()) < i_dynobject.GetDuration())
        {
            holder->SetAuraDuration(i_dynobject.GetDuration());
            holder->UpdateAuraDuration();
        }
    }
    else
    {
        holder = CreateSpellAuraHolder(spellInfo, target, i_dynobject.GetCaster());
        PersistentAreaAura* Aur = new PersistentAreaAura(spellInfo, eff_index, nullptr, holder, target, i_dynobject.GetCaster());
        holder->AddAura(Aur, eff_index);
        target->AddSpellAuraHolder(holder);
    }

    i_dynobject.AddAffected(target);
}

template<>
    inline void MaNGOS::DynamicObjectUpdater::Visit(CreatureMapType&  m)
{
    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        VisitHelper(itr->getSource());
    }
}

template<>
    inline void MaNGOS::DynamicObjectUpdater::Visit(PlayerMapType&  m)
{
    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        VisitHelper(itr->getSource());
    }
}

// SEARCHERS & LIST SEARCHERS & WORKERS

// Occupant searchers & workers

template<class Check>
    void MaNGOS::OccupantSearcher<Check>::Visit(GameObjectMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantSearcher<Check>::Visit(PlayerMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantSearcher<Check>::Visit(CreatureMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantSearcher<Check>::Visit(CorpseMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (CorpseMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantSearcher<Check>::Visit(DynamicObjectMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (DynamicObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantLastSearcher<Check>::Visit(GameObjectMapType& m)
{
    for (GameObjectMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantLastSearcher<Check>::Visit(PlayerMapType& m)
{
    for (PlayerMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantLastSearcher<Check>::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantLastSearcher<Check>::Visit(CorpseMapType& m)
{
    for (CorpseMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantLastSearcher<Check>::Visit(DynamicObjectMapType& m)
{
    for (DynamicObjectMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::OccupantListSearcher<Check>::Visit(PlayerMapType& m)
{
    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

template<class Check>
    void MaNGOS::OccupantListSearcher<Check>::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

template<class Check>
    void MaNGOS::OccupantListSearcher<Check>::Visit(CorpseMapType& m)
{
    for (CorpseMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

template<class Check>
    void MaNGOS::OccupantListSearcher<Check>::Visit(GameObjectMapType& m)
{
    for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

template<class Check>
    void MaNGOS::OccupantListSearcher<Check>::Visit(DynamicObjectMapType& m)
{
    for (DynamicObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

// Gameobject searchers

template<class Check>
    void MaNGOS::GameObjectSearcher<Check>::Visit(GameObjectMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::GameObjectLastSearcher<Check>::Visit(GameObjectMapType& m)
{
    for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
        }
    }
}

template<class Check>
    void MaNGOS::GameObjectListSearcher<Check>::Visit(GameObjectMapType& m)
{
    for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

// Unit searchers

template<class Check>
    void MaNGOS::UnitSearcher<Check>::Visit(CreatureMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::UnitSearcher<Check>::Visit(PlayerMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::UnitLastSearcher<Check>::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
        }
    }
}

template<class Check>
    void MaNGOS::UnitLastSearcher<Check>::Visit(PlayerMapType& m)
{
    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
        }
    }
}

template<class Check>
    void MaNGOS::UnitListSearcher<Check>::Visit(PlayerMapType& m)
{
    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

template<class Check>
    void MaNGOS::UnitListSearcher<Check>::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

// Creature searchers

template<class Check>
    void MaNGOS::CreatureSearcher<Check>::Visit(CreatureMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::CreatureLastSearcher<Check>::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
        }
    }
}

template<class Check>
    void MaNGOS::CreatureListSearcher<Check>::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

template<class Check>
    void MaNGOS::PlayerSearcher<Check>::Visit(PlayerMapType& m)
{
    // already found
    if (i_object)
    {
        return;
    }

    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_object = itr->getSource();
            return;
        }
    }
}

template<class Check>
    void MaNGOS::PlayerListSearcher<Check>::Visit(PlayerMapType& m)
{
    for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (i_check(itr->getSource()))
        {
            i_objects.push_back(itr->getSource());
        }
    }
}

template<class Builder>
    void MaNGOS::LocalizedPacketDo<Builder>::operator()(Player* p)
{
    int32 loc_idx = p->GetSession()->GetSessionDbLocaleIndex();
    uint32 cache_idx = loc_idx + 1;
    WorldPacket* data;

    // create if not cached yet
    if (i_data_cache.size() < cache_idx + 1 || !i_data_cache[cache_idx])
    {
        if (i_data_cache.size() < cache_idx + 1)
        {
            i_data_cache.resize(cache_idx + 1);
        }

        data = new WorldPacket();

        i_builder(*data, loc_idx);

        i_data_cache[cache_idx] = data;
    }
    else
    {
        data = i_data_cache[cache_idx];
    }

    p->SendDirectMessage(data);
}

template<class Builder>
    void MaNGOS::LocalizedPacketListDo<Builder>::operator()(Player* p)
{
    int32 loc_idx = p->GetSession()->GetSessionDbLocaleIndex();
    uint32 cache_idx = loc_idx + 1;
    WorldPacketList* data_list;

    // create if not cached yet
    if (i_data_cache.size() < cache_idx + 1 || i_data_cache[cache_idx].empty())
    {
        if (i_data_cache.size() < cache_idx + 1)
        {
            i_data_cache.resize(cache_idx + 1);
        }

        data_list = &i_data_cache[cache_idx];

        i_builder(*data_list, loc_idx);
    }
    else
    {
        data_list = &i_data_cache[cache_idx];
    }

    for (size_t i = 0; i < data_list->size(); ++i)
    {
        p->SendDirectMessage((*data_list)[i]);
    }
}
