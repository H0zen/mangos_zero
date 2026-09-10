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
#include "CinematicFlyover.h"
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
#include "LFGMgr.h"
#include "DisableMgr.h"
#include <cmath>
#include "Cast/Recipe/RecipeBook.h"

bool Player::addSpell(uint32 spell_id, bool active, bool learning, bool dependent, bool disabled)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {

        if (!IsInWorld() && !learning)
        {
            sLog.outError("Player::addSpell: nonexistent in SpellStore spell #%u request, deleting for all characters in `character_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM `character_spell` WHERE `spell` = '%u'", spell_id);
        }
        else
        {
            sLog.outError("Player::addSpell: nonexistent in SpellStore spell #%u request.", spell_id);
        }

        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, this, false))
    {

        if (!IsInWorld() && !learning)
        {
            sLog.outError("Player::addSpell: Broken spell #%u learning not allowed, deleting for all characters in `character_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM `character_spell` WHERE `spell` = '%u'", spell_id);
        }
        else
        {
            sLog.outError("Player::addSpell: Broken spell #%u learning not allowed.", spell_id);
        }

        return false;
    }

    PlayerSpellState state = learning ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;

    bool disabled_case = false;

    PlayerSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        uint32 next_active_spell_id = 0;
        bool dependent_set = false;

        if (sSpellMgr.IsRankedSpellNonStackableInSpellBook(spellInfo))
        {
            SpellChainMapNext const& nextMap = sSpellMgr.GetSpellChainNext();
            for (SpellChainMapNext::const_iterator next_itr = nextMap.lower_bound(spell_id); next_itr != nextMap.upper_bound(spell_id); ++next_itr)
            {
                if (HasSpell(next_itr->second))
                {

                    active = false;
                    next_active_spell_id = next_itr->second;
                    break;
                }
            }
        }

        PlayerSpell& playerSpell = itr->second;

        if (playerSpell.state != PLAYERSPELL_REMOVED && playerSpell.active == active &&
            playerSpell.dependent == dependent && playerSpell.disabled == disabled)
        {
            if (!IsInWorld() && !learning)
            {
                playerSpell.state = PLAYERSPELL_UNCHANGED;
            }

            return false;
        }

        if (playerSpell.state != PLAYERSPELL_REMOVED && !playerSpell.dependent && dependent)
        {
            playerSpell.dependent = dependent;
            if (playerSpell.state != PLAYERSPELL_NEW)
            {
                playerSpell.state = PLAYERSPELL_CHANGED;
            }
            dependent_set = true;
        }

        if (playerSpell.active != active && playerSpell.state != PLAYERSPELL_REMOVED && !playerSpell.disabled)
        {
            playerSpell.active = active;

            if (!IsInWorld() && !learning && !dependent_set)
            {
                playerSpell.state = PLAYERSPELL_UNCHANGED;
            }
            else if (playerSpell.state != PLAYERSPELL_NEW)
            {
                playerSpell.state = PLAYERSPELL_CHANGED;
            }

            if (active)
            {
                if (IsNeedCastPassiveLikeSpellAtLearn(spellInfo))
                {
                    CastSpell(this, spell_id, true);
                }
            }
            else if (IsInWorld())
            {
                if (!next_active_spell_id)
                {
                    WorldPacket data(SMSG_REMOVED_SPELL, 4);
                    data << uint16(spell_id);
                    GetSession()->SendPacket(&data);
                }
            }

            return active;
        }

        if (playerSpell.disabled != disabled && playerSpell.state != PLAYERSPELL_REMOVED)
        {
            if (playerSpell.state != PLAYERSPELL_NEW)
            {
                playerSpell.state = PLAYERSPELL_CHANGED;
            }
            playerSpell.disabled = disabled;

            if (disabled)
            {
                return false;
            }

            disabled_case = true;
        }
        else switch (playerSpell.state)
        {
            case PLAYERSPELL_UNCHANGED:
                return false;
            case PLAYERSPELL_REMOVED:
            {
                m_spells.erase(itr);
                state = PLAYERSPELL_CHANGED;
                break;
            }
            default:
            {

                if (!IsInWorld() && !learning && !dependent_set)
                {
                    playerSpell.state = PLAYERSPELL_UNCHANGED;
                }

                return false;
            }
        }
    }

    TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id);
    bool canAddToSpellBook = true;

    if (!disabled_case)
    {

        if (talentPos)
        {
            if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
            {
                for (int i = 0; i < MAX_TALENT_RANK; ++i)
                {

                    uint32 rankSpellId = talentInfo->RankID[i];
                    if (!rankSpellId || rankSpellId == spell_id)
                    {
                        continue;
                    }

                    removeSpell(rankSpellId, false, false);
                }
            }
        }

        else if (uint32 prev_spell = sSpellMgr.GetPrevSpellInChain(spell_id))
        {
            if (!IsInWorld() || disabled)
            {
                addSpell(prev_spell, active, true, true, disabled);
            }
            else
            {
                learnSpell(prev_spell, true);
            }
        }

        PlayerSpell newspell;
        newspell.state     = state;
        newspell.active    = active;
        newspell.dependent = dependent;
        newspell.disabled  = disabled;

        if (newspell.active && !newspell.disabled)
        {
            do
            {
                uint32 prev_spell_id = sSpellMgr.GetPrevSpellInChain(spell_id);
                if (!prev_spell_id)
                {
                    continue;
                }

                if ((m_spells.find(prev_spell_id) == m_spells.end()))
                {
                    continue;
                }

                PlayerSpell* lowerRank = &m_spells[prev_spell_id];
                if (lowerRank->state == PLAYERSPELL_REMOVED || !lowerRank->active)
                {
                    continue;
                }

                SpellEntry const *spell_old = sSpellStore.LookupEntry(prev_spell_id);
                SpellEntry const *spell_new = spellInfo;

                if (sSpellMgr.IsRankedSpellNonStackableInSpellBook(spell_old))
                {
                    if (IsInWorld())
                    {
                        WorldPacket data(SMSG_SUPERCEDED_SPELL, (4));
                        data << uint16(spell_old->ID);
                        data << uint16(spell_new->ID);
                        GetSession()->SendPacket(&data);
                    }

                    lowerRank->active = false;
                    if (lowerRank->state != PLAYERSPELL_NEW)
                    {
                        lowerRank->state = PLAYERSPELL_CHANGED;
                    }

                    canAddToSpellBook = false;
                }
            } while (0);
        }

        m_spells[spell_id] = newspell;

        if (newspell.disabled)
        {
            return false;
        }
    }

    if (talentPos)
    {

        m_usedTalentCount += GetTalentSpellCost(talentPos);
        UpdateFreeTalentPoints(false);
    }

    if (uint32 freeProfs = GetFreePrimaryProfessionPoints())
    {
        if (sSpellMgr.IsPrimaryProfessionFirstRankSpell(spell_id))
        {
            SetFreePrimaryProfessions(freeProfs - 1);
        }
    }

    if (talentPos && spellInfo->HasSpellEffect(SPELL_EFFECT_LEARN_SPELL))
    {

        CastSpell(this, spell_id, true);
    }

    else if (IsNeedCastPassiveLikeSpellAtLearn(spellInfo))
    {
        CastSpell(this, spell_id, true);
    }
    else if (spellInfo->HasSpellEffect(SPELL_EFFECT_SKILL_STEP))
    {
        CastSpell(this, spell_id, true);
        return false;
    }

    uint16 maxskill = GetMaxSkillValueForLevel();

    SpellLearnSkillNode const* spellLearnSkill = sSpellMgr.GetSpellLearnSkill(spell_id);

    if (spellLearnSkill)
    {
        uint32 skill_value = GetPureSkillValue(spellLearnSkill->skill);
        uint32 skill_max_value = GetPureMaxSkillValue(spellLearnSkill->skill);

        if (skill_value < spellLearnSkill->value)
        {
            skill_value = spellLearnSkill->value;
        }

        uint32 new_skill_max_value = spellLearnSkill->maxvalue == 0 ? maxskill : spellLearnSkill->maxvalue;

        if (skill_max_value < new_skill_max_value)
        {
            skill_max_value =  new_skill_max_value;
        }

        SetSkill(spellLearnSkill->skill, skill_value, skill_max_value, spellLearnSkill->step);
    }
    else
    {

        SkillLineAbilityMapBounds skill_bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spell_id);

        for (SkillLineAbilityMap::const_iterator _spell_idx = skill_bounds.first; _spell_idx != skill_bounds.second; ++_spell_idx)
        {
            SkillLineAbilityEntry const* skillAbility = _spell_idx->second;
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skillAbility->SkillLine);
            if (!pSkill)
            {
                continue;
            }

            if (HasSkill(pSkill->ID))
            {
                continue;
            }

            if (skillAbility->AcquireMethod == ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL ||

                (pSkill->ID == SKILL_POISONS && skillAbility->TrivialSkillLineRankHigh == 0) ||

                (pSkill->ID == SKILL_LOCKPICKING && skillAbility->TrivialSkillLineRankHigh == 0))
            {
                switch (GetSkillRangeType(pSkill, skillAbility->RaceMask != 0))
                {
                    case SKILL_RANGE_LANGUAGE:
                        SetSkill(pSkill->ID, 300, 300);
                        break;
                    case SKILL_RANGE_LEVEL:
                        SetSkill(pSkill->ID, 1, GetMaxSkillValueForLevel());
                        break;
                    case SKILL_RANGE_MONO:
                        SetSkill(pSkill->ID, 1, 1);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    SpellLearnSpellMapBounds spell_bounds = sSpellMgr.GetSpellLearnSpellMapBounds(spell_id);

    for (SpellLearnSpellMap::const_iterator itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
    {
        SpellLearnSpellNode const& spellLearn = itr2->second;
        if (!spellLearn.autoLearned)
        {
            if (!IsInWorld() || !spellLearn.active)
            {
                addSpell(spellLearn.spell, spellLearn.active, true, true, false);
            }
            else
            {
                learnSpell(spellLearn.spell, true);
            }
        }
    }

    return active && !disabled && canAddToSpellBook;
}

bool Player::IsNeedCastPassiveLikeSpellAtLearn(SpellEntry const* spellInfo) const
{
    ShapeshiftForm form = GetShapeshiftForm();

    if (IsNeedCastSpellAtFormApply(spellInfo, form))
    {
        return true;
    }

    if (!cast::RecipeOf(*spellInfo).Says().passive)
    {
        return false;
    }

    bool need_cast = !spellInfo->ShapeshiftMask || (!form && cast::RecipeOf(*spellInfo).Says().worksWithoutShapeshift);

    return need_cast && (!spellInfo->CasterAuraState || HasAuraState(AuraState(spellInfo->CasterAuraState)));
}

void Player::learnSpell(uint32 spell_id, bool dependent)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);

    bool disabled = (itr != m_spells.end()) ? itr->second.disabled : false;
    bool active = disabled ? itr->second.active : true;

    bool learning = addSpell(spell_id, active, true, dependent, false);

    if (learning && IsInWorld())
    {
        WorldPacket data(SMSG_LEARNED_SPELL, 4);
        data << uint32(spell_id);
        GetSession()->SendPacket(&data);
    }

    if (disabled)
    {
        SpellChainMapNext const& nextMap = sSpellMgr.GetSpellChainNext();
        for (SpellChainMapNext::const_iterator i = nextMap.lower_bound(spell_id); i != nextMap.upper_bound(spell_id); ++i)
        {
            PlayerSpellMap::iterator iter = m_spells.find(i->second);
            if (iter != m_spells.end() && iter->second.disabled)
            {
                learnSpell(i->second, false);
            }
        }
    }
}

void Player::removeSpell(uint32 spell_id, bool disabled, bool learn_low_rank)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
    {
        return;
    }

    PlayerSpell& playerSpell = itr->second;
    if (playerSpell.state == PLAYERSPELL_REMOVED || (disabled && playerSpell.disabled))
    {
        return;
    }

    SpellChainMapNext const& nextMap = sSpellMgr.GetSpellChainNext();
    for (SpellChainMapNext::const_iterator itr2 = nextMap.lower_bound(spell_id); itr2 != nextMap.upper_bound(spell_id); ++itr2)
    {
        if (HasSpell(itr2->second) && !GetTalentSpellPos(itr2->second))
        {
            removeSpell(itr2->second, disabled, false);
        }
    }

    itr = m_spells.find(spell_id);
    if (itr == m_spells.end() || playerSpell.state == PLAYERSPELL_REMOVED)
    {
        return;
    }

    bool cur_active = playerSpell.active;
    bool cur_dependent = playerSpell.dependent;

    if (disabled)
    {
        playerSpell.disabled = disabled;
        if (playerSpell.state != PLAYERSPELL_NEW)
        {
            playerSpell.state = PLAYERSPELL_CHANGED;
        }
    }
    else
    {
        if (playerSpell.state == PLAYERSPELL_NEW)
        {
            m_spells.erase(itr);
        }
        else
        {
            playerSpell.state = PLAYERSPELL_REMOVED;
        }
    }

    RemoveAuras(spell_id);

    if (PetAura const* petSpell = sSpellMgr.GetPetAura(spell_id))
    {
        RemovePetAura(petSpell);
    }

    TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id);
    if (talentPos)
    {

        uint32 talentCosts = GetTalentSpellCost(talentPos);

        if (talentCosts < m_usedTalentCount)
        {
            m_usedTalentCount -= talentCosts;
        }
        else
        {
            m_usedTalentCount = 0;
        }

        UpdateFreeTalentPoints(false);
    }

    if (sSpellMgr.IsPrimaryProfessionFirstRankSpell(spell_id))
    {
        uint32 freeProfs = GetFreePrimaryProfessionPoints() + 1;
        uint32 maxProfs = GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT)) ? sWorld.getConfig(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL) : 10;
        if (freeProfs <= maxProfs)
        {
            SetFreePrimaryProfessions(freeProfs);
        }
    }

    SpellLearnSkillNode const* spellLearnSkill = sSpellMgr.GetSpellLearnSkill(spell_id);
    if (spellLearnSkill)
    {
        uint32 prev_spell = sSpellMgr.GetPrevSpellInChain(spell_id);
        if (!prev_spell)
        {
            SetSkill(spellLearnSkill->skill, 0, 0);
        }
        else
        {

            SpellLearnSkillNode const* prevSkill = sSpellMgr.GetSpellLearnSkill(prev_spell);
            while (!prevSkill && prev_spell)
            {
                prev_spell = sSpellMgr.GetPrevSpellInChain(prev_spell);
                prevSkill = sSpellMgr.GetSpellLearnSkill(sSpellMgr.GetFirstSpellInChain(prev_spell));
            }

            if (!prevSkill)
            {
                SetSkill(spellLearnSkill->skill, 0, 0);
            }
            else
            {
                uint32 skill_value = GetPureSkillValue(prevSkill->skill);
                uint32 skill_max_value = GetPureMaxSkillValue(prevSkill->skill);

                if (skill_value >  prevSkill->value)
                {
                    skill_value = prevSkill->value;
                }

                uint32 new_skill_max_value = prevSkill->maxvalue == 0 ? GetMaxSkillValueForLevel() : prevSkill->maxvalue;

                if (skill_max_value > new_skill_max_value)
                {
                    skill_max_value =  new_skill_max_value;
                }

                SetSkill(prevSkill->skill, skill_value, skill_max_value, prevSkill->step);
            }
        }
    }
    else
    {

        SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spell_id);

        for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
        {
            SkillLineAbilityEntry const* skillAbility = _spell_idx->second;
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skillAbility->SkillLine);
            if (!pSkill)
            {
                continue;
            }

            if ((skillAbility->AcquireMethod == ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL &&
                pSkill->CategoryID != SKILL_CATEGORY_CLASS) ||

                ((pSkill->ID == SKILL_POISONS || pSkill->ID == SKILL_LOCKPICKING) && skillAbility->TrivialSkillLineRankHigh == 0))
            {

                if ((pSkill->CategoryID == SKILL_CATEGORY_SECONDARY || pSkill->CategoryID == SKILL_CATEGORY_PROFESSION) &&
                    (IsProfessionSkill(pSkill->ID) || skillAbility->RaceMask != 0))
                {
                    continue;
                }

                SetSkill(pSkill->ID, 0, 0);
            }
        }
    }

    SpellLearnSpellMapBounds spell_bounds = sSpellMgr.GetSpellLearnSpellMapBounds(spell_id);

    for (SpellLearnSpellMap::const_iterator itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
    {
        removeSpell(itr2->second.spell, disabled);
    }

    bool prev_activate = false;

    if (uint32 prev_id = sSpellMgr.GetPrevSpellInChain(spell_id))
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);

        if (talentPos)
        {
            if (learn_low_rank)
            {
                learnSpell(prev_id, false);
            }
        }

        else if (cur_active && sSpellMgr.IsRankedSpellNonStackableInSpellBook(spellInfo))
        {

            PlayerSpellMap::iterator prev_itr = m_spells.find(prev_id);
            if (prev_itr != m_spells.end())
            {
                PlayerSpell& spell = prev_itr->second;
                if (spell.dependent != cur_dependent)
                {
                    spell.dependent = cur_dependent;
                    if (spell.state != PLAYERSPELL_NEW)
                    {
                        spell.state = PLAYERSPELL_CHANGED;
                    }
                }

                if (cur_active && !spell.active && learn_low_rank)
                {
                    if (addSpell(prev_id, true, false, spell.dependent, spell.disabled))
                    {

                        WorldPacket data(SMSG_SUPERCEDED_SPELL, 4);
                        data << uint16(spell_id);
                        data << uint16(prev_id);
                        GetSession()->SendPacket(&data);
                        prev_activate = true;
                    }
                }
            }
        }
    }

    if (!prev_activate)
    {
        WorldPacket data(SMSG_REMOVED_SPELL, 4);
        data << uint16(spell_id);
        GetSession()->SendPacket(&data);
    }
}

uint32 Player::resetTalentsCost() const
{

    if (m_resetTalentsCost < 1 * GOLD)
    {
        return 1 * GOLD;
    }

    else if (m_resetTalentsCost < 5 * GOLD)
    {
        return 5 * GOLD;
    }

    else if (m_resetTalentsCost < 10 * GOLD)
    {
        return 10 * GOLD;
    }
    else
    {
        time_t months = (sWorld.GetGameTime() - m_resetTalentsTime) / MONTH;
        if (months > 0)
        {

            int32 new_cost = int32((m_resetTalentsCost) - 5 * GOLD * months);

            return uint32(new_cost < 10 * GOLD ? 10 * GOLD : new_cost);
        }
        else
        {

            int32 new_cost = m_resetTalentsCost + 5 * GOLD;

            if (new_cost > 50 * GOLD)
            {
                new_cost = 50 * GOLD;
            }
            return new_cost;
        }
    }
}

bool Player::resetTalents(bool no_cost)
{

    if (HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        RemoveAtLoginFlag(AT_LOGIN_RESET_TALENTS, true);
    }

    if (m_usedTalentCount == 0)
    {
        UpdateFreeTalentPoints(false);
        return false;
    }

    uint32 cost = 0;

    if (!no_cost)
    {
        cost = resetTalentsCost();

        if (GetMoney() < cost)
        {
            SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
            return false;
        }
    }

    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);

        if (!talentInfo)
        {
            continue;
        }

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);

        if (!talentTabInfo)
        {
            continue;
        }

        if ((getClassMask() & talentTabInfo->ClassMask) == 0)
        {
            continue;
        }

        for (int j = 0; j < MAX_TALENT_RANK; ++j)
        {
            if (talentInfo->RankID[j])
            {
                removeSpell(talentInfo->RankID[j], !cast::Recipes().StartsAs(talentInfo->RankID[j], cast::Start::Passive), false);
            }
        }
    }

    UpdateFreeTalentPoints(false);

    if (!no_cost)
    {
        ModifyMoney(-(int32)cost);

        m_resetTalentsCost = cost;
        m_resetTalentsTime = time(nullptr);
    }

    RemovePet(PET_SAVE_REAGENTS);
    return true;
}

void Player::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (target == this)
    {
        for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (m_inventory.Own(i) == nullptr)
            {
                continue;
            }

            m_inventory.Own(i)->BuildCreateUpdateBlockForPlayer(data, target);
        }
        for (int i = INVENTORY_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (m_inventory.Own(i) == nullptr)
            {
                continue;
            }

            m_inventory.Own(i)->BuildCreateUpdateBlockForPlayer(data, target);
        }
        for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
        {
            if (m_inventory.Own(i) == nullptr)
            {
                continue;
            }

            m_inventory.Own(i)->BuildCreateUpdateBlockForPlayer(data, target);
        }
    }

    Unit::BuildCreateUpdateBlockForPlayer(data, target);
}

void Player::DestroyForPlayer(Player* target) const
{
    Unit::DestroyForPlayer(target);

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_inventory.Own(i) == nullptr)
        {
            continue;
        }

        m_inventory.Own(i)->DestroyForPlayer(target);
    }

    if (target == this)
    {
        for (int i = INVENTORY_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (m_inventory.Own(i) == nullptr)
            {
                continue;
            }

            m_inventory.Own(i)->DestroyForPlayer(target);
        }
        for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
        {
            if (m_inventory.Own(i) == nullptr)
            {
                continue;
            }

            m_inventory.Own(i)->DestroyForPlayer(target);
        }
    }
}

bool Player::HasSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    if (itr == m_spells.end())
    {
        return false;
    }

    PlayerSpell const& playerSpell = itr->second;
    return playerSpell.state != PLAYERSPELL_REMOVED && !playerSpell.disabled;
}

bool Player::HasActiveSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    if (itr == m_spells.end())
    {
        return false;
    }

    PlayerSpell const& playerSpell = itr->second;
    return playerSpell.state != PLAYERSPELL_REMOVED && playerSpell.active && !playerSpell.disabled;
}

TrainerSpellState Player::GetTrainerSpellState(TrainerSpell const* trainer_spell, uint32 reqLevel) const
{
    if (!trainer_spell)
    {
        return TRAINER_SPELL_RED;
    }

    if (!trainer_spell->spell)
    {
        return TRAINER_SPELL_RED;
    }

    SpellEntry const* spell = sSpellStore.LookupEntry(trainer_spell->spell);
    SpellEntry const* TriggerSpell = sSpellStore.LookupEntry(spell->EffectTriggerSpell[0]);

    if (HasSpell(TriggerSpell->ID))
    {
        return TRAINER_SPELL_GRAY;
    }

    if (!IsSpellFitByClassAndRace(TriggerSpell->ID))
    {
        return TRAINER_SPELL_RED;
    }

    bool prof = SpellMgr::IsProfessionSpell(trainer_spell->spell);

    uint32 spellLevel = reqLevel ? reqLevel : TriggerSpell->SpellLevel;
    if (getLevel() < spellLevel)
    {
        return TRAINER_SPELL_RED;
    }

    if (SpellChainNode const* spell_chain = sSpellMgr.GetSpellChainNode(TriggerSpell->ID))
    {

        if (spell_chain->prev && !HasSpell(spell_chain->prev))
        {
            return TRAINER_SPELL_RED;
        }

        if (spell_chain->req && !HasSpell(spell_chain->req))
        {
            return TRAINER_SPELL_RED;
        }
    }

    if (!prof || GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL)))
    {
        if (trainer_spell->reqSkill && GetBaseSkillValue(trainer_spell->reqSkill) < trainer_spell->reqSkillValue)
        {
            return TRAINER_SPELL_RED;
        }
    }

    uint32 skill = spell->EffectMiscValue[1];

    if (spell->Effect[1] != SPELL_EFFECT_SKILL || !IsPrimaryProfessionSkill(skill))
    {
        return TRAINER_SPELL_GREEN;
    }

    if (sSpellMgr.IsPrimaryProfessionFirstRankSpell(spell->ID) && GetFreePrimaryProfessionPoints() == 0)
    {
        return TRAINER_SPELL_GREEN_DISABLED;
    }

    return TRAINER_SPELL_GREEN;
}
