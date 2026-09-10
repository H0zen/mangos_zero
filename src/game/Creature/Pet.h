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

#include <unordered_map>
#include "ObjectKind.h"
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include <ctime>
#include <vector>
#include <map>
#include "ObjectGuid.h"
#include "Creature.h"
#include "Stats/PetSheet.h"
#include "Unit.h"

class Transport;

enum PetType
{
    SUMMON_PET              = 0,
    HUNTER_PET              = 1,
    GUARDIAN_PET            = 2,
    MINI_PET                = 3,
    MAX_PET_TYPE            = 4
};

#define MAX_PET_STABLES         2

enum PetSaveMode
{
    PET_SAVE_AS_DELETED        = -1,
    PET_SAVE_AS_CURRENT        =  0,
    PET_SAVE_FIRST_STABLE_SLOT =  1,
    PET_SAVE_LAST_STABLE_SLOT  =  MAX_PET_STABLES,
    PET_SAVE_NOT_IN_SLOT       =  100,
    PET_SAVE_REAGENTS          =  101
};

enum PetDatabaseStatus
{
    PET_DB_NO_PET       = 0,
    PET_DB_DEAD         = 1,
    PET_DB_ALIVE        = 2,
};

enum PetModeFlags
{
    PET_MODE_UNKNOWN_0         = 0x0000001,
    PET_MODE_UNKNOWN_2         = 0x0000100,
    PET_MODE_DISABLE_ACTIONS   = 0x8000000,

    PET_MODE_DEFAULT           = PET_MODE_UNKNOWN_0 | PET_MODE_UNKNOWN_2,
};

enum HappinessState
{
    UNHAPPY = 1,
    CONTENT = 2,
    HAPPY   = 3
};

enum LoyaltyLevel
{
    REBELLIOUS  = 1,
    UNRULY      = 2,
    SUBMISSIVE  = 3,
    DEPENDABLE  = 4,
    FAITHFUL    = 5,
    BEST_FRIEND = 6
};

enum PetSpellState
{
    PETSPELL_UNCHANGED = 0,
    PETSPELL_CHANGED   = 1,
    PETSPELL_NEW       = 2,
    PETSPELL_REMOVED   = 3
};

enum PetSpellType
{
    PETSPELL_NORMAL = 0,
    PETSPELL_FAMILY = 1,
};

struct PetSpell
{
    uint8 active;

    PetSpellState state : 8;
    PetSpellType type   : 8;
};

enum ActionFeedback
{
    FEEDBACK_PET_NONE        = 0,
    FEEDBACK_PET_DEAD        = 1,
    FEEDBACK_NOTHING_TO_ATT  = 2,
    FEEDBACK_CANT_ATT_TARGET = 3,
    FEEDBACK_NO_PATH_TO      = 4
};

enum PetTalk
{
    PET_TALK_SPECIAL_SPELL  = 0,
    PET_TALK_ATTACK         = 1
};

enum PetNameInvalidReason
{

    PET_NAME_SUCCESS                                        = 0,

    PET_NAME_INVALID                                        = 1,
    PET_NAME_NO_NAME                                        = 2,
    PET_NAME_TOO_SHORT                                      = 3,
    PET_NAME_TOO_LONG                                       = 4,
    PET_NAME_MIXED_LANGUAGES                                = 6,
    PET_NAME_PROFANE                                        = 7,
    PET_NAME_RESERVED                                       = 8,
    PET_NAME_THREE_CONSECUTIVE                              = 11,
    PET_NAME_INVALID_SPACE                                  = 12,
    PET_NAME_CONSECUTIVE_SPACES                             = 13,
    PET_NAME_RUSSIAN_CONSECUTIVE_SILENT_CHARACTERS          = 14,
    PET_NAME_RUSSIAN_SILENT_CHARACTER_AT_BEGINNING_OR_END   = 15,
    PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME              = 16
};

typedef std::unordered_map<uint32, PetSpell> PetSpellMap;
typedef std::map<uint32, uint32> TeachSpellMap;
typedef std::vector<uint32> AutoSpellList;

#define HAPPINESS_LEVEL_SIZE        333000

extern const uint32 LevelUpLoyalty[6];
extern const uint32 LevelStartLoyalty[6];

#define ACTIVE_SPELLS_MAX           4

#define PET_FOLLOW_DIST  1.0f
#define PET_FOLLOW_ANGLE (M_PI_F/2.0f)

class Player;
struct ItemPrototype;

class Pet : public Creature
{
    public:
        explicit Pet(PetType type = MAX_PET_TYPE);
        virtual ~Pet();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        PetType getPetType() const { return m_petType; }
        void setPetType(PetType type) { m_petType = type; }
        bool isControlled() const { return getPetType() == SUMMON_PET || getPetType() == HUNTER_PET; }
        bool isTemporarySummoned() const { return Term().Bounded(); }
        bool IsPermanentPetFor(Player* owner);

        bool Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, uint32 pet_number);
        bool CreateBaseAtCreature(Creature* creature);
        bool LoadPetFromDB(Player* owner, uint32 petentry = 0, uint32 petnumber = 0, bool current = false);
        void SavePetToDB(PetSaveMode mode);
        void Unsummon(PetSaveMode mode, Unit* owner = nullptr);

        static void DeleteFromDB(uint32 guidlow, bool separate_transaction = true);
        static PetDatabaseStatus GetStatusFromDB(Player*);

        void SetDeathState(DeathState s) override;
        void Update(uint32 update_diff, uint32 diff) override;

        uint8 GetPetAutoSpellSize() const override { return m_autospells.size(); }
        uint32 GetPetAutoSpellOnPos(uint8 pos) const override
        {
            if (pos >= m_autospells.size())
            {
                return 0;
            }
            else
            {
                return m_autospells[pos];
            }
        }

        bool CanSwim() const override
        {
            Unit const* owner = GetOwner();
            if (owner)
            {
                return IsPlayer(owner) ? true : ((Creature const*)owner)->CanSwim();
            }
            else
            {
                return Creature::CanSwim();
            }
        }

        bool CanFly() const override { return false; }

        void RegenerateAll(uint32 update_diff) override;
        void LooseHappiness();
        void TickLoyaltyChange();
        void ModifyLoyalty(int32 addvalue);
        HappinessState GetHappinessState();
        uint32 GetMaxLoyaltyPoints(uint32 level);
        uint32 GetStartLoyaltyPoints(uint32 level);
        void KillLoyaltyBonus(uint32 level);
        uint32 GetLoyaltyLevel()
        {
            return GetLoyaltyByte();
        }

        void SetLoyaltyLevel(LoyaltyLevel level);
        void GivePetXP(uint32 xp);
        void GivePetLevel(uint32 level);
        void SynchronizeLevelWithOwner();
        bool InitStatsForLevel(uint32 level, Unit* owner = nullptr);
        bool HaveInDiet(ItemPrototype const* item) const;
        uint32 GetCurrentFoodBenefitLevel(uint32 itemlevel);
        void SetDuration(int32 dur) { Term().Grant(TEMPSPAWN_TIMED_DESPAWN, dur > 0 ? uint32(dur) : 0); }

        int32 GetBonusDamage()
        {
            return m_bonusdamage;
        }
        void SetBonusDamage(int32 damage) { m_bonusdamage = damage; }

        StatSheet& Sheet() override { return m_sheet; }
        StatSheet const& Sheet() const override { return m_sheet; }

        Pace& Pacing() override { return m_pace; }
        Pace const& Pacing() const override { return m_pace; }

        bool   CanTakeMoreActiveSpells(uint32 SpellIconID);
        void   ToggleAutocast(uint32 spellid, bool apply);
        bool   HasTPForSpell(uint32 spellid);
        int32  GetTPForSpell(uint32 spellid);

        void ApplyModeFlags(PetModeFlags mode, bool apply);
        PetModeFlags GetModeFlags() const { return m_petModeFlags; }

        bool HasSpell(uint32 spell) const override;
        void AddTeachSpell(uint32 learned_id, uint32 source_id) { m_teachspells[learned_id] = source_id; }

        void LearnPetPassives();
        void CastPetAuras(bool current);
        void CastOwnerTalentAuras();
        void CastPetAura(PetAura const* aura);

        void _LoadSpellCooldowns();
        void _SaveSpellCooldowns();
        void _LoadAuras(uint32 timediff);
        void _SaveAuras();
        void _LoadSpells();
        void _SaveSpells();

        bool addSpell(uint32 spell_id, ActiveStates active = ACT_DECIDE, PetSpellState state = PETSPELL_NEW, PetSpellType type = PETSPELL_NORMAL);
        bool learnSpell(uint32 spell_id);
        bool unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        bool removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        void CleanupActionBar();

        PetSpellMap     m_spells;
        TeachSpellMap   m_teachspells;
        AutoSpellList   m_autospells;

        void InitPetCreateSpells();
        void CheckLearning(uint32 spellid);
        uint32 resetTalentsCost() const;

        void  SetTP(int32 TP);

        int32   m_TrainingPoints;
        uint32  m_resetTalentsCost;
        time_t  m_resetTalentsTime;

        const uint64& GetAuraUpdateMask() const { return m_auraUpdateMask; }
        void SetAuraUpdateMask(uint8 slot) { m_auraUpdateMask |= (uint64(1) << slot); }
        void ResetAuraUpdateMask()
        {
            m_auraUpdateMask = 0;
        }

        const char* GetNameForLocaleIdx(int32 locale_idx) const override { return Occupant::GetNameForLocaleIdx(locale_idx); }

        bool    m_removed;

    protected:
        uint32  m_happinessTimer;
        uint32  m_loyaltyTimer;
        PetType m_petType;
        int32   m_loyaltyPoints;
        int32   m_bonusdamage;
        uint64  m_auraUpdateMask;
        bool    m_loading;

    private:

        PetSheet m_sheet;
        PetPace m_pace;
        PetModeFlags m_petModeFlags;

};
