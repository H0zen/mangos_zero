#pragma once

// What the server knows about one spell, worked out once and never revisited.
//
// A row of Spell.dbc is a storage format: 173 columns, five words of flags, three
// slots for effects whether or not they are used. Reading it at cast time is what
// makes a cast expensive and a spell system unreadable, because every question --
// is this passive, does it channel, which slot holds the aura -- gets asked again
// on every cast, from wherever the asking happens to be convenient.
//
// So the row is compiled once, at load, into this. After that the row is never
// read again. The measurements that shaped it, over the 22357 rows of 1.12.1:
//
//   - 18525 spells carry ONE effect, 3031 carry two, 798 carry three, 3 none. A
//     fixed array of three is mostly empty, so Operations holds only what is
//     there and a walk never sees a dead slot.
//   - 129 of the 160 flag bits are ever set, and they are near-independent: out
//     of some 16000 ordered pairs only 8 imply one another. Nothing can be
//     derived from anything, so every bit that matters is resolved here into a
//     name, once.
//   - Of 2964 groups of rows sharing a name -- the ranks of one spell -- only
//     27.7% carry the same flags on every rank. A recipe is therefore per spell
//     id and never per family; rank 6 inherits nothing from rank 1.
//
// The names given to flags below are PROPOSALS inherited from emulator lore, not
// facts: 1.12 ships no table naming them, and the two DBCs that once did were
// deleted by the patch. Each keeps the word it came from in `flags`, so a name
// that turns out wrong costs a rename and nothing else.

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
    /// The six ways a spell starts. A spell is exactly one of them.
    ///
    /// The order matters where the data disagrees with itself, and it does: 30
    /// rows carry a channel flag together with a cast time, and 2 are marked both
    /// passive and channelled. Passive wins over everything because such a spell
    /// is never cast at all; a swing-borne spell wins over a timer because the
    /// swing is what delivers it.
    enum class Start : uint8
    {
        Passive,        ///< never cast; it is simply true of its holder
        Instant,        ///< 11467 rows
        Timed,          ///< 5266 rows: the client draws a bar
        Channelled,     ///< 321 rows
        NextSwing,      ///< 279 rows: armed now, spent by the next weapon blow
        AutoRepeat,     ///< 4 rows in the whole game
    };

    /// What a blow from this spell has to beat. 15808 rows answer None: they
    /// never reach anyone's defences, and have no business in the combat path.
    enum class Defence : uint8
    {
        None,           ///< 15808 rows
        Magic,          ///< 5519
        Melee,          ///< 793
        Ranged,         ///< 237
    };

    /**
     * @brief One thing a spell does.
     *
     * The verb is the number the data carries. 1.12 names none of them -- the
     * client's own catalogue was deleted by patch 1.12 -- so the number is the
     * identity, and a routing table maps it to code. 117 distinct verbs are in
     * use, the highest is 129, and two of them cover half of all uses.
     */
    struct Operation
    {
        uint32 verb = 0;            ///< Spell.dbc column 61..63
        uint32 aura = 0;            ///< column 91..93, zero unless the verb applies one
        uint32 mechanic = 0;
        uint8 slot = 0;             ///< 0..2, the number the client's $s1 tokens count from

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

        uint32 periodMs = 0;        ///< a tick every this many, zero when it does not tick
        bool reachesArea = false;   ///< either implicit target names an area
        float amplitude = 0.0f;
        uint32 itemType = 0;
        int32 miscValue = 0;
        uint32 triggerSpell = 0;
        float chainAmplitude = 0.0f;

        /// Whether this operation is something its target would want. Settled
        /// once, because the answer walks trigger chains into other spells.
        bool positive = false;

        bool AppliesAura() const { return aura != 0; }
        bool Ticks() const { return periodMs != 0; }
    };

    /**
     * @brief The operations one spell carries, one to three of them.
     *
     * Storage is inline: a recipe never allocates, and a walk never touches a
     * slot the data left empty.
     */
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

            /// The operation in a slot, whether or not one is there. A slot the
            /// data left empty answers with zeros, which is what reading the row
            /// at that index has always given.
            const Operation& At(uint8 slot) const
            {
                static const Operation nothing;
                const Operation* operation = AtSlot(slot);
                return operation != nullptr ? *operation : nothing;
            }

            /// The operation filling a given client-facing slot, or nullptr.
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

            /// The book settles what a compiled row cannot answer on its own.
            friend class RecipeBook;

            Operation& Mutable(size_t i) { return m_items[i]; }

            Operation m_items[CAPACITY];
            uint8 m_count = 0;
    };

    /**
     * @brief The flag words, read once into names.
     *
     * Only the bits the server acts on are named. The rest stay in `words`,
     * untouched and unread, which is the honest place for a bit whose meaning
     * nobody has established.
     */
    struct Flags
    {
        uint32 words[5] = {};       ///< Spell.dbc columns 6..10, verbatim

        bool ranged = false;                    ///< c6.b01
        bool ability = false;                   ///< c6.b04
        bool tradeskill = false;                ///< c6.b05 -- 1159 of its 1161 rows create an item
        bool passive = false;                   ///< c6.b06
        bool hiddenFromClient = false;          ///< c6.b07
        bool notWhileShapeshifted = false;      ///< c6.b16
        bool onlyWhileStealthed = false;        ///< c6.b17
        bool damageScalesWithLevel = false;     ///< c6.b19
        bool stopsAttack = false;               ///< c6.b20
        bool cannotBeAvoided = false;           ///< c6.b21 -- no dodge, parry or block
        bool castableWhileDead = false;         ///< c6.b23
        bool castableWhileMounted = false;      ///< c6.b24
        bool spentWhileActive = false;          ///< c6.b25
        bool showsAsDebuff = false;             ///< c6.b26
        bool castableWhileSitting = false;      ///< c6.b27
        bool notInCombat = false;               ///< c6.b28
        bool ignoresInvulnerability = false;    ///< c6.b29
        bool fadesOnHeartbeat = false;          ///< c6.b30
        bool cannotBeCancelled = false;         ///< c6.b31

        bool drainsAllPower = false;            ///< c7.b01
        bool cannotBeRedirected = false;        ///< c7.b03
        bool doesNotBreakStealth = false;       ///< c7.b05
        bool cannotBeReflected = false;         ///< c7.b07
        bool channels = false;                  ///< c7.b02 or c7.b06 -- one concept, two bits, 20 rows carry both
        bool needsTargetOutOfCombat = false;    ///< c7.b08
        bool needsFacing = false;               ///< c7.b09
        bool makesNoThreat = false;             ///< c7.b10
        bool channelTracksTarget = false;       ///< c7.b14 -- implies the channel bit, 106 of 106
        bool ignoresSchoolImmunity = false;     ///< c7.b16
        bool cannotTargetSelf = false;          ///< c7.b19
        bool needsComboPointsOnTarget = false;  ///< c7.b20
        bool spendsComboPoints = false;         ///< c7.b22
        bool refundsPowerWhenTurned = false;    ///< c7.b27
        bool hiddenFromAuraBar = false;         ///< c7.b28
        bool armedByDodge = false;              ///< c7.b30

        bool canTargetDead = false;             ///< c8.b00
        bool ignoresLineOfSight = false;        ///< c8.b02
        bool autoRepeats = false;               ///< c8.b05
        bool keepsWeaponTimer = false;          ///< c8.b17 -- 60 of its 61 rows are ranged
        bool shieldReducesDamage = false;       ///< c8.b21
        bool cannotCrit = false;                ///< c8.b29
        bool triggeredCanProc = false;          ///< c8.b30
        bool isFoodOrDrink = false;             ///< c8.b31

        bool blockable = false;                 ///< c9.b03
        bool stacksPerCaster = false;           ///< c9.b07
        bool playersOnly = false;               ///< c9.b08
        bool needsMainHand = false;             ///< c9.b10 -- all 128 of its rows resolve as melee
        bool isRangedAttack = false;            ///< c9.b15
        bool cannotProc = false;                ///< c9.b16
        bool makesNoInitialThreat = false;      ///< c9.b17
        bool cannotMiss = false;                ///< c9.b18
        bool survivesDeath = false;             ///< c9.b20
        bool needsWand = false;                 ///< c9.b22
        bool needsOffHand = false;              ///< c9.b24
        bool takesNoDoneBonus = false;          ///< c9.b29
    };

    /// The numbers a row reaches for in other tables. The book resolves them so
    /// that compiling a recipe stays a pure function of what it is handed.
    struct Timings
    {
        /// Straight out of SpellCastTimes.dbc, sign and all: one row holds
        /// -1000000, and a rule that clamps before it adds to it gets a
        /// different answer than one that clamps after.
        int32 castTimeBaseMs = 0;
        int32 durationMs = 0;
        int32 durationPerLevelMs = 0;
        int32 maxDurationMs = 0;
        float rangeMin = 0.0f;
        float rangeMax = 0.0f;
    };

    /**
     * @brief One spell, compiled.
     *
     * Immutable once built. Holds no pointer into the world, so it is shared by
     * every cast of that spell on every map at once.
     */
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

            /// The cast time a caster-less question gets: the row's own, plus the
            /// half second a ranged spell spends being drawn, floored at zero.
            uint32 BareCastTimeMs() const { return m_bareCastTimeMs; }

            /// The duration as everything downstream wants it: -1 stays -1 and
            /// means no end, everything else is a magnitude.
            int32 DurationMs() const { return m_durationMs; }

            /// How many times a periodic aura of this spell can tick.
            uint16 MaxTicks() const { return m_maxTicks; }

            /// The share of spell power this spell takes.
            ///
            /// Every input is a property of the row -- the cast time, the
            /// duration, how many slots deal direct damage, whether any reaches
            /// an area, whether any leeches -- so it is two numbers, settled when
            /// the row is compiled: one for a blow that lands at once, one for a
            /// blow spread over time.
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

            /// True when nothing this spell does reaches anyone's defences.
            bool TouchesNoDefence() const { return m_defence == Defence::None; }

            /// Whether a target would want this cast on them. A spell is wanted
            /// only when every one of its operations is.
            bool IsPositive() const { return m_positive; }

            /// What the world's own tables say about this spell, found once
            /// rather than looked up by id on every proc, every bonus and every
            /// point of threat. Null where the tables say nothing.
            const SpellProcEventEntry* ProcRule() const { return m_procRule; }
            const SpellBonusEntry* Bonus() const { return m_bonus; }
            const SpellThreatEntry* Threat() const { return m_threat; }

            /// How much threat this spell makes for the damage it deals. One when
            /// no table says otherwise.
            float ThreatMultiplier() const { return m_threatMultiplier; }

            /// The group whose diminishing returns this spell shares. A control
            /// spell that arrives through a trigger lands in a different group
            /// than the same spell cast directly, so there are two.
            DiminishingGroup Diminishes(bool triggered) const
            {
                return m_diminishing[triggered ? 1 : 0];
            }

            /// What the spell does in one slot, zeros when the slot is empty.
            const Operation& At(uint8 slot) const { return m_operations.At(slot); }

            /// The same question about one slot. An empty slot is not wanted,
            /// because there is nothing in it to want.
            bool IsPositiveAt(uint8 slot) const
            {
                const Operation* operation = m_operations.AtSlot(slot);
                return operation != nullptr && operation->positive;
            }

            /// True when any of its operations ticks.
            bool Ticks() const;

            /// True when any of its operations applies an aura.
            bool AppliesAura() const;

        private:

            /// Whether a cast is wanted cannot be settled while the row is being
            /// compiled: the answer follows trigger chains into other spells, so
            /// it waits until every recipe exists. The book fills it then.
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
