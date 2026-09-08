// Compiling a spell means answering, once, every question a cast would otherwise
// ask the DBC row again on each use. The cases below are the answers that the
// 22357 rows of 1.12.1 showed to be worth getting right: which slots actually
// hold something, which of the six ways a spell starts wins when the data marks
// two of them, and the fact that ranks of one spell share nothing.

#include "doctest.h"

#include "Cast/Recipe/Recipe.h"
#include "DataStore/DBCStructure.h"

#include <cstring>

namespace
{
    constexpr uint32 ATTR_ON_NEXT_SWING_1 = 1u << 2;
    constexpr uint32 ATTR_PASSIVE         = 1u << 6;
    constexpr uint32 ATTR_ON_NEXT_SWING_2 = 1u << 10;
    constexpr uint32 ATTR_STOPS_ATTACK    = 1u << 20;

    constexpr uint32 ATTR_EX_CHANNELLED_1 = 1u << 2;
    constexpr uint32 ATTR_EX_CHANNELLED_2 = 1u << 6;
    constexpr uint32 ATTR_EX_TRACKS_TARGET = 1u << 14;

    constexpr uint32 ATTR_EXB_AUTO_REPEAT = 1u << 5;

    constexpr uint32 ATTR_EXC_CANNOT_MISS = 1u << 18;
    constexpr uint32 ATTR_EXC_REQ_OFFHAND = 1u << 24;

    /// A row of Spell.dbc, made the way the loader makes one.
    ///
    /// SpellEntry suppresses its own construction on purpose: a row is meant to
    /// come from the file, never from someone's imagination. The loader itself
    /// reads the file's bytes as the struct, and that is exactly what happens
    /// here -- on zeroed memory this test owns, so nothing forged escapes it.
    class Row
    {
        public:

            Row()
            {
                std::memset(m_bytes, 0, sizeof(m_bytes));
                Entry().ID = 133;
            }

            SpellEntry& Entry()
            {
                return *reinterpret_cast<SpellEntry*>(m_bytes);
            }

            const SpellEntry& Entry() const
            {
                return *reinterpret_cast<const SpellEntry*>(m_bytes);
            }

            SpellEntry* operator->() { return &Entry(); }
            const SpellEntry& operator*() const { return Entry(); }

        private:

            alignas(SpellEntry) unsigned char m_bytes[sizeof(SpellEntry)];
    };

    ClassFamilyMask ClassFlag(uint64 bits)
    {
        ClassFamilyMask mask;
        mask.Flags = bits;
        return mask;
    }

    cast::Timings NoTime()
    {
        return cast::Timings();
    }

    cast::Timings CastTime(uint32 ms)
    {
        cast::Timings timings;
        timings.castTimeBaseMs = ms;
        return timings;
    }
}

TEST_CASE("only the slots the data fills become operations")
{
    Row row;
    row->Effect[0] = 2;                                      // school damage
    row->Effect[2] = 6;                                      // apply aura
    row->EffectAura[2] = 3;                                  // periodic damage
    row->EffectAuraPeriod[2] = 3000;
    row->EffectBasePoints[1] = 999;                          // a value in an empty slot

    const cast::Recipe recipe = cast::Recipe::Compile(*row, NoTime());

    REQUIRE(recipe.Does().size() == 2);
    CHECK(recipe.Does()[0].verb == 2);
    CHECK(recipe.Does()[0].slot == 0);
    CHECK(recipe.Does()[1].verb == 6);
    CHECK(recipe.Does()[1].slot == 2);
    CHECK(recipe.Does()[1].aura == 3);
    CHECK(recipe.AppliesAura());
    CHECK(recipe.Ticks());
}

TEST_CASE("a spell with no effect at all compiles to nothing to do")
{
    const cast::Recipe recipe = cast::Recipe::Compile(*Row(), NoTime());

    CHECK(recipe.Does().empty());
    CHECK_FALSE(recipe.AppliesAura());
    CHECK_FALSE(recipe.Ticks());
}

TEST_CASE("the slot a client token names is the slot that answers")
{
    Row row;
    row->Effect[1] = 6;
    row->EffectMiscValue[1] = 42;

    const cast::Recipe recipe = cast::Recipe::Compile(*row, NoTime());

    REQUIRE(recipe.Does().AtSlot(1) != nullptr);
    CHECK(recipe.Does().AtSlot(1)->miscValue == 42);
    CHECK(recipe.Does().AtSlot(0) == nullptr);
    CHECK(recipe.Does().AtSlot(2) == nullptr);
}

TEST_CASE("a cast time is what separates timed from instant")
{
    Row row;
    row->Effect[0] = 2;

    CHECK(cast::Recipe::Compile(*row, NoTime()).Starts() == cast::Start::Instant);
    CHECK(cast::Recipe::Compile(*row, CastTime(3500)).Starts() == cast::Start::Timed);
}

TEST_CASE("either channel flag makes a spell channelled")
{
    Row row;

    row->AttributesEx = ATTR_EX_CHANNELLED_1;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Starts() == cast::Start::Channelled);

    row->AttributesEx = ATTR_EX_CHANNELLED_2;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Starts() == cast::Start::Channelled);

    row->AttributesEx = ATTR_EX_CHANNELLED_1 | ATTR_EX_CHANNELLED_2;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Starts() == cast::Start::Channelled);
}

TEST_CASE("a channel that also carries a cast time is still a channel")
{
    // 30 rows of 1.12.1 are marked both ways. The channel owns the clock.
    Row row;
    row->AttributesEx = ATTR_EX_CHANNELLED_1;

    CHECK(cast::Recipe::Compile(*row, CastTime(2000)).Starts() == cast::Start::Channelled);
}

TEST_CASE("passive wins over every other way of starting")
{
    // 2 rows are marked passive and channelled at once, and a passive spell is
    // never cast, so nothing else can describe how it starts.
    Row row;
    row->Attributes = ATTR_PASSIVE;
    row->AttributesEx = ATTR_EX_CHANNELLED_1;
    row->AttributesExB = ATTR_EXB_AUTO_REPEAT;

    CHECK(cast::Recipe::Compile(*row, CastTime(1500)).Starts() == cast::Start::Passive);
    CHECK(cast::Recipe::Compile(*row, CastTime(1500)).Says().passive);
}

TEST_CASE("a swing-borne spell beats a timer")
{
    Row row;

    row->Attributes = ATTR_ON_NEXT_SWING_1;
    CHECK(cast::Recipe::Compile(*row, CastTime(1500)).Starts() == cast::Start::NextSwing);

    row->Attributes = ATTR_ON_NEXT_SWING_2;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Starts() == cast::Start::NextSwing);
}

TEST_CASE("the four auto-repeating spells are their own way of starting")
{
    Row row;
    row->AttributesExB = ATTR_EXB_AUTO_REPEAT;

    const cast::Recipe recipe = cast::Recipe::Compile(*row, NoTime());
    CHECK(recipe.Starts() == cast::Start::AutoRepeat);
    CHECK(recipe.Says().autoRepeats);
}

TEST_CASE("what a blow has to beat comes from the row, and most beat nothing")
{
    Row row;

    CHECK(cast::Recipe::Compile(*row, NoTime()).Beats() == cast::Defence::None);
    CHECK(cast::Recipe::Compile(*row, NoTime()).TouchesNoDefence());

    row->DefenseType = 1;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Beats() == cast::Defence::Magic);
    row->DefenseType = 2;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Beats() == cast::Defence::Melee);
    row->DefenseType = 3;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Beats() == cast::Defence::Ranged);
    row->DefenseType = 77;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Beats() == cast::Defence::None);
}

TEST_CASE("a school out of range is physical, the way an absent one always was")
{
    Row row;
    row->School = 5;
    CHECK(cast::Recipe::Compile(*row, NoTime()).OfSchool() == combat::School::Shadow);

    row->School = 99;
    CHECK(cast::Recipe::Compile(*row, NoTime()).OfSchool() == combat::School::Physical);
}

TEST_CASE("named flags and the words they came from both survive")
{
    Row row;
    row->Attributes = ATTR_STOPS_ATTACK;
    row->AttributesEx = ATTR_EX_TRACKS_TARGET | ATTR_EX_CHANNELLED_1;
    row->AttributesExC = ATTR_EXC_CANNOT_MISS;
    row->AttributesExD = 0x2A;                               // nothing here is named yet

    const cast::Recipe recipe = cast::Recipe::Compile(*row, NoTime());

    CHECK(recipe.Says().stopsAttack);
    CHECK(recipe.Says().channelTracksTarget);
    CHECK(recipe.Says().cannotMiss);
    CHECK_FALSE(recipe.Says().passive);

    CHECK(recipe.Says().words[0] == ATTR_STOPS_ATTACK);
    CHECK(recipe.Says().words[3] == ATTR_EXC_CANNOT_MISS);
    CHECK(recipe.Says().words[4] == 0x2A);
}

TEST_CASE("ranks of one spell inherit nothing from each other")
{
    // Only 27.7% of the name groups in 1.12.1 carry the same flags on every rank,
    // so a recipe is per id and a second rank is compiled, never derived.
    Row rankOne;
    rankOne->ID = 133;
    rankOne->Attributes = ATTR_STOPS_ATTACK;
    rankOne->Effect[0] = 2;

    Row rankTwo;
    rankTwo->ID = 143;
    rankTwo->Effect[0] = 2;
    rankTwo->AttributesExB = ATTR_EXB_AUTO_REPEAT;

    const cast::Recipe first = cast::Recipe::Compile(*rankOne, CastTime(3500));
    const cast::Recipe second = cast::Recipe::Compile(*rankTwo, CastTime(3500));

    CHECK(first.Id() == 133);
    CHECK(second.Id() == 143);
    CHECK(first.Says().stopsAttack);
    CHECK_FALSE(second.Says().stopsAttack);
    CHECK(first.Starts() == cast::Start::Timed);
    CHECK(second.Starts() == cast::Start::AutoRepeat);
}

TEST_CASE("the numbers other tables hold are carried, not looked up again")
{
    Row row;
    cast::Timings timings;
    timings.castTimeBaseMs = 1500;
    timings.durationMs = 12000;
    timings.maxDurationMs = 12000;
    timings.rangeMax = 30.0f;

    const cast::Recipe recipe = cast::Recipe::Compile(*row, timings);

    CHECK(recipe.Takes().castTimeBaseMs == 1500);
    CHECK(recipe.Takes().durationMs == 12000);
    CHECK(recipe.Takes().rangeMax == doctest::Approx(30.0f));
}

TEST_CASE("a melee spell swings the main hand unless it demands the off hand")
{
    Row row;
    row->DefenseType = SPELL_DAMAGE_CLASS_MELEE;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Swings() == BASE_ATTACK);

    row->AttributesExC = ATTR_EXC_REQ_OFFHAND;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Swings() == OFF_ATTACK);
}

TEST_CASE("a ranged spell swings the ranged slot whatever else it says")
{
    Row row;
    row->DefenseType = SPELL_DAMAGE_CLASS_RANGED;
    row->AttributesExC = ATTR_EXC_REQ_OFFHAND;

    CHECK(cast::Recipe::Compile(*row, NoTime()).Swings() == RANGED_ATTACK);
}

TEST_CASE("a wand swings the ranged slot although its defence class names neither")
{
    Row row;
    row->DefenseType = SPELL_DAMAGE_CLASS_NONE;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Swings() == BASE_ATTACK);

    row->AttributesExB = ATTR_EXB_AUTO_REPEAT;
    CHECK(cast::Recipe::Compile(*row, NoTime()).Swings() == RANGED_ATTACK);
}

TEST_CASE("only the named spells of a family are let through from a trigger")
{
    Row row;
    CHECK_FALSE(cast::Recipe::Compile(*row, NoTime()).ProcsThoughTriggered());

    // Arcane Missiles, one of the two mage spells on the list
    row->SpellClassSet = SPELLFAMILY_MAGE;
    row->SpellClassMask = ClassFlag(UI64LIT(0x0000000000000080));
    CHECK(cast::Recipe::Compile(*row, NoTime()).ProcsThoughTriggered());

    // another mage spell, sharing the family but none of the named bits
    row->SpellClassMask = ClassFlag(UI64LIT(0x0000000000000001));
    CHECK_FALSE(cast::Recipe::Compile(*row, NoTime()).ProcsThoughTriggered());

    // the same bits under a family that is not on the list
    row->SpellClassSet = SPELLFAMILY_PRIEST;
    row->SpellClassMask = ClassFlag(UI64LIT(0x0000000000000080));
    CHECK_FALSE(cast::Recipe::Compile(*row, NoTime()).ProcsThoughTriggered());
}
