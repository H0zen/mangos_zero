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
#include "Cast/Roster/Roster.h"
#include "Cast/Targets/Catchment.h"

class WorldSession;
class WorldPacket;
class DynamicObj;
class Item;
class GameObject;
class Group;
class Aura;

enum SpellCastFlags
{
    CAST_FLAG_NONE              = 0x00000000,
    CAST_FLAG_HIDDEN_COMBATLOG  = 0x00000001,
    CAST_FLAG_UNKNOWN2          = 0x00000002,
    CAST_FLAG_UNKNOWN3          = 0x00000004,
    CAST_FLAG_UNKNOWN4          = 0x00000008,
    CAST_FLAG_UNKNOWN5          = 0x00000010,
    CAST_FLAG_AMMO              = 0x00000020,
    CAST_FLAG_UNKNOWN7          = 0x00000040,

    CAST_FLAG_UNKNOWN8          = 0x00000080,
    CAST_FLAG_UNKNOWN9          = 0x00000100,
};

bool IsQuestTameSpell(uint32 spellId);

class SpellCastTargets;

struct SpellCastTargetsReader
{

    explicit SpellCastTargetsReader(SpellCastTargets& _targets, Unit* _caster) : targets(_targets), caster(_caster) {}

    SpellCastTargets& targets;
    Unit* caster;
};

class SpellCastTargets
{
    public:
        SpellCastTargets();
        ~SpellCastTargets();

        void read(ByteBuffer& data, Unit* caster);

        void write(ByteBuffer& data) const;

        SpellCastTargetsReader ReadForCaster(Unit* caster) { return SpellCastTargetsReader(*this, caster); }

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

        Unit* m_unitTarget;
        GameObject* m_GOTarget;
        Item* m_itemTarget;

        ObjectGuid m_unitTargetGUID = 0;
        ObjectGuid m_GOTargetGUID = 0;
        ObjectGuid m_CorpseTargetGUID = 0;
        ObjectGuid m_itemTargetGUID = 0;
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
    SPELL_STATE_CREATED     = 0,
    SPELL_STATE_PREPARING   = 1,
    SPELL_STATE_CASTING     = 2,
    SPELL_STATE_DELAYED     = 3,
    SPELL_STATE_TRAVELING   = 4,
    SPELL_STATE_LANDING     = 5,
    SPELL_STATE_CHANNELING  = 6,
    SPELL_STATE_FINISHED    = 7,
};

typedef std::multimap<uint64, uint64> SpellTargetTimeMap;

class Spell
{
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
        bool ThrowWhatTheTableNames(const cast::Operation& operation);
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

        Spell(Unit* caster, SpellEntry const* info, bool triggered, ObjectGuid originalCasterGUID = 0, SpellEntry const* triggeredBy = nullptr);
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
        SpellCastResult CheckTheCasterMay(bool strict);
        SpellCastResult CheckWhereTheCasterStands();
        SpellCastResult CheckTheTradeSlot();
        SpellCastResult CheckTheTargetChosen(bool strict);
        SpellCastResult EnrolScriptedTargets();
        SpellCastResult CheckEachSlotCanRun();
        SpellCastResult CheckEachAuraCanHold();
        SpellCastResult CheckPetCast(Unit* target);

        void handle_immediate();
        uint64 handle_delayed(uint64 t_offset);

        void _handle_immediate_phase();
        void _handle_finish_phase();

        SpellCastResult CheckItems();
        SpellCastResult CheckRange(bool strict);
        SpellCastResult CheckPower();
        SpellCastResult CheckCasterAuras() const;

        int32 CalculateDamage(SpellEffectIndex i, Unit* target) { return m_caster->CalculateSpellDamage(target, Recipe(), Recipe().At(static_cast<uint8>(i)), &m_currentBasePoints[i]); }
        static uint32 CalculatePowerCost(SpellEntry const* spellInfo, Unit* caster, Spell const* spell = nullptr, Item* castItem = nullptr);

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

        SpellEntry const* GetSpellBonusLevelPenaltySpell(SpellEntry const* spellProto) const;

        SpellEntry const* m_spellInfo;

        const cast::Recipe& Recipe() const { return *m_recipe; }

        const cast::Recipe* m_recipe;
        SpellEntry const* m_triggeredBySpellInfo;
        int32 m_currentBasePoints[MAX_EFFECT_INDEX];
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
        uint64 GetDelayMoment() const { return m_roster.SoonestArrivalMs(); }

        bool IsNeedSendToClient() const;
        bool IsTriggeredSpellWithRedundentCastTime() const;

        CurrentSpellTypes GetCurrentContainer();

        Unit* GetCaster() const { return m_caster; }

        Occupant* GetAffectiveCasterObject() const;

        Unit* GetAffectiveCaster() const { return m_originalCasterGUID ? m_originalCaster : m_caster; }

        Occupant* GetCastingObject() const;

        uint32 GetPowerCost() const { return m_powerCost; }

        void UpdatePointers();

        bool CheckTargetCreatureType(Unit* target) const;

        void AddTriggeredSpell(SpellEntry const* spellInfo) { m_TriggerSpells.push_back(spellInfo); }
        void AddPrecastSpell(SpellEntry const* spellInfo) { m_preCastSpells.push_back(spellInfo); }
        void AddTriggeredSpell(uint32 spellId);
        void AddPrecastSpell(uint32 spellId);
        void CastPreCastSpells(Unit* target);
        void CastTriggerSpells();

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
        bool IgnoreItemRequirements() const;
        void UpdateOriginalCasterPointer();

        Unit* m_caster;

        ObjectGuid m_originalCasterGUID = 0;

        Unit* m_originalCaster;

        Spell** m_selfContainer;

        SpellSchoolMask m_spellSchoolMask;
        uint32 m_powerCost;
        int32 m_casttime;
        int32 m_duration;
        bool m_canReflect;
        bool m_autoRepeat;

        uint8 m_delayAtDamageCount;
        int32 GetNextDelayAtDamageMsTime()
        {
            return m_delayAtDamageCount < 5 ? 1000 - (m_delayAtDamageCount++) * 200 : 200;
        }

        uint64 m_delayStart;
        bool m_immediateHandled;

        bool m_referencedFromCurrentSpell;
        bool m_executedCurrently;
        bool m_needSpellLog;
        uint8 m_applyMultiplierMask;
        float m_damageMultipliers[3];

        Unit* unitTarget;
        Item* itemTarget;
        GameObject* gameObjTarget;
        SpellAuraHolder* m_spellAuraHolder;
        int32 damage;

        unit::Fade m_diminishLevel = unit::Fade::Full;
        bool m_diminishApplies = false;
        DiminishingGroup m_diminishGroup;

        GameObject* focusObject;

        int32 m_damage;
        int32 m_healing;
        int32 m_healthLeech;

        bool m_setsOffProcs;
        bool SetsOffProcs() const;

        void FillTargetMap();
        void SetTargetMap(const cast::Operation& operation, uint32 targetMode, UnitList& targetUnitMap);
        void PickWhatTheSlotImplies(const cast::Operation& operation, UnitList& targetUnitMap);
        void PickARandomChainInTheArea(uint32 targetMode, UnitList& targetUnitMap, float radius, uint32 chainTargets, uint32& mayHit);
        void PickTheChainFromTheVictim(const cast::Operation& operation, UnitList& targetUnitMap, float radius, uint32 chainTargets, uint32& mayHit);
        void PickTheAreaTheVerbWants(const cast::Operation& operation, UnitList& targetUnitMap, float radius);
        void PickTheNamedCreaturesInTheArea(const cast::Operation& operation, UnitList& targetUnitMap, float radius);
        void PickTheObjectsAroundTheSpot(SpellEffectIndex effIndex, uint32 targetMode, std::list<GameObject*>& found, float radius);
        void PickTheOneGroupmate(UnitList& targetUnitMap);
        void PickTheConeThisSpellOpens(const cast::Operation& operation, UnitList& targetUnitMap, float radius);
        void PickThePartyAround(UnitList& targetUnitMap, float radius);
        void PickTheChainOfWounded(UnitList& targetUnitMap, float radius, uint32 chainTargets, uint32& mayHit);
        void PickThePartyOfTheTargetsClass(UnitList& targetUnitMap, float radius);
        void PickTheSpotBesideTheCaster(const cast::Operation& operation, uint32 targetMode, UnitList& targetUnitMap, float radius);

        void FillAreaTargets(UnitList& targetUnitMap, float radius, cast::Around where, cast::Side side, Occupant* originalCaster = nullptr);
        void FillRaidOrPartyTargets(UnitList& targetUnitMap, Unit* member, float radius, bool raid, bool withPets, bool withcaster);

        ObjectGuid GetPrefilledOrUnitTargetGuid(SpellEffectIndex effIndex) const;
        void GetSpellRangeAndRadius(SpellEffectIndex effIndex, float& radius, uint32& EffectChainTarget, uint32& unMaxTargets) const;

        cast::Roster m_roster;
        uint8 m_needAliveTargetMask;

        void EnrolUnit(Unit* target, SpellEffectIndex effIndex);
        void EnrolUnit(ObjectGuid unitGuid, SpellEffectIndex effIndex);
        void EnrolObject(GameObject* target, SpellEffectIndex effIndex);
        void EnrolObject(ObjectGuid goGuid, SpellEffectIndex effIndex);
        void EnrolItem(Item* target, SpellEffectIndex effIndex);
        void DoAllEffectOnTarget(cast::UnitTarget* target);
        void HandleDelayedSpellLaunch(cast::UnitTarget* target);
        void InitializeDamageMultipliers();
        void ResetEffectDamageAndHeal();
        void DoSpellHitOnUnit(Unit* unit, uint32 effectMask, bool isReflected = false);
        void DoAllEffectOnTarget(cast::ObjectTarget* target);
        void DoAllEffectOnTarget(cast::ItemTarget* target);
        bool IsAliveUnitPresentInTargetList();
        SpellCastResult CanOpenLock(SpellEffectIndex effIndex, uint32 lockid, SkillType& skillid, int32& reqSkillValue, int32& skillValue);
        bool IsLockInRange(GameObject* go);
        SpellCastResult CanTameUnit(bool isGM = false);

        typedef std::list<SpellEntry const*> SpellInfoList;
        SpellInfoList m_TriggerSpells;
        SpellInfoList m_preCastSpells;

        uint32 m_spellState;
        uint32 m_timer;

        float m_castPositionX;
        float m_castPositionY;
        float m_castPositionZ;
        float m_castOrientation;
        bool m_IsTriggeredSpell;

        SpellEntry const* m_triggeredByAuraSpell;
};

enum ReplenishType
{
    REPLENISH_UNDEFINED = 0,
    REPLENISH_HEALTH    = 20,
    REPLENISH_MANA      = 21,
    REPLENISH_RAGE      = 22
};

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
