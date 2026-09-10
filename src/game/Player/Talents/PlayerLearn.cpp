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

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>

void Player::resetSpells()
{

    if (HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        RemoveAtLoginFlag(AT_LOGIN_RESET_SPELLS, true);
    }

    PlayerSpellMap smap = GetSpellMap();

    for (PlayerSpellMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
    {
        removeSpell(iter->first, false, false);
    }

    learnDefaultSpells();
    learnQuestRewardedSpells();
}

void Player::learnDefaultSpells()
{

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    for (PlayerCreateInfoSpells::const_iterator itr = info->spell.begin(); itr != info->spell.end(); ++itr)
    {
        uint32 tspell = *itr;
        DEBUG_LOG("PLAYER (Class: %u Race: %u): Adding initial spell, id = %u", uint32(getClass()), uint32(getRace()), tspell);
        if (!IsInWorld())
        {
            addSpell(tspell, true, true, true, false);
        }
        else
        {
            learnSpell(tspell, true);
        }
    }
}

void Player::learnQuestRewardedSpells(Quest const* quest)
{
    uint32 spell_id = quest->GetRewSpellCast();

    if (!spell_id)
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        return;
    }

    bool found = false;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->Effect[i] == SPELL_EFFECT_LEARN_SPELL && !HasSpell(spellInfo->EffectTriggerSpell[i]))
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        return;
    }

    uint32 learned_0 = spellInfo->EffectTriggerSpell[EFFECT_INDEX_0];
    if (sSpellMgr.GetSpellRank(learned_0) > 1 && !HasSpell(learned_0))
    {

        uint32 first_spell = sSpellMgr.GetFirstSpellInChain(learned_0);
        if (!HasSpell(first_spell))
        {
            return;
        }

        SpellEntry const* learnedInfo = sSpellStore.LookupEntry(learned_0);
        if (!learnedInfo)
        {
            return;
        }

        if (learnedInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_TRADE_SKILL && learnedInfo->Effect[EFFECT_INDEX_1] == 0)
        {

            for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
            {
                if (itr->second.state == PLAYERSPELL_REMOVED || itr->first == learned_0)
                {
                    continue;
                }

                SpellEntry const* itrInfo = sSpellStore.LookupEntry(itr->first);
                if (!itrInfo)
                {
                    return;
                }

                if (itrInfo->Effect[EFFECT_INDEX_0] != SPELL_EFFECT_TRADE_SKILL || itrInfo->Effect[EFFECT_INDEX_1] != 0)
                {
                    continue;
                }

                if (sSpellMgr.GetFirstSpellInChain(itr->first) != first_spell)
                {
                    continue;
                }

                if (!sSpellMgr.IsHighRankOfSpell(learned_0, itr->first))
                {
                    return;
                }
            }
        }
    }

    CastSpell(this, spell_id, true);
}

void Player::learnQuestRewardedSpells()
{

    for (auto itr = m_journal.All().begin(); itr != m_journal.All().end(); ++itr)
    {

        if (!itr->second.m_rewarded)
        {
            continue;
        }

        Quest const* quest = sObjectMgr.GetQuestTemplate(itr->first);
        if (!quest)
        {
            continue;
        }

        learnQuestRewardedSpells(quest);
    }
}

void Player::learnSkillRewardedSpells(uint32 skill_id, uint32 skill_value)
{
    uint32 raceMask  = getRaceMask();
    uint32 classMask = getClassMask();
    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* pAbility = sSkillLineAbilityStore.LookupEntry(j);
        if (!pAbility || pAbility->SkillLine != skill_id || pAbility->AcquireMethod != ABILITY_LEARNED_ON_GET_PROFESSION_SKILL)
        {
            continue;
        }

        if (pAbility->RaceMask && !(pAbility->RaceMask & raceMask))
        {
            continue;
        }

        if (pAbility->ClassMask && !(pAbility->ClassMask & classMask))
        {
            continue;
        }

        if (sSpellStore.LookupEntry(pAbility->Spell))
        {

            if (skill_value < pAbility->MinSkillLineRank)
            {
                removeSpell(pAbility->Spell);
            }

            else if (!IsInWorld())
            {
                addSpell(pAbility->Spell, true, true, true, false);
            }
            else
            {
                learnSpell(pAbility->Spell, true);
            }
        }
    }
}
