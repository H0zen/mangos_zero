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

#include "Utilities/Errors.h"
#include <string>
#include <vector>
#include "CreatureAISelector.h"
#include "Creature.h"
#include "CreatureAIImpl.h"
#include "NullCreatureAI.h"
#include "Policies/Singleton.h"
#include "MovementGenerator.h"
#include "ScriptMgr.h"
#include "Pet.h"
#include "Log.h"

namespace FactorySelector
{

    CreatureAI* selectAI(Creature* creature)
    {

        if ((!creature->IsPet() || !((Pet*)creature)->isControlled()) && !creature->IsCharmed())
        {
            if (CreatureAI* scriptedAI = sScriptMgr.GetCreatureAI(creature))
            {
                return scriptedAI;
            }
        }

        CreatureAIRegistry& ai_registry(CreatureAIRepository::Instance());

        const CreatureAICreator* ai_factory = nullptr;

        std::string ainame = creature->GetAIName();

        Unit* owner = nullptr;
        if ((creature->IsPet() && ((Pet*)creature)->isControlled() &&
            ((owner = creature->GetOwner()) &&IsPlayer(owner))) || creature->IsCharmed())
        {
            ai_factory = ai_registry.GetRegistryItem("PetAI");
        }
        else if (creature->IsTotem())
        {
            ai_factory = ai_registry.GetRegistryItem("TotemAI");
        }

        if (!ai_factory && !ainame.empty())
        {
            ai_factory = ai_registry.GetRegistryItem(ainame.c_str());
        }

        if (!ai_factory && creature->IsGuard())
        {
            ai_factory = ai_registry.GetRegistryItem("GuardAI");
        }

        if (!ai_factory)
        {
            int best_val = PERMIT_BASE_NO;
            typedef CreatureAIRegistry::RegistryMapType RMT;
            RMT const& l = ai_registry.GetRegisteredItems();
            for (RMT::const_iterator iter = l.begin(); iter != l.end(); ++iter)
            {
                const CreatureAICreator* factory = iter->second;
                const SelectableAI* p = dynamic_cast<const SelectableAI*>(factory);
                MANGOS_ASSERT(p != nullptr);
                int val = p->Permit(creature);
                if (val > best_val)
                {
                    best_val = val;
                    ai_factory = p;
                }
            }
        }

        ainame = (ai_factory == nullptr) ? "NullCreatureAI" : ai_factory->key();

        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature %u used AI is %s.", creature->GetGUIDLow(), ainame.c_str());
        return (ai_factory == nullptr ? new NullCreatureAI(creature) : ai_factory->Create(creature));
    }

    MovementGenerator* selectMovementGenerator(Creature* creature)
    {
        MovementGeneratorRegistry& mv_registry(MovementGeneratorRepository::Instance());
        MANGOS_ASSERT(creature->GetCreatureInfo() != nullptr);
        MovementGeneratorCreator const* mv_factory = mv_registry.GetRegistryItem(
            (creature->GetOwnerGuid() != 0 && GuidHigh(creature->GetOwnerGuid()) == HIGHGUID_PLAYER) ? FOLLOW_MOTION_TYPE : creature->GetDefaultMovementType());

        return (mv_factory == nullptr ? nullptr : mv_factory->Create(creature));
    }
}
