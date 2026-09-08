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

/**
 * @file Spell.h
 * @brief Spell casting system and spell-related structures.
 *
 * This file defines the Spell class which handles spell casting mechanics including:
 * - Spell targeting and target validation
 * - Spell effect execution
 * - Aura and damage application
 * - Projectile and animation handling
 * - Interrupt and loss of control handling
 * - Spell reflection and immunity
 * - Cooldown and resource management
 * - Triggered spell chains
 *
 * The file also contains SpellCastTargets for managing spell targets and related
 * flags and enumerations for spell behavior control.
 *
 * @see Spell for the main spell implementation
 * @see SpellCastTargets for spell target management
 * @see SpellEntry for spell database structures
 */

#pragma once

#include "Reaction.h"
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <list>
#include "GridDefines.h"
#include "SharedDefines.h"
#include "DBCEnums.h"
#include "ObjectGuid.h"
#include "LootMgr.h"
#include "Unit.h"
#include "Player.h"
#include "Cast/Recipe/Recipe.h"
#include "Cast/Recipe/RecipeBook.h"

class WorldSession;
class WorldPacket;
class DynamicObj;
class Item;
class GameObject;
class Group;
class Aura;

/// @brief Spell casting flag enumeration.
///
/// Contains various flags that modify spell casting behavior including
/// visibility in combat log and special rendering effects.
enum SpellCastFlags
{
    CAST_FLAG_NONE              = 0x00000000,    ///< No casting flags set
    CAST_FLAG_HIDDEN_COMBATLOG  = 0x00000001,    ///< Hide spell from combat log
    CAST_FLAG_UNKNOWN2          = 0x00000002,    ///< Unknown flag 2
    CAST_FLAG_UNKNOWN3          = 0x00000004,    ///< Unknown flag 3
    CAST_FLAG_UNKNOWN4          = 0x00000008,    ///< Unknown flag 4
    CAST_FLAG_UNKNOWN5          = 0x00000010,    ///< Unknown flag 5
    CAST_FLAG_AMMO              = 0x00000020,    ///< Display projectile visual for spell
    CAST_FLAG_UNKNOWN7          = 0x00000040,    ///< Unknown flag 7 (used for trade skill recast)
    ///<                                              !0x41 mask used to call CGTradeSkillInfo::DoRecast
    CAST_FLAG_UNKNOWN8          = 0x00000080,    ///< @brief Unknown flag 8
    CAST_FLAG_UNKNOWN9          = 0x00000100,    ///< @brief Unknown flag 9
};

/// @brief Spell knockback/push direction enumeration.
///
/// Defines the direction and method for pushing targets away from the spell impact.
enum SpellNotifyPushType
{
    PUSH_IN_FRONT,        ///< Push target straight in front
    PUSH_IN_FRONT_90,     ///< Push target at 90 degree angle
    PUSH_IN_FRONT_15,     ///< Push target at 15 degree angle
    PUSH_IN_BACK,         ///< Push target backwards
    PUSH_SELF_CENTER,     ///< Push target from self center
    PUSH_DEST_CENTER,     ///< Push target from spell destination
    PUSH_TARGET_CENTER    ///< Push target from target center
};

/// @brief Check if spell is a taming spell for quest purposes.
/// @param spellId Spell ID to check
/// @return True if spell is a quest taming spell, false otherwise
bool IsQuestTameSpell(uint32 spellId);

namespace MaNGOS
{
    struct SpellNotifierPlayer;
    struct SpellNotifierCreatureAndPlayer;
}

class SpellCastTargets;

/// @brief Reader helper for deserializing spell cast targets.
///
/// Provides a mechanism to read spell targets from network data while
/// maintaining reference to the caster for context.
struct SpellCastTargetsReader
{
    /// @brief Constructor
    /// @param _targets Reference to SpellCastTargets to populate
    /// @param _caster Pointer to the unit casting the spell
    explicit SpellCastTargetsReader(SpellCastTargets& _targets, Unit* _caster) : targets(_targets), caster(_caster) {}

    SpellCastTargets& targets;    /// Reference to the targets structure being populated
    Unit* caster;                 /// Pointer to casting unit for context
};

/// @brief Spell cast targets container and serialization.
///
/// Manages target selection for spells, including unit targets, game object targets,
/// item targets, and location-based targets. Handles serialization to/from network packets.
class SpellCastTargets
{
    public:
        SpellCastTargets();        /// Constructor initializes empty targets
        ~SpellCastTargets();       /// Destructor

        /// @brief Deserialize targets from network data
        /// @param data ByteBuffer containing serialized target data
        /// @param caster Unit casting the spell (for target validation)
        void read(ByteBuffer& data, Unit* caster);

        /// @brief Serialize targets to network data
        /// @param data ByteBuffer to write serialized targets
        void write(ByteBuffer& data) const;

        /// @brief Create a reader helper for deserialization
        /// @param caster Unit casting the spell
        /// @return SpellCastTargetsReader for deserialization
        SpellCastTargetsReader ReadForCaster(Unit* caster) { return SpellCastTargetsReader(*this, caster); }

        /// @brief Assignment operator
        /// @param target Source targets to copy
        /// @return Reference to this object
        SpellCastTargets& operator=(const SpellCastTargets& target)
        {
            m_unitTarget = target.m_unitTarget;
            m_itemTarget = target.m_itemTarget;
            m_GOTarget   = target.m_GOTarget;

            m_unitTargetGUID    = target.m_unitTargetGUID;
            m_GOTargetGUID      = target.m_GOTargetGUID;
            m_CorpseTargetGUID  = target.m_CorpseTargetGUID;
            m_itemTargetGUID    = target.m_itemTargetGUID;

            m_itemTargetEntry  = target.m_itemTargetEntry;

            m_srcX = target.m_srcX;
            m_srcY = target.m_srcY;
            m_srcZ = target.m_srcZ;

            m_destX = target.m_destX;
            m_destY = target.m_destY;
            m_destZ = target.m_destZ;

            m_strTarget = target.m_strTarget;

            m_targetMask = target.m_targetMask;

            return *this;
        }

        void setUnitTarget(Unit* target);
        ObjectGuid getUnitTargetGuid() const { return m_unitTargetGUID; }
        Unit* getUnitTarget() const { return m_unitTarget; }

        void setDestination(float x, float y, float z);
        void setSource(float x, float y, float z);
        void getDestination(float& x, float& y, float& z) const { x = m_destX; y = m_destY; z = m_destZ; }
        void getSource(float& x, float& y, float& z) const { x = m_srcX; y = m_srcY, z = m_srcZ; }

        void setGOTarget(GameObject* target);
        ObjectGuid getGOTargetGuid() const { return m_GOTargetGUID; }
        GameObject* getGOTarget() const { return m_GOTarget; }

        void setCorpseTarget(Corpse* corpse);
        ObjectGuid getCorpseTargetGuid() const { return m_CorpseTargetGUID; }

        void setItemTarget(Item* item);
        ObjectGuid getItemTargetGuid() const { return m_itemTargetGUID; }
        Item* getItemTarget() const { return m_itemTarget; }
        uint32 getItemTargetEntry() const { return m_itemTargetEntry; }

        void setTradeItemTarget(Player* caster);

        void updateTradeSlotItem()
        {
            if (m_itemTarget && (m_targetMask & TARGET_FLAG_TRADE_ITEM))
            {
                m_itemTargetGUID = m_itemTarget->GetObjectGuid();
                m_itemTargetEntry = m_itemTarget->GetEntry();
            }
        }

        bool IsEmpty() const { return !m_GOTargetGUID && !m_unitTargetGUID && !m_itemTarget && !m_CorpseTargetGUID; }

        void Update(Unit* caster);

        float m_srcX, m_srcY, m_srcZ;
        float m_destX, m_destY, m_destZ;
        std::string m_strTarget;

        uint16 m_targetMask;

    private:
        // objects (can be used at spell creating and after Update at casting
        Unit* m_unitTarget;
        GameObject* m_GOTarget;
        Item* m_itemTarget;

        // object GUID/etc, can be used always
        ObjectGuid m_unitTargetGUID;
        ObjectGuid m_GOTargetGUID;
        ObjectGuid m_CorpseTargetGUID;
        ObjectGuid m_itemTargetGUID;
        uint32 m_itemTargetEntry;
};

inline ByteBuffer& operator<< (ByteBuffer& buf, SpellCastTargets const& targets)
{
    targets.write(buf);
    return buf;
}

inline ByteBuffer& operator>> (ByteBuffer& buf, SpellCastTargetsReader const& targets)
{
    targets.targets.read(buf, targets.caster);
    return buf;
}

enum SpellState
{
    SPELL_STATE_CREATED     = 0,   // just created
    SPELL_STATE_PREPARING   = 1,   // cast time delay period, non channeled spell
    SPELL_STATE_CASTING     = 2,   // channeled time period spell casting state
    SPELL_STATE_DELAYED     = 3,   // spell is delayed (cast time pushed back) TODO: need to be implemented properly
    SPELL_STATE_TRAVELING   = 4,   // spell casted but need time to hit target(s)
    SPELL_STATE_LANDING     = 5,   // processing the effects
    SPELL_STATE_CHANNELING  = 6,   // channeled time period spell casting state
    SPELL_STATE_FINISHED    = 7,   // cast finished to success or fail
};

enum SpellTargets
{
    SPELL_TARGETS_HOSTILE,
    SPELL_TARGETS_NOT_FRIENDLY,
    SPELL_TARGETS_NOT_HOSTILE,
    SPELL_TARGETS_FRIENDLY,
    SPELL_TARGETS_AOE_DAMAGE,
    SPELL_TARGETS_ALL
};

typedef std::multimap<uint64, uint64> SpellTargetTimeMap;

class Spell
{
    friend struct MaNGOS::SpellNotifierPlayer;
    friend struct MaNGOS::SpellNotifierCreatureAndPlayer;
    friend void Unit::SetCurrentCastedSpell(Spell* pSpell);

    public:

        void EffectEmpty(const cast::Operation& operation);
        void EffectNULL(const cast::Operation& operation);
        void EffectUnused(const cast::Operation& operation);
        void EffectDistract(const cast::Operation& operation);
        void EffectPull(const cast::Operation& operation);
        void EffectSchoolDMG(const cast::Operation& operation);
        void EffectEnvironmentalDMG(const cast::Operation& operation);
        void EffectInstaKill(const cast::Operation& operation);
        void EffectDummy(const cast::Operation& operation);
        void EffectTeleportUnits(const cast::Operation& operation);
        void EffectApplyAura(const cast::Operation& operation);
        void EffectSendEvent(const cast::Operation& operation);
        void EffectPowerBurn(const cast::Operation& operation);
        void EffectPowerDrain(const cast::Operation& operation);
        void EffectHeal(const cast::Operation& operation);
        void EffectBind(const cast::Operation& operation);
        void EffectTeleportGraveyard(const cast::Operation& operation);
        void EffectHealthLeech(const cast::Operation& operation);
        void EffectQuestComplete(const cast::Operation& operation);
        void EffectCreateItem(const cast::Operation& operation);
        void EffectPersistentAA(const cast::Operation& operation);
        void EffectEnergize(const cast::Operation& operation);
        void EffectOpenLock(const cast::Operation& operation);
        void EffectSummonChangeItem(const cast::Operation& operation);
        void EffectProficiency(const cast::Operation& operation);
        void EffectApplyAreaAura(const cast::Operation& operation);
        void EffectSummon(const cast::Operation& operation);
        void EffectLearnSpell(const cast::Operation& operation);
        void EffectDispel(const cast::Operation& operation);
        void EffectDualWield(const cast::Operation& operation);
        void EffectPickPocket(const cast::Operation& operation);
        void EffectAddFarsight(const cast::Operation& operation);
        void EffectSummonPossessed(const cast::Operation& operation);
        void EffectSummonWild(const cast::Operation& operation);
        void EffectSummonGuardian(const cast::Operation& operation);
        void EffectHealMechanical(const cast::Operation& operation);
        void EffectTeleUnitsFaceCaster(const cast::Operation& operation);
        void EffectLearnSkill(const cast::Operation& operation);
        void EffectAddHonor(const cast::Operation& operation);
        void EffectTradeSkill(const cast::Operation& operation);
        void EffectEnchantItemPerm(const cast::Operation& operation);
        void EffectEnchantItemTmp(const cast::Operation& operation);
        void EffectTameCreature(const cast::Operation& operation);
        void EffectSummonPet(const cast::Operation& operation);
        void EffectLearnPetSpell(const cast::Operation& operation);
        void EffectWeaponDmg(const cast::Operation& operation);
        void EffectTriggerSpell(const cast::Operation& operation);
        void EffectTriggerMissileSpell(const cast::Operation& operation);
        void EffectThreat(const cast::Operation& operation);
        void EffectHealMaxHealth(const cast::Operation& operation);
        void EffectInterruptCast(const cast::Operation& operation);
        void EffectSummonObjectWild(const cast::Operation& operation);
        void EffectScriptEffect(const cast::Operation& operation);
        void EffectSanctuary(const cast::Operation& operation);
        void EffectAddComboPoints(const cast::Operation& operation);
        void EffectDuel(const cast::Operation& operation);
        void EffectStuck(const cast::Operation& operation);
        void EffectSummonPlayer(const cast::Operation& operation);
        void EffectActivateObject(const cast::Operation& operation);
        void EffectSummonTotem(const cast::Operation& operation);
        void EffectEnchantHeldItem(const cast::Operation& operation);
        void EffectSummonObject(const cast::Operation& operation);
        void EffectResurrect(const cast::Operation& operation);
        void EffectParry(const cast::Operation& operation);
        void EffectBlock(const cast::Operation& operation);
        void EffectLeapForward(const cast::Operation& operation);
        void EffectTransmitted(const cast::Operation& operation);
        void EffectDisEnchant(const cast::Operation& operation);
        void EffectInebriate(const cast::Operation& operation);
        void EffectFeedPet(const cast::Operation& operation);
        void EffectDismissPet(const cast::Operation& operation);
        void EffectReputation(const cast::Operation& operation);
        void EffectSelfResurrect(const cast::Operation& operation);
        void EffectSkinning(const cast::Operation& operation);
        void EffectCharge(const cast::Operation& operation);
        void EffectSendTaxi(const cast::Operation& operation);
        void EffectSummonCritter(const cast::Operation& operation);
        void EffectKnockBack(const cast::Operation& operation);
        void EffectPlayerPull(const cast::Operation& operation);
        void EffectDispelMechanic(const cast::Operation& operation);
        void EffectSummonDeadPet(const cast::Operation& operation);
        void EffectDestroyAllTotems(const cast::Operation& operation);
        void EffectDurabilityDamage(const cast::Operation& operation);
        void EffectSkill(const cast::Operation& operation);
        void EffectTaunt(const cast::Operation& operation);
        void EffectDurabilityDamagePCT(const cast::Operation& operation);
        void EffectModifyThreatPercent(const cast::Operation& operation);
        void EffectResurrectNew(const cast::Operation& operation);
        void EffectAddExtraAttacks(const cast::Operation& operation);
        void EffectSpiritHeal(const cast::Operation& operation);
        void EffectSkinPlayerCorpse(const cast::Operation& operation);
        void EffectSummonDemon(const cast::Operation& operation);
        void EffectPlayMusic(const cast::Operation& operation);

        Spell(Unit* caster, SpellEntry const* info, bool triggered, ObjectGuid originalCasterGUID = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        ~Spell();

        SpellCastResult prepare(SpellCastTargets const* targets, Aura* triggeredByAura = nullptr, uint32 chance = 0);

        void cancel();

        void update(uint32 difftime);
        void cast(bool skipCheck = false);
        void finish(bool ok = true);
        void TakePower();
        void TakeAmmo();
        void TakeReagents();
        void TakeCastItem();

        SpellCastResult CheckCast(bool strict);
        SpellCastResult CheckPetCast(Unit* target);

        // handlers
        void handle_immediate();
        uint64 handle_delayed(uint64 t_offset);
        // handler helpers
        void _handle_immediate_phase();
        void _handle_finish_phase();

        SpellCastResult CheckItems();
        SpellCastResult CheckRange(bool strict);
        SpellCastResult CheckPower();
        SpellCastResult CheckCasterAuras() const;

        int32 CalculateDamage(SpellEffectIndex i, Unit* target) { return m_caster->CalculateSpellDamage(target, Recipe(), Recipe().At(static_cast<uint8>(i)), &m_currentBasePoints[i]); }
        static uint32 CalculatePowerCost(SpellEntry const* spellInfo, Unit* caster, Spell const* spell = nullptr, Item* castItem = nullptr);

        bool HaveTargetsForEffect(SpellEffectIndex effect) const;
        void Delayed();
        void DelayedChannel();
        uint32 getState() const { return m_spellState; }
        void setState(uint32 state) { m_spellState = state; }

        void DoCreateItem(SpellEffectIndex eff_idx, uint32 itemtype);

        void WriteSpellGoTargets(WorldPacket* data);
        void WriteAmmoToPacket(WorldPacket* data);

        template<typename T> Occupant* FindCorpseUsing();

        bool CheckTarget(Unit* target, const cast::Operation& operation);
        bool CanAutoCast(Unit* target);

        static void  SendCastResult(Player* caster, SpellEntry const* spellInfo, SpellCastResult result);
        void SendCastResult(SpellCastResult result);
        void SendSpellStart();
        void SendSpellGo();
        void SendSpellCooldown();
        void SendLogExecute();
        void SendInterrupted(SpellCastResult result);
        void SendChannelUpdate(uint32 time);
        void SendChannelStart(uint32 duration);
        void SendResurrectRequest(Player* target);

        void HandleEffects(Unit* pUnitTarget, Item* pItemTarget, GameObject* pGOTarget, SpellEffectIndex i, float DamageMultiplier = 1.0);
        void HandleThreatSpells();
        // void HandleAddAura(Unit* Target);

        SpellEntry const* GetSpellBonusLevelPenaltySpell(SpellEntry const* spellProto) const;

        SpellEntry const* m_spellInfo;

        /// Everything the row already answered, worked out once at load.
        const cast::Recipe& Recipe() const { return *m_recipe; }

        const cast::Recipe* m_recipe;
        SpellEntry const* m_triggeredBySpellInfo;
        int32 m_currentBasePoints[MAX_EFFECT_INDEX];        // cache SpellEntry::CalculateSimpleValue and use for set custom base points
        Item* m_CastItem;
        SpellCastTargets m_targets;

        bool IsTriggered() const {return m_IsTriggeredSpell;}

        int32 GetCastTime() const { return m_casttime; }
        uint32 GetCastedTime()
        {
            return m_timer;
        }

        bool IsAutoRepeat() const { return m_autoRepeat; }
        void SetAutoRepeat(bool rep) { m_autoRepeat = rep; }
        void ReSetTimer()
        {
            m_timer = m_casttime > 0 ? m_casttime : 0;
        }
        bool IsNextMeleeSwingSpell() const
        {
            return Recipe().Starts() == cast::Start::NextSwing;
        }
        bool IsRangedSpell() const
        {
            return Recipe().Says().ranged;
        }
        bool IsChannelActive() const { return m_caster->GetUInt32Value(UNIT_CHANNEL_SPELL) != 0; }
        bool IsMeleeAttackResetSpell() const { return !m_IsTriggeredSpell && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_AUTOATTACK);  }
        bool IsRangedAttackResetSpell() const { return !m_IsTriggeredSpell && IsRangedSpell() && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_AUTOATTACK); }

        bool IsDeletable() const { return !m_referencedFromCurrentSpell && !m_executedCurrently; }
        void SetReferencedFromCurrent(bool yes) { m_referencedFromCurrentSpell = yes; }
        void SetExecutedCurrently(bool yes) { m_executedCurrently = yes; }
        uint64 GetDelayStart() const { return m_delayStart; }
        void SetDelayStart(uint64 m_time) { m_delayStart = m_time; }
        uint64 GetDelayMoment() const { return m_delayMoment; }

        bool IsNeedSendToClient() const;                    // use for hide spell cast for client in case when cast not have client side affect (animation or log entries)
        bool IsTriggeredSpellWithRedundentCastTime() const; // use for ignore some spell data for triggered spells like cast time, some triggered spells have redundent copy data from main spell for client use purpose

        CurrentSpellTypes GetCurrentContainer();

        // caster types:
        // formal spell caster, in game source of spell affects cast
        Unit* GetCaster() const { return m_caster; }
        // real source of cast affects, explicit caster, or DoT/HoT applier, or GO owner, or wild GO itself. Can be nullptr
        Occupant* GetAffectiveCasterObject() const;
        // limited version returning nullptr in cases wild gameobject caster object, need for Aura (auras currently not support non-Unit caster)
        Unit* GetAffectiveCaster() const { return m_originalCasterGUID ? m_originalCaster : m_caster; }
        // m_originalCasterGUID can store GO guid, and in this case this is visual caster
        Occupant* GetCastingObject() const;

        uint32 GetPowerCost() const { return m_powerCost; }

        void UpdatePointers();                              // must be used at call Spell code after time delay (non triggered spell cast/update spell call/etc)

        bool CheckTargetCreatureType(Unit* target) const;

        void AddTriggeredSpell(SpellEntry const* spellInfo) { m_TriggerSpells.push_back(spellInfo); }
        void AddPrecastSpell(SpellEntry const* spellInfo) { m_preCastSpells.push_back(spellInfo); }
        void AddTriggeredSpell(uint32 spellId);
        void AddPrecastSpell(uint32 spellId);
        void CastPreCastSpells(Unit* target);
        void CastTriggerSpells();

        void CleanupTargetList();
        void ClearCastItem();

        typedef std::list<Unit*> UnitList;

        void SetSelfContainer(Spell** pCurrentContainer) { m_selfContainer = pCurrentContainer; }
        Spell** GetSelfContainer()
        {
            return m_selfContainer;
        }

    protected:
        bool HasGlobalCooldown();
        void TriggerGlobalCooldown();
        void CancelGlobalCooldown();

        void SendLoot(ObjectGuid guid, LootType loottype, LockType lockType);
        bool IgnoreItemRequirements() const;                // some item use spells have unexpected reagent data
        void UpdateOriginalCasterPointer();

        Unit* m_caster;

        ObjectGuid m_originalCasterGUID;                    // real source of cast (aura caster/etc), used for spell targets selection
        // e.g. damage around area spell trigered by victim aura and da,age emeies of aura caster
        Unit* m_originalCaster;                             // cached pointer for m_originalCaster, updated at Spell::UpdatePointers()

        Spell** m_selfContainer;                            // pointer to our spell container (if applicable)

        // Spell data
        SpellSchoolMask m_spellSchoolMask;                  // Spell school (can be overwrite for some spells (wand shoot for example)
        WeaponAttackType m_attackType;                      // For weapon based attack
        uint32 m_powerCost;                                 // Calculated spell cost     initialized only in Spell::prepare
        int32 m_casttime;                                   // Calculated spell cast time initialized only in Spell::prepare
        int32 m_duration;
        bool m_canReflect;                                  // can reflect this spell?
        bool m_autoRepeat;

        uint8 m_delayAtDamageCount;
        int32 GetNextDelayAtDamageMsTime()
        {
            return m_delayAtDamageCount < 5 ? 1000 - (m_delayAtDamageCount++) * 200 : 200;
        }

        // Delayed spells system
        uint64 m_delayStart;                                // time of spell delay start, filled by event handler, zero = just started
        uint64 m_delayMoment;                               // moment of next delay call, used internally
        bool m_immediateHandled;                            // were immediate actions handled? (used by delayed spells only)

        // These vars are used in both delayed spell system and modified immediate spell system
        bool m_referencedFromCurrentSpell;                  // mark as references to prevent deleted and access by dead pointers
        bool m_executedCurrently;                           // mark as executed to prevent deleted and access by dead pointers
        bool m_needSpellLog;                                // need to send spell log?
        uint8 m_applyMultiplierMask;                        // by effect: damage multiplier needed?
        float m_damageMultipliers[3];                       // by effect: damage multiplier

        // Current targets, to be used in SpellEffects (MUST BE USED ONLY IN SPELL EFFECTS)
        Unit* unitTarget;
        Item* itemTarget;
        GameObject* gameObjTarget;
        SpellAuraHolder* m_spellAuraHolder;                 // spell aura holder for current target, created only if spell has aura applying effect
        int32 damage;

        // this is set in Spell Hit, but used in Apply Aura handler
        unit::Fade m_diminishLevel = unit::Fade::Full;
        bool m_diminishApplies = false;
        DiminishingGroup m_diminishGroup;

        // -------------------------------------------
        GameObject* focusObject;

        // Damage and healing in effects need just calculate
        int32 m_damage;                                     // Damage   in effects count here
        int32 m_healing;                                    // Healing in effects count here
        int32 m_healthLeech;                                // Health leech in effects for all targets count here

        //******************************************
        // Spell trigger system
        //******************************************
        bool   m_canTrigger;                                // Can start trigger (m_IsTriggeredSpell can`t use for this)
        uint8  m_negativeEffectMask;                        // Use for avoid sent negative spell procs for additional positive effects only targets
        uint32 m_procAttacker;                              // Attacker trigger flags
        uint32 m_procVictim;                                // Victim   trigger flags
        void   prepareDataForTriggerSystem();

        //*****************************************
        // Spell target filling
        //*****************************************
        void FillTargetMap();
        void SetTargetMap(const cast::Operation& operation, uint32 targetMode, UnitList& targetUnitMap);

        void FillAreaTargets(UnitList& targetUnitMap, float radius, SpellNotifyPushType pushType, SpellTargets spellTargets, Occupant* originalCaster = nullptr);
        void FillRaidOrPartyTargets(UnitList& targetUnitMap, Unit* member, float radius, bool raid, bool withPets, bool withcaster);

        // Returns GUID either of the 1st target from the implicit target list, or of explicit one (selected victim)
        ObjectGuid GetPrefilledOrUnitTargetGuid(SpellEffectIndex effIndex) const;
        void GetSpellRangeAndRadius(SpellEffectIndex effIndex, float& radius, uint32& EffectChainTarget, uint32& unMaxTargets) const;

        //*****************************************
        // Spell target subsystem
        //*****************************************
        // Targets store structures and data
        struct TargetInfo
        {
            ObjectGuid targetGUID;
            uint64 timeDelay;
            uint32 HitInfo;
            uint32 damage;
            SpellMissInfo missCondition: 8;
            SpellMissInfo reflectResult: 8;
            uint8  effectMask: 8;
            bool   processed: 1;
        };
        uint8 m_needAliveTargetMask;                        // Mask req. alive targets

        struct GOTargetInfo
        {
            ObjectGuid targetGUID;
            uint64 timeDelay;
            uint8  effectMask: 8;
            bool   processed: 1;
        };

        struct ItemTargetInfo
        {
            Item*  item;
            uint8 effectMask;
        };

        typedef std::list<TargetInfo>     TargetList;
        typedef std::list<GOTargetInfo>   GOTargetList;
        typedef std::list<ItemTargetInfo> ItemTargetList;

        TargetList     m_UniqueTargetInfo;
        GOTargetList   m_UniqueGOTargetInfo;
        ItemTargetList m_UniqueItemInfo;

        void AddUnitTarget(Unit* target, SpellEffectIndex effIndex);
        void AddUnitTarget(ObjectGuid unitGuid, SpellEffectIndex effIndex);
        void AddGOTarget(GameObject* target, SpellEffectIndex effIndex);
        void AddGOTarget(ObjectGuid goGuid, SpellEffectIndex effIndex);
        void AddItemTarget(Item* target, SpellEffectIndex effIndex);
        void DoAllEffectOnTarget(TargetInfo* target);
        void HandleDelayedSpellLaunch(TargetInfo* target);
        void InitializeDamageMultipliers();
        void ResetEffectDamageAndHeal();
        void DoSpellHitOnUnit(Unit* unit, uint32 effectMask, bool isReflected = false);
        void DoAllEffectOnTarget(GOTargetInfo* target);
        void DoAllEffectOnTarget(ItemTargetInfo* target);
        bool IsAliveUnitPresentInTargetList();
        SpellCastResult CanOpenLock(SpellEffectIndex effIndex, uint32 lockid, SkillType& skillid, int32& reqSkillValue, int32& skillValue);
        bool IsLockInRange(GameObject* go);
        SpellCastResult CanTameUnit(bool isGM = false);
        // -------------------------------------------

        // List For Triggered Spells
        typedef std::list<SpellEntry const*> SpellInfoList;
        SpellInfoList m_TriggerSpells;                      // casted by caster to same targets settings in m_targets at success finish of current spell
        SpellInfoList m_preCastSpells;                      // casted by caster to each target at spell hit before spell effects apply

        uint32 m_spellState;
        uint32 m_timer;

        float m_castPositionX;
        float m_castPositionY;
        float m_castPositionZ;
        float m_castOrientation;
        bool m_IsTriggeredSpell;

        // if need this can be replaced by Aura copy
        // we can't store original aura link to prevent access to deleted auras
        // and in same time need aura data and after aura deleting.
        SpellEntry const* m_triggeredByAuraSpell;
};

enum ReplenishType
{
    REPLENISH_UNDEFINED = 0,
    REPLENISH_HEALTH    = 20,
    REPLENISH_MANA      = 21,
    REPLENISH_RAGE      = 22
};

namespace MaNGOS
{
    struct SpellNotifierPlayer              // Currently unused. When put to use this one requires handling for source-location (smilar to below)
    {
        Spell::UnitList& i_data;
        Spell& i_spell;
        const uint32& i_index;
        float i_radius;
        Occupant* i_originalCaster;

        SpellNotifierPlayer(Spell& spell, Spell::UnitList& data, const uint32& i, float radius)
            : i_data(data), i_spell(spell), i_index(i), i_radius(radius)
        {
            i_originalCaster = i_spell.GetAffectiveCasterObject();
        }

        void Visit(PlayerMapType& m)
        {
            if (!i_originalCaster)
            {
                return;
            }

            for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                Player* pPlayer = itr->getSource();
                if (!pPlayer->IsAlive() || pPlayer->IsTaxiFlying())
                {
                    continue;
                }

                if (IsFriendly(*i_originalCaster, *pPlayer))
                {
                    continue;
                }

                if (pPlayer->Where().WithinDist(Geometry::Vector3(i_spell.m_targets.m_destX, i_spell.m_targets.m_destY, i_spell.m_targets.m_destZ), i_radius))
                {
                    i_data.push_back(pPlayer);
                }
            }
        }
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    struct SpellNotifierCreatureAndPlayer
    {
        Spell::UnitList* i_data;
        Spell& i_spell;
        SpellNotifyPushType i_push_type;
        float i_radius;
        SpellTargets i_TargetType;
        Occupant* i_originalCaster;
        Occupant* i_castingObject;
        bool i_playerControlled;
        float i_centerX;
        float i_centerY;
        float i_centerZ;

        float GetCenterX() const { return i_centerX; }
        float GetCenterY() const { return i_centerY; }

        SpellNotifierCreatureAndPlayer(Spell& spell, Spell::UnitList& data, float radius, SpellNotifyPushType type,
            SpellTargets TargetType = SPELL_TARGETS_NOT_FRIENDLY, Occupant* originalCaster = nullptr)
                : i_data(&data), i_spell(spell), i_push_type(type), i_radius(radius), i_TargetType(TargetType),
            i_originalCaster(originalCaster), i_castingObject(i_spell.GetCastingObject())
        {
            if (!i_originalCaster)
            {
                i_originalCaster = i_spell.GetAffectiveCasterObject();
            }
            i_playerControlled = i_originalCaster  ? i_originalCaster->IsControlledByPlayer() : false;

            switch (i_push_type)
            {
                case PUSH_IN_FRONT:
                case PUSH_IN_FRONT_90:
                case PUSH_IN_FRONT_15:
                case PUSH_IN_BACK:
                case PUSH_SELF_CENTER:
                    if (i_castingObject)
                    {
                        i_centerX = i_castingObject->Where().X();
                        i_centerY = i_castingObject->Where().Y();
                    }
                    break;
                case PUSH_DEST_CENTER:
                    if (i_spell.m_targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
                    {
                        i_spell.m_targets.getSource(i_centerX, i_centerY, i_centerZ);
                    }
                    else
                    {
                        i_spell.m_targets.getDestination(i_centerX, i_centerY, i_centerZ);
                    }
                    break;
                case PUSH_TARGET_CENTER:
                    if (Unit* target = i_spell.m_targets.getUnitTarget())
                    {
                        i_centerX = target->Where().X();
                        i_centerY = target->Where().Y();
                    }
                    break;
                default:
                    sLog.outError("SpellNotifierCreatureAndPlayer: unsupported PUSH_* case %u.", i_push_type);
            }
        }

        template<class T> inline void Visit(GridRefManager<T>&  m)
        {
            MANGOS_ASSERT(i_data);

            if (!i_originalCaster || !i_castingObject)
            {
                return;
            }

            for (typename GridRefManager<T>::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                // GM OFF Spell must pass the checks.
                bool gmSpell = (i_spell.m_spellInfo->ID == 1509);
                // there are still more spells which can be casted on dead, but
                // they are no AOE and don't have such a nice SPELL_ATTR flag

                if (!gmSpell)
                {
                    if ((i_TargetType != SPELL_TARGETS_ALL && !itr->getSource()->IsTargetableForAttack(i_spell.m_spellInfo->HasAttribute(SPELL_ATTR_EX3_CAST_ON_DEAD))) ||
                        // mostly phase check
                        !itr->getSource()->Where().ShareFrame(i_originalCaster->Where()))
                    {
                        continue;
                    }

                    switch (i_TargetType)
                    {
                        case SPELL_TARGETS_HOSTILE:
                            if (!IsHostile(*i_originalCaster, *itr->getSource()))
                            {
                                continue;
                            }
                            break;
                        case SPELL_TARGETS_NOT_FRIENDLY:
                            if (IsFriendly(*i_originalCaster, *itr->getSource()))
                            {
                                continue;
                            }
                            break;
                        case SPELL_TARGETS_NOT_HOSTILE:
                            if (IsHostile(*i_originalCaster, *itr->getSource()))
                            {
                                continue;
                            }
                            break;
                        case SPELL_TARGETS_FRIENDLY:
                            if (!IsFriendly(*i_originalCaster, *itr->getSource()))
                            {
                                continue;
                            }
                            break;
                        case SPELL_TARGETS_AOE_DAMAGE:
                        {
                            if (itr->getSource()->IsCreature() && ((Creature*)itr->getSource())->IsTotem())
                            {
                                continue;
                            }

                            if (i_playerControlled)
                            {
                                if (IsFriendly(*i_originalCaster, *itr->getSource()))
                                {
                                    continue;
                                }
                            }
                            else
                            {
                                if (!IsHostile(*i_originalCaster, *itr->getSource()))
                                {
                                    continue;
                                }
                            }
                        }
                        break;
                        case SPELL_TARGETS_ALL:
                            break;
                        default: continue;
                    }
                }

                // we don't need to check InMap here, it's already done some lines above
                switch (i_push_type)
                {
                    case PUSH_IN_FRONT:
                        if (InFrontPhased(*i_castingObject, *((Unit*)(itr->getSource())), i_radius, 2 * M_PI_F / 3))
                        {
                            i_data->push_back(itr->getSource());
                        }
                        break;
                    case PUSH_IN_FRONT_90:
                        if (InFrontPhased(*i_castingObject, *((Unit*)(itr->getSource())), i_radius, M_PI_F / 2))
                        {
                            i_data->push_back(itr->getSource());
                        }
                        break;
                    case PUSH_IN_FRONT_15:
                        if (InFrontPhased(*i_castingObject, *((Unit*)(itr->getSource())), i_radius, M_PI_F / 12))
                        {
                            i_data->push_back(itr->getSource());
                        }
                        break;
                    case PUSH_IN_BACK:
                        if (InBackPhased(*i_castingObject, *((Unit*)(itr->getSource())), i_radius, 2 * M_PI_F / 3))
                        {
                            i_data->push_back(itr->getSource());
                        }
                        break;
                    case PUSH_SELF_CENTER:
                        if (i_castingObject->Where().WithinDist(((Unit*)(itr->getSource()))->Where(), i_radius))
                        {
                            i_data->push_back(itr->getSource());
                        }
                        break;
                    case PUSH_DEST_CENTER:
                        if (itr->getSource()->Where().WithinDist(Geometry::Vector3(i_centerX, i_centerY, i_centerZ), i_radius))
                        {
                            i_data->push_back(itr->getSource());
                        }
                        break;
                    case PUSH_TARGET_CENTER:
                        if (i_spell.m_targets.getUnitTarget() && i_spell.m_targets.getUnitTarget()->Where().WithinDist(((Unit*)(itr->getSource()))->Where(), i_radius))
                        {
                            i_data->push_back(itr->getSource());
                        }
                        break;
                }
            }
        }

#ifdef WIN32
        template<> inline void Visit(CorpseMapType&) {}
        template<> inline void Visit(GameObjectMapType&) {}
        template<> inline void Visit(DynamicObjectMapType&) {}
        template<> inline void Visit(CameraMapType&) {}
#endif
    };

#ifndef WIN32
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(CorpseMapType&) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(GameObjectMapType&) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(DynamicObjectMapType&) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(CameraMapType&) {}
#endif
}

typedef void(Spell::*pEffect)(const cast::Operation& operation);

class SpellEvent : public BasicEvent
{
    public:
        SpellEvent(Spell* spell);
        virtual ~SpellEvent();

        bool Execute(uint64 e_time, uint32 p_time) override;
        void Abort(uint64 e_time) override;
        bool IsDeletable() const override;
    protected:
        Spell* m_Spell;
};
