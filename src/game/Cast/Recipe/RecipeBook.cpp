#include "Cast/Recipe/RecipeBook.h"

#include "DataStore/DBCStores.h"
#include "DataStore/DBCStructure.h"
#include "DataStore/SpellMgr.h"

namespace cast
{
    namespace
    {
        Timings TimingsOf(const SpellEntry& row)
        {
            Timings timings;

            if (const SpellCastTimesEntry* cast = sSpellCastTimesStore.LookupEntry(row.CastingTimeIndex))
            {
                timings.castTimeBaseMs = cast->Base;
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

        SettlePositivity();
        SettleAnnouncements();
        return m_recipes.size();
    }

    void RecipeBook::SettleGroups()
    {
        for (auto& recipe : m_recipes)
        {
            const SpellEntry* row = sSpellStore.LookupEntry(recipe.m_id);
            if (row == nullptr)
            {
                continue;
            }

            recipe.m_diminishing[0] = GetDiminishingReturnsGroupForSpell(row, false);
            recipe.m_diminishing[1] = GetDiminishingReturnsGroupForSpell(row, true);

            recipe.m_procRule = sSpellMgr.GetSpellProcEvent(recipe.m_id);
            recipe.m_bonus = sSpellMgr.GetSpellBonusData(recipe.m_id);
            recipe.m_threat = sSpellMgr.GetSpellThreatEntry(recipe.m_id);
            recipe.m_threatMultiplier = recipe.m_threat != nullptr ? recipe.m_threat->multiplier : 1.0f;
        }
    }

    void RecipeBook::SettlePositivity()
    {
        for (auto& recipe : m_recipes)
        {
            const SpellEntry* row = sSpellStore.LookupEntry(recipe.m_id);
            if (row == nullptr)
            {
                continue;
            }

            bool wanted = true;
            for (size_t i = 0; i < recipe.m_operations.size(); ++i)
            {
                Operation& operation = recipe.m_operations.Mutable(i);
                operation.positive = IsPositiveEffect(row, SpellEffectIndex(operation.slot));
                wanted = wanted && operation.positive;
            }

            recipe.m_positive = wanted;
        }
    }

    void RecipeBook::SettleAnnouncements()
    {
        for (auto& recipe : m_recipes)
        {
            switch (recipe.Beats())
            {
                case Defence::Melee:
                    recipe.m_announces.byCaster = PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT;
                    if (recipe.Swings() == OFF_ATTACK)
                    {
                        recipe.m_announces.byCaster |= PROC_FLAG_SUCCESSFUL_OFFHAND_HIT;
                    }
                    recipe.m_announces.byTarget = PROC_FLAG_TAKEN_MELEE_SPELL_HIT;
                    break;
                case Defence::Ranged:
                    if (recipe.Says().autoRepeats)
                    {
                        recipe.m_announces.byCaster = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
                        recipe.m_announces.byTarget = PROC_FLAG_TAKEN_RANGED_HIT;
                    }
                    else
                    {
                        recipe.m_announces.byCaster = PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT;
                        recipe.m_announces.byTarget = PROC_FLAG_TAKEN_RANGED_SPELL_HIT;
                    }
                    break;
                default:
                    if (recipe.IsPositive())
                    {
                        recipe.m_announces.byCaster = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
                        recipe.m_announces.byTarget = PROC_FLAG_TAKEN_POSITIVE_SPELL;
                    }
                    else if (recipe.Says().autoRepeats)      // a wand swinging on its own
                    {
                        recipe.m_announces.byCaster = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
                        recipe.m_announces.byTarget = PROC_FLAG_TAKEN_RANGED_HIT;
                    }
                    else
                    {
                        recipe.m_announces.byCaster = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
                        recipe.m_announces.byTarget = PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT;
                    }
                    break;
            }

            // the four hunter traps, so that Entrapment hears them go off
            if (recipe.ClassSet() == SPELLFAMILY_HUNTER && (recipe.ClassMask() & UI64LIT(0x000020000000001C)))
            {
                recipe.m_announces.byCaster |= PROC_FLAG_ON_TRAP_ACTIVATION;
            }

            recipe.m_unwantedSlots = 0;
            for (const auto& operation : recipe.Does())
            {
                if (!operation.positive)
                {
                    recipe.m_unwantedSlots |= uint8(1 << operation.slot);
                }
            }
        }
    }

    RecipeBook& Recipes()
    {
        static RecipeBook book;
        return book;
    }

    void SettleRecipeGroups()
    {
        Recipes().SettleGroups();
    }

    const Recipe& RecipeOf(const SpellEntry& row)
    {
        const Recipe* recipe = Recipes().Find(row.ID);
        MANGOS_ASSERT(recipe != nullptr);
        return *recipe;
    }
}
