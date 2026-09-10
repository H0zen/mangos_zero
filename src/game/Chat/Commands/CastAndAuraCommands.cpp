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

#include <string>
#include "Chat.h"
#include "Language.h"
#include "SpellAuras.h"
#include "SpellMgr.h"

bool AddAuraToPlayer(const SpellEntry* spellInfo, Unit* target, Occupant* caster);

bool ChatHandler::HandleCastCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell)
    {
        return false;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo)
    {
        return false;
    }

    if (!m_session)
    {
        target->CastSpell(target, spell, true);
        PSendSysMessage("Cast spell %u (%s) on %s",
            spell, spellInfo->Name_lang[GetSessionDbcLocale()],
            target->GetName());
        return true;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args)
    {
        return false;
    }

    m_session->GetPlayer()->CastSpell(target, spell, triggered);

    return true;
}

bool ChatHandler::HandleCastBackCommand(char* args)
{
    Creature* caster = getSelectedCreature();

    if (!caster)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell || !sSpellStore.LookupEntry(spell))
    {
        return false;
    }

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args)
    {
        return false;
    }

    caster->SetFacingToObject(m_session->GetPlayer());

    caster->CastSpell(m_session->GetPlayer(), spell, triggered);

    return true;
}

bool ChatHandler::HandleCastDistCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell)
    {
        return false;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo)
    {
        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    float dist;
    if (!ExtractFloat(&args, dist))
    {
        return false;
    }

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args)
    {
        return false;
    }

    float x, y, z;
    ClosePointNear(*m_session->GetPlayer(), x, y, z, dist);

    m_session->GetPlayer()->CastSpell(x, y, z, spell, triggered);
    return true;
}

bool ChatHandler::HandleCastTargetCommand(char* args)
{
    Creature* caster = getSelectedCreature();

    if (!caster)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!caster->getVictim())
    {
        SendSysMessage(LANG_SELECTED_TARGET_NOT_HAVE_VICTIM);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell || !sSpellStore.LookupEntry(spell))
    {
        return false;
    }

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args)
    {
        return false;
    }

    caster->SetFacingToObject(m_session->GetPlayer());

    caster->CastSpell(caster->getVictim(), spell, triggered);

    return true;
}

bool ChatHandler::HandleCastSelfCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell)
    {
        return false;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo)
    {
        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args)
    {
        return false;
    }

    target->CastSpell(target, spell, triggered);

    return true;
}

bool ChatHandler::HandleAuraCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 spellID = ExtractSpellIdFromLink(&args);

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellID);
    if (!spellInfo)
    {
        return false;
    }

    if (!IsSpellAppliesAura(spellInfo, (1 << EFFECT_INDEX_0) | (1 << EFFECT_INDEX_1) | (1 << EFFECT_INDEX_2)) &&
        !spellInfo->HasSpellEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA))
    {
        PSendSysMessage(LANG_SPELL_NO_HAVE_AURAS, spellID);
        SetSentErrorMessage(true);
        return false;
    }

    return AddAuraToPlayer(spellInfo, target, m_session->GetPlayer());
}

bool ChatHandler::HandleUnAuraCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    std::string argstr = args;
    if (argstr == "all")
    {
        target->RemoveAllAuras();
        return true;
    }

    uint32 spellID = ExtractSpellIdFromLink(&args);
    if (!spellID)
    {
        return false;
    }

    target->RemoveAuras(spellID);

    return true;
}

bool ChatHandler::HandleAuraGroupCommand(char* args)
{

    uint32 spellID = ExtractSpellIdFromLink(&args);

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellID);
    if (!spellInfo)
    {
        PSendSysMessage("Spell %u does not exists", spellID);
        return false;
    }

    if (!IsSpellAppliesAura(spellInfo, (1 << EFFECT_INDEX_0) | (1 << EFFECT_INDEX_1) | (1 << EFFECT_INDEX_2)) &&
        !spellInfo->HasSpellEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA))
    {
        PSendSysMessage(LANG_SPELL_NO_HAVE_AURAS, spellID);
        SetSentErrorMessage(true);
        return false;
    }

    Unit* rawTarget = getSelectedUnit();
    Player* playerTarget ;

    if (rawTarget)
    {
        if (IsCreature(rawTarget))
        {
            SendSysMessage(LANG_NO_CHAR_SELECTED);
            SetSentErrorMessage(true);
            return false;
        }

        playerTarget = (Player*)rawTarget;
    }
    else
    {
        playerTarget = m_session->GetPlayer();
    }

    Group* grp = playerTarget->GetGroup();

    if (!grp)
    {
        std::string nameLink = GetNameLink(playerTarget);

        if (playerTarget->IsDead())
        {
            PSendSysMessage(LANG_COMMAND_AURAGROUP_CANNOT_APPLY_AURA_PLAYER_IS_DEAD, nameLink.c_str());
            return false;
        }
        else
        {
            AddAuraToPlayer(spellInfo, playerTarget, m_session->GetPlayer());
            PSendSysMessage(LANG_COMMAND_AURAGROUP_AURA_APPLIED, spellInfo->ID, nameLink.c_str());
            return true;
        }
    }
    else
    {

        for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pl = itr->getSource();

            if (!pl || !pl->GetSession())
            {
                continue;
            }

            std::string nameLink = GetNameLink(pl);

            if (pl->IsDead())
            {
                PSendSysMessage(LANG_COMMAND_AURAGROUP_CANNOT_APPLY_AURA_PLAYER_IS_DEAD, nameLink.c_str());
                continue;
            }

            AddAuraToPlayer(spellInfo, pl, m_session->GetPlayer());
            PSendSysMessage(LANG_COMMAND_AURAGROUP_AURA_APPLIED, spellInfo->ID, nameLink.c_str());

        }

        return true;
    }
}

bool ChatHandler::HandleUnAuraGroupCommand(char* args)
{

    if (!*args)
    {
        return false;
    }

    bool removeAll = false;

    std::string argstr = args;
    if (argstr == "all")
    {
        removeAll = true;
    }

    uint32 spellIdToRemove;

    if (!removeAll)
    {
        spellIdToRemove = ExtractSpellIdFromLink(&args);
        if (!spellIdToRemove)
        {
            return false;
        }
    }

    Unit* rawTarget = getSelectedUnit();
    Player* playerTarget;

    if (rawTarget)
    {
        if (IsCreature(rawTarget))
        {
            SendSysMessage(LANG_NO_CHAR_SELECTED);
            SetSentErrorMessage(true);
            return false;
        }

        playerTarget = (Player*)rawTarget;
    }
    else
    {
        playerTarget = m_session->GetPlayer();
    }

    Group* grp = playerTarget->GetGroup();

    if (!grp)
    {
        std::string nameLink = GetNameLink(playerTarget);

        if (playerTarget->IsDead())
        {
            PSendSysMessage(LANG_COMMAND_AURAGROUP_CANNOT_UNAURA_DEAD_PLAYER, nameLink.c_str());
            return false;
        }
        else
        {
            if (removeAll)
            {
                playerTarget->RemoveAllAuras();
                PSendSysMessage(LANG_COMMAND_AURAGROUP_ALL_AURA_REMOVED, nameLink.c_str());
            }
            else
            {
                playerTarget->RemoveAuras(spellIdToRemove);
                PSendSysMessage(LANG_COMMAND_AURAGROUP_AURA_REMOVED_FOR_SPELL, spellIdToRemove, nameLink.c_str());
            }

            return true;
        }
    }
    else
    {

        for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pl = itr->getSource();

            if (!pl || !pl->GetSession())
            {
                continue;
            }

            std::string nameLink = GetNameLink(pl);
            if (pl->IsDead())
            {
                PSendSysMessage(LANG_COMMAND_AURAGROUP_CANNOT_UNAURA_DEAD_PLAYER, nameLink.c_str());
                continue;
            }
            else
            {
                if (removeAll)
                {
                    pl->RemoveAllAuras();
                    PSendSysMessage(LANG_COMMAND_AURAGROUP_ALL_AURA_REMOVED, nameLink.c_str());
                }
                else
                {
                    pl->RemoveAuras(spellIdToRemove);
                    PSendSysMessage(LANG_COMMAND_AURAGROUP_AURA_REMOVED_FOR_SPELL, spellIdToRemove, nameLink.c_str());
                }

            }

        }

        return true;
    }
}
