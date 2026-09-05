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

#pragma once

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "Inventory/Slots.h"
#include "Item.h"

#include <list>

class Bag;
class Item;
class Player;

/**
 * A temporary enchantment running down on an item.
 *
 * The time left is kept here rather than on the item, because an enchantment
 * that is not being counted must keep its remaining time in the item's own field
 * instead: a stone put away in a bag stops running and takes up where it left
 * off when the item comes back out.
 */
struct EnchantClock
{
    EnchantClock(Item* onItem, EnchantmentSlot which, uint32 left)
        : item(onItem), slot(which), leftduration(left) {}

    Item* item;
    EnchantmentSlot slot;
    uint32 leftduration;
};

/**
 * The regions a search covers.
 *
 * Searches do not all cover the same ground, and the differences matter: an
 * entry is looked for where a player could spend it, a guid is looked for
 * wherever it could be, and a count is asked with or without the bank by the
 * caller. Naming the regions puts each of those differences at the call instead
 * of in the shape of a loop.
 */
enum SearchScope : uint32
{
    /// The nineteen worn places, the four worn bag slots and the backpack --
    /// everything the character holds himself rather than inside a bag.
    SCOPE_ON_HIM        = 0x01,
    SCOPE_KEYRING       = 0x02,
    SCOPE_IN_BAGS       = 0x04,
    /// The bank's own places. The six slots a bank bag hangs in are not among
    /// them, so a search never finds a bank bag itself -- only what is inside
    /// one, and only when SCOPE_IN_BANK_BAGS is asked for as well.
    SCOPE_BANK          = 0x08,
    SCOPE_IN_BANK_BAGS  = 0x10,

    /// What he can reach without walking to a bank.
    SCOPE_TO_HAND       = SCOPE_ON_HIM | SCOPE_KEYRING | SCOPE_IN_BAGS,
    SCOPE_IN_THE_BANK   = SCOPE_BANK | SCOPE_IN_BANK_BAGS,
    SCOPE_EVERYWHERE    = SCOPE_TO_HAND | SCOPE_IN_THE_BANK,
};

/**
 * Everything a character owns that sits in a numbered place: what he wears, the
 * bags he wears and what is inside them, the backpack, the bank and its bag
 * slots, the keyring, and the row of things a vendor will sell back to him.
 *
 * The character himself holds one item per place, and a bag holds its own. So a
 * lookup either reads a place here, or reads the bag that sits in a place here
 * and asks the bag. That is the whole of it, and it is why a bag is reached
 * through this and never the other way round.
 *
 * Where a place is -- worn, carried, banked -- is a question about the number
 * alone, so those answers are static and can be had without a character. They
 * are also the answers that decide what a move is allowed to do, which is why
 * they belong beside the places rather than beside the character.
 *
 * The buyback row is here too, with the place that the next sale overwrites. It
 * is a queue of twelve kept in slot order rather than in a container of its own,
 * because the client reads it out of the character's fields like any other
 * region.
 */
class Inventory
{
    public:
        explicit Inventory(Player& owner);

        /// A place inside the character rather than inside one of his bags.
        static bool IsHisOwn(uint8 bag) { return bag == INVENTORY_SLOT_BAG_0; }

        /// Carried loose: the backpack, the keyring, and the inside of any bag
        /// he wears. Not what he wears, and not the bank.
        static bool IsCarried(uint8 bag, uint8 slot);
        static bool IsCarried(uint16 place) { return IsCarried(Container(place), Slot(place)); }

        /// Worn: the nineteen gear places, and the four places a bag hangs in.
        static bool IsWorn(uint8 bag, uint8 slot);
        static bool IsWorn(uint16 place) { return IsWorn(Container(place), Slot(place)); }

        /// In the bank: its own places, its bag slots, and inside those bags.
        static bool IsBanked(uint8 bag, uint8 slot);
        static bool IsBanked(uint16 place) { return IsBanked(Container(place), Slot(place)); }

        /// A place that takes a bag rather than an ordinary item -- the four he
        /// wears and the six in the bank.
        static bool HoldsBag(uint16 place);

        /// Which swing a worn place feeds, or MAX_ATTACK when it feeds none.
        static uint32 AttackFrom(uint8 slot);

        /// The two halves of a place, packed as one number and taken apart.
        static uint16 Packed(uint8 bag, uint8 slot) { return uint16(uint16(bag) << 8) | slot; }
        static uint8 Container(uint16 place) { return uint8(place >> 8); }
        static uint8 Slot(uint16 place) { return uint8(place & 255); }

        /// What is in a place, following into a bag when the place names one.
        Item* At(uint8 bag, uint8 slot) const;
        Item* At(uint16 place) const { return At(Container(place), Slot(place)); }

        /// What is in one of the character's own places, without the bag walk.
        /// Out of range reads as empty so a caller need not bound its own loop.
        Item* Own(uint8 slot) const { return slot < PLAYER_SLOTS_COUNT ? m_place[slot] : nullptr; }
        void Own(uint8 slot, Item* item);

        /// The bag in a place, or null when the place is empty or holds
        /// something that is not a bag.
        Bag* BagAt(uint8 slot) const;

        /// Does he have this place at all? A bag's places exist only as far as
        /// the bag is deep, so this one needs the character. Asking for "any
        /// place in that bag" is refused when exact is set.
        bool Exists(uint8 bag, uint8 slot, bool exact) const;

        /// How many of an entry he has, one item's stack at a time. One item may
        /// be left out -- the one being moved, whose old place still holds it
        /// while the move is judged.
        uint32 Count(uint32 entry, uint32 scope, Item const* except = nullptr) const;

        /// Whether the count reaches what is wanted. Stops as soon as it does,
        /// and never counts what is already promised to a trade window.
        bool Holds(uint32 entry, uint32 count, uint32 scope) const;

        /// A guid is looked for wherever it could be, the bank included: the
        /// client names an item by guid and expects to be understood.
        Item* ByGuid(ObjectGuid guid) const;

        /// An entry is looked for only where he could spend it -- worn, in the
        /// backpack or in a bag. Not the keyring, and not the bank.
        Item* ByEntry(uint32 entry) const;

        /// The buyback place the next sale takes. It walks the row so that the
        /// oldest sale is the one lost.
        uint32 NextBuyback() const { return m_nextBuyback; }
        void NextBuyback(uint32 slot) { m_nextBuyback = slot; }

        /**
         * What the client is told, as distinct from what actually moves.
         *
         * Every one of the character's own places is private on the wire, so an
         * item he holds is an object only he is ever sent. It joins the world
         * when he takes it and leaves when he loses it, and he is sent its block
         * either way. What sits inside a bag is public instead, and reaches
         * onlookers through the bag itself. A caller that is
         * building state rather than playing it -- a character being loaded, a
         * swap in the middle of itself -- passes false and nothing is sent.
         *
         * Nothing is sent either while the owner is out of the world: there is
         * no one to send it to, and he will be given the whole of it when he
         * arrives.
         */
        void Arrived(Item* item, bool tell);
        void Changed(Item* item, bool tell);
        void Gone(Item* item, bool tell);

        /**
         * One of a worn item's enchantments has changed, and onlookers are shown
         * the new one.
         *
         * Only the first two are ever shown -- the permanent one and whatever is
         * on the weapon at the moment. The face has room for more, and nothing
         * fills it.
         *
         * An item that is not in a worn place has no face, so nothing is written
         * for it. That is checked against the places themselves rather than
         * against the item's own slot number, because an item inside a bag
         * carries its place in that bag and those numbers run over the worn ones.
         */
        void ShowsEnchant(Item const* item, uint32 which, uint32 enchantId);

        /**
         * What he owns that is running down: an item with a life of its own, and
         * a temporary enchantment on one.
         *
         * The two keep their remaining time in different places. An item's is a
         * field on the item, so the item is only listed here and counts itself
         * down. An enchantment's is held here, and is written back into the item
         * when it stops being counted -- which is what lets a stone survive
         * being put away and taken out again.
         *
         * A thing arriving starts both, and a thing leaving stops both, so they
         * are one call apiece.
         */
        void StartClocks(Item* item);
        void StopClocks(Item* item);

        /// Only the enchantments, for the one case where the item's own life is
        /// already being counted: a stack being merged into one already held.
        void StartEnchantClocks(Item* item);

        /// A single enchantment, replacing whatever was counted for that slot.
        /// A duration of nothing only stops the old one.
        void StartEnchantClock(Item* item, EnchantmentSlot which, uint32 duration);

        void RunClocks(uint32 elapsed, bool realTimeOnly);
        void RunEnchantClocks(uint32 elapsed);

        /// Everything still running, told to him at once when he enters the
        /// world and has been told nothing yet.
        void SendClocks();

        /// Each running enchantment's remaining time put back into its item, so
        /// that whoever writes the item down records where it had got to.
        void SettleClocks();

        /**
         * Puts an item away, in as many places as the plan says.
         *
         * A plan is a list of places and how much of the stack goes in each,
         * worked out beforehand by what will fit. Every place but the last gets
         * a copy of the stack, and the last gets the item itself, so a stack
         * that splits across three places ends up as three items. Where a place
         * already holds the same thing the two stacks are added instead, and the
         * item that arrived is destroyed.
         *
         * What comes back is the last item written, which is not always the item
         * handed in.
         */
        Item* Store(ItemPosCountVec const& plan, Item* item, bool tell);

        /// Puts an item in a worn place and makes it his. The place is not
         /// checked against what the item is: that is settled before this.
        void Wear(uint8 slot, Item* item);

        /// The same, for a character being brought back into the world, where
        /// nothing has to be weighed because it was already his when he left.
        void QuickWear(uint16 place, Item* item);

        /// Takes an item out of its place without destroying it. Whatever the
        /// item was doing for him -- a set bonus, a stat, an aura -- is undone
        /// before this, not here.
        void Take(uint8 bag, uint8 slot, bool tell);

        /**
         * The vendor's buyback row: twelve places holding what he has sold, with
         * the price and the hour beside each.
         *
         * A sale takes the next place in the row, or the emptiest one, or failing
         * that the oldest -- so what is lost to a thirteenth sale is always what
         * was sold longest ago.
         */
        void ToBuyback(Item* item);
        Item* InBuyback(uint32 slot) const;
        void ClearBuyback(uint32 slot, bool destroy);

    private:
        /// Hands every item in the wanted regions to the visitor and stops when
        /// the visitor returns false. Every search above is this walk with a
        /// different question, so the walk is written once.
        template <typename Visit>
        void Walk(uint32 scope, Visit visit) const;

        /// What onlookers see of a worn place: the piece, who made it, its first
        /// two enchantments and its random suffix. Nineteen places have one; the
        /// rest of what he owns is his own business.
        void Shows(uint8 slot, Item const* item);

        /// One place of a plan. A copy of the stack goes in unless this is the
        /// last place, which takes the item itself.
        Item* Put(uint16 place, Item* item, uint32 count, bool clone, bool tell);

        /// An item picked up or bought is bound to him then, rather than when he
        /// first wears it, if that is how it was made. A bag is the exception:
        /// one that binds on being worn binds when it is hung up.
        static bool BindsOnArrival(ItemPrototype const& proto, uint16 place);

        Player& m_owner;
        Item* m_place[PLAYER_SLOTS_COUNT];
        uint32 m_nextBuyback;

        /// Items whose own life is running down. The remaining time is theirs,
        /// not this list's; the list only says which ones to ask.
        std::list<Item*> m_running;

        /// Temporary enchantments still running, with the time left on each.
        std::list<EnchantClock> m_runningEnchants;
};
