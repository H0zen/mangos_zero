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

#include "Reaction.h"
#include "Utilities/Errors.h"
#include <string>
#include <list>
#include "Utilities/MathDefines.h"
#include <cstdlib>
#include "Unit.h"
#include "MovementDefines.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "PlayerRegistry.h"
#include "ObjectLookup.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MovementGenerator.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Movement/Spline/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include "Transports.h"
#include "TransportMap.h"
#include "Fleet.h"

#include <math.h>
#include "Cast/Recipe/RecipeBook.h"

float baseMoveSpeed[MAX_MOVE_TYPE] =
{
    2.5f,
    7.0f,
    4.5f,
    4.722222f,
    2.5f,
    3.141594f,
};

void MovementInfo::Read(ByteBuffer& data)
{
    data >> moveFlags >> time;
    data >> pos.x >> pos.y >> pos.z >> pos.o;

    if (HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        data >> t_guid;
        data >> t_pos.x;
        data >> t_pos.y;
        data >> t_pos.z;
        data >> t_pos.o;
        data >> t_time;
    }
    if (HasMovementFlag(MOVEFLAG_SWIMMING))
    {
        data >> s_pitch;
    }

    if (!HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        data >> fallTime;
    }

    if (HasMovementFlag(MOVEFLAG_FALLING))
    {
        data >> jump.velocity;
        data >> jump.sinAngle;
        data >> jump.cosAngle;
        data >> jump.xyspeed;
    }

    if (HasMovementFlag(MOVEFLAG_SPLINE_ELEVATION))
    {
        data >> u_unk1;
    }

}

void MovementInfo::Write(ByteBuffer& data) const
{
    data << moveFlags << time;
    data << pos.x << pos.y << pos.z << pos.o;

    if (HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        data << t_guid;
        data << t_pos.x;
        data << t_pos.y;
        data << t_pos.z;
        data << t_pos.o;
        data << t_time;
    }
    if (HasMovementFlag(MOVEFLAG_SWIMMING))
    {
        data << s_pitch;
    }

    if (!HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        data << fallTime;
    }

    if (HasMovementFlag(MOVEFLAG_FALLING))
    {
        data << jump.velocity;
        data << jump.sinAngle;
        data << jump.cosAngle;
        data << jump.xyspeed;
    }

    if (HasMovementFlag(MOVEFLAG_SPLINE_ELEVATION))
    {
        data << u_unk1;
    }

}

bool GlobalCooldownMgr::HasGlobalCooldown(SpellEntry const* spellInfo) const
{
    GlobalCooldownList::const_iterator itr = m_GlobalCooldowns.find(spellInfo->StartRecoveryCategory);
    return itr != m_GlobalCooldowns.end() && itr->second.duration && getMSTimeDiff(itr->second.cast_time, GameTime::GetGameTimeMS()) < itr->second.duration;
}

void GlobalCooldownMgr::AddGlobalCooldown(SpellEntry const* spellInfo, uint32 gcd)
{
    m_GlobalCooldowns[spellInfo->StartRecoveryCategory] = GlobalCooldown(gcd, GameTime::GetGameTimeMS());
}

void GlobalCooldownMgr::CancelGlobalCooldown(SpellEntry const* spellInfo)
{
    m_GlobalCooldowns[spellInfo->StartRecoveryCategory].duration = 0;
}

Unit::Unit()
    : movespline(new Movement::MoveSpline()),
    m_conjured(*this),
    m_disguise(*this),
    m_retinue(*this),
    i_motionMaster(this),
    m_ThreatManager(this),
    m_HostileRefManager(this),
    m_schoolLockoutMask(SPELL_SCHOOL_MASK_NONE),
    m_schoolLockoutExpire(0),
    m_health(0),
    m_maxHealth(0)
{
    m_objectTypeId = TYPEID_UNIT;
    m_updateFlag = (UPDATEFLAG_ALL | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION);

    m_attackTimer[BASE_ATTACK]   = 0;
    m_attackTimer[OFF_ATTACK]    = 0;
    m_attackTimer[RANGED_ATTACK] = 0;
    m_modAttackSpeedPct[BASE_ATTACK] = 1.0f;
    m_modAttackSpeedPct[OFF_ATTACK] = 1.0f;
    m_modAttackSpeedPct[RANGED_ATTACK] = 1.0f;

    m_extraAttacks = 0;

    m_state = 0;
    m_deathState = ALIVE;

    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        m_currentSpells[i] = nullptr;
    }

    m_castCounter = 0;

    m_AuraFlags = 0;

    m_Visibility = VISIBILITY_ON;
    m_AINotifyScheduled = false;

    m_detectInvisibilityMask = 0;
    m_invisibilityMask = 0;
    m_transform = 0;
    m_immune.Clear();

    m_tallies.Value(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, 0.5f);

    for (int i = 0; i < MAX_ATTACK; ++i)
    {
        m_weaponDamage[i][MINDAMAGE] = BASE_MINDAMAGE;
        m_weaponDamage[i][MAXDAMAGE] = BASE_MAXDAMAGE;
    }
    m_attacking = nullptr;
    m_modMeleeHitChance = 0.0f;
    m_modRangedHitChance = 0.0f;
    m_modSpellHitChance = 0.0f;
    m_baseSpellCritChance = 0;

    m_CombatTimer = 0;

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        m_threatModifier[i] = 1.0f;
    }
    m_isSorted = true;

    for (int i = 0; i < MAX_REACTIVE; ++i)
    {
        m_reactiveTimer[i] = 0;
    }

    m_dummyCombatState = false;
}

Unit::~Unit()
{

    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        if (m_currentSpells[i])
        {
            m_currentSpells[i]->SetReferencedFromCurrent(false);
            m_currentSpells[i] = nullptr;
        }
    }

    delete movespline;

    MANGOS_ASSERT(m_conjured.Empty());
    MANGOS_ASSERT(m_auras.NothingDeferred());
}

void Unit::Update(uint32 update_diff, uint32 p_time)
{
    if (!IsInWorld())
    {
        return;
    }

    m_Events.Update(update_diff);
    _UpdateSpells(update_diff);

    CleanupDeletedAuras();

    m_recovery.RunHold(update_diff);

    if (IsInCombat() && GetCharmerOrOwnerPlayerOrPlayerItself() && !m_dummyCombatState)
    {

        if (m_HostileRefManager.isEmpty())
        {

            if (m_CombatTimer <= update_diff)
            {
                CombatStop();
            }
            else
            {
                m_CombatTimer -= update_diff;
            }
        }
    }

    if (uint32 base_att = getAttackTimer(BASE_ATTACK))
    {
        setAttackTimer(BASE_ATTACK, (update_diff >= base_att ? 0 : base_att - update_diff));
    }

    if (uint32 base_att = getAttackTimer(OFF_ATTACK))
    {
        setAttackTimer(OFF_ATTACK, (update_diff >= base_att ? 0 : base_att - update_diff));
    }

    UpdateReactives(update_diff);

    if (IsAlive())
    {
        ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, GetHealth() < GetMaxHealth() * 0.20f);
    }
    UpdateSplineMovement(p_time);
    i_motionMaster.UpdateMotion(p_time);
}

bool Unit::UpdateMeleeAttackingState()
{
    Unit* victim = getVictim();
    if (!victim || IsNonMeleeSpellCasted(false))
    {
        return false;
    }

    if (!isAttackReady(BASE_ATTACK) && !(isAttackReady(OFF_ATTACK) && haveOffhandWeapon()))
    {
        return false;
    }

    uint8 swingError = 0;
    if (!InMeleeReach(*this, *victim))
    {
        setAttackTimer(BASE_ATTACK, 100);
        setAttackTimer(OFF_ATTACK, 100);
        swingError = 1;
    }

    else if (!Where().HasInArc(victim->Where(), 2 * M_PI_F / 3))
    {
        setAttackTimer(BASE_ATTACK, 100);
        setAttackTimer(OFF_ATTACK, 100);
        swingError = 2;
    }
    else
    {
        if (isAttackReady(BASE_ATTACK))
        {

            if (haveOffhandWeapon())
            {
                if (getAttackTimer(OFF_ATTACK) < ATTACK_DISPLAY_DELAY)
                {
                    setAttackTimer(OFF_ATTACK, ATTACK_DISPLAY_DELAY);
                }
            }
            AttackerStateUpdate(victim, BASE_ATTACK);
            resetAttackTimer(BASE_ATTACK);
        }
        if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
        {

            uint32 base_att = getAttackTimer(BASE_ATTACK);
            if (base_att < ATTACK_DISPLAY_DELAY)
            {
                setAttackTimer(BASE_ATTACK, ATTACK_DISPLAY_DELAY);
            }

            AttackerStateUpdate(victim, OFF_ATTACK);
            resetAttackTimer(OFF_ATTACK);
        }
    }

    Player* player = (IsPlayer(this) ? (Player*)this : nullptr);
    if (player && swingError != player->Arms().SwingError())
    {
        if (swingError == 1)
        {
            player->SendAttackSwingNotInRange();
        }
        else if (swingError == 2)
        {
            player->SendAttackSwingBadFacingAttack();
        }
        player->Arms().SwingError(swingError);
    }

    return swingError == 0;
}

bool Unit::haveOffhandWeapon() const
{
    if (!CanUseEquippedWeapon(OFF_ATTACK))
    {
        return false;
    }

    if (IsPlayer(this))
    {
        return ((Player*)this)->GetWeaponForAttack(OFF_ATTACK, true, true);
    }
    else
    {
        uint8 itemClass = GetByteValue(UNIT_VIRTUAL_ITEM_INFO + (1 * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_CLASS);
        if (itemClass == ITEM_CLASS_WEAPON)
        {
            return true;
        }

        return false;
    }
}

void Unit::WriteMovementInfo(ByteBuffer& out) const
{

    Map* on = GetMap();
    TransportMap* hull = on ? on->AsTransport() : nullptr;
    Transport* vessel = hull ? hull->Vessel() : nullptr;

    if (!vessel)
    {

        MovementInfo ashore = m_movementInfo;
        ashore.ChangePosition(Where().X(), Where().Y(), Where().Z(), Where().Facing());
        ashore.Write(out);
        return;
    }

    MovementInfo onDeck = m_movementInfo;
    onDeck.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
    onDeck.SetTransportData(vessel->GetObjectGuid(), Where().X(), Where().Y(), Where().Z(),
                            Where().Facing(), 0);
    onDeck.ChangePosition(0.0f, 0.0f, 0.0f, Where().Facing());
    onDeck.Write(out);
}

void Unit::SendHeartBeat()
{
    m_movementInfo.UpdateTime(GameTime::GetGameTimeMS());
    WorldPacket data(MSG_MOVE_HEARTBEAT, 31);
    data << GetPackGUID();
    WriteMovementInfo(data);
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Unit::resetAttackTimer(WeaponAttackType type)
{
    m_attackTimer[type] = uint32(GetAttackTime(type) * m_modAttackSpeedPct[type]);
}

float CombatReachBetween(Unit const& attacker, Unit const& victim, bool forMeleeRange,
                         float flat_mod)
{

    float reach = attacker.GetCombatReachValue() +
                  victim.GetCombatReachValue() +
                  BASE_MELEERANGE_OFFSET + flat_mod;

    if (forMeleeRange && reach < ATTACK_DISTANCE)
    {
        reach = ATTACK_DISTANCE;
    }

    return reach;
}

float CombatDistanceBetween(Unit const& attacker, Unit const& target, bool forMeleeRange)
{
    if (!attacker.Where().ShareFrame(target.Where()))
    {
        return Geometry::Placement::Unreachable();
    }

    const float reach = CombatReachBetween(attacker, target, forMeleeRange, 0.0f);
    const float centres = (attacker.Where().Pos() - target.Where().Pos()).magnitude();
    return Geometry::Placement::Gap(centres, reach);
}

bool InMeleeReach(Unit const& attacker, Unit const& victim, float flat_mod)
{
    const float reach = CombatReachBetween(attacker, victim, true, flat_mod);

    return attacker.Where().WithinDist(victim.Where().Pos(), reach) &&
           attacker.Where().ShareFrame(victim.Where());
}

void Unit::RemoveAurasOfType(AuraType auraType)
{
    for (auto* aura : GetAurasByType(auraType))
    {
        RemoveAuras(aura->GetId());
    }
}

void Unit::RemoveAurasOfType(AuraType auraType, SpellAuraHolder* except)
{
    for (auto* aura : GetAurasByType(auraType))
    {
        if (aura->GetHolder() == except)
        {
            continue;
        }

        RemoveAuras(aura->GetId(), except);
    }
}

void Unit::RemoveAurasOfType(AuraType auraType, ObjectGuid casterGuid)
{
    for (auto* aura : GetAurasByType(auraType))
    {
        if (aura->GetCasterGuid() == casterGuid)
        {
            RemoveStacks(aura->GetId(), 1, casterGuid);
        }
    }
}

void Unit::DealDamageMods(Unit* pVictim, uint32& damage, uint32* absorb)
{
    if (!pVictim->IsAlive() || pVictim->IsTaxiFlying() || (IsCreature(pVictim) && ((Creature*)pVictim)->IsInEvadeMode()))
    {
        if (absorb)
        {
            *absorb += damage;
        }
        damage = 0;
        return;
    }

    uint32 originalDamage = damage;

    if (IsCreature(this) && ((Creature*)this)->AI())
    {
        ((Creature*)this)->AI()->DamageDeal(pVictim, damage);
    }

    if (IsCreature(pVictim) && ((Creature*)pVictim)->AI())
    {
        ((Creature*)pVictim)->AI()->DamageTaken(this, damage);
    }

    if (absorb && originalDamage > damage)
    {
        *absorb += (originalDamage - damage);
    }
}

uint32 Unit::DealDamage(Unit* pVictim, uint32 damage, CleanDamage const* cleanDamage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto, bool durabilityLoss)
{

    if (damagetype != DOT)
    {
        if (damagetype != SELF_DAMAGE_ROGUE_FALL)
        {
            RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
        }
        RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);

        if (IsPlayer(pVictim) && !pVictim->IsStandState() && !pVictim->hasUnitState(UNIT_STAT_STUNNED))
        {
            pVictim->SetStandState(UNIT_STAND_STATE_STAND);
        }
    }

    if (!damage)
    {

        if (cleanDamage && cleanDamage->damage && (damageSchoolMask & SPELL_SCHOOL_MASK_NORMAL) &&IsPlayer(pVictim) && (pVictim->GetPowerType() == POWER_RAGE))
        {
            ((Player*)pVictim)->RewardRage(cleanDamage->damage, false);
        }

        return 0;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageStart");

    uint32 health = pVictim->GetHealth();
    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "deal dmg:%d to health:%d ", damage, health);

    if (cleanDamage && damagetype == DIRECT_DAMAGE && this != pVictim && IsPlayer(this) && GetPowerType() == POWER_RAGE && cleanDamage->attackType != RANGED_ATTACK)
    {
        ((Player*)this)->RewardRage(damage, true);
    }

    if (IsCreature(pVictim) && pVictim->GetCreatureType() == CREATURE_TYPE_CRITTER)
    {

        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamage critter, critter dies");

        ((Creature*)pVictim)->TappedBy(this);

        JustKilledCreature((Creature*)pVictim, nullptr);
        pVictim->SetHealth(0);

        return damage;
    }

    bool duel_hasEnded = false;
    if (IsPlayer(pVictim) && ((Player*)pVictim)->Duelling().Stands() && damage >= (health - 1))
    {

        if (((Player*)pVictim)->Duelling().Against() == this || ((Player*)pVictim)->Duelling().Against()->GetObjectGuid() == GetOwnerGuid())
        {
            damage = health - 1;
        }

        duel_hasEnded = true;
    }

    if (pVictim != this && damagetype != DOT)
    {
        SetInCombatWith(pVictim);
        pVictim->SetInCombatWith(this);

        if (Player* attackedPlayer = pVictim->GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            SetContestedPvP(attackedPlayer);
        }
    }

    if (Creature* victim = static_cast<Creature*>(pVictim))
    {
        if (!victim->IsPet() && !victim->Claim().IsClaimed())
        {
            victim->TappedBy(this);
        }

        if (IsControlledByPlayer())
        {
            victim->LowerPlayerDamageReq(health < damage ? health : damage);
        }
    }

    if (health <= damage)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamage %s Killed %s", GetGuidStr().c_str(), pVictim->GetGuidStr().c_str());

        Player* player_tap = GetCharmerOrOwnerPlayerOrPlayerItself();
        Group* group_tap = nullptr;

        if (IsCreature(pVictim))
        {
            group_tap = ((Creature*)pVictim)->Claim().HoldingGroup();

            if (Player* recipient = ((Creature*)pVictim)->Claim().Taker())
            {
                player_tap = recipient;
            }
        }

        else
        {
            if (player_tap)
            {
                group_tap = player_tap->GetGroup();
            }
        }

        bool damageFromSpiritOfRedemtionTalent = spellProto && spellProto->ID == 27795;

        Aura* spiritOfRedemtionTalentReady = nullptr;
        if (!damageFromSpiritOfRedemtionTalent &&
            IsPlayer(pVictim) && pVictim->getClass() == CLASS_PRIEST)
        {
            const auto vDummyAuras = pVictim->GetAurasByType(SPELL_AURA_DUMMY);
            for (auto* aura : vDummyAuras)
            {
                if (aura->GetSpellProto()->SpellIconID == 1654)
                {
                    spiritOfRedemtionTalentReady = aura;
                    break;
                }
            }
        }

        bool isRewardAllowed = true;
        if (Creature* creature = static_cast<Creature*>(pVictim))
        {
            isRewardAllowed = creature->Taking().EnoughPlayerDamage();
            if (!isRewardAllowed)
            {
                creature->Claim().StakedBy(nullptr);
            }
        }

        if (player_tap && player_tap != pVictim)
        {
            player_tap->ProcDamageAndSpell(pVictim, PROC_FLAG_KILL, PROC_FLAG_KILLED, PROC_EX_NONE, 0);

            if (isRewardAllowed)
            {
                WorldPacket data(SMSG_PARTYKILLLOG, (8 + 8));
                data << player_tap->GetObjectGuid();
                data << pVictim->GetObjectGuid();

                if (group_tap)
                {
                    group_tap->BroadcastPacket(&data, false, group_tap->GetMemberGroup(player_tap->GetObjectGuid()), player_tap->GetObjectGuid());
                }

                player_tap->SendDirectMessage(&data);
            }
        }
        else if (IsCreature(this) && this != pVictim)
        {
            ProcDamageAndSpell(pVictim, PROC_FLAG_KILL, PROC_FLAG_KILLED, PROC_EX_NONE, 0);
        }

        if (isRewardAllowed && player_tap != pVictim)
        {
            if (group_tap)
            {
                group_tap->RewardGroupAtKill(pVictim, player_tap);
            }
            else if (player_tap)
            {
                player_tap->RewardSinglePlayerAtKill(pVictim);
            }
        }

        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageAttackStop");
        pVictim->CombatStop();
        pVictim->GetHostileRefManager().deleteReferences();

        if (spiritOfRedemtionTalentReady)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamage: Spirit of Redemtion ready");

            uint32 ressSpellId = pVictim->GetUInt32Value(PLAYER_SELF_RES_SPELL);
            if (!ressSpellId)
            {
                ressSpellId = ((Player*)pVictim)->GetResurrectionSpellId();
            }

            pVictim->RemoveAllAurasOnDeath();

            pVictim->SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);

            pVictim->CastSpell(pVictim, 27827, true, nullptr, spiritOfRedemtionTalentReady);
        }
        else
        {
            pVictim->SetHealth(0);
        }

        if (Creature* killer = static_cast<Creature*>(this))
        {
            if (CreatureAI* ai = killer->AI())
            {
                ai->KilledUnit(pVictim);
            }
        }

        PetOwnerKilledUnit(pVictim);

        if (IsPlayer(pVictim))
        {
            Player* playerVictim = (Player*)pVictim;

            if (!damageFromSpiritOfRedemtionTalent)
            {
                playerVictim->SetPvPDeath(player_tap != nullptr);
            }

            if (durabilityLoss && !player_tap && !playerVictim->Battle().InOne())
            {
                DEBUG_LOG("DealDamage: Killed %s, looing 10 percents durability", pVictim->GetGuidStr().c_str());
                playerVictim->DurabilityLossAll(0.10f, false);

                WorldPacket data(SMSG_DURABILITY_DAMAGE_DEATH, 0);
                playerVictim->GetSession()->SendPacket(&data);
            }

            if (!spiritOfRedemtionTalentReady)
            {
                DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "SET JUST_DIED");
                pVictim->SetDeathState(JUST_DIED);
            }

            if (duel_hasEnded)
            {
                playerVictim->Duelling().Against()->CombatStopWithPets(true);
                playerVictim->CombatStopWithPets(true);

                playerVictim->Duelling().Complete(DUEL_INTERRUPTED);
            }

            if (player_tap)
            {
                if (BattleGround* bg = playerVictim->Battle().Ground())
                {
                    bg->HandleKillPlayer(playerVictim, player_tap);
                }
                else if (pVictim != this)
                {

                    if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(playerVictim->GetCachedZoneId()))
                    {
                        outdoorPvP->HandlePlayerKill(player_tap, playerVictim);
                    }
                }

            }
        }
        else
        {
            JustKilledCreature((Creature*)pVictim, player_tap);
        }
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageAlive");

        pVictim->ModifyHealth(- (int32)damage);

        if (damagetype != DOT)
        {
            if (!getVictim())
            {

                Attack(pVictim, (damagetype == DIRECT_DAMAGE));
            }

            pVictim->AttackedBy(this);
        }

        if (damagetype == DIRECT_DAMAGE || damagetype == SPELL_DIRECT_DAMAGE)
        {
            if (!spellProto || !(spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_DIRECT_DAMAGE))
            {
                pVictim->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_DIRECT_DAMAGE);
            }
        }
        if (!IsPlayer(pVictim))
        {
            float threat = damage * (spellProto != nullptr ? cast::RecipeOf(*spellProto).ThreatMultiplier() : 1.0f);
            pVictim->AddThreat(this, threat, (cleanDamage && cleanDamage->hitOutCome == MELEE_HIT_CRIT), damageSchoolMask, spellProto);
        }
        else
        {

            if (this != pVictim && pVictim->GetPowerType() == POWER_RAGE)
            {
                uint32 rage_damage = damage + (cleanDamage ? cleanDamage->damage : 0);
                ((Player*)pVictim)->RewardRage(rage_damage, false);
            }

            if (roll_chance_f(sWorld.getConfig(CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE)))
            {
                EquipmentSlots slot = EquipmentSlots(urand(0, EQUIPMENT_SLOT_END - 1));
                ((Player*)pVictim)->DurabilityPointLossForEquipSlot(slot);
            }
        }

        if (IsPlayer(this))
        {

            if (roll_chance_f(sWorld.getConfig(CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE)))
            {
                EquipmentSlots slot = EquipmentSlots(urand(0, EQUIPMENT_SLOT_END - 1));
                ((Player*)this)->DurabilityPointLossForEquipSlot(slot);
            }
        }

        SpellAuraHolderMap& vAuras = pVictim->GetSpellAuraHolderMap();
        for (SpellAuraHolderMap::const_iterator i = vAuras.begin(), next; i != vAuras.end(); i = next)
        {
            const SpellEntry* se = i->second->GetSpellProto();
            next = i; ++next;
            if (spellProto && spellProto->ID == se->ID)
            {
                continue;
            }
            if (!se->ProcFlags && (se->AuraInterruptFlags & AURA_INTERRUPT_FLAG_DAMAGE))
            {
                pVictim->RemoveAuras(i->second->GetId());
                next = vAuras.begin();
            }
        }

        if (damagetype != NODAMAGE && damage &&IsPlayer(pVictim))
        {
            if (damagetype != DOT)
            {
                for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
                {

                    if (i == CURRENT_CHANNELED_SPELL)
                    {
                        continue;
                    }

                    if (Spell* spell = pVictim->GetCurrentSpell(CurrentSpellTypes(i)))
                    {
                        if (spell->getState() == SPELL_STATE_PREPARING)
                        {
                            if (spell->m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_ABORT_ON_DMG)
                            {
                                pVictim->InterruptSpell(CurrentSpellTypes(i));
                            }
                            else
                            {
                                spell->Delayed();
                            }
                        }
                    }
                }
            }

            if (Spell* spell = pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL])
            {
                if (spell->getState() == SPELL_STATE_CASTING)
                {
                    uint32 channelInterruptFlags = spell->m_spellInfo->ChannelInterruptFlags;
                    if (channelInterruptFlags & CHANNEL_FLAG_DELAY)
                    {
                        if (pVictim != this)
                        {
                            spell->DelayedChannel();
                        }
                    }
                    else if ((channelInterruptFlags & (CHANNEL_FLAG_DAMAGE | CHANNEL_FLAG_DAMAGE2)))
                    {
                        DETAIL_LOG("Spell %u canceled at damage!", spell->m_spellInfo->ID);
                        pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                    }
                }
                else if (spell->getState() == SPELL_STATE_DELAYED)

                {
                    DETAIL_LOG("Spell %u canceled at damage!", spell->m_spellInfo->ID);
                    pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                }
            }
        }

        if (duel_hasEnded)
        {
            MANGOS_ASSERT(IsPlayer(pVictim));
            Player* he = (Player*)pVictim;

            MANGOS_ASSERT(he->Duelling().Stands());

            he->SetHealth(1);

            he->Duelling().Against()->CombatStopWithPets(true);
            he->CombatStopWithPets(true);

            he->CastSpell(he, 7267, true);
            he->Duelling().Complete(DUEL_WON);
        }
    }

    DeliverOwedSplits();

    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "DealDamageEnd returned %d damage", damage);

    return damage;
}

struct PetOwnerKilledUnitHelper
{
    explicit PetOwnerKilledUnitHelper(Unit* pVictim) : m_victim(pVictim) {}
    void operator()(Unit* pTarget) const
    {
        if (IsCreature(pTarget))
        {
            if (((Creature*)pTarget)->AI())
            {
                ((Creature*)pTarget)->AI()->OwnerKilledUnit(m_victim);
            }
        }
    }

    Unit* m_victim;
};

void Unit::JustKilledCreature(Creature* victim, Player* responsiblePlayer)
{
    victim->m_deathState = DEAD;

    if (victim->GetCreatureType() == CREATURE_TYPE_CRITTER && IsPlayer(this))
    {
        if (CreatureInfo const* normalInfo = ObjectMgr::GetCreatureTemplate(victim->GetEntry()))
        {
            ((Player*)this)->Journal().CreatureKilled(normalInfo, victim->GetObjectGuid());
        }
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(victim->GetUInt32Value(UNIT_CREATED_BY_SPELL));
    if (spellInfo && cast::RecipeOf(*spellInfo).Says().farsight && cast::RecipeOf(*spellInfo).Says().channels)
    {
        Unit* creator = GetMap()->GetUnit(victim->GetCreatorGuid());
        if (creator && creator->GetCharmGuid() == victim->GetObjectGuid())
        {
            Spell* channeledSpell = creator->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
            if (channeledSpell && channeledSpell->m_spellInfo->ID == spellInfo->ID)
            {
                creator->InterruptNonMeleeSpells(false);
            }
        }
    }

    if (victim->AI())
    {
        victim->AI()->JustDied(this);
    }

    Unit* pOwner = victim->GetCharmerOrOwner();
    if (victim->IsTemporarySummon())
    {
        TemporarySummon* pSummon = (TemporarySummon*)victim;
        if ((GuidHigh(pSummon->GetSummonerGuid()) == HIGHGUID_UNIT))
        {
            if (Creature* pSummoner = victim->GetMap()->GetCreature(pSummon->GetSummonerGuid()))
            {
                if (pSummoner->AI())
                {
                    pSummoner->AI()->SummonedCreatureJustDied(victim);
                }
            }
        }
    }
    else if (pOwner &&IsCreature(pOwner))
    {
        if (((Creature*)pOwner)->AI())
        {
            ((Creature*)pOwner)->AI()->SummonedCreatureJustDied(victim);
        }
    }

    if (InstanceData* mapInstance = victim->GetInstanceData())
    {
        mapInstance->OnCreatureDeath(victim);
    }

    if (responsiblePlayer)
    {
        if (BattleGround* bg = responsiblePlayer->Battle().Ground())
        {
            bg->HandleKillUnit(victim, responsiblePlayer);
        }

    }

    if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(responsiblePlayer ? responsiblePlayer->GetCachedZoneId() : GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z())))
    {
        outdoorPvP->HandleCreatureDeath(victim);
    }

    GetMap()->Scripts().Start(DBS_ON_CREATURE_DEATH, victim->GetEntry(), victim, responsiblePlayer ? responsiblePlayer : this);

    victim->Links().Died();

    if (victim->GetInstanceId())
    {
        Map* m = victim->GetMap();
        Player* creditedPlayer = GetCharmerOrOwnerPlayerOrPlayerItself();

        if (m->IsDungeon() && creditedPlayer)
        {
            if (m->IsRaid())
            {
                if (victim->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_INSTANCE_BIND)
                {
                    ((DungeonMap*)m)->PermBindAllPlayers(creditedPlayer);
                }
            }
            else
            {
                DungeonPersistentState* save = ((DungeonMap*)m)->GetPersistanceState();

                time_t resettime = victim->GetRespawnTimeEx() + 2 * HOUR;
                if (save->GetResetTime() < resettime)
                {
                    save->SetResetTime(resettime);
                }
            }
        }
    }

    bool isPet = victim->IsPet();

    DEBUG_FILTER_LOG(LOG_FILTER_DAMAGE, "SET JUST_DIED");
    victim->SetDeathState(JUST_DIED);

    if (isPet)
    {
        return;
    }

    victim->SetKilledTime(time(nullptr));
    victim->DeleteThreatList();

    victim->PrepareBodyLootState();

}

void Unit::PetOwnerKilledUnit(Unit* pVictim)
{

    CallForAllControlledUnits(PetOwnerKilledUnitHelper(pVictim), CONTROLLED_MINIPET | CONTROLLED_GUARDIANS);
}

void Unit::CastStop(uint32 except_spellid)
{
    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
    {
        if (m_currentSpells[i] && m_currentSpells[i]->m_spellInfo->ID != except_spellid)
        {
            InterruptSpell(CurrentSpellTypes(i), false);
        }
    }
}

void Unit::CastSpell(Unit* Victim, uint32 spellId, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell: unknown spell id %i by caster: %s triggered by aura %u (eff %u)", spellId, GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell: unknown spell id %i by caster: %s", spellId, GetGuidStr().c_str());
        }
        return;
    }

    CastSpell(Victim, spellInfo, triggered, castItem, triggeredByAura, originalCaster, triggeredBy);
}

void Unit::CastSpell(Unit* Victim, SpellEntry const* spellInfo, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell: unknown spell by caster: %s triggered by aura %u (eff %u)", GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell: unknown spell by caster: %s", GetGuidStr().c_str());
        }
        return;
    }

    if (castItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->ID);
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
        {
            originalCaster = triggeredByAura->GetCasterGuid();
        }

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    Spell* spell = new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    SpellCastTargets targets;
    targets.setUnitTarget(Victim);

    if (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
    {
        targets.setDestination(Victim->Where().X(), Victim->Where().Y(), Victim->Where().Z());
    }
    if (spellInfo->Targets & TARGET_FLAG_SOURCE_LOCATION)
    {
        if (Occupant* caster = spell->GetCastingObject())
        {
            targets.setSource(caster->Where().X(), caster->Where().Y(), caster->Where().Z());
        }
    }

    spell->m_CastItem = castItem;
    spell->prepare(&targets, triggeredByAura);

    SpellLinkedSet linkedSet = sSpellMgr.GetSpellLinked(spellInfo->ID, SPELL_LINKED_TYPE_REMOVEONCAST);
    if (linkedSet.size() > 0)
    {
        for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
        {
            Victim->RemoveAuras(*itr);
        }
    }
}

void Unit::CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastCustomSpell: unknown spell id %i by caster: %s triggered by aura %u (eff %u)", spellId, GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastCustomSpell: unknown spell id %i by caster: %s", spellId, GetGuidStr().c_str());
        }
        return;
    }

    CastCustomSpell(Victim, spellInfo, bp0, bp1, bp2, triggered, castItem, triggeredByAura, originalCaster, triggeredBy);
}

void Unit::CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastCustomSpell: unknown spell by caster: %s triggered by aura %u (eff %u)", GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastCustomSpell: unknown spell by caster: %s", GetGuidStr().c_str());
        }
        return;
    }

    if (castItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->ID);
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
        {
            originalCaster = triggeredByAura->GetCasterGuid();
        }

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    Spell* spell = new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    if (bp0)
    {
        spell->m_currentBasePoints[EFFECT_INDEX_0] = *bp0;
    }

    if (bp1)
    {
        spell->m_currentBasePoints[EFFECT_INDEX_1] = *bp1;
    }

    if (bp2)
    {
        spell->m_currentBasePoints[EFFECT_INDEX_2] = *bp2;
    }

    SpellCastTargets targets;
    targets.setUnitTarget(Victim);
    spell->m_CastItem = castItem;

    if (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
    {
        targets.setDestination(Victim->Where().X(), Victim->Where().Y(), Victim->Where().Z());
    }

    if (spellInfo->Targets & TARGET_FLAG_SOURCE_LOCATION)
    {
        if (Occupant* caster = spell->GetCastingObject())
        {
            targets.setSource(caster->Where().X(), caster->Where().Y(), caster->Where().Z());
        }
    }

    spell->prepare(&targets, triggeredByAura);
}

void Unit::CastSpell(float x, float y, float z, uint32 spellId, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell(x,y,z): unknown spell id %i by caster: %s triggered by aura %u (eff %u)", spellId, GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell(x,y,z): unknown spell id %i by caster: %s", spellId, GetGuidStr().c_str());
        }
        return;
    }

    CastSpell(x, y, z, spellInfo, triggered, castItem, triggeredByAura, originalCaster, triggeredBy);
}

void Unit::CastSpell(float x, float y, float z, SpellEntry const* spellInfo, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell(x,y,z): unknown spell by caster: %s triggered by aura %u (eff %u)", GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell(x,y,z): unknown spell by caster: %s", GetGuidStr().c_str());
        }
        return;
    }

    if (castItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->ID);
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
        {
            originalCaster = triggeredByAura->GetCasterGuid();
        }

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    Spell* spell = new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    SpellCastTargets targets;

    if (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
    {
        targets.setDestination(x, y, z);
    }
    if (spellInfo->Targets & TARGET_FLAG_SOURCE_LOCATION)
    {
        targets.setSource(x, y, z);
    }

    if (!(targets.m_targetMask & (TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_SOURCE_LOCATION)))
    {
        targets.setDestination(x, y, z);
    }

    spell->m_CastItem = castItem;
    spell->prepare(&targets, triggeredByAura);
}

uint32 Unit::SpellNonMeleeDamageLog(Unit* pVictim, uint32 spellID, uint32 damage)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellID);
    SpellNonMeleeDamage damageInfo(this, pVictim, spellInfo->ID, SpellSchools(spellInfo->School));
    CalculateSpellDamage(&damageInfo, damage, spellInfo);
    damageInfo.target->CalculateAbsorbResistBlock(this, &damageInfo, spellInfo);
    DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    SendSpellNonMeleeDamageLog(&damageInfo);
    DealSpellDamage(&damageInfo, true);
    return damageInfo.damage;
}

void Unit::CalculateSpellDamage(SpellNonMeleeDamage* damageInfo, int32 damage, SpellEntry const* spellInfo, WeaponAttackType attackType)
{
    SpellSchoolMask damageSchoolMask = GetSchoolMask(damageInfo->school);
    Unit* pVictim = damageInfo->target;

    if (damage < 0)
    {
        return;
    }

    if (!pVictim)
    {
        return;
    }

    if ((!this->IsAlive() || !pVictim->IsAlive()) && (!IsCreature(this) || this->GetDeathState() != DEAD))
    {
        return;
    }

    bool crit = IsSpellCrit(pVictim, spellInfo, damageSchoolMask, attackType);

    switch (spellInfo->DefenseType)
    {

        case SPELL_DAMAGE_CLASS_RANGED:
        {

            switch (spellInfo->ID)
            {

                case 24274:    case 24275:    case 24239:
                {
                    damage = SpellDamageBonusDone(pVictim, spellInfo, damage, SPELL_DIRECT_DAMAGE);
                    damage = pVictim->SpellDamageBonusTaken(this, spellInfo, damage, SPELL_DIRECT_DAMAGE);
                    break;
                }
                default:
                {
                    damage = MeleeDamageBonusDone(pVictim, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);
                    damage = pVictim->MeleeDamageBonusTaken(this, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);
                }
                break;
            }

            if (crit)
            {
                damageInfo->HitInfo |= SPELL_HIT_TYPE_CRIT;
                damage = SpellCriticalDamageBonus(spellInfo, damage, pVictim);
            }
        }
        break;
        case SPELL_DAMAGE_CLASS_MELEE:
        {

            switch (spellInfo->ID)
            {

                case 20467:    case 20963:    case 20964:    case 20965:    case 20966:

                case 20424:

                case 25735:       case 25736:      case 25737:    case 25738:    case 25739:     case 25740:
                case 25713:    case 25742:
                {
                    damage = SpellDamageBonusDone(pVictim, spellInfo, damage, SPELL_DIRECT_DAMAGE);
                    damage = pVictim->SpellDamageBonusTaken(this, spellInfo, damage, SPELL_DIRECT_DAMAGE);
                    break;
                }
                default:
                {
                    damage = MeleeDamageBonusDone(pVictim, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);
                    damage = pVictim->MeleeDamageBonusTaken(this, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);
                }
                break;
            }

            if (crit)
            {
                damageInfo->HitInfo |= SPELL_HIT_TYPE_CRIT;
                damage = SpellCriticalDamageBonus(spellInfo, damage, pVictim);
            }
        }
        break;

        case SPELL_DAMAGE_CLASS_NONE:
        case SPELL_DAMAGE_CLASS_MAGIC:
        {

            damage = SpellDamageBonusDone(pVictim, spellInfo, damage, SPELL_DIRECT_DAMAGE);
            damage = pVictim->SpellDamageBonusTaken(this, spellInfo, damage, SPELL_DIRECT_DAMAGE);

            if (crit)
            {
                damageInfo->HitInfo |= SPELL_HIT_TYPE_CRIT;
                damage = SpellCriticalDamageBonus(spellInfo, damage, pVictim);
            }
        }
        break;
    }

    if (damage > 0)
    {

        if (damageSchoolMask & SPELL_SCHOOL_MASK_NORMAL)
        {
            damage = CalcArmorReducedDamage(pVictim, damage);
        }
    }
    else
    {
        damage = 0;
    }
    damageInfo->damage = damage;
}

void Unit::DealSpellDamage(SpellNonMeleeDamage* damageInfo, bool durabilityLoss)
{
    if (!damageInfo)
    {
        return;
    }

    Unit* pVictim = damageInfo->target;

    if (!pVictim)
    {
        return;
    }

    if (!pVictim->IsAlive() || pVictim->IsTaxiFlying() || (IsCreature(pVictim) && ((Creature*)pVictim)->IsInEvadeMode()))
    {
        return;
    }

    SpellEntry const* spellProto = sSpellStore.LookupEntry(damageInfo->SpellID);
    if (spellProto == nullptr)
    {
        sLog.outError("Unit::DealSpellDamage have wrong damageInfo->SpellID: %u", damageInfo->SpellID);
        return;
    }

    if (damageInfo->damage && spellProto->ID == 35395)
    {
        SpellAuraHolderMap const& vAuras = pVictim->GetSpellAuraHolderMap();
        for (SpellAuraHolderMap::const_iterator itr = vAuras.begin(); itr != vAuras.end(); ++itr)
        {
            SpellEntry const* spellInfo = (*itr).second->GetSpellProto();
            if (spellInfo->AttributesExC & 0x40000 && spellInfo->SpellClassSet == SPELLFAMILY_PALADIN && ((*itr).second->GetCasterGuid() == GetObjectGuid()))
            {
                (*itr).second->RefreshHolder();
            }
        }
    }

    CleanDamage cleanDamage(0, BASE_ATTACK, damageInfo->HitInfo & SPELL_HIT_TYPE_CRIT ? MELEE_HIT_CRIT : MELEE_HIT_NORMAL);
    DealDamage(pVictim, damageInfo->damage, &cleanDamage, SPELL_DIRECT_DAMAGE, GetSchoolMask(damageInfo->school), spellProto, durabilityLoss);
}

void Unit::HandleEmoteCommand(uint32 emote_id)
{
    WorldPacket data(SMSG_EMOTE, 4 + 8);
    data << uint32(emote_id);
    data << GetObjectGuid();
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Unit::HandleEmoteState(uint32 emote_id)
{
    SetUInt32Value(UNIT_NPC_EMOTESTATE, emote_id);
}

void Unit::HandleEmote(uint32 emote_id)
{
    if (!emote_id)
    {
        HandleEmoteState(0);
    }
    else if (EmotesEntry const* emoteEntry = sEmotesStore.LookupEntry(emote_id))
    {
        if (emoteEntry->EmoteType)
        {
            HandleEmoteState(emote_id);
        }
        else
        {
            HandleEmoteCommand(emote_id);
        }
    }
}

void Unit::HandleProcExtraAttackFor(Unit* victim)
{
    while (m_extraAttacks)
    {
        --m_extraAttacks;
        AttackerStateUpdate(victim, BASE_ATTACK, true);
    }
}

uint32 Unit::GetDefenseSkillValue(Unit const* target) const
{
    if (IsPlayer(this))
    {

        uint32 value = (target &&IsPlayer(target))
            ? ((Player*)this)->GetMaxSkillValue(SKILL_DEFENSE)
            : ((Player*)this)->GetSkillValue(SKILL_DEFENSE);
        return value;
    }
    else
    {
        return GetUnitMeleeSkill(target);
    }
}

uint32 Unit::GetWeaponSkillValue(WeaponAttackType attType, Unit const* target) const
{
    uint32 value;
    if (IsPlayer(this))
    {
        Item* item = ((Player*)this)->GetWeaponForAttack(attType, true, true);

        if (attType != BASE_ATTACK && !item)
        {
            return 0;
        }

        if (IsInFeralForm())
        {
            return GetMaxSkillValueForLevel();
        }

        uint32 skill = item ? item->GetSkill() : uint32(SKILL_UNARMED);

        value = (target &&IsPlayer(target))
            ? ((Player*)this)->GetMaxSkillValue(skill)
            : ((Player*)this)->GetSkillValue(skill);
    }
    else
    {
        value = GetUnitMeleeSkill(target);
    }
    return value;
}

void Unit::_UpdateSpells(uint32 time)
{
    if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
    {
        _UpdateAutoRepeatSpell();
    }

    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        if (m_currentSpells[i] && m_currentSpells[i]->getState() == SPELL_STATE_FINISHED)
        {
            m_currentSpells[i]->SetReferencedFromCurrent(false);
            m_currentSpells[i] = nullptr;
        }
    }

    m_auras.EachHolder([time](SpellAuraHolder* holder) { holder->UpdateHolder(time); });

    m_auras.RemoveWhere(
        [](SpellAuraHolder* holder)
        {
            return !(holder->IsPermanent() || holder->IsPassive()) && holder->GetAuraDuration() == 0;
        },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder, AURA_REMOVE_BY_EXPIRE); });

    m_conjured.RemoveDespawnedObjects();
}

void Unit::_UpdateAutoRepeatSpell()
{

    if ((IsPlayer(this) && ((Player*)this)->isMoving()) || IsNonMeleeSpellCasted(false, false, true))
    {

        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category == 351)
        {
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        }
        m_AutoRepeatFirstCast = true;
        return;
    }

    if (m_AutoRepeatFirstCast && getAttackTimer(RANGED_ATTACK) < 500)
    {
        setAttackTimer(RANGED_ATTACK, 500);
    }
    m_AutoRepeatFirstCast = false;

    if (isAttackReady(RANGED_ATTACK))
    {

        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->CheckCast(true) != SPELL_CAST_OK)
        {
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            return;
        }

        Spell* spell = new Spell(this, m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo, true);
        spell->prepare(&(m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_targets));

        resetAttackTimer(RANGED_ATTACK);
    }
}

void Unit::SetCurrentCastedSpell(Spell* pSpell)
{
    MANGOS_ASSERT(pSpell);

    CurrentSpellTypes CSpellType = pSpell->GetCurrentContainer();

    if (pSpell == m_currentSpells[CSpellType])
    {
        return;
    }

    InterruptSpell(CSpellType, false);

    switch (CSpellType)
    {
        case CURRENT_GENERIC_SPELL:
        {

            InterruptSpell(CURRENT_CHANNELED_SPELL, false);

            if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
            {

                if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category == 351)
                {
                    InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
                }
                m_AutoRepeatFirstCast = true;
            }
            break;
        }
        case CURRENT_CHANNELED_SPELL:
        {

            InterruptSpell(CURRENT_GENERIC_SPELL, false);
            InterruptSpell(CURRENT_CHANNELED_SPELL);

            if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL] &&
                m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category == 351)
            {
                InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            }
            break;
        }
        case CURRENT_AUTOREPEAT_SPELL:
        {

            if (pSpell->m_spellInfo->Category == 351)
            {

                InterruptSpell(CURRENT_GENERIC_SPELL, false);
                InterruptSpell(CURRENT_CHANNELED_SPELL, false);
            }

            m_AutoRepeatFirstCast = true;
            break;
         }
        default:
        {

            break;
        }
    }

    if (m_currentSpells[CSpellType])
    {
        m_currentSpells[CSpellType]->SetReferencedFromCurrent(false);
    }

    m_currentSpells[CSpellType] = pSpell;
    pSpell->SetReferencedFromCurrent(true);

    pSpell->SetSelfContainer(&(m_currentSpells[pSpell->GetCurrentContainer()]));

}

void Unit::InterruptSpell(CurrentSpellTypes spellType, bool withDelayed)
{
    if (m_currentSpells[spellType] && (withDelayed || m_currentSpells[spellType]->getState() != SPELL_STATE_DELAYED))
    {

        if (spellType == CURRENT_AUTOREPEAT_SPELL)
        {
            if (IsPlayer(this))
            {
                ((Player*)this)->SendAutoRepeatCancel();
            }
        }

        if (m_currentSpells[spellType]->getState() != SPELL_STATE_FINISHED)
        {
            m_currentSpells[spellType]->cancel();
        }

        if (m_currentSpells[spellType])
        {
            m_currentSpells[spellType]->SetReferencedFromCurrent(false);
            m_currentSpells[spellType] = nullptr;
        }
    }
}

void Unit::ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs)
{
    if (IsPlayer(this) || !unTimeMs)
    {
        return;
    }

    m_schoolLockoutMask = idSchoolMask;
    m_schoolLockoutExpire = time(nullptr) + unTimeMs / IN_MILLISECONDS;
}

bool Unit::IsSchoolLockedOut(SpellSchoolMask schoolMask) const
{
    if (!m_schoolLockoutMask || !schoolMask)
    {
        return false;
    }

    if (!(m_schoolLockoutMask & schoolMask))
    {
        return false;
    }

    if (time(nullptr) >= m_schoolLockoutExpire)
    {
        return false;
    }
    return true;
}

void Unit::FinishSpell(CurrentSpellTypes spellType, bool ok )
{
    Spell* spell = m_currentSpells[spellType];
    if (!spell)
    {
        return;
    }

    if (spellType == CURRENT_CHANNELED_SPELL)
    {
        spell->SendChannelUpdate(0);
    }

    spell->finish(ok);
}

bool Unit::IsClientControlled(Player const* exactClient ) const
{

    if (!HasUnitFlag(UNIT_FLAG_POSSESSED))
    {
        return false;
    }

    if (HasUnitFlag((UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_CONFUSED | UNIT_FLAG_FLEEING)))
    {
        return false;
    }

    if (ObjectGuid guid = GetCharmerGuid())
    {

        if (HasUnitFlag(UNIT_FLAG_POSSESSED) && (guid != 0 && GuidHigh(guid) == HIGHGUID_PLAYER))
        {
            return (exactClient ? (exactClient->GetObjectGuid() == guid) : true);
        }
        return false;
    }

    if (IsPlayer(this))
    {
        return (exactClient ? (exactClient == this) : true);
    }
    return false;
}

bool Unit::IsNonMeleeSpellCasted(bool withDelayed, bool skipChanneled, bool skipAutorepeat, bool forMovement, bool forAutoIgnore) const
{

    if (m_currentSpells[CURRENT_GENERIC_SPELL] &&
        (m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_FINISHED) &&
        (withDelayed || m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_DELAYED))
    {
        return true;
    }

    else if (!skipChanneled && m_currentSpells[CURRENT_CHANNELED_SPELL] &&
        (m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() != SPELL_STATE_FINISHED))
    {
        return true;
    }

    else if (!skipAutorepeat && m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
    {
        return true;
    }

    return false;
}

void Unit::InterruptNonMeleeSpells(bool withDelayed, uint32 spell_id)
{

    if (m_currentSpells[CURRENT_GENERIC_SPELL] && (!spell_id || m_currentSpells[CURRENT_GENERIC_SPELL]->m_spellInfo->ID == spell_id))
    {
        InterruptSpell(CURRENT_GENERIC_SPELL, withDelayed);
    }

    if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL] && (!spell_id || m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->ID == spell_id))
    {
        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, withDelayed);
    }

    if (m_currentSpells[CURRENT_CHANNELED_SPELL] && (!spell_id || m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->ID == spell_id))
    {
        InterruptSpell(CURRENT_CHANNELED_SPELL, true);
    }
}

Spell* Unit::FindCurrentSpellBySpellId(uint32 spell_id) const
{
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        if (m_currentSpells[i] && m_currentSpells[i]->m_spellInfo->ID == spell_id)
        {
            return m_currentSpells[i];
        }
    }
    return nullptr;
}

void Unit::SetInFront(Unit const* target)
{
    Place().Face(Where().BearingTo(target->Where()));
}

void Unit::SetFacingTo(float ori)
{
    Movement::MoveSplineInit init(*this);
    init.SetFacing(ori);
    init.Launch();
}

void Unit::SetFacingToObject(Occupant* pObject)
{

    if (!IsStopped())
    {
        return;
    }

    SetFacingTo(Where().BearingTo(pObject->Where()));
}

bool Unit::isInAccessablePlaceFor(Creature const* c) const
{
    if (IsInWater())
    {
        return c->CanSwim();
    }
    else
    {
        return c->CanWalk() || c->CanFly();
    }
}

bool Unit::IsInWater() const
{
    return GetMap()->GetTerrain()->IsInWater(Where().X(), Where().Y(), Where().Z());
}

bool Unit::IsUnderWater() const
{
    return GetMap()->GetTerrain()->IsUnderWater(Where().X(), Where().Y(), Where().Z());
}

void Unit::DeMorph()
{
    SetDisplayId(GetNativeDisplayId());
}

void Unit::RemoveAurasByCaster(ObjectGuid casterGuid)
{
    m_auras.RemoveWhere(
        [casterGuid](SpellAuraHolder* holder) { return holder->GetCasterGuid() == casterGuid; },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder); });
}

void Unit::SendSpellNonMeleeDamageLog(SpellNonMeleeDamage* log)
{
    WorldPacket data(SMSG_SPELLNONMELEEDAMAGELOG, (16 + 4 + 4 + 1 + 4 + 4 + 1 + 1 + 4 + 4 + 1));
    data << log->target->GetPackGUID();
    data << log->attacker->GetPackGUID();
    data << uint32(log->SpellID);
    data << uint32(log->damage);
    data << uint8(log->school);
    data << uint32(log->absorb);
    data << uint32(log->resist);
    data << uint8(log->physicalLog);
    data << uint8(log->unused);
    data << uint32(log->blocked);
    data << uint32(log->HitInfo);
    data << uint8(0);
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Unit::SendSpellNonMeleeDamageLog(Unit* target, uint32 SpellID, uint32 Damage, SpellSchoolMask damageSchoolMask, uint32 AbsorbedDamage, uint32 Resist, bool PhysicalDamage, uint32 Blocked, bool CriticalHit)
{
    SpellNonMeleeDamage log(this, target, SpellID, GetFirstSchoolInMask(damageSchoolMask));
    log.damage = Damage - AbsorbedDamage - Resist - Blocked;
    log.absorb = AbsorbedDamage;
    log.resist = Resist;
    log.physicalLog = PhysicalDamage;
    log.blocked = Blocked;
    log.HitInfo = SPELL_HIT_TYPE_UNK1 | SPELL_HIT_TYPE_UNK3 | SPELL_HIT_TYPE_UNK6;
    if (CriticalHit)
    {
        log.HitInfo |= SPELL_HIT_TYPE_CRIT;
    }
    SendSpellNonMeleeDamageLog(&log);
}

void Unit::SendPeriodicAuraLog(SpellPeriodicAuraLogInfo* pInfo)
{
    Aura* aura = pInfo->aura;
    Modifier* mod = aura->GetModifier();

    WorldPacket data(SMSG_PERIODICAURALOG, 30);
    data << aura->GetTarget()->GetPackGUID();
    data << PackGuid(aura->GetCasterGuid());
    data << uint32(aura->GetId());
    data << uint32(1);
    data << uint32(mod->m_auraname);
    switch (mod->m_auraname)
    {
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            data << uint32(pInfo->damage);
            data << uint32(aura->GetSpellProto()->School);
            data << uint32(pInfo->absorb);
            data << uint32(pInfo->resist);
            break;
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
            data << uint32(pInfo->damage);
            break;
        case SPELL_AURA_OBS_MOD_MANA:
        case SPELL_AURA_PERIODIC_ENERGIZE:
            data << uint32(mod->m_miscvalue);
            data << uint32(pInfo->damage);
            break;
        case SPELL_AURA_PERIODIC_MANA_LEECH:
            data << uint32(mod->m_miscvalue);
            data << uint32(pInfo->damage);
            data << float(pInfo->multiplier);
            break;
        default:
            sLog.outError("Unit::SendPeriodicAuraLog: unknown aura %u", uint32(mod->m_auraname));
            return;
    }

    Deliver(Audience::Around(*aura->GetTarget()).AndSubject(), &data);
}

void Unit::OweSplits(const std::vector<combat::SplitShare>& splits, SpellSchoolMask school)
{
    m_owedSplits.insert(m_owedSplits.end(), splits.begin(), splits.end());
    m_owedSplitSchool = school;
}

void Unit::DeliverOwedSplits()
{
    if (m_owedSplits.empty())
    {
        return;
    }

    static thread_local uint32 depth = 0;
    if (depth >= MAX_PROC_DEPTH)
    {
        sLog.outError("Unit::DeliverOwedSplits: split chain deeper than %u, dropped", MAX_PROC_DEPTH);
        m_owedSplits.clear();
        return;
    }

    std::vector<combat::SplitShare> owed;
    owed.swap(m_owedSplits);

    const SpellSchoolMask school = m_owedSplitSchool;

    ++depth;

    for (const combat::SplitShare& share : owed)
    {
        Unit* target = ObjectLookup::GetUnit(*this, share.target);
        if (!target || !target->IsAlive())
        {
            continue;
        }

        uint32 amount = uint32(share.amount);
        uint32 absorbed = 0;
        DealDamageMods(target, amount, &absorbed);

        SendSpellNonMeleeDamageLog(target, share.spellId, amount, school, absorbed, 0, false, 0, false);

        CleanDamage cleanDamage = CleanDamage(amount, BASE_ATTACK, MELEE_HIT_NORMAL);
        DealDamage(target, amount, &cleanDamage, DIRECT_DAMAGE, school, nullptr, false);
    }

    --depth;
}

void Unit::ProcDamageAndSpell(Unit* pVictim, uint32 procAttacker, uint32 procVictim, uint32 procExtra, uint32 amount, WeaponAttackType attType, SpellEntry const* procSpell)
{

    static thread_local uint32 depth = 0;
    if (depth >= MAX_PROC_DEPTH)
    {
        sLog.outError("Unit::ProcDamageAndSpell: proc chain deeper than %u, dropped at spell %u",
                      MAX_PROC_DEPTH, procSpell ? procSpell->ID : 0);
        return;
    }

    ++depth;

    if (procAttacker)
    {
        ProcDamageAndSpellFor(false, pVictim, procAttacker, procExtra, attType, procSpell, amount);
    }

    if (pVictim && pVictim->IsAlive() && procVictim)
    {
        pVictim->ProcDamageAndSpellFor(true, this, procVictim, procExtra, attType, procSpell, amount);
    }

    --depth;
}

void Unit::SendSpellMiss(Unit* target, uint32 spellID, SpellMissInfo missInfo)
{
    WorldPacket data(SMSG_SPELLLOGMISS, (4 + 8 + 1 + 4 + 8 + 1 + (missInfo == SPELL_MISS_NONE ? 0 : 8)));
    data << uint32(spellID);
    data << GetObjectGuid();
    data << uint8(0);
    data << uint32(1);

    data << target->GetObjectGuid();
    data << uint8(missInfo);
    if (missInfo != SPELL_MISS_NONE)
    {
        data << float(0) << float(0);
    }

    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Unit::SendAttackStateUpdate(CalcDamageInfo* damageInfo)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Sending SMSG_ATTACKERSTATEUPDATE");

    WorldPacket data(SMSG_ATTACKERSTATEUPDATE, (16 + 45));
    data << (uint32)damageInfo->HitInfo;
    data << GetPackGUID();
    data << damageInfo->target->GetPackGUID();
    data << (uint32)(damageInfo->damage);

    data << (uint8)1;

    data << uint32(GetFirstSchoolInMask(damageInfo->damageSchoolMask));
    data << float(damageInfo->damage);
    data << uint32(damageInfo->damage);
    data << uint32(damageInfo->absorb);
    data << uint32(damageInfo->resist);

    data << uint32(damageInfo->TargetState);
    if (damageInfo->absorb == 0)
    {
        data << (uint32)0;
    }
    else
    {
        data << (uint32) - 1;
    }

    data << uint32(0);

    data << uint32(damageInfo->blocked_amount);
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Unit::SendAttackStateUpdate(uint32 HitInfo, Unit* target, SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage, uint32 Resist, VictimState TargetState, uint32 BlockedAmount)
{
    CalcDamageInfo dmgInfo;
    dmgInfo.HitInfo = HitInfo;
    dmgInfo.attacker = this;
    dmgInfo.target = target;
    dmgInfo.damage = Damage - AbsorbDamage - Resist - BlockedAmount;
    dmgInfo.damageSchoolMask = damageSchoolMask;
    dmgInfo.absorb = AbsorbDamage;
    dmgInfo.resist = Resist;
    dmgInfo.TargetState = TargetState;
    dmgInfo.blocked_amount = BlockedAmount;
    SendAttackStateUpdate(&dmgInfo);
}

FactionTemplateEntry const* Unit::getFactionTemplateEntry() const
{
    FactionTemplateEntry const* entry = sFactionTemplateStore.LookupEntry(getFaction());
    if (!entry)
    {
        static ObjectGuid guid = 0;

        if (GetObjectGuid() != guid)
        {
            guid = GetObjectGuid();

            if (GuidHigh(guid) == HIGHGUID_PET)
            {
                sLog.outError("%s (base creature entry %u) have invalid faction template id %u, owner %s", GetGuidStr().c_str(), GetEntry(), getFaction(), GuidString(GetOwnerGuid()).c_str());
            }
            else
            {
                sLog.outError("%s have invalid faction template id %u", GetGuidStr().c_str(), getFaction());
            }
        }
    }
    return entry;
}

bool Unit::Attack(Unit* victim, bool meleeAttack)
{
    if (!victim || victim == this)
    {
        return false;
    }

    if (!IsAlive() || !victim->IsInWorld() || !victim->IsAlive())
    {
        return false;
    }

    if (IsPlayer(this) && IsMounted())
    {
        return false;
    }

    if (IsPlayer(victim))
    {
        if (((Player*)victim)->isGameMaster())
        {
            return false;
        }
    }
    else
    {
        if (((Creature*)victim)->IsInEvadeMode())
        {
            return false;
        }
    }

    if (HasAuraType(SPELL_AURA_MOD_UNATTACKABLE))
    {
        RemoveAurasOfType(SPELL_AURA_MOD_UNATTACKABLE);
    }

    if (m_attacking)
    {
        if (m_attacking == victim)
        {

            if (meleeAttack && !hasUnitState(UNIT_STAT_MELEE_ATTACKING))
            {
                addUnitState(UNIT_STAT_MELEE_ATTACKING);
                SendMeleeAttackStart(victim);
                return true;
            }
            return false;
        }

        AttackStop(true);
    }

    else
    {

        if (IsCreature(this))
        {
            ((Creature*)this)->SetCombatAnchor(Geometry::Vector3(Where().X(), Where().Y(), Where().Z()));
        }
    }

    SetTargetGuid(victim->GetObjectGuid());

    if (meleeAttack)
    {
        addUnitState(UNIT_STAT_MELEE_ATTACKING);
    }

    m_attacking = victim;
    m_attacking->_addAttacker(this);

    if (IsCreature(this))
    {
        ((Creature*)this)->SendAIReaction(AI_REACTION_HOSTILE);
        ((Creature*)this)->CallAssistance();
    }

    if (haveOffhandWeapon())
    {
        resetAttackTimer(OFF_ATTACK);
    }

    if (meleeAttack)
    {
        SendMeleeAttackStart(victim);
    }

    return true;
}

void Unit::AttackedBy(Unit* attacker)
{

    if (IsCreature(this) && ((Creature*)this)->AI())
    {
        ((Creature*)this)->AI()->AttackedBy(attacker);
    }

    if (attacker == this)
    {
        return;
    }

    if (Pet* pet = GetPet())
    {
        pet->AttackedBy(attacker);
    }
}

bool Unit::AttackStop(bool targetSwitch )
{
    if (!m_attacking)
    {
        return false;
    }

    Unit* victim = m_attacking;

    m_attacking->_removeAttacker(this);
    m_attacking = nullptr;

    SetTargetGuid(0);

    clearUnitState(UNIT_STAT_MELEE_ATTACKING);

    InterruptSpell(CURRENT_MELEE_SPELL);

    if (!targetSwitch && IsCreature(this))
    {
        ((Creature*)this)->SetNoCallAssistance(false);

        if (((Creature*)this)->HasSearchedAssistance())
        {
            ((Creature*)this)->SetNoSearchAssistance(false);
            Pacing().Reckon(MOVE_RUN, false);
        }
    }

    SendMeleeAttackStop(victim);

    return true;
}

void Unit::CombatStop(bool includingCast)
{
    if (includingCast && IsNonMeleeSpellCasted(false))
    {
        InterruptNonMeleeSpells(false);
    }

    AttackStop();
    RemoveAllAttackers();

    if (IsPlayer(this))
    {
        ((Player*)this)->SendAttackSwingCancelAttack();
    }
    else if (Colours().Flags() & TEMPFACTION_RESTORE_COMBAT_STOP)
    {
        Colours().TakeOff();
    }

    ClearInCombat();
}

struct CombatStopWithPetsHelper
{
    explicit CombatStopWithPetsHelper(bool _includingCast) : includingCast(_includingCast) {}
    void operator()(Unit* unit) const { unit->CombatStop(includingCast); }
    bool includingCast;
};

void Unit::CombatStopWithPets(bool includingCast)
{
    CombatStop(includingCast);
    CallForAllControlledUnits(CombatStopWithPetsHelper(includingCast), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

struct IsAttackingPlayerHelper
{
    explicit IsAttackingPlayerHelper() {}
    bool operator()(Unit const* unit) const { return unit->isAttackingPlayer(); }
};

bool Unit::isAttackingPlayer() const
{
    if (hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        return true;
    }

    return CheckAllControlledUnits(IsAttackingPlayerHelper(), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

void Unit::RemoveAllAttackers()
{
    while (!m_attackers.empty())
    {
        AttackerSet::iterator iter = m_attackers.begin();
        if (!(*iter)->AttackStop())
        {
            sLog.outError("WORLD: Unit has an attacker that isn't attacking it!");
            m_attackers.erase(iter);
        }
    }
}

void Unit::ModifyAuraState(AuraState flag, bool apply)
{
    if (apply)
    {
        if (!HasFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1)))
        {
            SetFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1));
            if (IsPlayer(this))
            {
                const PlayerSpellMap& sp_list = ((Player*)this)->GetSpellMap();
                for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
                {
                    if (itr->second.state == PLAYERSPELL_REMOVED)
                    {
                        continue;
                    }
                    SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);
                    if (!spellInfo || !(cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
                    {
                        continue;
                    }
                    if (AuraState(spellInfo->CasterAuraState) == flag)
                    {
                        CastSpell(this, itr->first, true, nullptr);
                    }
                }
            }
        }
    }
    else
    {
        if (HasFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1)))
        {
            RemoveFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1));

            Unit::SpellAuraHolderMap& tAuras = GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::iterator itr = tAuras.begin(); itr != tAuras.end();)
            {
                SpellEntry const* spellProto = (*itr).second->GetSpellProto();
                if (spellProto->CasterAuraState == flag)
                {

                    if (spellProto->SpellIconID == 2006 && spellProto->IsFitToFamilyMask(UI64LIT(0x0000000000100000)))
                    {
                        ++itr;
                        continue;
                    }

                    RemoveHolder(itr->second);
                    itr = tAuras.begin();
                }
                else
                {
                    ++itr;
                }
            }
        }
    }
}

Unit* Unit::GetOwner() const
{
    if (ObjectGuid ownerid = GetOwnerGuid())
    {
        return ObjectLookup::GetUnit(*this, ownerid);
    }
    return nullptr;
}

Unit* Unit::GetCharmer() const
{
    if (ObjectGuid charmerid = GetCharmerGuid())
    {
        return ObjectLookup::GetUnit(*this, charmerid);
    }
    return nullptr;
}

bool Unit::IsCharmerOrOwnerPlayerOrPlayerItself() const
{
    if (IsPlayer(this))
    {
        return true;
    }

    return (GetCharmerOrOwnerGuid() != 0 && GuidHigh(GetCharmerOrOwnerGuid()) == HIGHGUID_PLAYER);
}

Player* Unit::GetCharmerOrOwnerPlayerOrPlayerItself()
{
    ObjectGuid guid = GetCharmerOrOwnerGuid();
    if ((guid != 0 && GuidHigh(guid) == HIGHGUID_PLAYER))
    {
        return sPlayerRegistry.Find(guid);
    }

    return IsPlayer(this) ? (Player*)this : nullptr;
}

Player const* Unit::GetCharmerOrOwnerPlayerOrPlayerItself() const
{
    ObjectGuid guid = GetCharmerOrOwnerGuid();
    if ((guid != 0 && GuidHigh(guid) == HIGHGUID_PLAYER))
    {
        return sPlayerRegistry.Find(guid);
    }

    return IsPlayer(this) ? (Player const*)this : nullptr;
}

Pet* Unit::GetPet() const
{
    if (ObjectGuid pet_guid = GetPetGuid())
    {
        Map* on = FindMap();
        if (!on)
        {
            return nullptr;
        }

        if (Pet* pet = on->GetPet(pet_guid))
        {
            return pet;
        }

        if (TransportMap* hull = on->AsTransport())
        {
            Transport* vessel = hull->Vessel();
            if (Map* sailed = vessel ? vessel->GetMap() : nullptr)
            {
                if (Pet* pet = sailed->GetPet(pet_guid))
                {
                    return pet;
                }
            }
        }
        else
        {
            for (Transport* vessel : sFleet.On(on->GetId()))
            {
                if (TransportMap* deck = vessel->AsMap())
                {
                    if (Pet* pet = deck->GetPet(pet_guid))
                    {
                        return pet;
                    }
                }
            }
        }

        sLog.outError("Unit::GetPet: %s not exist.", GuidString(pet_guid).c_str());
        const_cast<Unit*>(this)->SetPet(0);
    }

    return nullptr;
}

Pet* Unit::_GetPet(ObjectGuid guid) const
{
    return GetMap()->GetPet(guid);
}

Unit* Unit::GetCharm() const
{
    if (ObjectGuid charm_guid = GetCharmGuid())
    {
        if (Unit* pet = ObjectLookup::GetUnit(*this, charm_guid))
        {
            return pet;
        }

        sLog.outError("Unit::GetCharm: Charmed %s not exist.", GuidString(charm_guid).c_str());
        const_cast<Unit*>(this)->SetCharm(nullptr);
    }

    return nullptr;
}

void Unit::Uncharm()
{
    if (Unit* charm = GetCharm())
    {
        charm->RemoveAurasOfType(SPELL_AURA_MOD_CHARM);
        charm->RemoveAurasOfType(SPELL_AURA_MOD_POSSESS);
        charm->RemoveAurasOfType(SPELL_AURA_MOD_POSSESS_PET);
    }
}

void Unit::SetPet(Pet* pet)
{
    SetPetGuid(pet ? pet->GetObjectGuid() : 0);
}

void Unit::SetCharm(Unit* pet)
{
    SetCharmGuid(pet ? pet->GetObjectGuid() : 0);
}

int32 Unit::DealHeal(Unit* pVictim, uint32 addhealth, SpellEntry const* spellProto, bool critical)
{
    int32 gain = pVictim->ModifyHealth(int32(addhealth));

    Unit* unit = this;

    if (IsCreature(this) && ((Creature*)this)->IsTotem() && ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
    {
        unit = GetOwner();
    }

    if (IsPlayer(unit))
    {
        unit->SendHealSpellLog(pVictim, spellProto->ID, addhealth, critical);
    }

    if (IsCreature(pVictim) && ((Creature*)pVictim)->AI())
    {
        ((Creature*)pVictim)->AI()->HealedBy(this, addhealth);
    }

    return gain;
}

Unit* Unit::SelectMagnetTarget(Unit* victim, Spell* spell, SpellEffectIndex eff)
{
    if (!victim)
    {
        return nullptr;
    }

    if (spell && (spell->m_spellInfo->DefenseType == SPELL_DAMAGE_CLASS_NONE || spell->m_spellInfo->DefenseType == SPELL_DAMAGE_CLASS_MAGIC))
    {
        const auto magnetAuras = victim->GetAurasByType(SPELL_AURA_SPELL_MAGNET);
        for (auto* aura : magnetAuras)
        {
            if (Unit* magnet = aura->GetCaster())
            {
                if (magnet->IsAlive() && HasLineOfSight(*magnet, *this) && spell->CheckTarget(magnet, spell->Recipe().At(static_cast<uint8>(eff))))
                {
                    if (SpellAuraHolder* holder = aura->GetHolder())
                    {
                        if (holder->DropAuraCharge())
                        {
                            victim->RemoveHolder(holder);
                        }
                    }
                    return magnet;
                }
            }
        }
    }

    return victim;
}

void Unit::SendHealSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, bool critical)
{

    WorldPacket data(SMSG_SPELLHEALLOG, (8 + 8 + 4 + 4 + 1));
    data << pVictim->GetPackGUID();
    data << GetPackGUID();
    data << uint32(SpellID);
    data << uint32(Damage);
    data << uint8(critical ? 1 : 0);
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Unit::SendEnergizeSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype)
{
    WorldPacket data(SMSG_SPELLENERGIZELOG, (8 + 8 + 4 + 4 + 4 + 1));
    data << pVictim->GetPackGUID();
    data << GetPackGUID();
    data << uint32(SpellID);
    data << uint32(powertype);
    data << uint32(Damage);
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Unit::EnergizeBySpell(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype)
{
    SendEnergizeSpellLog(pVictim, SpellID, Damage, powertype);

    pVictim->ModifyPower(powertype, Damage);
}

bool Unit::IsImmuneToDamage(SpellSchoolMask shoolMask)
{

    if (m_immune.AnyOf(IMMUNITY_SCHOOL, shoolMask))
    {
        return true;
    }

    SpellImmuneList const& damageList = m_immune.Of(IMMUNITY_DAMAGE);
    for (SpellImmuneList::const_iterator itr = damageList.begin(); itr != damageList.end(); ++itr)
    {
        if (itr->type & shoolMask)
        {
            return true;
        }
    }
    return false;
}

void Unit::ApplySpellImmune(uint32 spellId, uint32 op, uint32 type, bool apply)
{
    if (apply)
    {
        m_immune.Grant(spellId, op, type);
    }
    else
    {
        m_immune.Revoke(spellId, op);
    }
}

void Unit::ApplySpellDispelImmunity(const SpellEntry* spellProto, DispelType type, bool apply)
{
    ApplySpellImmune(spellProto->ID, IMMUNITY_DISPEL, type, apply);

    if (apply && cast::RecipeOf(*spellProto).Says().dispelsOnImmunity)
    {
        RemoveAurasWithDispelType(type);
    }
}

float Unit::GetWeaponProcChance() const
{

    if (isAttackReady(BASE_ATTACK))
    {
        return (GetAttackTime(BASE_ATTACK) * 1.8f / 1000.0f);
    }
    else if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
    {
        return (GetAttackTime(OFF_ATTACK) * 1.6f / 1000.0f);
    }

    return 0.0f;
}

float Unit::GetPPMProcChance(uint32 WeaponSpeed, float PPM) const
{

    if (PPM <= 0.0f)
    {
        return 0.0f;
    }
    return WeaponSpeed * PPM / 600.0f;
}

void Unit::Mount(uint32 mount, uint32 spellId)
{
    if (!mount)
    {
        return;
    }

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOUNTING);

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, mount);
}

void Unit::Unmount(bool from_aura)
{
    if (!IsMounted())
    {
        return;
    }

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_NOT_MOUNTED);

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);

    if (from_aura)
    {
        WorldPacket data(SMSG_DISMOUNT, 8);
        data << GetPackGUID();
        Deliver(Audience::Around(*this).AndSubject(), &data);
    }
}

void Unit::SetInCombatWith(Unit* enemy)
{
    Unit* eOwner = enemy->GetCharmerOrOwnerOrSelf();
    if (eOwner->IsPvP())
    {
        SetInCombatState(true, enemy);
        return;
    }

    if (IsPlayer(eOwner) && ((Player*)eOwner)->Duelling().Stands())
    {
        if (Player const* myOwner = GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            if (myOwner->Duelling().With((Player const*)eOwner))
            {
                SetInCombatState(true, enemy);
                return;
            }
        }
    }

    SetInCombatState(false, enemy);
}

void Unit::SetInDummyCombatState(bool state)
{
    if (state)
    {
        m_dummyCombatState = true;
        SetInCombatState(false);
    }
    else
    {
        m_dummyCombatState = false;
    }
}

void Unit::SetInCombatState(bool PvP, Unit* enemy)
{

    if (!IsAlive())
    {
        return;
    }

    if (PvP)
    {
        m_CombatTimer = 5000;
    }

    if (IsInCombat())
    {
        return;
    }

    bool creatureNotInCombat = IsCreature(this) && !HasUnitFlag(UNIT_FLAG_IN_COMBAT);

    SetUnitFlag(UNIT_FLAG_IN_COMBAT);

    if (IsCharmed() || (!IsPlayer(this) && ((Creature*)this)->IsPet()))
    {
        SetUnitFlag(UNIT_FLAG_PET_IN_COMBAT);
    }

    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
    {
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
        {
            if (IsNonCombatSpell(spell->m_spellInfo))
            {
                InterruptSpell(CurrentSpellTypes(i), false);
            }
        }
    }

    if (creatureNotInCombat)
    {

        RemoveUnitFlag(UNIT_FLAG_OOC_NOT_ATTACKABLE);

        Creature* pCreature = (Creature*)this;

        if (pCreature->AI())
        {
            pCreature->AI()->EnterCombat(enemy);
        }

        if (GetMap()->IsDungeon() && (pCreature->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_AGGRO_ZONE) && enemy && enemy->IsControlledByPlayer())
        {
            pCreature->SetInCombatWithZone();
        }

        if (InstanceData* mapInstance = GetInstanceData())
        {
            mapInstance->OnCreatureEnterCombat(pCreature);
        }

        pCreature->Links().Aggroed(enemy);
    }

}

void Unit::ClearInCombat()
{
    m_CombatTimer = 0;
    RemoveUnitFlag(UNIT_FLAG_IN_COMBAT);

    if (IsCharmed() || (!IsPlayer(this) && ((Creature*)this)->IsPet()))
    {
        RemoveUnitFlag(UNIT_FLAG_PET_IN_COMBAT);
    }

    if (IsCreature(this))
    {
        Creature* cThis = static_cast<Creature*>(this);
        if (cThis->GetCreatureInfo()->UnitFlags & UNIT_FLAG_OOC_NOT_ATTACKABLE && !(cThis->GetTemporaryFactionFlags() & TEMPFACTION_TOGGLE_OOC_NOT_ATTACK))
        {
            SetUnitFlag(UNIT_FLAG_OOC_NOT_ATTACKABLE);
        }

        clearUnitState(UNIT_STAT_ATTACK_PLAYER);
    }
}

bool Unit::IsTargetableForAttack(bool inverseAlive ) const
{
    if (IsPlayer(this) && ((Player*)this)->isGameMaster())
    {
        return false;
    }

    if (HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
    {
        return false;
    }

    if (HasUnitFlag(UNIT_FLAG_OOC_NOT_ATTACKABLE))
    {
        return false;
    }

    if (IsAlive() == inverseAlive)
    {
        return false;
    }

    return IsInWorld() && !hasUnitState(UNIT_STAT_DIED) && !IsTaxiFlying();
}

int32 Unit::ModifyHealth(int32 dVal)
{
    if (dVal == 0)
    {
        return 0;
    }

    int32 curHealth = (int32)GetHealth();

    int32 val = dVal + curHealth;
    if (val <= 0)
    {
        SetHealth(0);
        return -curHealth;
    }

    int32 maxHealth = (int32)GetMaxHealth();

    int32 gain;
    if (val < maxHealth)
    {
        SetHealth(val);
        gain = val - curHealth;
    }
    else
    {
        SetHealth(maxHealth);
        gain = maxHealth - curHealth;
    }

    return gain;
}

int32 Unit::ModifyPower(Powers power, int32 dVal)
{
    if (dVal == 0)
    {
        return 0;
    }

    int32 curPower = (int32)GetPower(power);

    int32 val = dVal + curPower;
    if (val <= 0)
    {
        SetPower(power, 0);
        return -curPower;
    }

    int32 maxPower = (int32)GetMaxPower(power);

    int32 gain;
    if (val < maxPower)
    {
        SetPower(power, val);
        gain = val - curPower;
    }
    else
    {
        SetPower(power, maxPower);
        gain = maxPower - curPower;
    }

    return gain;
}

bool Unit::CanDetectInvisibilityOf(Unit const* u) const
{
    if (uint32 mask = (m_detectInvisibilityMask & u->m_invisibilityMask))
    {
        for (int32 i = 0; i < 32; ++i)
        {
            if (((1 << i) & mask) == 0)
            {
                continue;
            }

            int32 invLevel = GetMaxPositiveAuraModifierByMiscValue(SPELL_AURA_MOD_INVISIBILITY, i);

            int32 detectLevel = (i == 6 && IsPlayer(this)) ? ((Player*)this)->Drinking().Amount() : GetMaxPositiveAuraModifierByMiscValue(SPELL_AURA_MOD_INVISIBILITY_DETECTION, i);

            if (invLevel <= detectLevel)
            {
                return true;
            }
        }
    }

    return false;
}

void Unit::SetDeathState(DeathState s)
{
    if (s != ALIVE && s != JUST_ALIVED)
    {
        CombatStop();
        DeleteThreatList();
        ClearComboPointHolders();

        if (IsNonMeleeSpellCasted(false))
        {
            InterruptNonMeleeSpells(false);
        }
    }

    if (s == JUST_DIED)
    {
        RemoveAllAurasOnDeath();
        m_retinue.RemoveGuardians();
        m_retinue.UnsummonAllTotems();

        StopMoving(true);
        i_motionMaster.Clear(false, true);
        i_motionMaster.MoveIdle();

        ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);

        ClearAllReactives();
        m_diminishing.Clear();
    }
    else if (s == JUST_ALIVED)
    {
        RemoveUnitFlag(UNIT_FLAG_SKINNABLE);
    }

    if (m_deathState != ALIVE && s == ALIVE)
    {

    }
    m_deathState = s;
}

int32 Unit::CalculateSpellDamage(Unit const* target, const cast::Recipe& recipe, const cast::Operation& operation, int32 const* effBasePoints)
{
    Player* unitPlayer = (IsPlayer(this)) ? (Player*)this : nullptr;

    uint8 comboPoints = unitPlayer ? unitPlayer->GetComboPoints() : 0;

    int32 level = int32(getLevel());
    if (level > (int32)recipe.MaxLevel() && recipe.MaxLevel() > 0)
    {
        level = (int32)recipe.MaxLevel();
    }
    else if (level < (int32)recipe.BaseLevel())
    {
        level = (int32)recipe.BaseLevel();
    }
    level -= (int32)recipe.CasterLevel();

    int32 baseDice = int32(operation.baseDice);
    float basePointsPerLevel = operation.pointsPerLevel;
    float randomPointsPerLevel = operation.dicePerLevel;
    int32 basePoints = effBasePoints
        ? *effBasePoints - baseDice
        : operation.basePoints;

    basePoints += int32(level * basePointsPerLevel);
    int32 randomPoints = int32(operation.dieSides + level * randomPointsPerLevel);
    float comboDamage = operation.pointsPerCombo;

    switch (randomPoints)
    {
        case 0:
        case 1: basePoints += baseDice; break;
        default:
        {

            int32 randvalue = baseDice >= randomPoints
                ? irand(randomPoints, baseDice)
                : irand(baseDice, randomPoints);

            basePoints += randvalue;
            break;
        }
    }

    int32 value = basePoints;

    if (comboDamage != 0 && unitPlayer && target && (target->GetObjectGuid() == unitPlayer->GetComboTargetGuid()))
    {
        value += (int32)(comboDamage * comboPoints);
    }

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(recipe.Id(), SPELLMOD_ALL_EFFECTS, value);

    }

    if (recipe.Says().damageScalesWithLevel && recipe.CasterLevel() &&
        operation.verb != SPELL_EFFECT_WEAPON_PERCENT_DAMAGE &&
        operation.verb != SPELL_EFFECT_KNOCK_BACK &&
        (operation.verb != SPELL_EFFECT_APPLY_AURA || operation.aura != SPELL_AURA_MOD_DECREASE_SPEED))
    {
        value = int32(value * 0.25f * exp(getLevel() * (70 - recipe.CasterLevel()) / 1000.0f));
    }

    return value;
}

bool Unit::IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const
{
    return IsVisibleForOrDetect(u, viewPoint, false, inVisibleList, false);
}

bool Unit::IsInvisibleForAlive() const
{
    if (m_AuraFlags & UNIT_AURAFLAG_ALIVE_INVISIBLE)
    {
        return true;
    }

    Creature const* creature = static_cast<Creature const*>(this);
    return creature && creature->IsSpiritService();
}

CreatureRecord Unit::Record() const
{
    return m_creatureInfo ? CreatureRecord(*m_creatureInfo) : CreatureRecord();
}

uint32 Unit::GetCreatureType() const
{

    SpellShapeshiftFormEntry const* form = sSpellShapeshiftFormStore.LookupEntry(GetShapeshiftForm());
    if (form && form->CreatureType > 0)
    {
        return form->CreatureType;
    }

    if (CreatureRecord const record = Record())
    {
        return record.Kind();
    }

    return CREATURE_TYPE_HUMANOID;
}

void Unit::SetLevel(uint32 lvl)
{
    SetUInt32Value(UNIT_FIELD_LEVEL, lvl);

    if ((IsPlayer(this)) && ((Player*)this)->GetGroup())
    {
        ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_LEVEL);
    }
}

void Unit::SetHealth(uint32 val)
{
    uint32 maxHealth = GetMaxHealth();
    if (maxHealth < val)
    {
        val = maxHealth;
    }

    if (m_health != val)
    {
        m_health = val;
        ResendField(UNIT_FIELD_HEALTH);
    }

    if (IsPlayer(this))
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_HP);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_HP);
            }
        }
    }
}

void Unit::SetMaxHealth(uint32 val)
{
    uint32 health = GetHealth();
    if (m_maxHealth != val)
    {
        m_maxHealth = val;
        ResendField(UNIT_FIELD_MAXHEALTH);
    }

    ResendField(UNIT_FIELD_HEALTH);

    if (IsPlayer(this))
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_HP);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_HP);
            }
        }
    }

    if (val < health)
    {
        SetHealth(val);
    }
}

void Unit::SetHealthPercent(float percent)
{
    uint32 newHealth = GetMaxHealth() * percent / 100.0f;
    SetHealth(newHealth);
}

void Unit::AddToWorld()
{
    Object::AddToWorld();
    ScheduleAINotify(0);

}

void Unit::RemoveFromWorld()
{

    if (IsInWorld())
    {
        Uncharm();
        RemoveTrackedAurasOfOthers();
        m_retinue.RemoveGuardians();
        m_conjured.RemoveAllObjects();
        m_conjured.RemoveAllAreas();
        CleanupDeletedAuras();
        GetViewPoint().Event_RemovedFromWorld();
    }

    Object::RemoveFromWorld();
}

void Unit::CleanupsBeforeDelete()
{
    if (m_mirror.IsOpen())
    {
        InterruptNonMeleeSpells(true);
        m_Events.KillAllEvents(false);
        CombatStop();
        ClearComboPointHolders();
        DeleteThreatList();
        if (IsPlayer(this))
        {
            GetHostileRefManager().setOnlineOfflineState(false);
        }
        else
        {
            GetHostileRefManager().deleteReferences();
        }
        RemoveAllAuras(AURA_REMOVE_BY_DELETE);
    }
    Occupant::CleanupsBeforeDelete();
}

CharmInfo& Unit::InitCharmInfo()
{
    if (!m_charmInfo)
    {
        m_charmInfo.emplace(*this);
    }

    return m_charmInfo.value();
}

CharmInfo::CharmInfo(Unit& driven)
    : m_driven(driven), m_CommandState(COMMAND_FOLLOW), m_reactState(REACT_PASSIVE), m_petnumber(0)
{
    for (int i = 0; i < CREATURE_MAX_SPELLS; ++i)
    {
        m_charmspells[i].SetActionAndType(0, ACT_DISABLED);
    }
}

void CharmInfo::InitPetActionBar()
{

    for (uint32 i = 0; i < ACTION_BAR_INDEX_PET_SPELL_START - ACTION_BAR_INDEX_START; ++i)
    {
        SetActionBar(ACTION_BAR_INDEX_START + i, COMMAND_ATTACK - i, ACT_COMMAND);
    }

    for (uint32 i = 0; i < ACTION_BAR_INDEX_PET_SPELL_END - ACTION_BAR_INDEX_PET_SPELL_START; ++i)
    {
        SetActionBar(ACTION_BAR_INDEX_PET_SPELL_START + i, 0, ACT_DISABLED);
    }

    for (uint32 i = 0; i < ACTION_BAR_INDEX_END - ACTION_BAR_INDEX_PET_SPELL_END; ++i)
    {
        SetActionBar(ACTION_BAR_INDEX_PET_SPELL_END + i, COMMAND_ATTACK - i, ACT_REACTION);
    }
}

void CharmInfo::InitEmptyActionBar()
{
    for (uint32 x = ACTION_BAR_INDEX_START + 1; x < ACTION_BAR_INDEX_END; ++x)
    {
        SetActionBar(x, 0, ACT_PASSIVE);
    }
}

void CharmInfo::InitPossessCreateSpells()
{
    InitEmptyActionBar();

    if (IsPlayer(&m_driven))
    {
        return;
    }

    SetActionBar(ACTION_BAR_INDEX_START, COMMAND_ATTACK, ACT_COMMAND);

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        uint32 const spellId = m_driven.Knowing().Slot(x);

        if (cast::Recipes().StartsAs(spellId, cast::Start::Passive))
        {
            m_driven.CastSpell(&m_driven, spellId, true);
        }
        else
        {
            AddSpellToActionBar(spellId, ACT_PASSIVE);
        }
    }
}

void CharmInfo::InitCharmCreateSpells()
{
    if (IsPlayer(&m_driven))
    {
        InitEmptyActionBar();
        return;
    }

    InitPetActionBar();

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        uint32 spellId = m_driven.Knowing().Slot(x);

        if (!spellId)
        {
            m_charmspells[x].SetActionAndType(spellId, ACT_DISABLED);
            continue;
        }

        if (cast::Recipes().StartsAs(spellId, cast::Start::Passive))
        {
            m_driven.CastSpell(&m_driven, spellId, true);
            m_charmspells[x].SetActionAndType(spellId, ACT_PASSIVE);
        }
        else
        {
            m_charmspells[x].SetActionAndType(spellId, ACT_DISABLED);

            ActiveStates newstate;
            bool onlyselfcast = true;
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

            for (uint32 i = 0; i < 3 && onlyselfcast; ++i)
            {
                if (spellInfo->ImplicitTargetA[i] != TARGET_SELF && spellInfo->ImplicitTargetA[i] != 0)
                {
                    onlyselfcast = false;
                }
            }

            if (onlyselfcast || !cast::Recipes().IsPositive(spellId))
            {
                newstate = ACT_DISABLED;
            }
            else
            {
                newstate = ACT_PASSIVE;
            }

            AddSpellToActionBar(spellId, newstate);
        }
    }
}

bool CharmInfo::AddSpellToActionBar(uint32 spell_id, ActiveStates newstate)
{
    uint32 first_id = sSpellMgr.GetFirstSpellInChain(spell_id);

    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (uint32 action = PetActionBar[i].GetAction())
        {
            if (PetActionBar[i].IsActionBarForSpell() && sSpellMgr.GetFirstSpellInChain(action) == first_id)
            {
                PetActionBar[i].SetAction(spell_id);
                return true;
            }
        }
    }

    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (!PetActionBar[i].GetAction() && PetActionBar[i].IsActionBarForSpell())
        {
            SetActionBar(i, spell_id, newstate == ACT_DECIDE ? ACT_DISABLED : newstate);
            return true;
        }
    }
    return false;
}

bool CharmInfo::RemoveSpellFromActionBar(uint32 spell_id)
{
    uint32 first_id = sSpellMgr.GetFirstSpellInChain(spell_id);

    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (uint32 action = PetActionBar[i].GetAction())
        {
            if (PetActionBar[i].IsActionBarForSpell() && sSpellMgr.GetFirstSpellInChain(action) == first_id)
            {
                SetActionBar(i, 0, ACT_DISABLED);
                return true;
            }
        }
    }

    return false;
}

void CharmInfo::ToggleCreatureAutocast(uint32 spellid, bool apply)
{
    if (cast::Recipes().StartsAs(spellid, cast::Start::Passive))
    {
        return;
    }

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        if (spellid == m_charmspells[x].GetAction())
        {
            m_charmspells[x].SetType(apply ? ACT_ENABLED : ACT_DISABLED);
        }
    }
}

void CharmInfo::SetPetNumber(uint32 petnumber, bool statwindow)
{
    m_petnumber = petnumber;
    if (statwindow)
    {
        m_driven.SetUInt32Value(UNIT_FIELD_PETNUMBER, m_petnumber);
    }
    else
    {
        m_driven.SetUInt32Value(UNIT_FIELD_PETNUMBER, 0);
    }
}

void CharmInfo::LoadPetActionBar(const std::string& data)
{
    InitPetActionBar();

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != (ACTION_BAR_INDEX_END - ACTION_BAR_INDEX_START) * 2)
    {
        return;
    }

    int index;
    Tokens::iterator iter;
    for (iter = tokens.begin(), index = ACTION_BAR_INDEX_START; index < ACTION_BAR_INDEX_END; ++iter, ++index)
    {

        uint8 type  = (uint8)std::strtoul((*iter).c_str(), nullptr, 10);
        ++iter;
        uint32 action = std::strtoul((*iter).c_str(), nullptr, 10);

        PetActionBar[index].SetActionAndType(action, ActiveStates(type));

        if (PetActionBar[index].IsActionBarForSpell() && !sSpellStore.LookupEntry(PetActionBar[index].GetAction()))
        {
            SetActionBar(index, 0, ACT_DISABLED);
        }
    }
}

void CharmInfo::BuildActionBar(WorldPacket* data)
{
    for (uint32 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        *data << uint32(PetActionBar[i].packedData);
    }
}

void CharmInfo::SetSpellAutocast(uint32 spell_id, bool state)
{
    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (spell_id == PetActionBar[i].GetAction() && PetActionBar[i].IsActionBarForSpell())
        {
            PetActionBar[i].SetType(state ? ACT_ENABLED : ACT_DISABLED);
            break;
        }
    }
}

bool Unit::IsFrozen() const
{
    return HasAuraState(AURA_STATE_FROZEN);
}

struct ProcTriggeredData
{
    ProcTriggeredData(SpellProcEventEntry const* _spellProcEvent, SpellAuraHolder* _triggeredByHolder)
        : spellProcEvent(_spellProcEvent), triggeredByHolder(_triggeredByHolder)
    {}
    SpellProcEventEntry const* spellProcEvent;
    SpellAuraHolder* triggeredByHolder;
};

typedef std::list< ProcTriggeredData > ProcTriggeredList;
typedef std::list< uint32> RemoveSpellList;

uint32 createProcExtendMask(SpellNonMeleeDamage* damageInfo, SpellMissInfo missCondition)
{
    uint32 procEx = PROC_EX_NONE;

    if (missCondition != SPELL_MISS_NONE)
    {
        switch (missCondition)
        {
            case SPELL_MISS_MISS:    procEx |= PROC_EX_MISS;   break;
            case SPELL_MISS_RESIST:  procEx |= PROC_EX_RESIST; break;
            case SPELL_MISS_DODGE:   procEx |= PROC_EX_DODGE;  break;
            case SPELL_MISS_PARRY:   procEx |= PROC_EX_PARRY;  break;
            case SPELL_MISS_BLOCK:   procEx |= PROC_EX_BLOCK;  break;
            case SPELL_MISS_EVADE:   procEx |= PROC_EX_EVADE;  break;
            case SPELL_MISS_IMMUNE:  procEx |= PROC_EX_IMMUNE; break;
            case SPELL_MISS_IMMUNE2: procEx |= PROC_EX_IMMUNE; break;
            case SPELL_MISS_DEFLECT: procEx |= PROC_EX_DEFLECT; break;
            case SPELL_MISS_ABSORB:  procEx |= PROC_EX_ABSORB; break;
            case SPELL_MISS_REFLECT: procEx |= PROC_EX_REFLECT; break;
            default:
                break;
        }
    }
    else
    {

        if (damageInfo->blocked)
        {
            procEx |= PROC_EX_BLOCK;
        }

        if (damageInfo->absorb)
        {
            procEx |= PROC_EX_ABSORB;
        }

        if (damageInfo->HitInfo & SPELL_HIT_TYPE_CRIT)
        {
            procEx |= PROC_EX_CRITICAL_HIT;
        }
        else
        {
            procEx |= PROC_EX_NORMAL_HIT;
        }
    }
    return procEx;
}

void Unit::ProcDamageAndSpellFor(bool isVictim, Unit* pTarget, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, SpellEntry const* procSpell, uint32 damage)
{

    if (procFlag & MELEE_BASED_TRIGGER_MASK)
    {

        if (IsPlayer(this))
        {

            if (procExtra & (PROC_EX_NORMAL_HIT | PROC_EX_MISS | PROC_EX_RESIST))
            {
                if (!IsPlayer(pTarget) && pTarget->GetCreatureType() != CREATURE_TYPE_CRITTER)
                {
                    ((Player*)this)->UpdateCombatSkills(pTarget, attType, isVictim);
                }
            }

            if (isVictim && procExtra & (PROC_EX_DODGE | PROC_EX_PARRY | PROC_EX_BLOCK))
            {
                ((Player*)this)->UpdateDefense();
            }
        }

        if (procExtra & (PROC_EX_CRITICAL_HIT | PROC_EX_PARRY | PROC_EX_DODGE | PROC_EX_BLOCK))
        {

            if (isVictim)
            {

                if (procExtra & PROC_EX_DODGE)
                {

                    if (getClass() != CLASS_ROGUE)
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, true);
                        StartReactiveTimer(REACTIVE_DEFENSE);
                    }
                }

                if (procExtra & PROC_EX_PARRY)
                {

                    if (getClass() == CLASS_HUNTER)
                    {
                        ModifyAuraState(AURA_STATE_HUNTER_PARRY, true);
                        StartReactiveTimer(REACTIVE_HUNTER_PARRY);
                    }
                    else
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, true);
                        StartReactiveTimer(REACTIVE_DEFENSE);
                    }
                }

                if (procExtra & PROC_EX_BLOCK)
                {
                    ModifyAuraState(AURA_STATE_DEFENSE, true);
                    StartReactiveTimer(REACTIVE_DEFENSE);
                }
            }
            else
            {

                if (procExtra & PROC_EX_DODGE && IsPlayer(this) && getClass() == CLASS_WARRIOR)
                {
                    ((Player*)this)->AddComboPoints(pTarget, 1);
                    StartReactiveTimer(REACTIVE_OVERPOWER);
                }
            }
        }
    }

    RemoveSpellList removedSpells;
    ProcTriggeredList procTriggered;

    for (SpellAuraHolderMap::const_iterator itr = GetSpellAuraHolderMap().begin(); itr != GetSpellAuraHolderMap().end(); ++itr)
    {

        if (itr->second->IsDeleted())
        {
            continue;
        }

        SpellProcEventEntry const* spellProcEvent = nullptr;

        if (!IsTriggeredAtSpellProcEvent(pTarget, itr->second, procSpell, procFlag, procExtra, attType, isVictim, spellProcEvent))
        {
            continue;
        }

        itr->second->SetInUse(true);
        procTriggered.push_back(ProcTriggeredData(spellProcEvent, itr->second));
    }

    if (procTriggered.empty())
    {
        return;
    }

    for (ProcTriggeredList::const_iterator itr = procTriggered.begin(); itr != procTriggered.end(); ++itr)
    {

        SpellAuraHolder* triggeredByHolder = itr->triggeredByHolder;
        if (triggeredByHolder->IsDeleted())
        {
            continue;
        }

        SpellProcEventEntry const* spellProcEvent = itr->spellProcEvent;
        bool useCharges = triggeredByHolder->GetAuraCharges() > 0;
        bool procSuccess = true;
        bool anyAuraProc = false;

        uint32 cooldown = 0;
        if (IsPlayer(this) && spellProcEvent && spellProcEvent->cooldown)
        {
            cooldown = spellProcEvent->cooldown;
        }

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            Aura* triggeredByAura = triggeredByHolder->GetAuraByEffectIndex(SpellEffectIndex(i));
            if (!triggeredByAura)
            {
                continue;
            }

            Modifier* auraModifier = triggeredByAura->GetModifier();

            if (procSpell)
            {
                if (spellProcEvent)
                {
                    if (spellProcEvent->spellFamilyMask[i])
                    {
                        if (!procSpell->IsFitToFamilyMask(spellProcEvent->spellFamilyMask[i]))
                        {
                            continue;
                        }
                    }

                    else if (!triggeredByAura->CanProcFrom(procSpell, spellProcEvent->procEx, procExtra, damage != 0, !spellProcEvent->schoolMask))
                    {
                        continue;
                    }
                }
                else if (!triggeredByAura->CanProcFrom(procSpell, PROC_EX_NONE, procExtra, damage != 0, true))
                {
                    continue;
                }
            }

            SpellAuraProcResult procResult = (*this.*AuraProcHandler[auraModifier->m_auraname])(pTarget, damage, triggeredByAura, procSpell, procFlag, procExtra, cooldown);
            switch (procResult)
            {
                case SPELL_AURA_PROC_CANT_TRIGGER:
                    continue;
                case SPELL_AURA_PROC_FAILED:
                    procSuccess = false;
                    break;
                case SPELL_AURA_PROC_OK:
                    break;
            }

            anyAuraProc = true;
        }

        if (useCharges && procSuccess && anyAuraProc && !triggeredByHolder->IsDeleted())
        {

            if (triggeredByHolder->DropAuraCharge())
            {
                removedSpells.push_back(triggeredByHolder->GetId());
            }
        }

        triggeredByHolder->SetInUse(false);
    }

    if (!removedSpells.empty())
    {

        removedSpells.sort();
        removedSpells.unique();

        for (RemoveSpellList::const_iterator i = removedSpells.begin(); i != removedSpells.end(); ++i)
        {
            RemoveAuras(*i);
        }
    }
}

Player* Unit::GetSpellModOwner() const
{
    if (IsPlayer(this))
    {
        return (Player*)this;
    }
    if (((Creature*)this)->IsPet() || ((Creature*)this)->IsTotem())
    {
        Unit* owner = GetOwner();
        if (owner &&IsPlayer(owner))
        {
            return (Player*)owner;
        }
    }
    return nullptr;
}

void Unit::SendPetCastFail(uint32 spellid, SpellCastResult msg)
{
    if (msg == SPELL_CAST_OK)
    {
        return;
    }

    Unit* owner = GetCharmerOrOwner();
    if (!owner || !IsPlayer(owner))
    {
        return;
    }

    WorldPacket data(SMSG_PET_CAST_FAILED, 4 + 1 + 1);
    data << uint32(spellid);
    data << uint8(0);
    data << uint8(msg);
    switch (msg)
    {
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND:
            data << int32(0);
            data << int32(0);
            break;
        case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
            data << int32(0);
            break;
        case SPELL_FAILED_REQUIRES_AREA:
            data << int32(GetTerrain()->GetAreaId(Where().X(), Where().Y(), Where().Z()));
            break;
        case SPELL_FAILED_PREVENTED_BY_MECHANIC:
            data << int32(0);
            break;
        default:
            break;
    }
    static_cast<Player*>(owner)->SendDirectMessage(&data);
}

void Unit::SendPetActionFeedback(uint8 msg)
{
    Unit* owner = GetOwner();
    if (!owner || !IsPlayer(owner))
    {
        return;
    }

    WorldPacket data(SMSG_PET_ACTION_FEEDBACK, 1);
    data << uint8(msg);
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::SendPetTalk(uint32 pettalk)
{
    Unit* owner = GetOwner();
    if (!owner || !IsPlayer(owner))
    {
        return;
    }

    WorldPacket data(SMSG_PET_ACTION_SOUND, 8 + 4);
    data << GetObjectGuid();
    data << uint32(pettalk);
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::SendPetAIReaction()
{
    Unit* owner = GetOwner();
    if (!owner || !IsPlayer(owner))
    {
        return;
    }

    WorldPacket data(SMSG_AI_REACTION, 8 + 4);
    data << GetObjectGuid();
    data << uint32(AI_REACTION_HOSTILE);
    ((Player*)owner)->GetSession()->SendPacket(&data);
}

void Unit::StopMoving(bool forceSendStop )
{
    if (IsStopped() && !forceSendStop)
    {
        return;
    }

    clearUnitState(UNIT_STAT_MOVING);

    if (!IsInWorld())
    {
        return;
    }

    Movement::MoveSplineInit init(*this);
    init.Stop();
}

void Unit::InterruptMoving(bool forceSendStop )
{
    bool isMoving = false;

    if (!movespline->Finalized())
    {
        Movement::Location loc = movespline->ComputePosition();
        MovedTo(loc.x, loc.y, loc.z, loc.orientation);
        isMoving = true;
    }

    StopMoving(forceSendStop || isMoving);
}

void Unit::SetImmobilizedState(bool apply, bool stun)
{
    const uint32 immobilized = (UNIT_STAT_ROOT | UNIT_STAT_STUNNED);
    const uint32 state = stun ? UNIT_STAT_STUNNED : UNIT_STAT_ROOT;
    if (apply)
    {
        addUnitState(state);
        if (!IsPlayer(this))
        {
            StopMoving();
        }
        else
        {

            ((Player*)this)->m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
            if (stun)
            {
                SetStandState(UNIT_STAND_STATE_STAND);
            }
            SetRoot(true);
        }
    }
    else
    {
        clearUnitState(state);

        if (!hasUnitState(immobilized) && (IsPlayer(this)))
        {
            SetRoot(false);
        }
    }
}

void Unit::SetStunned(bool apply)
{
    SetIncapacitatedState(apply, UNIT_FLAG_STUNNED);
}

void Unit::SetIncapacitatedState(bool apply, uint32 state, ObjectGuid casterGuid, uint32 spellID, uint32 time)
{

    const uint32 filter = (UNIT_FLAG_STUNNED | UNIT_FLAG_CONFUSED | UNIT_FLAG_FLEEING);
    if (!state || !(state & filter) || (state & ~filter))
    {
        return;
    }

    Player* controller = GetCharmerOrOwnerPlayerOrPlayerItself();
    const bool control = controller ? controller->IsClientControl(this) : false;
    const bool movement = (state != UNIT_FLAG_STUNNED);
    const bool stun = (state & UNIT_FLAG_STUNNED);
    const bool fleeing = (state & UNIT_FLAG_FLEEING);

    if (apply)
    {
        if (fleeing && HasAuraType(SPELL_AURA_PREVENTS_FLEEING))
        {
            if (state == UNIT_FLAG_FLEEING)
            {
                return;
            }
            else
            {
                state &= ~UNIT_FLAG_FLEEING;
            }
        }
        SetUnitFlag(state);
    }
    else
    {
        RemoveUnitFlag(state);
    }

    if (movement)
    {
        GetMotionMaster()->MovementExpired(false);
    }
    if (apply)
    {
        CastStop(GetObjectGuid() == casterGuid ? spellID : 0);
    }

    if (IsCreature(this))
    {
        if (HasUnitFlag(filter))
        {
            if (!(GetTargetGuid() == 0))
            {
                SetTargetGuid(0);
            }
        }
        else if (IsAlive())
        {
            if (Unit* victim = getVictim())
            {
                SetTargetGuid(victim->GetObjectGuid());
                if (movement)
                {
                    GetMotionMaster()->MoveChase(victim);
                }
            }
            else if (movement)
            {
                GetMotionMaster()->Initialize();
            }

            if (!apply && fleeing)
            {

                if (Unit* caster = IsInWorld() ? GetMap()->GetUnit(casterGuid) : nullptr)
                {
                    ((Creature*)this)->AttackedBy(caster);
                }
            }
        }
    }

    if (stun)
    {
        SetImmobilizedState(apply, true);
    }

    if (!movement)
    {
        return;
    }

    if (controller)
    {
        const bool remove = !controller->IsClientControl(this);
        if (control && remove)
        {
            controller->SetClientControl(this, 0);
        }
        else if (!control && !remove)
        {
            controller->SetClientControl(this, 1);
        }
    }

    if (HasUnitFlag(UNIT_FLAG_CONFUSED))
    {
        GetMotionMaster()->MoveConfused();
    }
    else if (HasUnitFlag(UNIT_FLAG_FLEEING))
    {
        GetMotionMaster()->MoveFleeing(IsInWorld() ? GetMap()->GetUnit(casterGuid) : nullptr, time);
    }
}

bool Unit::IsSitState() const
{
    uint8 s = getStandState();
    return s == UNIT_STAND_STATE_SIT_CHAIR ||
        s == UNIT_STAND_STATE_SIT_LOW_CHAIR  ||
        s == UNIT_STAND_STATE_SIT_MEDIUM_CHAIR ||
        s == UNIT_STAND_STATE_SIT_HIGH_CHAIR ||
        s == UNIT_STAND_STATE_SIT;
}

bool Unit::IsStandState() const
{
    uint8 s = getStandState();
    return !IsSitState() && s != UNIT_STAND_STATE_SLEEP && s != UNIT_STAND_STATE_KNEEL;
}

bool Unit::IsSeatedState() const
{
    uint8 standState = getStandState();
    return standState != UNIT_STAND_STATE_SLEEP && standState != UNIT_STAND_STATE_STAND;
}

void Unit::SetStandState(uint8 state)
{
    SetByteValue(UNIT_FIELD_BYTES_1, 0, state);

    if (!IsSeatedState())
    {
        RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_NOT_SEATED);
    }

    if (IsPlayer(this))
    {
        WorldPacket data(SMSG_STANDSTATE_UPDATE, 1);
        data << (uint8)state;
        ((Player*)this)->GetSession()->SendPacket(&data);
    }
}

bool Unit::IsPolymorphed() const
{
    return GetSpellSpecific(GetTransform()) == SPELL_MAGE_POLYMORPH;
}

void Unit::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(UNIT_FIELD_DISPLAYID, modelId);

    UpdateModelData();

    if (IsCreature(this) && ((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (!pet->isControlled())
        {
            return;
        }
        Unit* owner = GetOwner();
        if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
        {
            ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MODEL_ID);
        }
    }
}

void Unit::UpdateModelData()
{
    if (CreatureModelInfo const* modelInfo = sObjectMgr.GetCreatureModelInfo(GetDisplayId()))
    {

        SetBoundingRadius(GetObjectScale() * modelInfo->bounding_radius);

        if (IsPlayer(this))
        {
            SetCombatReach(1.5f);
        }
        else
        {
            SetCombatReach(GetObjectScale() * modelInfo->combat_reach);
        }
    }
}

float Unit::GetObjectScaleMod() const
{
    int32 modValue = 0;
    const auto scaleAuraList = GetAurasByType(SPELL_AURA_MOD_SCALE);
    for (auto* aura : scaleAuraList)
    {
        modValue += aura->GetModifier()->m_amount;
    }

    float result = (100 + modValue) / 100.0f;

    if (result < 0.01f)
    {
        result = 0.01f;
    }
    else if (result > 100.0f)
    {
        result = 100.0f;
    }

    return result;
}

void Unit::ClearComboPointHolders()
{
    while (!m_ComboPointHolders.empty())
    {
        uint32 lowguid = *m_ComboPointHolders.begin();

        Player* plr = sObjectMgr.GetPlayer(MakeGuid(HIGHGUID_PLAYER, lowguid));
        if (plr && plr->GetComboTargetGuid() == GetObjectGuid())
        {
            plr->ClearComboPoints();
        }
        else
        {
            m_ComboPointHolders.erase(lowguid);
        }
    }
}

void Unit::ClearAllReactives()
{
    for (int i = 0; i < MAX_REACTIVE; ++i)
    {
        m_reactiveTimer[i] = 0;
    }

    if (HasAuraState(AURA_STATE_DEFENSE))
    {
        ModifyAuraState(AURA_STATE_DEFENSE, false);
    }
    if (getClass() == CLASS_HUNTER && HasAuraState(AURA_STATE_HUNTER_PARRY))
    {
        ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
    }

    if (getClass() == CLASS_WARRIOR && IsPlayer(this))
    {
        ((Player*)this)->ClearComboPoints();
    }
}

void Unit::UpdateReactives(uint32 p_time)
{
    for (int i = 0; i < MAX_REACTIVE; ++i)
    {
        ReactiveType reactive = ReactiveType(i);

        if (!m_reactiveTimer[reactive])
        {
            continue;
        }

        if (m_reactiveTimer[reactive] <= p_time)
        {
            m_reactiveTimer[reactive] = 0;

            switch (reactive)
            {
                case REACTIVE_DEFENSE:
                    if (HasAuraState(AURA_STATE_DEFENSE))
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, false);
                    }
                    break;
                case REACTIVE_HUNTER_PARRY:
                    if (getClass() == CLASS_HUNTER && HasAuraState(AURA_STATE_HUNTER_PARRY))
                    {
                        ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
                    }
                    break;
                case REACTIVE_OVERPOWER:
                    if (getClass() == CLASS_WARRIOR && IsPlayer(this))
                    {
                        ((Player*)this)->ClearComboPoints();
                    }
                    break;
                default:
                    break;
            }
        }
        else
        {
            m_reactiveTimer[reactive] -= p_time;
        }
    }
}

Unit* Unit::SelectRandomUnfriendlyTarget(Unit* except , float radius ) const
{
    std::list<Unit*> targets;

    MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck u_check(this, radius);
    MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(this, searcher, radius);

    if (except)
    {
        targets.remove(except);
    }

    for (std::list<Unit*>::iterator tIter = targets.begin(); tIter != targets.end();)
    {
        if (!HasLineOfSight(*this, *(*tIter)))
        {
            std::list<Unit*>::iterator tIter2 = tIter;
            ++tIter;
            targets.erase(tIter2);
        }
        else
        {
            ++tIter;
        }
    }

    if (targets.empty())
    {
        return nullptr;
    }

    uint32 rIdx = urand(0, targets.size() - 1);
    std::list<Unit*>::const_iterator tcIter = targets.begin();
    for (uint32 i = 0; i < rIdx; ++i)
    {
        ++tcIter;
    }

    return *tcIter;
}

Unit* Unit::SelectRandomFriendlyTarget(Unit* except , float radius ) const
{
    std::list<Unit*> targets;

    MaNGOS::AnyFriendlyUnitInObjectRangeCheck u_check(this, radius);
    MaNGOS::UnitListSearcher<MaNGOS::AnyFriendlyUnitInObjectRangeCheck> searcher(targets, u_check);

    Cell::VisitAllObjects(this, searcher, radius);

    if (except)
    {
        targets.remove(except);
    }

    for (std::list<Unit*>::iterator tIter = targets.begin(); tIter != targets.end();)
    {
        if (!HasLineOfSight(*this, *(*tIter)))
        {
            std::list<Unit*>::iterator tIter2 = tIter;
            ++tIter;
            targets.erase(tIter2);
        }
        else
        {
            ++tIter;
        }
    }

    if (targets.empty())
    {
        return nullptr;
    }

    uint32 rIdx = urand(0, targets.size() - 1);
    std::list<Unit*>::const_iterator tcIter = targets.begin();
    for (uint32 i = 0; i < rIdx; ++i)
    {
        ++tcIter;
    }

    return *tcIter;
}

Unit* Unit::FindLowestHpFriendlyUnit(float fRange, uint32 uiMinHPDiff, bool bPercent, Unit* except) const
{
    std::list<Unit*> targets;

    if (Unit* pVictim = getVictim())
    {
        HostileReference* pReference = pVictim->GetHostileRefManager().getFirst();

        while (pReference)
        {
            if (Unit* pTarget = pReference->getSourceUnit())
            {
                if (pTarget->IsAlive() && IsFriendly(*this, *pTarget) && InReach(*this, *pTarget, fRange) &&
                    ((bPercent && (100 - pTarget->GetHealthPercent() > uiMinHPDiff)) || (!bPercent && (pTarget->GetMaxHealth() - pTarget->GetHealth() > uiMinHPDiff))))
                {
                    targets.push_back(pTarget);
                }
            }
            pReference = pReference->next();
        }
    }
    else
    {
        MaNGOS::MostHPMissingInRangeCheck u_check(this, fRange, uiMinHPDiff, bPercent);
        MaNGOS::UnitListSearcher<MaNGOS::MostHPMissingInRangeCheck> searcher(targets, u_check);

        Cell::VisitAllObjects(this, searcher, fRange);
    }

    if (except)
    {
        targets.remove(except);
    }

    if (targets.empty())
    {
        return nullptr;
    }

    return *targets.begin();
}

Unit* Unit::FindFriendlyUnitMissingBuff(float range, uint32 spellid, Unit* except) const
{
    std::list<Unit*> targets;

    MaNGOS::FriendlyMissingBuffInRangeCheck u_check(this, range, spellid);
    MaNGOS::UnitListSearcher<MaNGOS::FriendlyMissingBuffInRangeCheck> searcher(targets, u_check);

    Cell::VisitGridObjects(this, searcher, range);

    if (except)
    {
        targets.remove(except);
    }

    if (targets.empty())
    {
        return nullptr;
    }

    return *targets.begin();
}

Unit* Unit::FindFriendlyUnitCC(float range) const
{
    Unit* pUnit = nullptr;

    MaNGOS::FriendlyCCedInRangeCheck u_check(this, range);
    MaNGOS::UnitSearcher<MaNGOS::FriendlyCCedInRangeCheck> searcher(pUnit, u_check);

    Cell::VisitGridObjects(this, searcher, range);

    return pUnit;
}

bool Unit::hasNegativeAuraWithInterruptFlag(uint32 flag)
{
    for (SpellAuraHolderMap::const_iterator iter = m_auras.All().begin(); iter != m_auras.All().end(); ++iter)
    {
        if (!iter->second->IsPositive() && iter->second->GetSpellProto()->AuraInterruptFlags & flag)
        {
            return true;
        }
    }
    return false;
}

void Unit::ApplyAttackTimePercentMod(WeaponAttackType att, float val, bool apply)
{
    if (val > 0)
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], val, !apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME + att, val, !apply);
    }
    else
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], -val, apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME + att, -val, apply);
    }
}

void Unit::ApplyCastTimePercentMod(float val, bool apply)
{
    if (val > 0)
    {
        ApplyCastSpeedMod(val, !apply);
    }
    else
    {
        ApplyCastSpeedMod(-val, apply);
    }
}

void Unit::UpdateAuraForGroup(uint8 slot)
{
    if (IsPlayer(this))
    {
        Player* player = (Player*)this;
        if (player->GetGroup())
        {
            player->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_AURAS);
            player->SetAuraUpdateMask(slot);
        }
    }
    else if (IsCreature(this) && ((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_AURAS);
                pet->SetAuraUpdateMask(slot);
            }
        }
    }
}

float Unit::GetAPMultiplier(WeaponAttackType attType, bool normalized)
{
    if (!normalized || !IsPlayer(this))
    {
        return float(GetAttackTime(attType)) / 1000.0f;
    }

    Item* Weapon = ((Player*)this)->GetWeaponForAttack(attType, true, false);
    if (!Weapon)
    {
        return 2.4f;
    }

    switch (Weapon->GetProto()->InventoryType)
    {
        case INVTYPE_2HWEAPON:
            return 3.3f;
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
            return 2.8f;
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
        default:
            return Weapon->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER ? 1.7f : 2.4f;
    }
}

Aura* Unit::GetDummyAura(uint32 spell_id) const
{
    const auto mDummy = GetAurasByType(SPELL_AURA_DUMMY);
    for (auto* aura : mDummy)
    {
        if (aura->GetId() == spell_id)
        {
            return aura;
        }
    }
    return nullptr;
}

void Unit::SetContestedPvP(Player* attackedPlayer)
{
    Player* player = GetCharmerOrOwnerPlayerOrPlayerItself();

    if (!player || (attackedPlayer && (attackedPlayer == player || player->Duelling().With(attackedPlayer))))
    {
        return;
    }

    player->SetContestedPvPTimer(30000);

    if (!player->hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        player->addUnitState(UNIT_STAT_ATTACK_PLAYER);
        player->SetPlayerFlag(PLAYER_FLAGS_CONTESTED_PVP);

        UpdateVisibilityAndView();
    }

    if (!hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        addUnitState(UNIT_STAT_ATTACK_PLAYER);

        UpdateVisibilityAndView();
    }
}

void Unit::AddPetAura(PetAura const* petSpell)
{
    m_auras.ForItsPet().insert(petSpell);
    if (Pet* pet = GetPet())
    {
        pet->CastPetAura(petSpell);
    }
}

void Unit::RemovePetAura(PetAura const* petSpell)
{
    m_auras.ForItsPet().erase(petSpell);
    if (Pet* pet = GetPet())
    {
        pet->RemoveAuras(petSpell->GetAura(pet->GetEntry()));
    }
}

void Unit::RemoveAurasAtMechanicImmunity(uint32 mechMask, uint32 exceptSpellId, bool non_positive )
{
    Unit::SpellAuraHolderMap& auras = GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::iterator iter = auras.begin(); iter != auras.end();)
    {
        SpellEntry const* spell = iter->second->GetSpellProto();
        if (spell->ID == exceptSpellId)
        {
            ++iter;
        }
        else if (non_positive && iter->second->IsPositive())
        {
            ++iter;
        }
        else if (cast::RecipeOf(*spell).Says().ignoresInvulnerability)
        {
            ++iter;
        }
        else if (iter->second->HasMechanicMask(mechMask))
        {
            RemoveAuras(spell->ID);

            if (auras.empty())
            {
                break;
            }
            else
            {
                iter = auras.begin();
            }
        }
        else
        {
            ++iter;
        }
    }
}

void Unit::NearTeleportTo(float x, float y, float z, float orientation, bool casting )
{
    DisableSpline();

    if (IsPlayer(this))
    {
        ((Player*)this)->TeleportTo(GetMapId(), x, y, z, orientation, TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET | (casting ? TELE_TO_SPELL : 0));
    }
    else
    {
        Creature* c = (Creature*)this;

        if (!c->GetMotionMaster()->empty())
        {
            if (MovementGenerator* movgen = c->GetMotionMaster()->top())
            {
                movgen->Interrupt(*c);
            }
        }

        GetMap()->CreatureRelocation((Creature*)this, x, y, z, orientation);

        SendHeartBeat();

        if (!c->GetMotionMaster()->empty())
        {
            if (MovementGenerator* movgen = c->GetMotionMaster()->top())
            {
                movgen->Reset(*c);
            }
        }
    }
}

void Unit::MonsterMoveWithSpeed(float x, float y, float z, float speed, bool generatePath, bool forceDestination)
{
    Movement::MoveSplineInit init(*this);
    init.MoveTo(x, y, z, generatePath, forceDestination, 30.f);
    init.SetVelocity(speed);
    init.Launch();
}

struct SetPvPHelper
{
    explicit SetPvPHelper(bool _state) : state(_state) {}
    void operator()(Unit* unit) const { unit->SetPvP(state); }
    bool state;
};

void Unit::SetPvP(bool state)
{
    if (state)
    {
        SetUnitFlag(UNIT_FLAG_PVP);
    }
    else
    {
        RemoveUnitFlag(UNIT_FLAG_PVP);
    }

    CallForAllControlledUnits(SetPvPHelper(state), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

struct StopAttackFactionHelper
{
    explicit StopAttackFactionHelper(uint32 _faction_id) : faction_id(_faction_id) {}
    void operator()(Unit* unit) const { unit->StopAttackFaction(faction_id); }
    uint32 faction_id;
};

void Unit::StopAttackFaction(uint32 faction_id)
{
    if (Unit* victim = getVictim())
    {
        if (victim->getFactionTemplateEntry()->Faction == faction_id)
        {
            AttackStop();
            if (IsNonMeleeSpellCasted(false))
            {
                InterruptNonMeleeSpells(false);
            }

            if (IsPlayer(this))
            {
                ((Player*)this)->SendAttackSwingCancelAttack();
            }
        }
    }

    AttackerSet const& attackers = getAttackers();
    for (AttackerSet::const_iterator itr = attackers.begin(); itr != attackers.end();)
    {
        if ((*itr)->getFactionTemplateEntry()->Faction == faction_id)
        {
            (*itr)->AttackStop();
            itr = attackers.begin();
        }
        else
        {
            ++itr;
        }
    }

    GetHostileRefManager().deleteReferencesForFaction(faction_id);

    CallForAllControlledUnits(StopAttackFactionHelper(faction_id), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

void Unit::CleanupDeletedAuras()
{
    m_auras.SweepDeferred();
}

bool Unit::CheckAndIncreaseCastCounter()
{
    uint32 maxCasts = sWorld.getConfig(CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN);

    if (maxCasts && m_castCounter >= maxCasts)
    {
        return false;
    }

    ++m_castCounter;
    return true;
}

SpellAuraHolder* Unit::GetSpellAuraHolder(uint32 spellid) const
{
    SpellAuraHolderMap::const_iterator itr = m_auras.All().find(spellid);
    return itr != m_auras.All().end() ? itr->second : nullptr;
}

SpellAuraHolder* Unit::GetSpellAuraHolder(uint32 spellid, ObjectGuid casterGuid) const
{
    SpellAuraHolderConstBounds bounds = GetSpellAuraHolderBounds(spellid);
    for (SpellAuraHolderMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        if (iter->second->GetCasterGuid() == casterGuid)
        {
            return iter->second;
        }
    }
    return nullptr;
}

class RelocationNotifyEvent : public BasicEvent
{
    public:
        RelocationNotifyEvent(Unit& owner) : BasicEvent(), m_owner(owner)
        {
            m_owner._SetAINotifyScheduled(true);
        }

        bool Execute(uint64 , uint32 )
        {
            float radius = MAX_CREATURE_ATTACK_RADIUS * sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
            if (IsPlayer(&m_owner))
            {
                MaNGOS::PlayerRelocationNotifier notify((Player&)m_owner);
                Cell::VisitAllObjects(&m_owner, notify, radius);
            }
            else
            {
                MaNGOS::CreatureRelocationNotifier notify((Creature&)m_owner);
                Cell::VisitAllObjects(&m_owner, notify, radius);
            }
            m_owner._SetAINotifyScheduled(false);
            return true;
        }

        void Abort(uint64)
        {
            m_owner._SetAINotifyScheduled(false);
        }

    private:
        Unit& m_owner;
};

void Unit::ScheduleAINotify(uint32 delay)
{
    if (!IsAINotifyScheduled())
    {
        m_Events.AddEvent(new RelocationNotifyEvent(*this), m_Events.CalculateTime(delay));
    }
}

void Unit::OnRelocated()
{

    float dx = m_last_notified_position.x - Where().X();
    float dy = m_last_notified_position.y - Where().Y();
    float dz = m_last_notified_position.z - Where().Z();
    float distsq = dx * dx + dy * dy + dz * dz;
    if (distsq > World::GetRelocationLowerLimitSq())
    {
        m_last_notified_position.x = Where().X();
        m_last_notified_position.y = Where().Y();
        m_last_notified_position.z = Where().Z();

        GetViewPoint().Call_UpdateVisibilityForOwner();
        UpdateObjectVisibility();
    }
    ScheduleAINotify(World::GetRelocationAINotifyDelay());
}

void Unit::UpdateSplineMovement(uint32 t_diff)
{
    enum
    {
        POSITION_UPDATE_DELAY = 400,
    };

    if (movespline->Finalized())
    {

        if (m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ENABLED))
        {
            DisableSpline();
        }

        return;
    }

    movespline->updateState(t_diff);
    bool arrived = movespline->Finalized();

    if (arrived)
    {
        DisableSpline();
    }

    m_movesplineTimer.Update(t_diff);
    if (m_movesplineTimer.Passed() || arrived)
    {
        m_movesplineTimer.Reset(POSITION_UPDATE_DELAY);
        Movement::Location loc = movespline->ComputePosition();

        MovedTo(loc.x, loc.y, loc.z, loc.orientation);
    }
}

void Unit::DisableSpline()
{
    m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD));
    movespline->_Interrupt();
}

void Unit::SetFeared(bool apply, ObjectGuid casterGuid, uint32 spellID, uint32 time)
{
    SetIncapacitatedState(apply, UNIT_FLAG_FLEEING, casterGuid, spellID, time);
}

void Unit::SetConfused(bool apply, ObjectGuid casterGuid, uint32 spellID)
{
    SetIncapacitatedState(apply, UNIT_FLAG_CONFUSED, casterGuid, spellID);
}

void Unit::SetFeignDeath(bool apply, ObjectGuid casterGuid )
{
    if (apply)
    {
        if (!IsPlayer(this))
        {
            StopMoving();
        }
        else
        {
            ((Player*)this)->m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
        }

        SetUnitFlag(UNIT_FLAG_UNK_29);

        SetDynFlag(UNIT_DYNFLAG_DEAD);

        addUnitState(UNIT_STAT_DIED);
        CombatStop();
        RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        if (casterGuid == GetObjectGuid())
        {
            FinishSpell(CURRENT_GENERIC_SPELL, false);
        }
        InterruptNonMeleeSpells(true);
        GetHostileRefManager().deleteReferences();
    }
    else
    {

        RemoveUnitFlag(UNIT_FLAG_UNK_29);

        RemoveDynFlag(UNIT_DYNFLAG_DEAD);

        clearUnitState(UNIT_STAT_DIED);

        if (!IsPlayer(this) && IsAlive())
        {

            if (getVictim())
            {
                GetMotionMaster()->MoveChase(getVictim());
            }
            else
            {
                GetMotionMaster()->Initialize();
            }
        }
    }
}
