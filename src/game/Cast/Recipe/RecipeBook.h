#pragma once

// Every spell, compiled, held for the life of the process.
//
// Built once from Spell.dbc and the four small tables its indices point into.
// After Fill() nothing in here changes, so any number of maps read from it at
// once without a lock, and a cast never touches a DBC row.
//
// Lookup is by id into a flat vector rather than a map: 1.12 numbers spells up to
// about 27000 and ships 22357 of them, so the holes cost a pointer each and buy a
// bounds check instead of a hash.

#include "Cast/Recipe/Recipe.h"

#include <memory>
#include <vector>

struct SpellEntry;

namespace cast
{
    class RecipeBook
    {
        public:

            /// Compiles every row of Spell.dbc. Returns how many recipes were made.
            size_t Fill();

            /// The recipe for a spell id, or nullptr when no such spell exists.
            const Recipe* Find(uint32 spellId) const
            {
                return spellId < m_byId.size() ? m_byId[spellId] : nullptr;
            }

            /// Whether a spell id starts the given way. An id nothing answers to
            /// starts no way at all, which is what a missing row has always meant.
            bool StartsAs(uint32 spellId, Start start) const
            {
                const Recipe* recipe = Find(spellId);
                return recipe != nullptr && recipe->Starts() == start;
            }

            size_t Count() const { return m_recipes.size(); }

            /// For a caller that wants to walk everything once, such as a report.
            const std::vector<Recipe>& All() const { return m_recipes; }

        private:

            std::vector<Recipe> m_recipes;
            std::vector<const Recipe*> m_byId;
    };

    /// The one book. Filled while the world loads.
    RecipeBook& Recipes();

    /// The recipe for a row that came out of the store. A row the book has never
    /// seen is a programming error, not a data one, so this asserts rather than
    /// handing back something to check.
    const Recipe& RecipeOf(const SpellEntry& row);
}
