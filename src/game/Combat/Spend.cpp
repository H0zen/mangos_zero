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

#include "Combat/Spend.h"

#include "SpellAuras.h"
#include "Unit.h"

#include <vector>

namespace combat
{
    namespace
    {
        /// The shield a share was decided against, found again by the pair that
        /// identifies an aura: the spell, and who cast it.
        Aura* FindShield(Unit& victim, AuraType type, const AbsorbShare& share)
        {
            for (Aura* aura : victim.GetAurasByType(type))
            {
                if (aura->GetId() == share.spellId && aura->GetCasterGuid() == share.caster)
                {
                    return aura;
                }
            }
            return nullptr;
        }
    }

    void SpendShields(Unit& victim, const Outcome& outcome)
    {
        if (outcome.absorbs.empty())
        {
            return;
        }

        // Collected first and removed after, because removing an aura rewrites
        // the list this walks.
        std::vector<uint32> spent;

        for (const AbsorbShare& share : outcome.absorbs)
        {
            Aura* aura = FindShield(victim, SPELL_AURA_SCHOOL_ABSORB, share);
            if (!aura)
            {
                aura = FindShield(victim, SPELL_AURA_MANA_SHIELD, share);
            }

            // Gone between the decision and here -- dispelled by something that
            // ran in between. The share simply does not happen.
            if (!aura)
            {
                continue;
            }

            if (Modifier* mod = aura->GetModifier())
            {
                mod->m_amount -= share.amount;
                if (mod->m_amount < 0)
                {
                    mod->m_amount = 0;
                }
            }

            if (share.exhausted)
            {
                spent.push_back(share.spellId);
            }
        }

        if (outcome.manaSpent > 0)
        {
            victim.ApplyPowerMod(POWER_MANA, uint32(outcome.manaSpent), false);
        }

        for (const uint32 spellId : spent)
        {
            victim.RemoveAurasDueToSpell(spellId, nullptr, AURA_REMOVE_BY_SHIELD_BREAK);
        }
    }
}
