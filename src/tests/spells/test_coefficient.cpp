// What share of spell power a spell takes is a property of its row: the cast
// time, the duration, how many slots strike at once, whether any reaches an area,
// whether any leeches. These cases pin the two numbers a row compiles to -- one
// for a blow that lands at once, one for a blow spread over time.

#include "doctest.h"

#include "Cast/Recipe/Recipe.h"
#include "DataStore/DBCStructure.h"
#include "Unit/Auras/SpellAuraDefines.h"

#include <cstring>

namespace
{
    /// A row is meant to come from the file, so the test reads its own zeroed
    /// bytes as one, the way the loader reads the file's.
    class Row
    {
        public:

            Row()
            {
                std::memset(m_bytes, 0, sizeof(m_bytes));
                Entry().ID = 133;
            }

            SpellEntry& Entry() { return *reinterpret_cast<SpellEntry*>(m_bytes); }
            SpellEntry* operator->() { return &Entry(); }
            const SpellEntry& operator*() const { return *reinterpret_cast<const SpellEntry*>(m_bytes); }

        private:

            alignas(SpellEntry) unsigned char m_bytes[sizeof(SpellEntry)];
    };

    cast::Timings Timed(int32 castMs, int32 durationMs = 0)
    {
        cast::Timings timings;
        timings.castTimeBaseMs = castMs;
        timings.durationMs = durationMs;
        return timings;
    }

    /// A spell that only strikes.
    Row DirectDamage()
    {
        Row row;
        row->Effect[0] = SPELL_EFFECT_SCHOOL_DAMAGE;
        return row;
    }

    /// A spell that only burns: one aura, ticking.
    Row OverTime(uint32 periodMs)
    {
        Row row;
        row->Effect[0] = SPELL_EFFECT_APPLY_AURA;
        row->EffectAura[0] = SPELL_AURA_PERIODIC_DAMAGE;
        row->EffectAuraPeriod[0] = periodMs;
        return row;
    }
}

TEST_CASE("a three and a half second cast takes a whole share")
{
    Row row = DirectDamage();
    const cast::Recipe recipe = cast::Recipe::Compile(*row, Timed(3500));

    CHECK(recipe.Coefficient(false) == doctest::Approx(1.0f));
}

TEST_CASE("a cast is weighed between one and a half and seven seconds")
{
    Row row = DirectDamage();

    // Anything quicker counts as a second and a half.
    CHECK(cast::Recipe::Compile(*row, Timed(500)).Coefficient(false)
          == doctest::Approx(1500.0f / 3500.0f));
    CHECK(cast::Recipe::Compile(*row, Timed(0)).Coefficient(false)
          == doctest::Approx(1500.0f / 3500.0f));

    // Anything slower counts as seven seconds.
    CHECK(cast::Recipe::Compile(*row, Timed(10000)).Coefficient(false)
          == doctest::Approx(2.0f));
}

TEST_CASE("the half second a ranged spell spends drawing counts toward its share")
{
    Row row = DirectDamage();
    row->Attributes = 1u << 1;                              // ranged

    const cast::Recipe recipe = cast::Recipe::Compile(*row, Timed(3000));

    CHECK(recipe.BareCastTimeMs() == 3500);
    CHECK(recipe.Coefficient(false) == doctest::Approx(1.0f));
}

TEST_CASE("a cast time the table stores as nonsense stays nonsense, not half a second")
{
    // One row of SpellCastTimes.dbc holds -1000000, and the ranged half second is
    // added to it before the floor rather than after.
    Row row = DirectDamage();
    row->Attributes = 1u << 1;

    const cast::Recipe recipe = cast::Recipe::Compile(*row, Timed(-1000000));

    CHECK(recipe.BareCastTimeMs() == 0);
}

TEST_CASE("what burns is shared out over its ticks")
{
    Row row = OverTime(3000);
    const cast::Recipe recipe = cast::Recipe::Compile(*row, Timed(0, 12000));

    CHECK(recipe.MaxTicks() == 4);
    // A second and a half of weight, with nothing spread over time to divide it.
    CHECK(recipe.Coefficient(false) == doctest::Approx(1500.0f / 3500.0f));
    // Twelve seconds of the fifteen a full share is measured against, over four ticks.
    CHECK(recipe.Coefficient(true) == doctest::Approx(0.2f));
}

TEST_CASE("a spell with no duration ticks once")
{
    Row row = OverTime(3000);
    CHECK(cast::Recipe::Compile(*row, Timed(0)).MaxTicks() == 1);
}

TEST_CASE("an aura with no period of its own counts as six ticks")
{
    Row row = OverTime(0);
    CHECK(cast::Recipe::Compile(*row, Timed(0, 12000)).MaxTicks() == 6);
}

TEST_CASE("a duration past thirty seconds stops counting there")
{
    Row row = OverTime(3000);
    CHECK(cast::Recipe::Compile(*row, Timed(0, 60000)).MaxTicks() == 10);
}

TEST_CASE("a spell that reaches an area takes half a share")
{
    Row row = DirectDamage();
    row->ImplicitTargetA[0] = TARGET_ALL_ENEMY_IN_AREA;

    const cast::Recipe recipe = cast::Recipe::Compile(*row, Timed(3500));

    CHECK(recipe.Does()[0].reachesArea);
    CHECK(recipe.Coefficient(false) == doctest::Approx(0.5f));
}

TEST_CASE("a spell that leeches takes half a share")
{
    Row row;
    row->Effect[0] = SPELL_EFFECT_HEALTH_LEECH;

    CHECK(cast::Recipe::Compile(*row, Timed(3500)).Coefficient(false)
          == doctest::Approx(0.5f));
}

TEST_CASE("a channel is weighed by how long it lasts, not by its cast bar")
{
    Row row = DirectDamage();
    row->AttributesEx = 1u << 2;                            // channelled

    const cast::Recipe recipe = cast::Recipe::Compile(*row, Timed(0, 5000));

    CHECK(recipe.Starts() == cast::Start::Channelled);
    CHECK(recipe.Coefficient(false) == doctest::Approx(5000.0f / 3500.0f));
}

TEST_CASE("a spell that both strikes and burns splits its share between the two")
{
    Row row;
    row->Effect[0] = SPELL_EFFECT_SCHOOL_DAMAGE;
    row->Effect[1] = SPELL_EFFECT_APPLY_AURA;
    row->EffectAura[1] = SPELL_AURA_PERIODIC_DAMAGE;
    row->EffectAuraPeriod[1] = 3000;

    const cast::Recipe recipe = cast::Recipe::Compile(*row, Timed(3500, 12000));

    // Twelve seconds spread against a three and a half second cast: 0.8 / 1.8 of
    // the share goes to what burns, the rest to what strikes.
    const float toSpread = 0.8f / 1.8f;
    CHECK(recipe.Coefficient(false) == doctest::Approx(1.0f - toSpread).epsilon(0.01));

    const float spread = (12000.0f / 15000.0f) / 4.0f;
    CHECK(recipe.Coefficient(true) == doctest::Approx(toSpread * spread).epsilon(0.01));
}
