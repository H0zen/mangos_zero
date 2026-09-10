#pragma once

#include "Combat/School.h"
#include "DataStore/SharedDefines.h"
#include "Platform/Define.h"

#include <cstddef>

struct SpellEntry;
struct SpellProcEventEntry;
struct SpellBonusEntry;
struct SpellThreatEntry;

namespace cast
{

    enum class Start : uint8
    {
        Passive,
        Instant,
        Timed,
        Channelled,
        NextSwing,
        AutoRepeat,
    };

    enum class Defence : uint8
    {
        None,
        Magic,
        Melee,
        Ranged,
    };

    struct Operation
    {
        uint32 verb = 0;
        uint32 aura = 0;
        uint32 mechanic = 0;
        uint8 slot = 0;

        int32 basePoints = 0;
        int32 dieSides = 0;
        uint32 baseDice = 0;
        float dicePerLevel = 0.0f;
        float pointsPerLevel = 0.0f;
        float pointsPerCombo = 0.0f;

        uint32 targetA = 0;
        uint32 targetB = 0;
        uint32 radiusIndex = 0;
        uint32 chainTargets = 0;

        uint32 periodMs = 0;
        bool reachesArea = false;
        float amplitude = 0.0f;
        uint32 itemType = 0;
        int32 miscValue = 0;
        uint32 triggerSpell = 0;

        float chainAmplitude = 0.0f;
        bool positive = false;

        bool AppliesAura() const { return aura != 0; }
        bool Ticks() const { return periodMs != 0; }
    };

    class Operations
    {
        public:

            static constexpr size_t CAPACITY = 3;

            void Add(const Operation& operation)
            {
                if (m_count < CAPACITY)
                {
                    m_items[m_count++] = operation;
                }
            }

            size_t size() const { return m_count; }
            bool empty() const { return m_count == 0; }

            const Operation& operator[](size_t i) const { return m_items[i]; }

            const Operation* begin() const { return m_items; }
            const Operation* end() const { return m_items + m_count; }

            const Operation& At(uint8 slot) const
            {
                static const Operation nothing;
                const Operation* operation = AtSlot(slot);
                return operation != nullptr ? *operation : nothing;
            }

            const Operation* AtSlot(uint8 slot) const
            {
                for (const auto& operation : *this)
                {
                    if (operation.slot == slot)
                    {
                        return &operation;
                    }
                }
                return nullptr;
            }

        private:

            friend class RecipeBook;

            Operation& Mutable(size_t i) { return m_items[i]; }

            Operation m_items[CAPACITY];
            uint8 m_count = 0;
    };

    struct Flags
    {
        uint32 words[5] = {};

        bool ranged = false;
        bool ability = false;
        bool tradeskill = false;
        bool passive = false;
        bool hiddenFromClient = false;
        bool notWhileShapeshifted = false;
        bool indoorsOnly = false;
        bool outdoorsOnly = false;
        bool onlyWhileStealthed = false;
        bool damageScalesWithLevel = false;
        bool stopsAttack = false;
        bool cannotBeAvoided = false;
        bool castableWhileDead = false;
        bool castableWhileMounted = false;
        bool spentWhileActive = false;
        bool showsAsDebuff = false;
        bool castableWhileSitting = false;
        bool notInCombat = false;
        bool ignoresInvulnerability = false;
        bool fadesOnHeartbeat = false;
        bool cannotBeCancelled = false;

        bool drainsAllPower = false;
        bool cannotBeRedirected = false;
        bool doesNotBreakStealth = false;
        bool cannotBeReflected = false;
        bool channels = false;
        bool needsTargetOutOfCombat = false;
        bool needsFacing = false;
        bool makesNoThreat = false;
        bool farsight = false;
        bool channelTracksTarget = false;
        bool dispelsOnImmunity = false;
        bool ignoresSchoolImmunity = false;
        bool cannotTargetSelf = false;
        bool needsComboPointsOnTarget = false;
        bool spendsComboPoints = false;
        bool refundsPowerWhenTurned = false;
        bool hiddenFromAuraBar = false;
        bool armedByDodge = false;

        bool canTargetDead = false;
        bool ignoresLineOfSight = false;
        bool autoRepeats = false;
        bool keepsWeaponTimer = false;
        bool worksWithoutShapeshift = false;
        bool shieldReducesDamage = false;
        bool cannotCrit = false;
        bool triggeredCanProc = false;
        bool isFoodOrDrink = false;

        bool ignoresResurrectionTimer = false;
        bool blockable = false;
        bool castOnDead = false;
        bool stacksPerCaster = false;
        bool playersOnly = false;
        bool needsMainHand = false;
        bool isRangedAttack = false;
        bool cannotProc = false;
        bool makesNoInitialThreat = false;
        bool cannotMiss = false;
        bool survivesDeath = false;
        bool needsWand = false;
        bool needsOffHand = false;
        bool takesNoDoneBonus = false;

        bool survivesIncapacity = false;
    };

    struct Timings
    {

        int32 castTimeBaseMs = 0;
        int32 durationMs = 0;
        int32 durationPerLevelMs = 0;
        int32 maxDurationMs = 0;
        float rangeMin = 0.0f;
        float rangeMax = 0.0f;
    };

    struct Announcement
    {
        uint32 byCaster = 0;
        uint32 byTarget = 0;
    };

    class Recipe
    {
        public:

            static Recipe Compile(const SpellEntry& row, const Timings& timings);

            uint32 Id() const { return m_id; }
            Start Starts() const { return m_start; }
            Defence Beats() const { return m_defence; }
            combat::School OfSchool() const { return m_school; }

            const Operations& Does() const { return m_operations; }
            const Flags& Says() const { return m_flags; }
            const Timings& Takes() const { return m_timings; }

            uint32 BareCastTimeMs() const { return m_bareCastTimeMs; }

            int32 DurationMs() const { return m_durationMs; }

            uint16 MaxTicks() const { return m_maxTicks; }

            float Coefficient(bool overTime) const { return m_coefficient[overTime ? 1 : 0]; }

            uint32 DispelKind() const { return m_dispel; }
            uint32 Mechanic() const { return m_mechanic; }
            uint32 PowerKind() const { return m_powerKind; }
            uint32 Cost() const { return m_cost; }
            uint32 CostPerLevel() const { return m_costPerLevel; }
            uint32 CostPerSecond() const { return m_costPerSecond; }
            uint32 CostPercent() const { return m_costPercent; }

            uint32 CooldownMs() const { return m_cooldownMs; }
            uint32 CategoryCooldownMs() const { return m_categoryCooldownMs; }
            uint32 Category() const { return m_category; }
            uint32 GlobalCooldownMs() const { return m_globalCooldownMs; }
            uint32 GlobalCooldownCategory() const { return m_globalCooldownCategory; }

            uint32 InterruptedBy() const { return m_interrupt; }
            uint32 AuraInterruptedBy() const { return m_auraInterrupt; }
            uint32 ChannelInterruptedBy() const { return m_channelInterrupt; }

            uint32 ProcOn() const { return m_procOn; }
            uint32 ProcChance() const { return m_procChance; }
            uint32 ProcCharges() const { return m_procCharges; }

            uint32 MaxStack() const { return m_maxStack; }
            uint32 MaxTargets() const { return m_maxTargets; }
            uint32 MaxTargetLevel() const { return m_maxTargetLevel; }
            uint32 CasterLevel() const { return m_casterLevel; }
            uint32 BaseLevel() const { return m_baseLevel; }
            uint32 MaxLevel() const { return m_maxLevel; }

            uint32 ClassSet() const { return m_classSet; }
            uint64 ClassMask() const { return m_classMask; }
            uint32 PreventionKind() const { return m_prevention; }
            float MissileSpeed() const { return m_missileSpeed; }

            uint32 ShapeshiftMask() const { return m_shapeshiftMask; }
            uint32 ShapeshiftExcluded() const { return m_shapeshiftExcluded; }
            uint32 NeedsFocusObject() const { return m_focusObject; }
            uint32 NeedsCasterState() const { return m_casterState; }
            uint32 NeedsTargetState() const { return m_targetState; }
            uint32 TargetCreatureTypes() const { return m_targetCreatureTypes; }

            bool TouchesNoDefence() const { return m_defence == Defence::None; }

            WeaponAttackType Swings() const { return m_swings; }

            const Announcement& Announces() const { return m_announces; }

            uint8 UnwantedSlots() const { return m_unwantedSlots; }

            bool ProcsThoughTriggered() const { return m_procsThoughTriggered; }

            bool IsPositive() const { return m_positive; }

            const SpellProcEventEntry* ProcRule() const { return m_procRule; }
            const SpellBonusEntry* Bonus() const { return m_bonus; }
            const SpellThreatEntry* Threat() const { return m_threat; }

            float ThreatMultiplier() const { return m_threatMultiplier; }

            DiminishingGroup Diminishes(bool triggered) const
            {
                return m_diminishing[triggered ? 1 : 0];
            }

            const Operation& At(uint8 slot) const { return m_operations.At(slot); }

            bool IsPositiveAt(uint8 slot) const
            {
                const Operation* operation = m_operations.AtSlot(slot);
                return operation != nullptr && operation->positive;
            }

            bool Ticks() const;

            bool AppliesAura() const;

        private:

            friend class RecipeBook;

            uint32 m_id = 0;
            bool m_positive = false;
            DiminishingGroup m_diminishing[2] = {DIMINISHING_NONE, DIMINISHING_NONE};

            const SpellProcEventEntry* m_procRule = nullptr;
            const SpellBonusEntry* m_bonus = nullptr;
            const SpellThreatEntry* m_threat = nullptr;
            float m_threatMultiplier = 1.0f;
            Start m_start = Start::Instant;
            Defence m_defence = Defence::None;
            WeaponAttackType m_swings = BASE_ATTACK;
            Announcement m_announces;
            uint8 m_unwantedSlots = 0;
            bool m_procsThoughTriggered = false;
            combat::School m_school = combat::School::Physical;

            Operations m_operations;
            Flags m_flags;
            Timings m_timings;

            uint32 m_bareCastTimeMs = 0;
            int32 m_durationMs = 0;
            uint16 m_maxTicks = 0;
            float m_coefficient[2] = {};

            uint32 m_dispel = 0;
            uint32 m_mechanic = 0;
            uint32 m_powerKind = 0;
            uint32 m_cost = 0;
            uint32 m_costPerLevel = 0;
            uint32 m_costPerSecond = 0;
            uint32 m_costPercent = 0;

            uint32 m_cooldownMs = 0;
            uint32 m_categoryCooldownMs = 0;
            uint32 m_category = 0;
            uint32 m_globalCooldownMs = 0;
            uint32 m_globalCooldownCategory = 0;

            uint32 m_interrupt = 0;
            uint32 m_auraInterrupt = 0;
            uint32 m_channelInterrupt = 0;

            uint32 m_procOn = 0;
            uint32 m_procChance = 0;
            uint32 m_procCharges = 0;

            uint32 m_maxStack = 0;
            uint32 m_maxTargets = 0;
            uint32 m_maxTargetLevel = 0;
            uint32 m_casterLevel = 0;
            uint32 m_baseLevel = 0;
            uint32 m_maxLevel = 0;

            uint32 m_classSet = 0;
            uint64 m_classMask = 0;
            uint32 m_prevention = 0;
            float m_missileSpeed = 0.0f;

            uint32 m_shapeshiftMask = 0;
            uint32 m_shapeshiftExcluded = 0;
            uint32 m_focusObject = 0;
            uint32 m_casterState = 0;
            uint32 m_targetState = 0;
            uint32 m_targetCreatureTypes = 0;
    };
}
