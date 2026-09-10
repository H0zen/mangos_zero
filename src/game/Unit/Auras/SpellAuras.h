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

#include "SpellAuraDefines.h"
#include "ObjectMgr.h"
#include "Cast/Recipe/Recipe.h"

struct Modifier
{

    AuraType m_auraname;

    int32 m_amount;

    int32 m_miscvalue;

    uint32 periodictime;
};

class Unit;
struct SpellEntry;
struct SpellModifier;
struct ProcTriggerSpell;

class Aura;

struct ReapplyAffectedPassiveAurasHelper;

class SpellAuraHolder
{
    public:
        SpellAuraHolder(SpellEntry const* spellproto, Unit* target, Occupant* caster, Item* castItem);
        Aura* m_auras[MAX_EFFECT_INDEX];

        void AddAura(Aura* aura, SpellEffectIndex index);
        void RemoveAura(SpellEffectIndex index);
        void ApplyAuraModifiers(bool apply, bool real = false);
        void _AddSpellAuraHolder();
        void _RemoveSpellAuraHolder();
        void HandleSpellSpecificBoosts(bool apply);
        void CleanupTriggeredSpells();

        void setDiminishGroup(DiminishingGroup group) { m_AuraDRGroup = group; }
        DiminishingGroup getDiminishGroup() const { return m_AuraDRGroup; }

        uint32 GetStackAmount() const { return m_stackAmount; }
        void SetStackAmount(uint32 stackAmount);
        bool ModStackAmount(int32 num);

        Aura* GetAuraByEffectIndex(SpellEffectIndex index) const { return m_auras[index]; }

        uint32 GetId() const { return m_spellProto->ID; }
        SpellEntry const* GetSpellProto() const { return m_spellProto; }

        const cast::Recipe& Recipe() const { return *m_recipe; }

        ObjectGuid const& GetCasterGuid() const { return m_casterGuid; }
        void SetCasterGuid(ObjectGuid guid) { m_casterGuid = guid; }
        ObjectGuid const& GetCastItemGuid() const { return m_castItemGuid; }
        Unit* GetCaster() const;
        Unit* GetTarget() const { return m_target; }
        void SetTarget(Unit* target) { m_target = target; }

        bool IsPermanent() const { return m_permanent; }
        void SetPermanent(bool permanent) { m_permanent = permanent; }
        bool IsPassive() const { return m_isPassive; }
        bool IsDeathPersistent() const { return m_isDeathPersist; }
        bool IsPersistent() const;
        bool IsPositive() const;
        bool IsAreaAura() const;
        bool IsWeaponBuffCoexistableWith(SpellAuraHolder const* ref) const;
        bool IsNeedVisibleSlot(Unit const* caster) const;
        bool IsRemovedOnShapeLost() const { return m_isRemovedOnShapeLost; }
        bool IsInUse() const { return m_in_use;}
        bool IsDeleted() const { return m_deleted;}
        bool IsEmptyHolder() const;

        void SetDeleted()
        {
            m_deleted = true;
        }

        void SetInUse(bool state)
        {
            if (state)
            {
                ++m_in_use;
            }
            else
            {
                if (m_in_use)
                {
                    --m_in_use;
                }
            }
        }

        void UpdateHolder(uint32 diff) { SetInUse(true); Update(diff); SetInUse(false); }
        void Update(uint32 diff);
        void RefreshHolder();

        TrackedAuraType GetTrackedAuraType() const { return m_trackedAuraType; }
        void SetTrackedAuraType(TrackedAuraType val) { m_trackedAuraType = val; }
        void UnregisterAndCleanupTrackedAuras();

        int32 GetAuraMaxDuration() const { return m_maxDuration; }
        void SetAuraMaxDuration(int32 duration);
        int32 GetAuraDuration() const { return m_duration; }
        void SetAuraDuration(int32 duration) { m_duration = duration; }

        uint8 GetAuraSlot() const { return m_auraSlot; }
        void SetAuraSlot(uint8 slot) { m_auraSlot = slot; }
        uint8 GetAuraLevel() const { return m_auraLevel; }
        void SetAuraLevel(uint8 level) { m_auraLevel = level; }
        uint32 GetAuraCharges() const { return m_procCharges; }
        void SetAuraCharges(uint32 charges)
        {
            if (m_procCharges == charges)
            {
                return;
            }
            m_procCharges = charges;

            UpdateAuraApplication();
        }
        bool DropAuraCharge()
        {
            if (m_procCharges == 0)
            {
                return false;
            }

            --m_procCharges;
            UpdateAuraApplication();
            return m_procCharges == 0;
        }

        time_t GetAuraApplyTime() const { return m_applyTime; }

        void SetRemoveMode(AuraRemoveMode mode) { m_removeMode = mode; }
        void SetLoadedState(ObjectGuid const& casterGUID, ObjectGuid const& itemGUID, uint32 stackAmount, uint32 charges, int32 maxduration, int32 duration)
        {
            m_casterGuid   = casterGUID;
            m_castItemGuid = itemGUID;
            m_procCharges  = charges;
            m_stackAmount  = stackAmount;
            SetAuraMaxDuration(maxduration);
            SetAuraDuration(duration);
        }

        bool HasMechanic(uint32 mechanic) const;
        bool HasMechanicMask(uint32 mechanicMask) const;

        void UpdateAuraDuration();
        void SendAuraDurationForCaster(Player* caster);

        void SetAura(uint32 slot, bool remove) { m_target->SetUInt32Value(UNIT_FIELD_AURA + slot, remove ? 0 : GetId()); }
        void SetAuraFlag(uint32 slot, bool add);
        void SetAuraLevel(uint32 slot, uint32 level);

        ~SpellAuraHolder();
    private:
        void UpdateAuraApplication();
        bool HeartbeatResist(uint32 diff);

        SpellEntry const* m_spellProto;
        const cast::Recipe* m_recipe;

        Unit* m_target;
        ObjectGuid m_casterGuid = 0;
        ObjectGuid m_castItemGuid = 0;
        time_t m_applyTime;

        uint8 m_auraSlot;
        uint8 m_auraLevel;
        uint32 m_procCharges;
        uint32 m_stackAmount;
        int32 m_maxDuration;
        int32 m_duration;
        int32 m_timeCla;

        AuraRemoveMode m_removeMode: 8;
        DiminishingGroup m_AuraDRGroup: 8;
        TrackedAuraType m_trackedAuraType: 8;

        bool m_permanent: 1;
        bool m_isPassive: 1;
        bool m_isDeathPersist: 1;
        bool m_isRemovedOnShapeLost: 1;
        bool m_isHeartbeatSubject: 1;
        bool m_deleted: 1;

        uint32 m_in_use;
};

typedef void(Aura::*pAuraHandler)(bool Apply, bool Real);

class Aura
{
    friend struct ReapplyAffectedPassiveAurasHelper;
    friend Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster, Item* castItem);

    public:

        void HandleNULL(bool, bool)
        {

        }
        void HandleUnused(bool, bool)
        {

        }
        void HandleNoImmediateEffect(bool, bool)
        {

        }
        void HandleModDetectRange(bool Apply, bool Real);
        void HandleBindSight(bool Apply, bool Real);
        void HandleModPossess(bool Apply, bool Real);
        void HandlePeriodicDamage(bool Apply, bool Real);
        void HandleAuraDummy(bool Apply, bool Real);
        void HandleModConfuse(bool Apply, bool Real);
        void HandleModCharm(bool Apply, bool Real);
        void HandleModFear(bool Apply, bool Real);
        void HandlePeriodicHeal(bool Apply, bool Real);
        void HandleModAttackSpeed(bool Apply, bool Real);
        void HandleModThreat(bool Apply, bool Real);
        void HandleModTaunt(bool Apply, bool Real);
        void HandleFeignDeath(bool Apply, bool Real);
        void HandleAuraModDisarm(bool Apply, bool Real);
        void HandleAuraModStalked(bool Apply, bool Real);
        void HandleAuraWaterWalk(bool Apply, bool Real);
        void HandleAuraFeatherFall(bool Apply, bool Real);
        void HandleAuraHover(bool Apply, bool Real);
        void HandleAddModifier(bool Apply, bool Real);
        void HandleAuraModStun(bool Apply, bool Real);
        void HandleModDamageDone(bool Apply, bool Real);
        void HandleAuraUntrackable(bool Apply, bool Real);
        void HandleAuraEmpathy(bool Apply, bool Real);
        void HandleModOffhandDamagePercent(bool apply, bool Real);
        void HandleAuraModRangedAttackPower(bool Apply, bool Real);
        void HandleAuraModIncreaseEnergyPercent(bool Apply, bool Real);
        void HandleAuraModIncreaseHealthPercent(bool Apply, bool Real);
        void HandleAuraModRegenInterrupt(bool Apply, bool Real);
        void HandleModMeleeSpeedPct(bool Apply, bool Real);
        void HandlePeriodicTriggerSpell(bool Apply, bool Real);
        void HandlePeriodicTriggerSpellWithValue(bool apply, bool Real);
        void HandlePeriodicEnergize(bool Apply, bool Real);
        void HandleAuraModResistanceExclusive(bool Apply, bool Real);
        void HandleAuraSafeFall(bool Apply, bool Real);
        void HandleModStealth(bool Apply, bool Real);
        void HandleInvisibility(bool Apply, bool Real);
        void HandleInvisibilityDetect(bool Apply, bool Real);
        void HandleAuraModTotalHealthPercentRegen(bool Apply, bool Real);
        void HandleAuraModTotalManaPercentRegen(bool Apply, bool Real);
        void HandleAuraModResistance(bool Apply, bool Real);
        void HandleAuraModRoot(bool Apply, bool Real);
        void HandleAuraModSilence(bool Apply, bool Real);
        void HandleAuraModStat(bool Apply, bool Real);
        void HandleDetectAmore(bool Apply, bool Real);
        void HandleAuraModIncreaseSpeed(bool Apply, bool Real);
        void HandleAuraModIncreaseMountedSpeed(bool Apply, bool Real);
        void HandleAuraModDecreaseSpeed(bool Apply, bool Real);
        void HandleAuraModUseNormalSpeed(bool Apply, bool Real);
        void HandleAuraModIncreaseHealth(bool Apply, bool Real);
        void HandleAuraModIncreaseEnergy(bool Apply, bool Real);
        void HandleAuraModShapeshift(bool Apply, bool Real);
        void HandleAuraModEffectImmunity(bool Apply, bool Real);
        void HandleAuraModStateImmunity(bool Apply, bool Real);
        void HandleAuraModSchoolImmunity(bool Apply, bool Real);
        void HandleAuraModDmgImmunity(bool Apply, bool Real);
        void HandleAuraModDispelImmunity(bool Apply, bool Real);
        void HandleAuraProcTriggerSpell(bool Apply, bool Real);
        void HandleAuraTrackCreatures(bool Apply, bool Real);
        void HandleAuraTrackResources(bool Apply, bool Real);
        void HandleAuraModParryPercent(bool Apply, bool Real);
        void HandleAuraModDodgePercent(bool Apply, bool Real);
        void HandleAuraModBlockPercent(bool Apply, bool Real);
        void HandleAuraModCritPercent(bool Apply, bool Real);
        void HandlePeriodicLeech(bool Apply, bool Real);
        void HandleModHitChance(bool Apply, bool Real);
        void HandleModSpellHitChance(bool Apply, bool Real);
        void HandleAuraModScale(bool Apply, bool Real);
        void HandlePeriodicManaLeech(bool Apply, bool Real);
        void HandlePeriodicHealthFunnel(bool apply, bool Real);
        void HandleModCastingSpeed(bool Apply, bool Real);
        void HandleAuraMounted(bool Apply, bool Real);
        void HandleWaterBreathing(bool Apply, bool Real);
        void HandleModBaseResistance(bool Apply, bool Real);
        void HandleModRegen(bool Apply, bool Real);
        void HandleModPowerRegen(bool Apply, bool Real);
        void HandleModPowerRegenPCT(bool Apply, bool Real);
        void HandleChannelDeathItem(bool Apply, bool Real);
        void HandlePeriodicDamagePCT(bool Apply, bool Real);
        void HandleAuraModAttackPower(bool Apply, bool Real);
        void HandleAuraTransform(bool Apply, bool Real);
        void HandleModSpellCritChance(bool Apply, bool Real);
        void HandleAuraModIncreaseSwimSpeed(bool Apply, bool Real);
        void HandleModPowerCostPCT(bool Apply, bool Real);
        void HandleModPowerCost(bool Apply, bool Real);
        void HandleFarSight(bool Apply, bool Real);
        void HandleModPossessPet(bool Apply, bool Real);
        void HandleModMechanicImmunity(bool Apply, bool Real);
        void HandleModMechanicImmunityMask(bool Apply, bool Real);
        void HandleAuraModSkill(bool Apply, bool Real);
        void HandleModDamagePercentDone(bool Apply, bool Real);
        void HandleModPercentStat(bool Apply, bool Real);
        void HandleAurasVisible(bool Apply, bool Real);
        void HandleModResistancePercent(bool Apply, bool Real);
        void HandleAuraModBaseResistancePCT(bool Apply, bool Real);
        void HandleAuraTrackStealthed(bool Apply, bool Real);
        void HandleForceReaction(bool Apply, bool Real);
        void HandleAuraModRangedHaste(bool Apply, bool Real);
        void HandleRangedAmmoHaste(bool Apply, bool Real);
        void HandleModHealingDone(bool Apply, bool Real);
        void HandleModTotalPercentStat(bool Apply, bool Real);
        void HandleAuraModTotalThreat(bool Apply, bool Real);
        void HandleModUnattackable(bool Apply, bool Real);
        void HandleAuraModPacify(bool Apply, bool Real);
        void HandleAuraGhost(bool Apply, bool Real);
        void HandleAuraModAttackPowerPercent(bool apply, bool Real);
        void HandleAuraModRangedAttackPowerPercent(bool apply, bool Real);
        void HandleSpiritOfRedemption(bool apply, bool Real);
        void HandleShieldBlockValue(bool apply, bool Real);
        void HandleModSpellCritChanceShool(bool apply, bool Real);
        void HandleAuraRetainComboPoints(bool apply, bool Real);
        void HandleModSpellDamagePercentFromStat(bool apply, bool Real);
        void HandleModSpellHealingPercentFromStat(bool apply, bool Real);
        void HandleAuraModPacifyAndSilence(bool Apply, bool Real);
        void HandleAuraModResistenceOfStatPercent(bool apply, bool Real);
        void HandleAuraPowerBurn(bool apply, bool Real);
        void HandleSchoolAbsorb(bool apply, bool Real);
        void HandlePreventFleeing(bool apply, bool Real);
        void HandleManaShield(bool apply, bool Real);
        void HandleInterruptRegen(bool apply, bool Real);

        virtual ~Aura();

        void SetModifier(AuraType t, int32 a, uint32 pt, int32 miscValue);
        Modifier*       GetModifier()       { return &m_modifier; }
        Modifier const* GetModifier() const { return &m_modifier; }
        int32 GetMiscValue() const { return Operation().miscValue; }

        const cast::Operation& Operation() const { return *m_operation; }

        SpellEntry const* GetSpellProto() const { return GetHolder()->GetSpellProto(); }
        const cast::Recipe& Recipe() const { return GetHolder()->Recipe(); }
        uint32 GetId() const { return GetHolder()->GetSpellProto()->ID; }
        ObjectGuid const& GetCastItemGuid() const { return GetHolder()->GetCastItemGuid(); }
        ObjectGuid const& GetCasterGuid() const { return GetHolder()->GetCasterGuid(); }
        Unit* GetCaster() const { return GetHolder()->GetCaster(); }
        Unit* GetTarget() const { return GetHolder()->GetTarget(); }

        SpellEffectIndex GetEffIndex() const { return m_effIndex; }
        int32 GetBasePoints() const { return m_currentBasePoints; }

        int32 GetAuraMaxDuration() const { return GetHolder()->GetAuraMaxDuration(); }
        int32 GetAuraDuration() const { return GetHolder()->GetAuraDuration(); }
        time_t GetAuraApplyTime() const { return m_applyTime; }
        uint32 GetAuraTicks() const { return m_periodicTick; }
        uint32 GetAuraMaxTicks() const
        {
            int32 maxDuration = GetAuraMaxDuration();
            return maxDuration > 0 && m_modifier.periodictime > 0 ? maxDuration / m_modifier.periodictime : 0;
        }
        uint32 GetStackAmount() const { return GetHolder()->GetStackAmount(); }

        void SetLoadedState(int32 damage, uint32 periodicTime)
        {
            m_modifier.m_amount = damage;
            m_modifier.periodictime = periodicTime;

            if (uint32 maxticks = GetAuraMaxTicks())
            {
                m_periodicTick = maxticks - GetAuraDuration() / m_modifier.periodictime;
            }
        }

        bool IsPositive()
        {
            return m_positive;
        }

        bool IsPersistent() const { return m_isPersistent; }
        bool IsAreaAura() const { return m_isAreaAura; }
        bool IsPeriodic() const { return m_isPeriodic; }
        bool IsInUse() const { return m_in_use; }

        void SetInUse(bool state)
        {
            if (state)
            {
                ++m_in_use;
            }
            else
            {
                if (m_in_use)
                {
                    --m_in_use;
                }
            }
        }
        void ApplyModifier(bool apply, bool Real = false);

        void UpdateAura(uint32 diff) { SetInUse(true); Update(diff); SetInUse(false); }

        void SetRemoveMode(AuraRemoveMode mode) { m_removeMode = mode; }

        virtual Unit* GetTriggerTarget() const { return m_spellAuraHolder->GetTarget(); }

        void HandleShapeshiftBoosts(bool apply);

        void TriggerSpell();

        bool isAffectedOnSpell(SpellEntry const* spell) const;
        bool CanProcFrom(SpellEntry const* spell, uint32 EventProcEx, uint32 procEx, bool active, bool useClassMask) const;

        SpellAuraHolder* GetHolder()
        {
            return m_spellAuraHolder;
        }
        SpellAuraHolder const* GetHolder() const { return m_spellAuraHolder; }

        bool IsLastAuraOnHolder();
    protected:
        Aura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr);

        virtual void Update(uint32 diff);

        void PeriodicTick();
        void PeriodicDummyTick();

        void ReapplyAffectedPassiveAuras();

        Modifier m_modifier;
        SpellModifier* m_spellmod;

        time_t m_applyTime;

        int32 m_currentBasePoints;
        int32 m_periodicTimer;
        uint32 m_periodicTick;

        AuraRemoveMode m_removeMode: 8;

        SpellEffectIndex m_effIndex : 8;

        const cast::Operation* m_operation;

        bool m_positive: 1;
        bool m_isPeriodic: 1;
        bool m_isAreaAura: 1;
        bool m_isPersistent: 1;

        uint32 m_in_use;

        SpellAuraHolder* const m_spellAuraHolder;
    private:
        void ReapplyAffectedPassiveAuras(Unit* target);
};

class AreaAura : public Aura
{
    public:
        AreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr, uint32 originalRankSpellId = 0);
        ~AreaAura();
    protected:
        void Update(uint32 diff) override;
    private:
        float m_radius;
        AreaAuraType m_areaAuraType;
        uint32       m_originalRankSpellId;
};

class PersistentAreaAura : public Aura
{
    public:
        PersistentAreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr);
        ~PersistentAreaAura();
    protected:
        void Update(uint32 diff) override;
};

class SingleEnemyTargetAura : public Aura
{
    friend Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster, Item* castItem);

    public:
        ~SingleEnemyTargetAura();
        Unit* GetTriggerTarget() const override;

    protected:
        SingleEnemyTargetAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster  = nullptr, Item* castItem = nullptr);
        ObjectGuid m_castersTargetGuid = 0;
};

Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr);

SpellAuraHolder* CreateSpellAuraHolder(SpellEntry const* spellproto, Unit* target, Occupant* caster, Item* castItem = nullptr);
