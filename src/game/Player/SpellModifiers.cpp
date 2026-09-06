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

#include "SpellModifiers.h"

#include "Opcodes.h"
#include "Player.h"
#include "Spell.h"
#include "WorldPacket.h"

void SpellModifiers::Add(SpellModifier* mod, bool apply)
{
    uint16 const opcode = (mod->type == SPELLMOD_FLAT) ? SMSG_SET_FLAT_SPELL_MODIFIER
                                                       : SMSG_SET_PCT_SPELL_MODIFIER;

    for (int eff = 0; eff < 64; ++eff)
    {
        uint64 const family = uint64(1) << eff;
        if (!mod->mask.IsFitToFamilyMask(family))
        {
            continue;
        }

        int32 total = 0;
        for (auto const* standing : m_byNumber[mod->op])
        {
            if (standing->type == mod->type && standing->mask.IsFitToFamilyMask(family))
            {
                total += standing->value;
            }
        }

        total += apply ? mod->value : -mod->value;

        WorldPacket data(opcode, (1 + 1 + 4));
        data << uint8(eff);
        data << uint8(mod->op);
        data << int32(total);
        m_owner.SendDirectMessage(&data);
    }

    if (apply)
    {
        m_byNumber[mod->op].push_back(mod);
        return;
    }

    if (mod->charges == -1)
    {
        --m_awaitingRemoval;
    }

    m_byNumber[mod->op].remove(mod);
    delete mod;
}

SpellModifier* SpellModifiers::From(SpellModOp op, uint32 spellId) const
{
    for (auto* standing : m_byNumber[op])
    {
        if (standing->spellId == spellId)
        {
            return standing;
        }
    }

    return nullptr;
}

bool SpellModifiers::Affects(SpellEntry const* spellInfo, SpellModifier* mod, Spell const* spell) const
{
    if (!mod || !spellInfo)
    {
        return false;
    }

    // spent, but held until the spell that spent it has finished
    if (mod->charges == -1 && mod->lastAffected)
    {
        if (spell)
        {
            if (mod->lastAffected != spell)
            {
                return false;
            }
        }
        else if (mod->lastAffected != m_owner.FindCurrentSpellBySpellId(spellInfo->ID))
        {
            return false;
        }
    }

    return mod->isAffectedOnSpell(spellInfo);
}

void SpellModifiers::Spent(Spell const* spell)
{
    if (!spell || m_awaitingRemoval == 0)
    {
        return;
    }

    for (int number = 0; number < MAX_SPELLMOD; ++number)
    {
        for (auto itr = m_byNumber[number].begin(); itr != m_byNumber[number].end();)
        {
            SpellModifier* mod = *itr;
            ++itr;

            if (mod && mod->charges == -1 && (mod->lastAffected == spell || mod->lastAffected == nullptr))
            {
                // taking the aura off calls back into Add, which edits the list
                m_owner.RemoveAuras(mod->spellId);

                if (m_byNumber[number].empty())
                {
                    break;
                }

                itr = m_byNumber[number].begin();
            }
        }
    }
}

void SpellModifiers::Restore(Spell const* spell)
{
    for (int number = 0; number < MAX_SPELLMOD; ++number)
    {
        for (auto* mod : m_byNumber[number])
        {
            if (mod->lastAffected != spell)
            {
                continue;
            }

            mod->lastAffected = nullptr;

            if (mod->charges == -1)
            {
                mod->charges = 1;
                if (m_awaitingRemoval > 0)
                {
                    --m_awaitingRemoval;
                }
            }
            else if (mod->charges > 0)
            {
                ++mod->charges;
            }
        }
    }
}
