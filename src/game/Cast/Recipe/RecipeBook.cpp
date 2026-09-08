#include "Cast/Recipe/RecipeBook.h"

#include "DataStore/DBCStores.h"
#include "DataStore/DBCStructure.h"

namespace cast
{
    namespace
    {
        Timings TimingsOf(const SpellEntry& row)
        {
            Timings timings;

            if (const SpellCastTimesEntry* cast = sSpellCastTimesStore.LookupEntry(row.CastingTimeIndex))
            {
                timings.castTimeMs = cast->Base > 0 ? static_cast<uint32>(cast->Base) : 0;
            }

            if (const SpellDurationEntry* duration = sSpellDurationStore.LookupEntry(row.DurationIndex))
            {
                timings.durationMs = duration->Duration[0];
                timings.durationPerLevelMs = duration->Duration[1];
                timings.maxDurationMs = duration->Duration[2];
            }

            if (const SpellRangeEntry* range = sSpellRangeStore.LookupEntry(row.RangeIndex))
            {
                timings.rangeMin = range->RangeMin;
                timings.rangeMax = range->RangeMax;
            }

            return timings;
        }
    }

    size_t RecipeBook::Fill()
    {
        m_recipes.clear();
        m_byId.clear();

        m_recipes.reserve(sSpellStore.GetNumRows());

        uint32 highest = 0;
        for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
        {
            const SpellEntry* row = sSpellStore.LookupEntry(id);
            if (row == nullptr)
            {
                continue;
            }

            m_recipes.push_back(Recipe::Compile(*row, TimingsOf(*row)));
            highest = highest > row->ID ? highest : row->ID;
        }

        // Addresses are taken only after the vector has stopped growing.
        m_byId.assign(static_cast<size_t>(highest) + 1, nullptr);
        for (const auto& recipe : m_recipes)
        {
            m_byId[recipe.Id()] = &recipe;
        }

        return m_recipes.size();
    }

    RecipeBook& Recipes()
    {
        static RecipeBook book;
        return book;
    }

    const Recipe& RecipeOf(const SpellEntry& row)
    {
        const Recipe* recipe = Recipes().Find(row.ID);
        MANGOS_ASSERT(recipe != nullptr);
        return *recipe;
    }
}
