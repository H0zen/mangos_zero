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

#include "Utilities/PackedValues.h"
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
#include "SkillGain.h"
#include "Stats/Experience.h"
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

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))

#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)

#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define SKILL_VALUE(x)         PAIR32_LOPART(x)

#define SKILL_MAX(x)           PAIR32_HIPART(x)

#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v,m)

#define SKILL_TEMP_BONUS(x)    int16(PAIR32_LOPART(x))

#define SKILL_PERM_BONUS(x)    int16(PAIR32_HIPART(x))

#define MAKE_SKILL_BONUS(t, p) MAKE_PAIR32(t,p)

void Player::HandleBaseModValue(BaseModGroup modGroup, BaseModType modType, float amount, bool apply)
{
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        sLog.outError("ERROR in HandleBaseModValue(): nonexistent BaseModGroup of wrong BaseModType!");
        return;
    }

    float val = 1.0f;

    switch (modType)
    {
        case FLAT_MOD:
            m_auraBaseMod[modGroup][modType] += apply ? amount : -amount;
            break;
        case PCT_MOD:
            if (amount <= -100.0f)
            {
                amount = -200.0f;
            }

            val = (100.0f + amount) / 100.0f;
            m_auraBaseMod[modGroup][modType] *= apply ? val : (1.0f / val);
            break;
    }

    if (!Tallied().Ready())
    {
        return;
    }

    switch (modGroup)
    {
        case CRIT_PERCENTAGE:              Sheet().Crit(BASE_ATTACK);                          break;
        case RANGED_CRIT_PERCENTAGE:       Sheet().Crit(RANGED_ATTACK);                        break;
        default: break;
    }
}

float Player::GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const
{
    if (modGroup >= BASEMOD_END || modType > MOD_END)
    {
        sLog.outError("trial to access nonexistent BaseModGroup or wrong BaseModType!");
        return 0.0f;
    }

    if (modType == PCT_MOD && m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
    {
        return 0.0f;
    }

    return m_auraBaseMod[modGroup][modType];
}

float Player::GetTotalBaseModValue(BaseModGroup modGroup) const
{
    if (modGroup >= BASEMOD_END)
    {
        sLog.outError("wrong BaseModGroup in GetTotalBaseModValue()!");
        return 0.0f;
    }

    if (m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
    {
        return 0.0f;
    }

    return m_auraBaseMod[modGroup][FLAT_MOD] * m_auraBaseMod[modGroup][PCT_MOD];
}

float Player::GetMeleeCritFromAgility()
{
    float valLevel1 = 0.0f;
    float valLevel60 = 0.0f;

    switch (getClass())
    {
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            valLevel1 = 4.6f;
            valLevel60 = 20.0f;
            break;
        case CLASS_MAGE:
            valLevel1 = 12.9f;
            valLevel60 = 20.0f;
            break;
        case CLASS_ROGUE:
            valLevel1 = 2.2f;
            valLevel60 = 29.0f;
            break;
        case CLASS_HUNTER:
            valLevel1 = 3.5f;
            valLevel60 = 53.0f;
            break;
        case CLASS_PRIEST:
            valLevel1 = 11.0f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARLOCK:
            valLevel1 = 8.4f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARRIOR:
            valLevel1 = 3.9f;
            valLevel60 = 20.0f;
            break;
        default:
            return 0.0f;
    }
    float classrate = valLevel1 * float(60.0f - getLevel()) / 59.0f + valLevel60 * float(getLevel() - 1.0f) / 59.0f;
    return GetStat(STAT_AGILITY) / classrate;
}

float Player::GetDodgeFromAgility()
{
    float valLevel1 = 0.0f;
    float valLevel60 = 0.0f;

    switch (getClass())
    {
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            valLevel1 = 4.6f;
            valLevel60 = 20.0f;
            break;
        case CLASS_MAGE:
            valLevel1 = 12.9f;
            valLevel60 = 20.0f;
            break;
        case CLASS_ROGUE:
            valLevel1 = 1.1f;
            valLevel60 = 14.5f;
            break;
        case CLASS_HUNTER:
            valLevel1 = 1.8f;
            valLevel60 = 26.5f;
            break;
        case CLASS_PRIEST:
            valLevel1 = 11.0f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARLOCK:
            valLevel1 = 8.4f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARRIOR:
            valLevel1 = 3.9f;
            valLevel60 = 20.0f;
            break;
        default:
            return 0.0f;
    }

    float classrate = valLevel1 * float(60.0f - getLevel()) / 59.0f + valLevel60 * float(getLevel() - 1.0f) / 59.0f;

    return GetStat(STAT_AGILITY) / classrate;
}

float Player::GetSpellCritFromIntellect()
{

    static const struct
    {
        float base;
        float rate0, rate1;
    }
    crit_data[MAX_CLASSES] =
    {
        {   0.0f,   0.0f,  10.0f  },
        {   0.0f,   0.0f,  10.0f  },
        {   3.70f, 14.77f,  0.65f },
        {   0.0f,   0.0f,  10.0f  },
        {   0.0f,   0.0f,  10.0f  },
        {   2.97f, 10.03f,  0.82f },
        {   0.0f,   0.0f,  10.0f  },
        {   3.54f, 11.51f,  0.80f },
        {   3.70f, 14.77f,  0.65f },
        {   3.18f, 11.30f,  0.82f },
        {   0.0f,   0.0f,  10.0f  },
        {   3.33f, 12.41f,  0.79f }
    };
    float crit_chance;

    if (IsPlayer(this))
    {
        int my_class = getClass();
        float crit_ratio = crit_data[my_class].rate0 + crit_data[my_class].rate1 * getLevel();
        crit_chance = crit_data[my_class].base + GetStat(STAT_INTELLECT) / crit_ratio;
    }
    else
    {
        crit_chance = m_baseSpellCritChance;
    }

    crit_chance = crit_chance > 0.0 ? crit_chance : 0.0;

    return crit_chance;
}

float Player::OCTRegenHPPerSpirit()
{
    float regen = 0.0f;

    float Spirit = GetStat(STAT_SPIRIT);
    uint8 Class = getClass();

    switch (Class)
    {
        case CLASS_DRUID:   regen = (Spirit * 0.11 + 1);    break;
        case CLASS_HUNTER:  regen = (Spirit * 0.43 - 5.5);  break;
        case CLASS_MAGE:    regen = (Spirit * 0.11 + 1);    break;
        case CLASS_PALADIN: regen = (Spirit * 0.25);        break;
        case CLASS_PRIEST:  regen = (Spirit * 0.15 + 1.4);  break;
        case CLASS_ROGUE:   regen = (Spirit * 0.84 - 13);   break;
        case CLASS_SHAMAN:  regen = (Spirit * 0.28 - 3.6);  break;
        case CLASS_WARLOCK: regen = (Spirit * 0.12 + 1.5);  break;
        case CLASS_WARRIOR: regen = (Spirit * 1.26 - 22.6); break;
    }

    return regen;
}

float Player::OCTRegenMPPerSpirit()
{
    float addvalue = 0.0;

    float Spirit = GetStat(STAT_SPIRIT);
    uint8 Class = getClass();

    switch (Class)
    {
        case CLASS_DRUID:   addvalue = (Spirit / 5 + 15);   break;
        case CLASS_HUNTER:  addvalue = (Spirit / 5 + 15);   break;
        case CLASS_MAGE:    addvalue = (Spirit / 4 + 12.5); break;
        case CLASS_PALADIN: addvalue = (Spirit / 5 + 15);   break;
        case CLASS_PRIEST:  addvalue = (Spirit / 4 + 12.5); break;
        case CLASS_SHAMAN:  addvalue = (Spirit / 5 + 17);   break;
        case CLASS_WARLOCK: addvalue = (Spirit / 5 + 15);   break;
    }

    addvalue /= 2.0f;

    return addvalue;
}

void Player::SetRegularAttackTime()
{
    for (int i = 0; i < MAX_ATTACK; ++i)
    {
        Item* tmpitem = GetWeaponForAttack(WeaponAttackType(i), true, false);
        if (tmpitem)
        {
            ItemPrototype const* proto = tmpitem->GetProto();
            if (proto->Delay)
            {
                SetAttackTime(WeaponAttackType(i), proto->Delay);
            }
            else
            {
                SetAttackTime(WeaponAttackType(i), BASE_ATTACK_TIME);
            }
        }
    }
}

bool Player::UpdateSkill(uint32 skill_id, uint32 step)
{
    if (!skill_id)
    {
        return false;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(skill_id);
    if (itr == mSkillStatus.end())
    {
        return false;
    }

    SkillStatusData &skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return false;
    }

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);
    uint32 data = GetUInt32Value(valueIndex);
    uint32 value = SKILL_VALUE(data);
    uint32 max = SKILL_MAX(data);

    if ((!max) || (!value) || (value >= max))
    {
        return false;
    }

    if (value * 512 < max * urand(0, 512))
    {
        uint32 new_value = value + step;
        if (new_value > max)
        {
            new_value = max;
        }

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, max));

        if (skillStatus.uState != SKILL_NEW)
        {
            skillStatus.uState = SKILL_CHANGED;
        }

        return true;
    }

    return false;
}

static skill::Chances ChancesPaid()
{
    skill::Chances paid;
    paid.orange = sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_ORANGE);
    paid.yellow = sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_YELLOW);
    paid.green = sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_GREEN);
    paid.grey = sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_GREY);

    return paid;
}

bool Player::UpdateCraftSkill(uint32 spellid)
{
    DEBUG_LOG("UpdateCraftSkill spellid %d", spellid);

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spellid);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* skill = _spell_idx->second;
        if (skill->SkillLine)
        {
            uint32 SkillValue = GetPureSkillValue(skill->SkillLine);

            uint32 craft_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_CRAFTING);

            return UpdateSkillPro(skill->SkillLine,
                skill::ChanceAt(SkillValue,
                                skill->TrivialSkillLineRankHigh,
                                (skill->TrivialSkillLineRankHigh + skill->TrivialSkillLineRankLow) / 2,
                                skill->TrivialSkillLineRankLow,
                                ChancesPaid()),
                craft_skill_gain);
        }
    }
    return false;
}

bool Player::UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator)
{
    DEBUG_LOG("UpdateGatherSkill(SkillId %d SkillLevel %d RedLevel %d)", SkillId, SkillValue, RedLevel);

    const uint32 gathering_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING);

    const int32 chance = skill::ChanceAt(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25,
                                         ChancesPaid()) * int32(Multiplicator);

    switch (SkillId)
    {
        case SKILL_HERBALISM:
        case SKILL_LOCKPICKING:
            return UpdateSkillPro(SkillId, chance, gathering_skill_gain);

        case SKILL_SKINNING:
            return UpdateSkillPro(SkillId,
                                  skill::Thinned(chance, SkillValue,
                                                 sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS)),
                                  gathering_skill_gain);
        case SKILL_MINING:
            return UpdateSkillPro(SkillId,
                                  skill::Thinned(chance, SkillValue,
                                                 sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS)),
                                  gathering_skill_gain);
    }

    return false;
}

bool Player::UpdateFishingSkill()
{
    DEBUG_LOG("UpdateFishingSkill");

    const uint32 SkillValue = GetPureSkillValue(SKILL_FISHING);
    const uint32 gathering_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING);

    return UpdateSkillPro(SKILL_FISHING, skill::FishingChance(SkillValue), gathering_skill_gain);
}

bool Player::UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step)
{
    DEBUG_LOG("UpdateSkillPro(SkillId %d, Chance %3.1f%%)", SkillId, Chance / 10.0);
    if (!SkillId)
    {
        return false;
    }

    if (Chance <= 0)
    {
        DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0);
        return false;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(SkillId);
    if (itr == mSkillStatus.end())
    {
        return false;
    }

    SkillStatusData &skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return false;
    }

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);

    uint32 data = GetUInt32Value(valueIndex);
    uint16 SkillValue = SKILL_VALUE(data);
    uint16 MaxValue   = SKILL_MAX(data);

    if (!MaxValue || !SkillValue || SkillValue >= MaxValue)
    {
        return false;
    }

    int32 Roll = irand(1, 1000);

    if (Roll <= Chance)
    {
        uint32 new_value = SkillValue + step;
        if (new_value > MaxValue)
        {
            new_value = MaxValue;
        }

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, MaxValue));

        if (skillStatus.uState != SKILL_NEW)
        {
            skillStatus.uState = SKILL_CHANGED;
        }

        DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% taken", Chance / 10.0);
        return true;
    }

    DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0);
    return false;
}

void Player::UpdateWeaponSkill(WeaponAttackType attType)
{

    Unit* pVictim = getVictim();
    if (pVictim && pVictim->IsCharmerOrOwnerPlayerOrPlayerItself())
    {
        return;
    }

    if (IsInFeralForm())
    {
        return;
    }

    if (GetShapeshiftForm() == FORM_TREE)
    {
        return;
    }

    uint32 weaponSkillGain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_WEAPON);

    Item* pWeapon = GetWeaponForAttack(attType, true, true);
    if (pWeapon && pWeapon->GetProto()->SubClass != ITEM_SUBCLASS_WEAPON_FISHING_POLE)
    {
        UpdateSkill(pWeapon->GetSkill(), weaponSkillGain);
    }
    else if (!pWeapon && attType == BASE_ATTACK)
    {
        UpdateSkill(SKILL_UNARMED, weaponSkillGain);
    }

    Sheet().AllCrits();
}

void Player::UpdateCombatSkills(Unit* pVictim, WeaponAttackType attType, bool defence)
{
    uint32 plevel = getLevel();
    uint32 greylevel = xp::GreyLevel(plevel);
    uint32 moblevel = pVictim->GetLevelForTarget(this);
    if (moblevel < greylevel)
    {
        return;
    }

    if (moblevel > plevel + 5)
    {
        moblevel = plevel + 5;
    }

    uint32 lvldif = moblevel - greylevel;
    if (lvldif < 3)
    {
        lvldif = 3;
    }

    int32 skilldif = 5 * plevel - (defence ? GetBaseDefenseSkillValue() : GetBaseWeaponSkillValue(attType));

    if (skilldif <= 0)
    {
        return;
    }

    float chance = float(3 * lvldif * skilldif) / plevel;
    if (!defence)
    {
        chance *= 0.1f * GetStat(STAT_INTELLECT);
    }

    chance = chance < 1.0f ? 1.0f : chance;

    if (roll_chance_f(chance))
    {
        if (defence)
        {
            UpdateDefense();
        }
        else
        {
            UpdateWeaponSkill(attType);
        }
    }
    else
    {
        return;
    }
}

void Player::ModifySkillBonus(uint32 skillid, int32 val, bool talent)
{
    SkillStatusMap::const_iterator itr = mSkillStatus.find(skillid);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
    {
        return;
    }

    uint32 bonusIndex = PLAYER_SKILL_BONUS_INDEX(itr->second.pos);

    uint32 bonus_val = GetUInt32Value(bonusIndex);
    int16 temp_bonus = SKILL_TEMP_BONUS(bonus_val);
    int16 perm_bonus = SKILL_PERM_BONUS(bonus_val);

    if (talent)
    {
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS(temp_bonus, perm_bonus + val));
    }
    else
    {
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS(temp_bonus + val, perm_bonus));
    }
}

void Player::UpdateSkillsForLevel()
{
    uint16 maxconfskill = sWorld.GetConfigMaxSkillValue();
    uint32 maxSkill = GetMaxSkillValueForLevel();

    bool alwaysMaxSkill = sWorld.getConfig(CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL);

    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        SkillStatusData &skillStatus = itr->second;
        if (skillStatus.uState == SKILL_DELETED)
        {
            continue;
        }

        uint32 pskill = itr->first;

        SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(pskill);
        if (!pSkill)
        {
            continue;
        }

        if (GetSkillRangeType(pSkill, false) != SKILL_RANGE_LEVEL)
        {
            continue;
        }

        uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);
        uint32 data = GetUInt32Value(valueIndex);
        uint32 max = SKILL_MAX(data);
        uint32 val = SKILL_VALUE(data);

        if (max != 1)
        {

            if (alwaysMaxSkill)
            {
                SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(maxSkill, maxSkill));
                if (skillStatus.uState != SKILL_NEW)
                {
                    skillStatus.uState = SKILL_CHANGED;
                }
            }
            else if (max != maxconfskill)
            {
                SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(val, maxSkill));
                if (skillStatus.uState != SKILL_NEW)
                {
                    skillStatus.uState = SKILL_CHANGED;
                }
            }
        }
    }
}

void Player::UpdateSkillsToMaxSkillsForLevel()
{
    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        SkillStatusData &skillStatus = itr->second;
        if (skillStatus.uState == SKILL_DELETED)
        {
            continue;
        }

        uint32 pskill = itr->first;
        if (IsProfessionOrRidingSkill(pskill))
        {
            continue;
        }
        uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);
        uint32 data = GetUInt32Value(valueIndex);

        uint32 max = SKILL_MAX(data);

        if (max > 1)
        {
            SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(max, max));
            if (skillStatus.uState != SKILL_NEW)
            {
                skillStatus.uState = SKILL_CHANGED;
            }
        }

        if (pskill == SKILL_DEFENSE)
        {
            Sheet().Defences();
        }
    }
}

void Player::SetSkill(uint16 id, uint16 currVal, uint16 maxVal, uint16 step )
{
    if (!id)
    {
        return;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(id);

    if (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED)
    {
        SkillStatusData &skillStatus = itr->second;
        if (currVal)
        {
            if (step)
            {
                SetUInt32Value(PLAYER_SKILL_INDEX(skillStatus.pos), MAKE_PAIR32(id, step));
            }

            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos), MAKE_SKILL_VALUE(currVal, maxVal));
            if (skillStatus.uState != SKILL_NEW)
            {
                skillStatus.uState = SKILL_CHANGED;
            }

        }
        else
        {

            SetUInt32Value(PLAYER_SKILL_INDEX(skillStatus.pos), 0);
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos), 0);
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos), 0);

            if (skillStatus.uState != SKILL_NEW)
            {
                skillStatus.uState = SKILL_DELETED;
            }
            else
            {
                mSkillStatus.erase(itr);
            }

            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
            {
                if (SkillLineAbilityEntry const* pAbility = sSkillLineAbilityStore.LookupEntry(j))
                {
                    if (pAbility->SkillLine == id)
                    {
                        removeSpell(sSpellMgr.GetFirstSpellInChain(pAbility->Spell));
                    }
                }
            }
        }
    }
    else if (currVal)
    {
        for (int i = 0; i < PLAYER_MAX_SKILLS; ++i)
        {
            if (!GetUInt32Value(PLAYER_SKILL_INDEX(i)))
            {
                SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(id);
                if (!pSkill)
                {
                    sLog.outError("Skill not found in SkillLineStore: skill #%u", id);
                    return;
                }

                SetUInt32Value(PLAYER_SKILL_INDEX(i), MAKE_PAIR32(id, step));
                SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(i), MAKE_SKILL_VALUE(currVal, maxVal));

                if (itr != mSkillStatus.end())
                {
                    itr->second.pos = i;
                    itr->second.uState = SKILL_CHANGED;
                }
                else
                {
                    mSkillStatus.insert(SkillStatusMap::value_type(id, SkillStatusData(i, SKILL_NEW)));
                }

                SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(i), 0);

                const auto mModSkill = GetAurasByType(SPELL_AURA_MOD_SKILL);
                for (auto* aura : mModSkill)
                {
                    if (aura->GetModifier()->m_miscvalue == int32(id))
                    {
                        aura->ApplyModifier(true);
                    }
                }

                const auto mModSkillTalent = GetAurasByType(SPELL_AURA_MOD_SKILL_TALENT);
                for (auto* aura : mModSkillTalent)
                {
                    if (aura->GetModifier()->m_miscvalue == int32(id))
                    {
                        aura->ApplyModifier(true);
                    }
                }

                learnSkillRewardedSpells(id, currVal);
                return;
            }
        }
    }
}

bool Player::HasSkill(uint32 skill) const
{
    if (!skill)
    {
        return false;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    return (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED);
}

uint16 Player::GetSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos));

    int32 result = int32(SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

uint16 Player::GetMaxSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos));

    int32 result = int32(SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

uint16 Player::GetPureMaxSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos)));
}

uint16 Player::GetBaseSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    int32 result = int32(SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos))));
    result += SKILL_PERM_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos)));
    return result < 0 ? 0 : result;
}

uint16 Player::GetPureSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos)));
}

int16 Player::GetSkillPermBonusValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_PERM_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos)));
}

int16 Player::GetSkillTempBonusValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_TEMP_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos)));
}

void Player::_ApplyAllStatBonuses()
{
    Tallied().Ready(false);

    _ApplyAllAuraMods();
    _ApplyAllItemMods();

    Tallied().Ready(true);

    Sheet().Everything();
}

void Player::_RemoveAllStatBonuses()
{
    Tallied().Ready(false);

    _RemoveAllItemMods();
    _RemoveAllAuraMods();

    Tallied().Ready(true);

    Sheet().Everything();
}
