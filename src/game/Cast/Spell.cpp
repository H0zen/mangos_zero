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

#include "Utilities/Errors.h"
#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "CreatureRecord.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "ObjectLookup.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "Corpse.h"
#include "Cast/Recipe/RecipeBook.h"

extern pEffect SpellEffects[TOTAL_SPELL_EFFECTS];

bool IsQuestTameSpell(uint32 spellId)
{
    SpellEntry const* spellproto = sSpellStore.LookupEntry(spellId);
    if (!spellproto)
    {
        return false;
    }

    return spellproto->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_THREAT &&
        spellproto->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_APPLY_AURA && spellproto->EffectAura[EFFECT_INDEX_1] == SPELL_AURA_DUMMY;
}

SpellCastTargets::SpellCastTargets()
{
    m_unitTarget = nullptr;
    m_itemTarget = nullptr;
    m_GOTarget   = nullptr;

    m_itemTargetEntry  = 0;

    m_srcX = m_srcY = m_srcZ = m_destX = m_destY = m_destZ = 0.0f;
    m_strTarget.clear();
    m_targetMask = 0;
}

SpellCastTargets::~SpellCastTargets()
{
}

void SpellCastTargets::setUnitTarget(Unit* target)
{
    if (!target)
    {
        return;
    }

    m_destX = target->Where().X();
    m_destY = target->Where().Y();
    m_destZ = target->Where().Z();
    m_unitTarget = target;
    m_unitTargetGUID = target->GetObjectGuid();
    m_targetMask |= TARGET_FLAG_UNIT;
}

void SpellCastTargets::setDestination(float x, float y, float z)
{
    m_destX = x;
    m_destY = y;
    m_destZ = z;
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

void SpellCastTargets::setSource(float x, float y, float z)
{
    m_srcX = x;
    m_srcY = y;
    m_srcZ = z;
    m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
}

void SpellCastTargets::setGOTarget(GameObject* target)
{
    m_GOTarget = target;
    m_GOTargetGUID = target->GetObjectGuid();

}

void SpellCastTargets::setItemTarget(Item* item)
{
    if (!item)
    {
        return;
    }

    m_itemTarget = item;
    m_itemTargetGUID = item->GetObjectGuid();
    m_itemTargetEntry = item->GetEntry();
    m_targetMask |= TARGET_FLAG_ITEM;
}

void SpellCastTargets::setTradeItemTarget(Player* caster)
{
    m_itemTargetGUID = static_cast<ObjectGuid>(uint64(TRADE_SLOT_NONTRADED));
    m_itemTargetEntry = 0;
    m_targetMask |= TARGET_FLAG_TRADE_ITEM;

    Update(caster);
}

void SpellCastTargets::setCorpseTarget(Corpse* corpse)
{
    m_CorpseTargetGUID = corpse->GetObjectGuid();
}

void SpellCastTargets::Update(Unit* caster)
{
    m_GOTarget   = m_GOTargetGUID ? caster->GetMap()->GetGameObject(m_GOTargetGUID) : nullptr;
    m_unitTarget = m_unitTargetGUID
        ? (m_unitTargetGUID == caster->GetObjectGuid() ? caster : ObjectLookup::GetUnit(*caster, m_unitTargetGUID))
        : nullptr;

    m_itemTarget = nullptr;
    if (IsPlayer(caster))
    {
        Player* player = ((Player*)caster);

        if (m_targetMask & TARGET_FLAG_ITEM)
        {
            m_itemTarget = player->GetItemByGuid(m_itemTargetGUID);
        }
        else if (m_targetMask & TARGET_FLAG_TRADE_ITEM)
        {
            if (TradeData* pTrade = player->GetTradeData())
            {
                if (m_itemTargetGUID < TRADE_SLOT_COUNT)
                {
                    m_itemTarget = pTrade->GetTraderData()->GetItem(TradeSlots(m_itemTargetGUID));
                }
            }
        }

        if (m_itemTarget)
        {
            m_itemTargetEntry = m_itemTarget->GetEntry();
        }
    }
}

void SpellCastTargets::read(ByteBuffer& data, Unit* caster)
{
    data >> m_targetMask;

    if (m_targetMask == TARGET_FLAG_SELF)
    {
        m_destX = caster->Where().X();
        m_destY = caster->Where().Y();
        m_destZ = caster->Where().Z();
        m_unitTarget = caster;
        m_unitTargetGUID = caster->GetObjectGuid();
        return;
    }

    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNK2))
    {
        data >> ReadPackedGuid(m_unitTargetGUID);
    }

    if (m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_OBJECT_UNK | TARGET_FLAG_GAMEOBJECT_ITEM))
    {
        data >> ReadPackedGuid(m_GOTargetGUID);
    }

    if ((m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM)) &&IsPlayer(caster))
    {
        data >> ReadPackedGuid(m_itemTargetGUID);
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data >> m_srcX >> m_srcY >> m_srcZ;
        if (!MaNGOS::IsValidMapCoord(m_srcX, m_srcY, m_srcZ))
        {
            throw ByteBufferException(false, data.rpos(), 0, data.size());
        }
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data >> m_destX >> m_destY >> m_destZ;
        if (!MaNGOS::IsValidMapCoord(m_destX, m_destY, m_destZ))
        {
            throw ByteBufferException(false, data.rpos(), 0, data.size());
        }
    }

    if (m_targetMask & TARGET_FLAG_STRING)
    {
        data >> m_strTarget;
    }

    if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
    {
        data >> ReadPackedGuid(m_CorpseTargetGUID);
    }

    Update(caster);
}

void SpellCastTargets::write(ByteBuffer& data) const
{
    data << uint16(m_targetMask);

    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_PVP_CORPSE | TARGET_FLAG_OBJECT | TARGET_FLAG_CORPSE | TARGET_FLAG_UNK2))
    {
        if (m_targetMask & TARGET_FLAG_UNIT)
        {
            if (m_unitTarget)
            {
                data << m_unitTarget->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_OBJECT_UNK))
        {
            if (m_GOTarget)
            {
                data << m_GOTarget->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
        {
            data << PackGuid(m_CorpseTargetGUID);
        }
        else
        {
            data << uint8(0);
        }
    }

    if (m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM))
    {
        if (m_itemTarget)
        {
            data << m_itemTarget->GetPackGUID();
        }
        else
        {
            data << uint8(0);
        }
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data << m_srcX << m_srcY << m_srcZ;
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data << m_destX << m_destY << m_destZ;
    }

    if (m_targetMask & TARGET_FLAG_STRING)
    {
        data << m_strTarget;
    }
}

Spell::Spell(Unit* caster, SpellEntry const* info, bool triggered, ObjectGuid originalCasterGUID, SpellEntry const* triggeredBy)
{
    MANGOS_ASSERT(caster != nullptr && info != nullptr);
    MANGOS_ASSERT(info == sSpellStore.LookupEntry(info->ID));

    m_spellInfo = info;
    m_recipe = cast::Recipes().Find(info->ID);
    MANGOS_ASSERT(m_recipe != nullptr);
    m_triggeredBySpellInfo = triggeredBy;
    m_caster = caster;
    m_selfContainer = nullptr;
    m_referencedFromCurrentSpell = false;
    m_executedCurrently = false;
    m_delayStart = 0;
    m_delayAtDamageCount = 0;

    m_applyMultiplierMask = 0;
    m_setsOffProcs = false;

    m_spellSchoolMask = GetSpellSchoolMask(info);

    if (Recipe().Swings() == RANGED_ATTACK)
    {

        if (!(m_caster->getClassMask() & CLASSMASK_WAND_USERS) &&IsPlayer(m_caster))
        {
            m_spellSchoolMask = GetSchoolMask(m_caster->GetWeaponDamageSchool(RANGED_ATTACK));
        }

    }

    m_healthLeech = 0;

    m_originalCasterGUID = originalCasterGUID ? originalCasterGUID : m_caster->GetObjectGuid();

    UpdateOriginalCasterPointer();

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        m_currentBasePoints[i] = m_spellInfo->CalculateSimpleValue(SpellEffectIndex(i));
    }

    m_spellState = SPELL_STATE_CREATED;

    m_castPositionX = m_castPositionY = m_castPositionZ = 0;
    m_TriggerSpells.clear();
    m_preCastSpells.clear();
    m_IsTriggeredSpell = triggered;

    m_CastItem = nullptr;

    unitTarget = nullptr;
    itemTarget = nullptr;
    gameObjTarget = nullptr;
    focusObject = nullptr;
    m_triggeredByAuraSpell  = nullptr;

    m_autoRepeat = Recipe().Starts() == cast::Start::AutoRepeat;

    m_powerCost = 0;
    m_casttime = 0;
    m_timer = 0;
    m_duration = 0;

    m_needAliveTargetMask = 0;

    m_canReflect = false;

    if (m_spellInfo->DefenseType == SPELL_DAMAGE_CLASS_MAGIC && !Recipe().Says().ignoresLineOfSight)
    {
        for (const auto& operation : Recipe().Does())
        {
            if (!IsPositiveTarget(operation.targetA, operation.targetB))
            {
                m_canReflect = true;
            }
            else
            {
                m_canReflect = Recipe().Says().cannotBeReflected;
            }

            if (m_canReflect)
            {
                continue;
            }
            else
            {
                break;
            }
        }
    }
}

Spell::~Spell()
{
}

SpellEntry const* Spell::GetSpellBonusLevelPenaltySpell(SpellEntry const* spellProto) const
{
    if (!spellProto || !m_triggeredBySpellInfo)
    {
        return spellProto;
    }

    if (m_currentBasePoints[EFFECT_INDEX_1] == int32(m_triggeredBySpellInfo->ID) &&
        cast::RecipeOf(*spellProto).Bonus())
    {
        return m_triggeredBySpellInfo;
    }

    return spellProto;
}

bool Spell::IsAliveUnitPresentInTargetList()
{

    if (m_needAliveTargetMask == 0)
    {
        return true;
    }

    uint8 needAliveTargetMask = m_needAliveTargetMask;

    for (const auto& enrolled : m_roster.Units())
    {
        if (enrolled.verdict == SPELL_MISS_NONE && (needAliveTargetMask & enrolled.slots))
        {
            Unit* unit = m_caster->GetObjectGuid() == enrolled.guid ? m_caster : ObjectLookup::GetUnit(*m_caster, enrolled.guid);

            if (unit && (unit->IsAlive() != IsDeathOnlySpell(m_spellInfo)))
            {
                needAliveTargetMask &= ~enrolled.slots;
            }
        }
    }

    return needAliveTargetMask == 0;
}

SpellCastResult Spell::prepare(SpellCastTargets const* targets, Aura* triggeredByAura, uint32 chance)
{
    m_targets = *targets;

    m_castPositionX = m_caster->Where().X();
    m_castPositionY = m_caster->Where().Y();
    m_castPositionZ = m_caster->Where().Z();
    m_castOrientation = m_caster->Where().Facing();

    if (triggeredByAura)
    {
        m_triggeredByAuraSpell = triggeredByAura->GetSpellProto();
    }

    SpellEvent* Event = new SpellEvent(this);
    m_caster->m_Events.AddEvent(Event, m_caster->m_Events.CalculateTime(1));

    if (!m_IsTriggeredSpell && m_caster->IsNonMeleeSpellCasted(false, true, true))
    {
        SendCastResult(SPELL_FAILED_SPELL_IN_PROGRESS);
        finish(false);
        return SPELL_FAILED_SPELL_IN_PROGRESS;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, m_caster))
    {
        SendCastResult(SPELL_FAILED_SPELL_UNAVAILABLE);
        finish(false);
        return SPELL_FAILED_SPELL_UNAVAILABLE;
    }

    m_powerCost = CalculatePowerCost(m_spellInfo, m_caster, this, m_CastItem);

    SpellCastResult result = CheckCast(true);
    if (result != SPELL_CAST_OK && !IsAutoRepeat())
    {
        if (triggeredByAura)
        {
            SendChannelUpdate(0);
            triggeredByAura->GetHolder()->SetAuraDuration(0);
        }
        SendCastResult(result);
        finish(false);
        return result;
    }

    if (chance)
    {
        if (!roll_chance_i(chance))
        {
            finish(false);
            return SPELL_FAILED_TRY_AGAIN;
        }
    }

    m_spellState = SPELL_STATE_PREPARING;

    m_setsOffProcs = SetsOffProcs();

    m_casttime = GetSpellCastTime(m_spellInfo, this);
    m_duration = CalculateSpellDuration(m_spellInfo, m_caster);

    ReSetTimer();

    if (!m_IsTriggeredSpell && isSpellBreakStealth(m_spellInfo))
    {

        if (!(m_spellInfo->SpellClassSet == SPELLFAMILY_ROGUE && (m_spellInfo->SpellClassMask & UI64LIT(0x00000080) || m_spellInfo->SpellClassMask & 2147483648)))
        {
            m_caster->RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
        }
        m_caster->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (!m_IsTriggeredSpell)
    {

        m_caster->SetCurrentCastedSpell(this);

        SendSpellStart();

        TriggerGlobalCooldown();
    }
    else
    {
        if (m_timer == 0)
        {
            cast(true);
        }
    }

    return SPELL_CAST_OK;
}

ObjectGuid Spell::GetPrefilledOrUnitTargetGuid(SpellEffectIndex effIndex) const
{
    for (const auto& enrolled : m_roster.Units())
    {
        if (enrolled.slots & (1 << effIndex))
        {
            return enrolled.guid;
        }
    }

    return m_targets.getUnitTargetGuid();
}

void Spell::Delayed()
{
    if (!m_caster || !IsPlayer(m_caster))
    {
        return;
    }

    if (m_spellState == SPELL_STATE_DELAYED)
    {
        return;
    }

    if (!(m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_DAMAGE))
    {
        return;
    }

    int32 resistChance = 100;
    ((Player*)m_caster)->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_NOT_LOSE_CASTING_TIME, resistChance, this);
    resistChance += m_caster->GetTotalAuraModifier(SPELL_AURA_RESIST_PUSHBACK) - 100;
    if (roll_chance_i(resistChance))
    {
        return;
    }

    int32 delaytime = GetNextDelayAtDamageMsTime();

    if (int32(m_timer) + delaytime > m_casttime)
    {
        delaytime = m_casttime - m_timer;
        m_timer = m_casttime;
    }
    else
    {
        m_timer += delaytime;
    }

    DETAIL_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u partially interrupted for (%d) ms at damage", m_spellInfo->ID, delaytime);

    WorldPacket data(SMSG_SPELL_DELAYED, 8 + 4);
    data << static_cast<ObjectGuid>(m_caster->GetObjectGuid());
    data << uint32(delaytime);

    if (IsPlayer(m_caster))
    {
        ((Player*)m_caster)->SendDirectMessage(&data);
    }
}

void Spell::DelayedChannel()
{
    if (!m_caster || !IsPlayer(m_caster) || getState() != SPELL_STATE_CASTING)
    {
        return;
    }

    int32 resistChance = 100;
    ((Player*)m_caster)->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_NOT_LOSE_CASTING_TIME, resistChance, this);
    resistChance += m_caster->GetTotalAuraModifier(SPELL_AURA_RESIST_PUSHBACK) - 100;
    if (roll_chance_i(resistChance))
    {
        return;
    }

    int32 delaytime = GetNextDelayAtDamageMsTime();

    if (int32(m_timer) < delaytime)
    {
        delaytime = m_timer;
        m_timer = 0;
    }
    else
    {
        m_timer -= delaytime;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u partially interrupted for %i ms, new duration: %u ms", m_spellInfo->ID, delaytime, m_timer);

    for (const auto& enrolled : m_roster.Units())
    {
        if (enrolled.verdict == SPELL_MISS_NONE)
        {
            if (Unit* unit = m_caster->GetObjectGuid() == enrolled.guid ? m_caster : ObjectLookup::GetUnit(*m_caster, enrolled.guid))
            {
                unit->DelaySpellAuraHolder(m_spellInfo->ID, delaytime, unit->GetObjectGuid());
            }
        }
    }

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {

        if (DynamicObject* dynObj = m_caster->Conjured().AreaOf(m_spellInfo->ID, SpellEffectIndex(j)))
        {
            dynObj->Delay(delaytime);
        }
    }

    SendChannelUpdate(m_timer);
}

void Spell::UpdateOriginalCasterPointer()
{
    if (m_originalCasterGUID == m_caster->GetObjectGuid())
    {
        m_originalCaster = m_caster;
    }
    else if ((GuidHigh(m_originalCasterGUID) == HIGHGUID_GAMEOBJECT))
    {
        GameObject* go = m_caster->IsInWorld() ? m_caster->GetMap()->GetGameObject(m_originalCasterGUID) : nullptr;
        m_originalCaster = go ? go->GetOwner() : nullptr;
    }
    else
    {
        Unit* unit = ObjectLookup::GetUnit(*m_caster, m_originalCasterGUID);
        m_originalCaster = unit && unit->IsInWorld() ? unit : nullptr;
    }
}

void Spell::UpdatePointers()
{
    UpdateOriginalCasterPointer();

    m_targets.Update(m_caster);
}

bool Spell::IsNeedSendToClient() const
{
    return m_spellInfo->SpellVisualID != 0 ||
        Recipe().Starts() == cast::Start::Channelled ||
        m_spellInfo->Speed > 0.0f ||
        (!m_triggeredByAuraSpell && !m_IsTriggeredSpell);
}

bool Spell::IsTriggeredSpellWithRedundentCastTime() const
{
    return m_triggeredByAuraSpell || (m_IsTriggeredSpell && (m_spellInfo->ManaCost || m_spellInfo->ManaCostPct));
}

SpellEvent::SpellEvent(Spell* spell) : BasicEvent()
{
    m_Spell = spell;
}

SpellEvent::~SpellEvent()
{
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->cancel();
    }

    if (m_Spell->IsDeletable())
    {
        delete m_Spell;
    }
    else
    {
        sLog.outError("~SpellEvent: %s %u tried to delete non-deletable spell %u. Was not deleted, causes memory leak.", (IsPlayer(m_Spell->GetCaster()) ? "Player" : "Creature"), m_Spell->GetCaster()->GetGUIDLow(), m_Spell->m_spellInfo->ID);
    }
}

bool SpellEvent::Execute(uint64 e_time, uint32 p_time)
{

    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->update(p_time);
    }

    switch (m_Spell->getState())
    {
        case SPELL_STATE_FINISHED:
        {

            if (m_Spell->IsDeletable())
            {

                return true;
            }

            break;
        }
        case SPELL_STATE_CASTING:
        {

            break;
        }
        case SPELL_STATE_DELAYED:
        {

            if (m_Spell->GetDelayStart() != 0)
            {

                if (m_Spell->Recipe().Starts() == cast::Start::Channelled)
                {

                    if (m_Spell->GetCaster()->IsNonMeleeSpellCasted(false, true, true))
                    {

                        m_Spell->cancel();
                    }
                    else
                    {

                        m_Spell->handle_immediate();
                    }

                }
                else
                {

                    uint64 t_offset = e_time - m_Spell->GetDelayStart();
                    uint64 n_offset = m_Spell->handle_delayed(t_offset);
                    if (n_offset)
                    {

                        m_Spell->GetCaster()->m_Events.AddEvent(this, m_Spell->GetDelayStart() + n_offset, false);
                        return false;
                    }

                }
            }
            else
            {

                m_Spell->SetDelayStart(e_time);

                m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + m_Spell->GetDelayMoment(), false);
                return false;
            }
            break;
        }
        default:
        {

            break;
        }
    }

    m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + 1, false);
    return false;
}

void SpellEvent::Abort(uint64 )
{

    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->cancel();
    }
}

bool SpellEvent::IsDeletable() const
{
    return m_Spell->IsDeletable();
}

bool Spell::IsLockInRange(GameObject* go)
{
    const SpellRangeEntry* srange = sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex);

    float dx = m_caster->Where().X() - go->Where().X();
    float dy = m_caster->Where().Y() - go->Where().Y();
    float dz = m_caster->Where().Z() - go->Where().Z();

    return (dx * dx + dy * dy + dz * dz < srange->RangeMax);
}

SpellCastResult Spell::CanOpenLock(SpellEffectIndex effIndex, uint32 lockId, SkillType& skillId, int32& reqSkillValue, int32& skillValue)
{
    if (!lockId)
    {
        return SPELL_CAST_OK;
    }

    LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

    if (!lockInfo)
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    bool reqKey = false;

    for (int j = 0; j < 8; ++j)
    {
        switch (lockInfo->Type[j])
        {

            case LOCK_KEY_ITEM:
            {
                if (lockInfo->Index[j] && m_CastItem && m_CastItem->GetEntry() == lockInfo->Index[j])
                {
                    return SPELL_CAST_OK;
                }
                reqKey = true;
                break;
            }

            case LOCK_KEY_SKILL:
            {
                reqKey = true;

                if (uint32(Recipe().At(static_cast<uint8>(effIndex)).miscValue) != lockInfo->Index[j])
                {
                    continue;
                }

                skillId = SkillByLockType(LockType(lockInfo->Index[j]));

                if (skillId != SKILL_NONE)
                {

                    uint32 spellSkillBonus = uint32(m_currentBasePoints[effIndex]);
                    reqSkillValue = lockInfo->Skill[j];

                    skillValue = m_CastItem || !IsPlayer(m_caster) ? 0
                        : ((Player*)m_caster)->GetSkillValue(skillId);

                    skillValue += spellSkillBonus;

                    if (skillValue < reqSkillValue)
                    {
                        return SPELL_FAILED_SKILL_NOT_HIGH_ENOUGH;
                    }
                }

                return SPELL_CAST_OK;
            }
        }
    }

    if (reqKey)
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    return SPELL_CAST_OK;
}

Occupant* Spell::GetAffectiveCasterObject() const
{
    if (!m_originalCasterGUID)
    {
        return m_caster;
    }

    if ((GuidHigh(m_originalCasterGUID) == HIGHGUID_GAMEOBJECT) && m_caster->IsInWorld())
    {
        return m_caster->GetMap()->GetGameObject(m_originalCasterGUID);
    }
    return m_originalCaster;
}

Occupant* Spell::GetCastingObject() const
{
    if ((GuidHigh(m_originalCasterGUID) == HIGHGUID_GAMEOBJECT))
    {
        return m_caster->IsInWorld() ? m_caster->GetMap()->GetGameObject(m_originalCasterGUID) : nullptr;
    }
    else
    {
        return m_caster;
    }
}

void Spell::ResetEffectDamageAndHeal()
{
    m_damage = 0;
    m_healing = 0;
}

void Spell::ClearCastItem()
{
    if (m_CastItem == m_targets.getItemTarget())
    {
        m_targets.setItemTarget(nullptr);
    }

    m_CastItem = nullptr;
}

void Spell::GetSpellRangeAndRadius(SpellEffectIndex effIndex, float& radius, uint32& EffectChainTarget, uint32& unMaxTargets) const
{
    if (Recipe().At(static_cast<uint8>(effIndex)).radiusIndex)
    {
        radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(Recipe().At(static_cast<uint8>(effIndex)).radiusIndex));
    }
    else
    {
        radius = Recipe().Takes().rangeMax;
    }

    if (Unit* realCaster = GetAffectiveCaster())
    {
        if (Player* modOwner = realCaster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_RADIUS, radius, this);
            modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_JUMP_TARGETS, EffectChainTarget, this);
        }
    }

    switch (m_spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (m_spellInfo->ID)
            {
                case 802:
                case 804:
                case 23138:
                case 24781:
                case 28560:
                    unMaxTargets = 1;
                    break;
                case 10258:
                case 28542:
                    unMaxTargets = 2;
                    break;
                case 28796:
                    unMaxTargets = 10;
                    break;
                case 25991:
                    unMaxTargets = 15;
                    break;
            }
            break;
        }
        default:
            break;
    }

    switch (m_spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (m_spellInfo->ID)
            {
                case 24811:
                {
                    if (effIndex == EFFECT_INDEX_0)
                    {
                        radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(Recipe().At(EFFECT_INDEX_1).radiusIndex));
                    }
                    break;
                }
                case 28241:
                {
                    if (SpellAuraHolder* auraHolder = m_caster->GetSpellAuraHolder(28158))
                    {
                        radius = 0.5f * (60000 - auraHolder->GetAuraDuration()) * 0.001f;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

SpellCastResult Spell::CanTameUnit(bool isGM)
{

    Unit* caster = GetAffectiveCaster();
    if (!caster || !IsPlayer(caster) ||
        !m_targets.getUnitTarget() ||IsPlayer(m_targets.getUnitTarget()))
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    Player* plrCaster = static_cast<Player*>(caster);

    if (plrCaster->getClass() != CLASS_HUNTER)
    {
        plrCaster->SendPetTameFailure(PETTAME_UNITSCANTTAME);
        return SPELL_FAILED_DONT_REPORT;
    }

    if (isGM && !ChatHandler(plrCaster).FindCommand("npc tame"))
    {
        plrCaster->SendPetTameFailure(PETTAME_UNKNOWNERROR);
        return SPELL_FAILED_DONT_REPORT;
    }

    Creature* target = (Creature*)m_targets.getUnitTarget();

    if (target->IsPet() || target->IsCharmed())
    {
        plrCaster->SendPetTameFailure(PETTAME_CREATUREALREADYOWNED);
        return SPELL_FAILED_DONT_REPORT;
    }

    if (target->getLevel() > plrCaster->getLevel() && !isGM)
    {
        plrCaster->SendPetTameFailure(PETTAME_TOOHIGHLEVEL);
        return SPELL_FAILED_DONT_REPORT;
    }

    if (!target->Record().IsTameable())
    {
        plrCaster->SendPetTameFailure(PETTAME_NOTTAMEABLE);
        return SPELL_FAILED_DONT_REPORT;
    }

    Pet* pet = plrCaster->GetPet();
    if (pet || plrCaster->GetCharmGuid())
    {
        plrCaster->SendPetTameFailure(PETTAME_ANOTHERSUMMONACTIVE);
        return SPELL_FAILED_DONT_REPORT;
    }
    else
    {
        PetDatabaseStatus status = Pet::GetStatusFromDB(plrCaster);
        if (status != PET_DB_NO_PET)
        {
            plrCaster->SendPetTameFailure(PETTAME_ANOTHERSUMMONACTIVE);
            return SPELL_FAILED_DONT_REPORT;
        }
    }
    return SPELL_CAST_OK;
}
