#include "Cast/Recipe/Recipe.h"

#include "DataStore/DBCStructure.h"
#include "DataStore/SpellMgr.h"

#include <cmath>

namespace cast
{
    namespace
    {
        /// Column 6..10 of Spell.dbc, by the bit numbers the measurements used.
        constexpr uint32 Bit(uint32 index)
        {
            return 1u << index;
        }

        bool Has(uint32 word, uint32 index)
        {
            return (word & Bit(index)) != 0;
        }

        Defence DefenceFrom(uint32 stored)
        {
            switch (stored)
            {
                case 1:  return Defence::Magic;
                case 2:  return Defence::Melee;
                case 3:  return Defence::Ranged;
                default: return Defence::None;
            }
        }

        /// Passive first, because such a spell is never cast; then the two ways a
        /// weapon carries a spell, because the weapon is what delivers it; then
        /// the channel, which owns its own clock; and a cast time last, which is
        /// the only thing left that can tell Timed from Instant.
        Start StartFrom(const Flags& flags, int32 castTimeMs)
        {
            if (flags.passive)
            {
                return Start::Passive;
            }

            const bool nextSwing = Has(flags.words[0], 2) || Has(flags.words[0], 10);
            if (nextSwing)
            {
                return Start::NextSwing;
            }

            // One bit is enough: all four rows that carry it also carry the ranged
            // bit, and no row carries it alone. Two of the four are Auto Shot and
            // the wand's Shoot; the other two are a retired row and a test row.
            if (flags.autoRepeats)
            {
                return Start::AutoRepeat;
            }

            if (flags.channels)
            {
                return Start::Channelled;
            }

            return castTimeMs > 0 ? Start::Timed : Start::Instant;
        }

        Flags FlagsFrom(const SpellEntry& row)
        {
            Flags flags;
            flags.words[0] = row.Attributes;
            flags.words[1] = row.AttributesEx;
            flags.words[2] = row.AttributesExB;
            flags.words[3] = row.AttributesExC;
            flags.words[4] = row.AttributesExD;

            const uint32 a = flags.words[0];
            flags.ranged = Has(a, 1);
            flags.ability = Has(a, 4);
            flags.tradeskill = Has(a, 5);
            flags.passive = Has(a, 6);
            flags.hiddenFromClient = Has(a, 7);
            flags.notWhileShapeshifted = Has(a, 16);
            flags.onlyWhileStealthed = Has(a, 17);
            flags.damageScalesWithLevel = Has(a, 19);
            flags.stopsAttack = Has(a, 20);
            flags.cannotBeAvoided = Has(a, 21);
            flags.castableWhileDead = Has(a, 23);
            flags.castableWhileMounted = Has(a, 24);
            flags.spentWhileActive = Has(a, 25);
            flags.showsAsDebuff = Has(a, 26);
            flags.castableWhileSitting = Has(a, 27);
            flags.notInCombat = Has(a, 28);
            flags.ignoresInvulnerability = Has(a, 29);
            flags.fadesOnHeartbeat = Has(a, 30);
            flags.cannotBeCancelled = Has(a, 31);

            const uint32 b = flags.words[1];
            flags.drainsAllPower = Has(b, 1);
            flags.cannotBeRedirected = Has(b, 3);
            flags.doesNotBreakStealth = Has(b, 5);
            flags.cannotBeReflected = Has(b, 7);
            flags.needsTargetOutOfCombat = Has(b, 8);
            flags.needsFacing = Has(b, 9);
            flags.makesNoThreat = Has(b, 10);
            flags.channels = Has(b, 2) || Has(b, 6);
            flags.channelTracksTarget = Has(b, 14);
            flags.ignoresSchoolImmunity = Has(b, 16);
            flags.cannotTargetSelf = Has(b, 19);
            flags.needsComboPointsOnTarget = Has(b, 20);
            flags.spendsComboPoints = Has(b, 22);
            flags.refundsPowerWhenTurned = Has(b, 27);
            flags.hiddenFromAuraBar = Has(b, 28);
            flags.armedByDodge = Has(b, 30);

            const uint32 c = flags.words[2];
            flags.canTargetDead = Has(c, 0);
            flags.ignoresLineOfSight = Has(c, 2);
            flags.autoRepeats = Has(c, 5);
            flags.keepsWeaponTimer = Has(c, 17);
            flags.shieldReducesDamage = Has(c, 21);
            flags.cannotCrit = Has(c, 29);
            flags.triggeredCanProc = Has(c, 30);
            flags.isFoodOrDrink = Has(c, 31);

            const uint32 d = flags.words[3];
            flags.blockable = Has(d, 3);
            flags.stacksPerCaster = Has(d, 7);
            flags.playersOnly = Has(d, 8);
            flags.needsMainHand = Has(d, 10);
            flags.isRangedAttack = Has(d, 15);
            flags.cannotProc = Has(d, 16);
            flags.makesNoInitialThreat = Has(d, 17);
            flags.cannotMiss = Has(d, 18);
            flags.survivesDeath = Has(d, 20);
            flags.needsWand = Has(d, 22);
            flags.needsOffHand = Has(d, 24);
            flags.takesNoDoneBonus = Has(d, 29);

            return flags;
        }

        Operations OperationsFrom(const SpellEntry& row)
        {
            Operations operations;

            for (uint8 slot = 0; slot < MAX_EFFECT_INDEX; ++slot)
            {
                if (row.Effect[slot] == 0)
                {
                    continue;
                }

                Operation operation;
                operation.verb = row.Effect[slot];
                operation.aura = row.EffectAura[slot];
                operation.mechanic = row.EffectMechanic[slot];
                operation.slot = slot;

                operation.basePoints = row.EffectBasePoints[slot];
                operation.dieSides = row.EffectDieSides[slot];
                operation.baseDice = row.EffectBaseDice[slot];
                operation.dicePerLevel = row.EffectDicePerLevel[slot];
                operation.pointsPerLevel = row.EffectRealPointsPerLevel[slot];
                operation.pointsPerCombo = row.EffectPointsPerCombo[slot];

                operation.targetA = row.ImplicitTargetA[slot];
                operation.targetB = row.ImplicitTargetB[slot];
                operation.radiusIndex = row.EffectRadiusIndex[slot];
                operation.chainTargets = row.EffectChainTargets[slot];

                operation.periodMs = row.EffectAuraPeriod[slot];
                operation.reachesArea =
                    IsAreaEffectTarget(Targets(row.ImplicitTargetA[slot])) ||
                    IsAreaEffectTarget(Targets(row.ImplicitTargetB[slot]));
                operation.amplitude = row.EffectAmplitude[slot];
                operation.itemType = row.EffectItemType[slot];
                operation.miscValue = row.EffectMiscValue[slot];
                operation.triggerSpell = row.EffectTriggerSpell[slot];

                operations.Add(operation);
            }

            return operations;
        }

        /// A duration as everything downstream wants it: no end stays no end,
        /// and the two rows that store a negative span mean the span.
        int32 DurationFrom(const Timings& timings)
        {
            return timings.durationMs == -1 ? -1 : std::abs(timings.durationMs);
        }

        /// The cast time a question without a caster gets. The half second is
        /// what a ranged spell spends being drawn; it is added before the floor,
        /// because one cast-time row holds -1000000 and clamping first would turn
        /// that into half a second of draw.
        uint32 BareCastTimeFrom(const SpellEntry& row, const Flags& flags, const Timings& timings)
        {
            int32 castTime = timings.castTimeBaseMs;

            if (flags.ranged)
            {
                castTime += 500;
            }

            // Holy Light carries 2.5 seconds in the table and is instant in the
            // game. The row is wrong and cannot be fixed where it lives.
            if (row.ID == 19968)
            {
                castTime = 0;
            }

            return castTime > 0 ? static_cast<uint32>(castTime) : 0;
        }

        uint16 MaxTicksFrom(const Operations& operations, int32 duration)
        {
            if (duration == 0)
            {
                return 1;
            }

            int32 span = duration > 30000 ? 30000 : duration;

            for (const auto& operation : operations)
            {
                if (operation.verb != SPELL_EFFECT_APPLY_AURA)
                {
                    continue;
                }

                if (operation.aura != SPELL_AURA_PERIODIC_DAMAGE &&
                    operation.aura != SPELL_AURA_PERIODIC_HEAL &&
                    operation.aura != SPELL_AURA_PERIODIC_LEECH)
                {
                    continue;
                }

                return operation.periodMs != 0
                       ? static_cast<uint16>(span / static_cast<int32>(operation.periodMs))
                       : 6;
            }

            return 6;
        }

        /// What the spell-power share is measured against: a cast time stretched
        /// or shrunk by what the spell actually does.
        uint32 WeighedCastTime(const Operations& operations, bool channelled, bool overTime,
                               uint32 bareCastTime, int32 duration)
        {
            uint32 weighed = channelled ? static_cast<uint32>(duration) : bareCastTime;

            weighed = weighed > 7000 ? 7000 : weighed;
            weighed = weighed < 1500 ? 1500 : weighed;

            if (overTime && !channelled)
            {
                weighed = 3500;
            }

            int32 spread = 0;
            uint8 extras = 0;
            bool direct = false;
            bool area = false;
            bool leech = false;

            for (const auto& operation : operations)
            {
                if (operation.reachesArea)
                {
                    area = true;
                }

                switch (operation.verb)
                {
                    case SPELL_EFFECT_SCHOOL_DAMAGE:
                    case SPELL_EFFECT_POWER_DRAIN:
                    case SPELL_EFFECT_HEALTH_LEECH:
                    case SPELL_EFFECT_ENVIRONMENTAL_DAMAGE:
                    case SPELL_EFFECT_POWER_BURN:
                    case SPELL_EFFECT_HEAL:
                        direct = true;
                        break;

                    case SPELL_EFFECT_APPLY_AURA:
                        switch (operation.aura)
                        {
                            case SPELL_AURA_PERIODIC_DAMAGE:
                            case SPELL_AURA_PERIODIC_HEAL:
                            case SPELL_AURA_PERIODIC_LEECH:
                                if (duration != 0)
                                {
                                    spread = duration;
                                }
                                break;

                            case SPELL_AURA_DUMMY:
                            case SPELL_AURA_MOD_DECREASE_SPEED:
                                ++extras;
                                break;

                            case SPELL_AURA_MOD_CONFUSE:
                            case SPELL_AURA_MOD_STUN:
                            case SPELL_AURA_MOD_ROOT:
                                extras += 2;
                                break;

                            default:
                                break;
                        }
                        break;

                    default:
                        break;
                }

                if (operation.verb == SPELL_EFFECT_HEALTH_LEECH ||
                    (operation.verb == SPELL_EFFECT_APPLY_AURA &&
                     operation.aura == SPELL_AURA_PERIODIC_LEECH))
                {
                    leech = true;
                }
            }

            // A spell that both strikes now and burns on shares its power between
            // the two, in proportion to how long each half lasts.
            if (spread > 0 && weighed > 0 && direct)
            {
                uint32 original = bareCastTime;
                original = original > 7000 ? 7000 : original;
                original = original < 1500 ? 1500 : original;

                const float toSpread = (spread / 15000.0f) /
                                       ((spread / 15000.0f) + (original / 3500.0f));

                if (overTime)
                {
                    weighed = static_cast<uint32>(weighed * toSpread);
                }
                else if (toSpread < 1.0f)
                {
                    weighed = static_cast<uint32>(weighed * (1 - toSpread));
                }
                else
                {
                    weighed = 0;
                }
            }

            if (area)
            {
                weighed /= 2;
            }

            if (leech)
            {
                weighed /= 2;
            }

            for (uint8 i = 0; i < extras; ++i)
            {
                weighed = static_cast<uint32>(weighed * 0.95f);
            }

            return weighed;
        }

        float CoefficientFrom(const Operations& operations, bool channelled, bool overTime,
                              uint32 bareCastTime, int32 duration, uint16 maxTicks)
        {
            float spread = 1.0f;
            if (overTime)
            {
                if (!channelled)
                {
                    spread = duration / 15000.0f;
                }

                if (maxTicks != 0)
                {
                    spread /= maxTicks;
                }
            }

            const float share = WeighedCastTime(operations, channelled, overTime,
                                                bareCastTime, duration) / 3500.0f;

            return share * spread;
        }
    }

    Recipe Recipe::Compile(const SpellEntry& row, const Timings& timings)
    {
        Recipe recipe;

        recipe.m_id = row.ID;
        recipe.m_school = combat::SchoolFromIndex(row.School);
        recipe.m_defence = DefenceFrom(row.DefenseType);
        recipe.m_flags = FlagsFrom(row);
        recipe.m_start = StartFrom(recipe.m_flags, timings.castTimeBaseMs);
        recipe.m_operations = OperationsFrom(row);
        recipe.m_timings = timings;

        recipe.m_durationMs = DurationFrom(timings);
        recipe.m_bareCastTimeMs = BareCastTimeFrom(row, recipe.m_flags, timings);
        recipe.m_maxTicks = MaxTicksFrom(recipe.m_operations, recipe.m_durationMs);

        // The raw pair, not Start: a row marked both passive and channelled still
        // weighed its power as a channel, and two rows are marked exactly that way.
        const bool channelled = recipe.m_flags.channels;
        recipe.m_coefficient[0] = CoefficientFrom(recipe.m_operations, channelled, false,
                                                  recipe.m_bareCastTimeMs, recipe.m_durationMs,
                                                  recipe.m_maxTicks);
        recipe.m_coefficient[1] = CoefficientFrom(recipe.m_operations, channelled, true,
                                                  recipe.m_bareCastTimeMs, recipe.m_durationMs,
                                                  recipe.m_maxTicks);

        recipe.m_dispel = row.DispelType;
        recipe.m_mechanic = row.Mechanic;
        recipe.m_powerKind = row.PowerType;
        recipe.m_cost = row.ManaCost;
        recipe.m_costPerLevel = row.ManaCostPerLevel;
        recipe.m_costPerSecond = row.ManaPerSecond;
        recipe.m_costPercent = row.ManaCostPct;

        recipe.m_cooldownMs = row.RecoveryTime;
        recipe.m_categoryCooldownMs = row.CategoryRecoveryTime;
        recipe.m_category = row.Category;
        recipe.m_globalCooldownMs = row.StartRecoveryTime;
        recipe.m_globalCooldownCategory = row.StartRecoveryCategory;

        recipe.m_interrupt = row.InterruptFlags;
        recipe.m_auraInterrupt = row.AuraInterruptFlags;
        recipe.m_channelInterrupt = row.ChannelInterruptFlags;

        recipe.m_procOn = row.ProcFlags;
        recipe.m_procChance = row.ProcChance;
        recipe.m_procCharges = row.ProcCharges;

        recipe.m_maxStack = row.CumulativeAura;
        recipe.m_maxTargets = row.MaxTargets;
        recipe.m_maxTargetLevel = row.MaxTargetLevel;
        recipe.m_casterLevel = row.SpellLevel;
        recipe.m_baseLevel = row.BaseLevel;
        recipe.m_maxLevel = row.MaxLevel;

        recipe.m_classSet = row.SpellClassSet;
        recipe.m_classMask = row.SpellClassMask.Flags;
        recipe.m_prevention = row.PreventionType;
        recipe.m_missileSpeed = row.Speed;

        recipe.m_shapeshiftMask = row.ShapeshiftMask;
        recipe.m_shapeshiftExcluded = row.ShapeshiftExclude;
        recipe.m_focusObject = row.RequiresSpellFocus;
        recipe.m_casterState = row.CasterAuraState;
        recipe.m_targetState = row.TargetAuraState;
        recipe.m_targetCreatureTypes = row.TargetCreatureType;

        return recipe;
    }

    bool Recipe::Ticks() const
    {
        for (const auto& operation : m_operations)
        {
            if (operation.Ticks())
            {
                return true;
            }
        }
        return false;
    }

    bool Recipe::AppliesAura() const
    {
        for (const auto& operation : m_operations)
        {
            if (operation.AppliesAura())
            {
                return true;
            }
        }
        return false;
    }
}
