#pragma once

#include "Cast/Recipe/Recipe.h"

struct SpellEntry;

namespace cast
{
    class RecipeBook
    {
        public:

            size_t Fill();

            const Recipe* Find(uint32 spellId) const
            {
                return spellId < m_byId.size() ? m_byId[spellId] : nullptr;
            }

            bool IsPositive(uint32 spellId) const
            {
                const Recipe* recipe = Find(spellId);
                return recipe != nullptr && recipe->IsPositive();
            }

            bool StartsAs(uint32 spellId, Start start) const
            {
                const Recipe* recipe = Find(spellId);
                return recipe != nullptr && recipe->Starts() == start;
            }

            size_t Count() const { return m_recipes.size(); }

            const std::vector<Recipe>& All() const { return m_recipes; }

            void SettleGroups();

        private:

            void SettlePositivity();
            void SettleAnnouncements();

            std::vector<Recipe> m_recipes;
            std::vector<const Recipe*> m_byId;
    };

    RecipeBook& Recipes();

    void SettleRecipeGroups();

    const Recipe& RecipeOf(const SpellEntry& row);
}
