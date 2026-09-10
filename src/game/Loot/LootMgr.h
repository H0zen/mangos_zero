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

#include <unordered_map>
#include <cmath>
#include <map>
#include <set>
#include "ByteBuffer.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "Utilities/LinkedReference/RefManager.h"

#include <vector>

class Player;
class LootStore;
class Occupant;

#define MAX_NR_LOOT_ITEMS 16

#define MAX_NR_QUEST_ITEMS 32

enum PermissionTypes : int
{
    ALL_PERMISSION    = 0,
    GROUP_PERMISSION  = 1,
    MASTER_PERMISSION = 2,
    OWNER_PERMISSION  = 3,
    NONE_PERMISSION   = 4
};

enum LootType : int
{
    LOOT_CORPSE                 = 1,
    LOOT_PICKPOCKETING          = 2,
    LOOT_FISHING                = 3,
    LOOT_DISENCHANTING          = 4,

    LOOT_SKINNING               = 6,

    LOOT_FISHINGHOLE            = 20,
    LOOT_FISHING_FAIL           = 21,
    LOOT_INSIGNIA               = 22
};

enum LootSlotType
{
    LOOT_SLOT_NORMAL  = 0,
    LOOT_SLOT_VIEW    = 1,
    LOOT_SLOT_MASTER  = 2,
    LOOT_SLOT_REQS    = 3,
    MAX_LOOT_SLOT_TYPE
};

namespace loot
{

    struct DropRates
    {
        float byQuality[MAX_ITEM_QUALITY] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        float referenced = 1.0f;
    };

    inline float ChanceOf(float stated, bool rated, bool isReference, uint32 quality,
                          DropRates const& rates)
    {
        if (stated >= 100.0f || !rated)
        {
            return stated;
        }

        if (isReference)
        {
            return stated * rates.referenced;
        }

        return stated * (quality < MAX_ITEM_QUALITY ? rates.byQuality[quality] : 1.0f);
    }

    inline uint32 Coin(uint32 least, uint32 most, uint32 rolled, float rate)
    {
        if (most == 0)
        {
            return 0;
        }

        if (most <= least)
        {
            return uint32(most * rate);
        }

        return uint32(rolled * rate);
    }
}

struct LootStoreItem
{
    uint32  itemid;
    float   chance;
    int32   mincountOrRef;
    uint8   group       : 7;
    bool    needs_quest : 1;
    uint8   maxcount    : 8;
    uint16  conditionId : 16;

    LootStoreItem(uint32 _itemid, float _chanceOrQuestChance, int8 _group, uint16 _conditionId, int32 _mincountOrRef, uint8 _maxcount)
        : itemid(_itemid), chance(fabs(_chanceOrQuestChance)), mincountOrRef(_mincountOrRef),
        group(_group), needs_quest(_chanceOrQuestChance < 0), maxcount(_maxcount), conditionId(_conditionId)
    {}

    bool Roll(bool rate) const;
    bool IsValid(LootStore const& store, uint32 entry) const;

};

struct LootItem
{
    uint32  itemid;
    int32   randomPropertyId;
    uint16  conditionId       : 16;
    uint8   count             : 8;
    bool    is_looted         : 1;
    bool    is_blocked        : 1;
    bool    freeforall        : 1;
    bool    is_underthreshold : 1;
    bool    is_counted        : 1;
    bool    needs_quest       : 1;

    ObjectGuid winner = 0;

    explicit LootItem(LootStoreItem const& li);

    LootItem(uint32 itemid_, uint32 count_, int32 randomPropertyId_ = 0);

    bool AllowedForPlayer(Player const* player, Occupant const* lootTarget) const;
    LootSlotType GetSlotTypeForSharedLoot(PermissionTypes permission, Player* viewer, Occupant const* lootTarget, bool condition_ok = false) const;
};

typedef std::vector<LootItem> LootItemList;

struct QuestItem
{
    uint8   index;
    bool    is_looted;

    QuestItem()
        : index(0), is_looted(false) {}

    QuestItem(uint8 _index, bool _islooted = false)
        : index(_index), is_looted(_islooted) {}
};

struct Loot;
class LootTemplate;

typedef std::vector<QuestItem> QuestItemList;
typedef std::map<uint32, QuestItemList*> QuestItemMap;
typedef std::vector<LootStoreItem> LootStoreItemList;
typedef std::unordered_map<uint32, LootTemplate*> LootTemplateMap;

typedef std::set<uint32> LootIdSet;

class LootStore
{
    public:
        explicit LootStore(char const* name, char const* entryName, bool ratesAllowed)
            : m_name(name), m_entryName(entryName), m_ratesAllowed(ratesAllowed) {}
        virtual ~LootStore()
        {
            Clear();
        }

        void Verify() const;

        void LoadAndCollectLootIds(LootIdSet& ids_set);
        void CheckLootRefs(LootIdSet* ref_set = nullptr) const;
        void ReportUnusedIds(LootIdSet const& ids_set) const;
        void ReportNotExistedId(uint32 id) const;

        bool HaveLootFor(uint32 loot_id) const { return m_LootTemplates.find(loot_id) != m_LootTemplates.end(); }
        bool HaveQuestLootFor(uint32 loot_id) const;
        bool HaveQuestLootForPlayer(uint32 loot_id, Player* player) const;

        bool HaveSharedQuestLootForPlayer(uint32 loot_id, Player* player) const;

        bool HaveStartingQuestLootForPlayer(uint32 loot_id, Player* player) const;

        LootTemplate const* GetLootFor(uint32 loot_id) const;

        char const* GetName() const { return m_name; }
        char const* GetEntryName() const { return m_entryName; }
        bool IsRatesAllowed() const { return m_ratesAllowed; }
    protected:
        void LoadLootTable();
        void Clear();
    private:
        LootTemplateMap m_LootTemplates;
        char const* m_name;
        char const* m_entryName;
        bool m_ratesAllowed;
};

class LootTemplate
{
    class LootGroup;
    typedef std::vector<LootGroup> LootGroups;

    public:

        void AddEntry(LootStoreItem& item);

        void Process(Loot& loot, LootStore const& store, bool rate, uint8 GroupId = 0) const;

        bool HasQuestDrop(LootTemplateMap const& store, uint8 GroupId = 0) const;

        bool HasQuestDropForPlayer(LootTemplateMap const& store, Player const* player, uint8 GroupId = 0) const;

        bool HasSharedQuestDropForPlayer(LootTemplateMap const& store, Player const* player, uint8 GroupId = 0) const;

        bool HasStartingQuestDropForPlayer(LootTemplateMap const& store, Player const* player, uint8 GroupId = 0) const;

        void Verify(LootStore const& store, uint32 Id) const;
        void CheckLootRefs(LootIdSet* ref_set) const;
    private:
        LootStoreItemList Entries;
        LootGroups        Groups;
};

class LootValidatorRef :  public Reference<Loot, LootValidatorRef>
{
    public:
        LootValidatorRef() {}
        void targetObjectDestroyLink() override {}
        void sourceObjectDestroyLink() override {}
};

class LootValidatorRefManager : public RefManager<Loot, LootValidatorRef>
{
    public:
        typedef LinkedListHead::Iterator< LootValidatorRef > iterator;

        LootValidatorRef* getFirst()
        {
            return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getFirst();
        }

        LootValidatorRef* getLast()
        {
            return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getLast();
        }

        iterator begin()
        {
            return iterator(getFirst());
        }

        iterator end()
        {
            return iterator(nullptr);
        }

        iterator rbegin()
        {
            return iterator(getLast());
        }

        iterator rend()
        {
            return iterator(nullptr);
        }
};

struct LootView;

ByteBuffer& operator<<(ByteBuffer& b, LootItem const& li);

ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv);

struct Loot
{
    friend ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv);

    QuestItemMap const& GetPlayerQuestItems() const { return m_playerQuestItems; }
    QuestItemMap const& GetPlayerFFAItems() const { return m_playerFFAItems; }
    QuestItemMap const& GetPlayerNonQuestNonFFAConditionalItems() const { return m_playerNonQuestNonFFAConditionalItems; }

    LootItemList items;
    uint32 gold;
    uint8 unlootedCount;
    LootType loot_type;

    Loot(Occupant const* lootTarget, uint32 _gold = 0) : gold(_gold), unlootedCount(0), loot_type(LOOT_CORPSE), m_lootTarget(lootTarget) {}
    ~Loot()
    {
        clear();
    }

    void addLootValidatorRef(LootValidatorRef* pLootValidatorRef)
    {
        m_LootValidatorRefManager.insertFirst(pLootValidatorRef);
    }

    void clear()
    {
        for (QuestItemMap::const_iterator itr = m_playerQuestItems.begin(); itr != m_playerQuestItems.end(); ++itr)
        {
            delete itr->second;
        }
        m_playerQuestItems.clear();

        for (QuestItemMap::const_iterator itr = m_playerFFAItems.begin(); itr != m_playerFFAItems.end(); ++itr)
        {
            delete itr->second;
        }
        m_playerFFAItems.clear();

        for (QuestItemMap::const_iterator itr = m_playerNonQuestNonFFAConditionalItems.begin(); itr != m_playerNonQuestNonFFAConditionalItems.end(); ++itr)
        {
            delete itr->second;
        }
        m_playerNonQuestNonFFAConditionalItems.clear();

        m_playersLooting.clear();
        items.clear();
        m_questItems.clear();
        gold = 0;
        unlootedCount = 0;
        m_LootValidatorRefManager.clearReferences();
    }

    bool empty() const { return items.empty() && gold == 0; }
    bool isLooted() const { return gold == 0 && unlootedCount == 0; }

    void NotifyItemRemoved(uint8 lootIndex);
    void NotifyQuestItemRemoved(uint8 questIndex);
    void NotifyMoneyRemoved();
    void AddLooter(ObjectGuid guid) { m_playersLooting.insert(guid); }
    void RemoveLooter(ObjectGuid guid) { m_playersLooting.erase(guid); }

    bool IsWinner(Player * player);

    void generateMoneyLoot(uint32 minAmount, uint32 maxAmount);
    bool FillLoot(uint32 loot_id, LootStore const& store, Player* loot_owner, bool personal, bool noEmptyError = false);

    void AddItem(LootStoreItem const& item);

    LootItem* LootItemInSlot(uint32 lootslot, Player* player, QuestItem** qitem = nullptr, QuestItem** ffaitem = nullptr, QuestItem** conditem = nullptr);
    uint32 GetMaxSlotInLootFor(Player* player) const;

    Occupant const* GetLootTarget() const { return m_lootTarget; }

    private:
        void FillNotNormalLootFor(Player* player);
        QuestItemList* FillFFALoot(Player* player);
        QuestItemList* FillQuestLoot(Player* player);
        QuestItemList* FillNonQuestNonFFAConditionalLoot(Player* player);

        LootItemList m_questItems;

        GuidSet m_playersLooting;

        QuestItemMap m_playerQuestItems;
        QuestItemMap m_playerFFAItems;
        QuestItemMap m_playerNonQuestNonFFAConditionalItems;

        LootValidatorRefManager m_LootValidatorRefManager;

        Occupant const* m_lootTarget;
};

struct LootView
{
    Loot& loot;
    Player* viewer;
    PermissionTypes permission;
    LootView(Loot& _loot, Player* _viewer, PermissionTypes _permission = ALL_PERMISSION)
        : loot(_loot), viewer(_viewer), permission(_permission) {}
};

extern LootStore LootTemplates_Creature;
extern LootStore LootTemplates_Fishing;
extern LootStore LootTemplates_Gameobject;
extern LootStore LootTemplates_Item;
extern LootStore LootTemplates_Mail;
extern LootStore LootTemplates_Pickpocketing;
extern LootStore LootTemplates_Skinning;
extern LootStore LootTemplates_Disenchant;

void LoadLootTemplates_Creature();

void LoadLootTemplates_Fishing();

void LoadLootTemplates_Gameobject();

void LoadLootTemplates_Item();

void LoadLootTemplates_Mail();

void LoadLootTemplates_Pickpocketing();

void LoadLootTemplates_Skinning();

void LoadLootTemplates_Disenchant();

void LoadLootTemplates_Reference();

inline void LoadLootTables()
{
    LoadLootTemplates_Creature();
    LoadLootTemplates_Fishing();
    LoadLootTemplates_Gameobject();
    LoadLootTemplates_Item();
    LoadLootTemplates_Mail();
    LoadLootTemplates_Pickpocketing();
    LoadLootTemplates_Skinning();
    LoadLootTemplates_Disenchant();

    LoadLootTemplates_Reference();
}
