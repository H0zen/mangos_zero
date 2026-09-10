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

#include "Utilities/MathDefines.h"
#include "Common/Locales.h"
#include <algorithm>
#include <cmath>
#include "Utilities/Errors.h"
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <list>
#include "DBCStores.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SharedDefines.h"

#include "DBCfmt.h"

#include <map>

typedef std::map<uint32, uint32> AreaIDByAreaFlag;
typedef std::map<uint32, uint32> AreaFlagByMapID;

struct WMOAreaTableTripple
{
    WMOAreaTableTripple(int32 r, int32 a, int32 g) : groupId(g), rootId(r), adtId(a)
    {
    }

    bool operator <(const WMOAreaTableTripple& b) const
    {
        return memcmp(this, &b, sizeof(WMOAreaTableTripple)) < 0;
    }

    int32 groupId;
    int32 rootId;
    int32 adtId;
};

typedef std::map<WMOAreaTableTripple, WMOAreaTableEntry const*> WMOAreaInfoByTripple;

DBCStorage <AreaTableEntry> sAreaStore(AreaTableEntryfmt);
static AreaIDByAreaFlag sAreaIDByAreaFlag;
static AreaFlagByMapID  sAreaFlagByMapID;

static WMOAreaInfoByTripple sWMOAreaInfoByTripple;

DBCStorage <AreaTriggerEntry> sAreaTriggerStore(AreaTriggerEntryfmt);
DBCStorage <AuctionHouseEntry> sAuctionHouseStore(AuctionHouseEntryfmt);
DBCStorage <BankBagSlotPricesEntry> sBankBagSlotPricesStore(BankBagSlotPricesEntryfmt);
DBCStorage <CharStartOutfitEntry> sCharStartOutfitStore(CharStartOutfitEntryfmt);
DBCStorage <ChatChannelsEntry> sChatChannelsStore(ChatChannelsEntryfmt);
DBCStorage <ChrClassesEntry> sChrClassesStore(ChrClassesEntryfmt);
DBCStorage <ChrRacesEntry> sChrRacesStore(ChrRacesEntryfmt);
DBCStorage <CinematicSequencesEntry> sCinematicSequencesStore(CinematicSequencesEntryfmt);
DBCStorage <CreatureDisplayInfoEntry> sCreatureDisplayInfoStore(CreatureDisplayInfofmt);
DBCStorage <CreatureDisplayInfoExtraEntry> sCreatureDisplayInfoExtraStore(CreatureDisplayInfoExtrafmt);
DBCStorage <CreatureFamilyEntry> sCreatureFamilyStore(CreatureFamilyfmt);
DBCStorage <CreatureSpellDataEntry> sCreatureSpellDataStore(CreatureSpellDatafmt);
DBCStorage <CreatureTypeEntry> sCreatureTypeStore(CreatureTypefmt);

DBCStorage <DurabilityQualityEntry> sDurabilityQualityStore(DurabilityQualityfmt);
DBCStorage <DurabilityCostsEntry> sDurabilityCostsStore(DurabilityCostsfmt);

DBCStorage <EmotesEntry> sEmotesStore(EmotesEntryfmt);
DBCStorage <EmotesTextEntry> sEmotesTextStore(EmotesTextEntryfmt);

typedef std::map<uint32, SimpleFactionsList> FactionTeamMap;
static FactionTeamMap sFactionTeamMap;
DBCStorage <FactionEntry> sFactionStore(FactionEntryfmt);
DBCStorage <FactionTemplateEntry> sFactionTemplateStore(FactionTemplateEntryfmt);

DBCStorage <GameObjectDisplayInfoEntry> sGameObjectDisplayInfoStore(GameObjectDisplayInfofmt);

DBCStorage <ItemBagFamilyEntry>           sItemBagFamilyStore(ItemBagFamilyfmt);
DBCStorage <ItemClassEntry>               sItemClassStore(ItemClassfmt);
DBCStorage <ItemRandomPropertiesEntry> sItemRandomPropertiesStore(ItemRandomPropertiesfmt);
DBCStorage <ItemSetEntry> sItemSetStore(ItemSetEntryfmt);
DBCStorage <LiquidTypeEntry> sLiquidTypeStore(LiquidTypefmt);
DBCStorage <LockEntry> sLockStore(LockEntryfmt);

DBCStorage <MailTemplateEntry> sMailTemplateStore(MailTemplateEntryfmt);

DBCStorage <MapEntry> sMapStore(MapEntryfmt);

DBCStorage <QuestSortEntry> sQuestSortStore(QuestSortEntryfmt);

DBCStorage <SkillLineEntry> sSkillLineStore(SkillLinefmt);
DBCStorage <SkillLineAbilityEntry> sSkillLineAbilityStore(SkillLineAbilityfmt);
DBCStorage <SkillRaceClassInfoEntry> sSkillRaceClassInfoStore(SkillRaceClassInfofmt);

DBCStorage <SoundEntriesEntry> sSoundEntriesStore(SoundEntriesfmt);

DBCStorage <SpellItemEnchantmentEntry> sSpellItemEnchantmentStore(SpellItemEnchantmentfmt);
DBCStorage <SpellEntry> sSpellStore(SpellEntryfmt);
DBCStorage <SpellFocusObjectEntry> sSpellFocusObjectStore(SpellFocusObjectfmt);
SpellCategoryStore sSpellCategoryStore;
PetFamilySpellsStore sPetFamilySpellsStore;

DBCStorage <SpellCastTimesEntry> sSpellCastTimesStore(SpellCastTimefmt);
DBCStorage <SpellDurationEntry> sSpellDurationStore(SpellDurationfmt);
DBCStorage <SpellRadiusEntry> sSpellRadiusStore(SpellRadiusfmt);
DBCStorage <SpellRangeEntry> sSpellRangeStore(SpellRangefmt);
DBCStorage <SpellShapeshiftFormEntry> sSpellShapeshiftFormStore(SpellShapeshiftfmt);
DBCStorage <StableSlotPricesEntry> sStableSlotPricesStore(StableSlotPricesfmt);
DBCStorage <TalentEntry> sTalentStore(TalentEntryfmt);
TalentSpellPosMap sTalentSpellPosMap;
DBCStorage <TalentTabEntry> sTalentTabStore(TalentTabEntryfmt);

typedef std::map<uint32, uint32> TalentInspectMap;
static TalentInspectMap sTalentPosInInspect;
static TalentInspectMap sTalentTabSizeInInspect;
static uint32 sTalentTabPages[12][3];

DBCStorage <TaxiNodesEntry> sTaxiNodesStore(TaxiNodesEntryfmt);
TaxiMask sTaxiNodesMask;

TaxiPathSetBySource sTaxiPathSetBySource;
DBCStorage <TaxiPathEntry> sTaxiPathStore(TaxiPathEntryfmt);

TaxiPathNodesByPath sTaxiPathNodesByPath;
static DBCStorage <TaxiPathNodeEntry> sTaxiPathNodeStore(TaxiPathNodeEntryfmt);

TransportAnimationsByEntry sTransportAnimationsByEntry;
static DBCStorage <TransportAnimationEntry> sTransportAnimationStore(TransportAnimationEntryfmt);

DBCStorage <WMOAreaTableEntry>  sWMOAreaTableStore(WMOAreaTableEntryfmt);
DBCStorage <WorldMapAreaEntry>  sWorldMapAreaStore(WorldMapAreaEntryfmt);

DBCStorage <WorldSafeLocsEntry> sWorldSafeLocsStore(WorldSafeLocsEntryfmt);

typedef std::list<std::string> StoreProblemList;

bool IsAcceptableClientBuild(uint32 build)
{
    int accepted_versions[] = EXPECTED_MANGOSD_CLIENT_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
    {
        if (int(build) == accepted_versions[i])
        {
            return true;
        }
    }
    return false;
}

std::string AcceptableClientBuildsListStr()
{
    std::ostringstream data;
    int accepted_versions[] = EXPECTED_MANGOSD_CLIENT_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
    {
        data << accepted_versions[i] << " ";
    }
    return data.str();
}

static bool LoadDBC_assert_print(uint32 fsize, uint32 rsize, const std::string& filename)
{
    sLog.outError("Size of '%s' setted by format string (%u) not equal size of C++ structure (%u).", filename.c_str(), fsize, rsize);

    return false;
}

template<class T>

inline void LoadDBC(uint32& availableDbcLocales, BarGoLink& bar, StoreProblemList& errlist, DBCStorage<T>& storage, const std::string& dbc_path, const std::string& filename)
{

    MANGOS_ASSERT(DBCFileLoader::GetFormatRecordSize(storage.GetFormat()) == sizeof(T) || LoadDBC_assert_print(DBCFileLoader::GetFormatRecordSize(storage.GetFormat()), sizeof(T), filename));

    std::string dbc_filename = dbc_path + filename;
    if (storage.Load(dbc_filename.c_str()))
    {
        bar.step();
        for (uint8 i = 0; fullLocaleNameList[i].name; ++i)
        {
            if (!(availableDbcLocales & (1 << i)))
            {
                continue;
            }

            std::string dbc_filename_loc = dbc_path + fullLocaleNameList[i].name + "/" + filename;
            if (!storage.LoadStringsFrom(dbc_filename_loc.c_str()))
            {
                availableDbcLocales &= ~(1 << i);
            }
        }
    }
    else
    {

        FILE* f = fopen(dbc_filename.c_str(), "rb");
        if (f)
        {
            char buf[100];
            snprintf(buf, 100, " (exist, but have %u fields instead %zu) Wrong client version DBC file?", storage.GetFieldCount(), strlen(storage.GetFormat()));
            errlist.push_back(dbc_filename + buf);
            fclose(f);
        }
        else
        {
            errlist.push_back(dbc_filename);
        }
    }
}

void LoadDBCStores(const std::string& dataPath)
{
    std::string dbcPath = dataPath + "dbc/";

    const uint32 DBCFilesCount = 50;

    BarGoLink bar(DBCFilesCount);

    StoreProblemList bad_dbc_files;

    uint32 availableDbcLocales = 0xFFFFFFFF;

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAreaStore,                dbcPath, "AreaTable.dbc");

    for (uint32 i = 1; i <= sAreaStore.GetNumRows(); ++i)
    {
        if (AreaTableEntry const* area = sAreaStore.LookupEntry(i))
        {

            sAreaIDByAreaFlag.insert(AreaIDByAreaFlag::value_type(uint16(area->AreaBit), area->ID));

            if (area->ParentAreaID == 0 && area->ContinentID != 0 && area->ContinentID != 1)
            {
                sAreaFlagByMapID.insert(AreaFlagByMapID::value_type(area->ContinentID, area->AreaBit));
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAreaTriggerStore,         dbcPath, "AreaTrigger.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAuctionHouseStore,        dbcPath, "AuctionHouse.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sBankBagSlotPricesStore,   dbcPath, "BankBagSlotPrices.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCharStartOutfitStore,     dbcPath, "CharStartOutfit.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChatChannelsStore,        dbcPath, "ChatChannels.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChrClassesStore,          dbcPath, "ChrClasses.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChrRacesStore,            dbcPath, "ChrRaces.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCinematicSequencesStore,  dbcPath, "CinematicSequences.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureDisplayInfoStore, dbcPath, "CreatureDisplayInfo.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureDisplayInfoExtraStore, dbcPath, "CreatureDisplayInfoExtra.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureFamilyStore,      dbcPath, "CreatureFamily.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureSpellDataStore,   dbcPath, "CreatureSpellData.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCreatureTypeStore,        dbcPath, "CreatureType.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sDurabilityCostsStore,     dbcPath, "DurabilityCosts.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sDurabilityQualityStore,   dbcPath, "DurabilityQuality.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sEmotesStore,              dbcPath, "Emotes.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sEmotesTextStore,          dbcPath, "EmotesText.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sFactionStore,             dbcPath, "Faction.dbc");
    for (uint32 i = 0; i < sFactionStore.GetNumRows(); ++i)
    {
        FactionEntry const* faction = sFactionStore.LookupEntry(i);
        if (faction && faction->ParentFactionID)
        {
            SimpleFactionsList& flist = sFactionTeamMap[faction->ParentFactionID];
            flist.push_back(i);
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sFactionTemplateStore,     dbcPath, "FactionTemplate.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGameObjectDisplayInfoStore, dbcPath, "GameObjectDisplayInfo.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemBagFamilyStore,       dbcPath, "ItemBagFamily.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemClassStore,           dbcPath, "ItemClass.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemRandomPropertiesStore, dbcPath, "ItemRandomProperties.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemSetStore,             dbcPath, "ItemSet.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sLiquidTypeStore,          dbcPath, "LiquidType.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sLockStore,                dbcPath, "Lock.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sMailTemplateStore,        dbcPath, "MailTemplate.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sMapStore,                 dbcPath, "Map.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sQuestSortStore,           dbcPath, "QuestSort.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillLineStore,           dbcPath, "SkillLine.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillLineAbilityStore,    dbcPath, "SkillLineAbility.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillRaceClassInfoStore,  dbcPath, "SkillRaceClassInfo.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSoundEntriesStore,        dbcPath, "SoundEntries.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellStore,               dbcPath, "Spell.dbc");
    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(i);
        if (spell && spell->Category)
        {
            sSpellCategoryStore[spell->Category].insert(i);
        }

#if MANGOS_ENDIAN == MANGOS_BIGENDIAN
        std::swap(*((uint32*)(&spell->SpellFamilyFlags)), *(((uint32*)(&spell->SpellFamilyFlags)) + 1));
#endif
    }

    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* skillLine = sSkillLineAbilityStore.LookupEntry(j);

        if (!skillLine)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(skillLine->Spell);
        if (spellInfo && (spellInfo->Attributes & (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE | SPELL_ATTR_HIDDEN_CLIENTSIDE | SPELL_ATTR_HIDE_IN_COMBAT_LOG)) == (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE | SPELL_ATTR_HIDDEN_CLIENTSIDE | SPELL_ATTR_HIDE_IN_COMBAT_LOG))
        {
            for (unsigned int i = 1; i < sCreatureFamilyStore.GetNumRows(); ++i)
            {
                CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(i);
                if (!cFamily)
                {
                    continue;
                }

                if (skillLine->SkillLine != cFamily->SkillLine[0] && skillLine->SkillLine != cFamily->SkillLine[1])
                {
                    continue;
                }

                sPetFamilySpellsStore[i].insert(spellInfo->ID);
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellCastTimesStore,      dbcPath, "SpellCastTimes.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellDurationStore,       dbcPath, "SpellDuration.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellFocusObjectStore,    dbcPath, "SpellFocusObject.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellItemEnchantmentStore, dbcPath, "SpellItemEnchantment.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellRadiusStore,         dbcPath, "SpellRadius.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellRangeStore,          dbcPath, "SpellRange.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellShapeshiftFormStore, dbcPath, "SpellShapeshiftForm.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sStableSlotPricesStore,    dbcPath, "StableSlotPrices.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTalentStore,              dbcPath, "Talent.dbc");

    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
        {
            continue;
        }
        for (int j = 0; j < 5; ++j)
        {
            if (talentInfo->RankID[j])
            {
                sTalentSpellPosMap[talentInfo->RankID[j]] = TalentSpellPos(i, j);
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTalentTabStore,           dbcPath, "TalentTab.dbc");

    {

        typedef std::map<uint32, uint32> TalentBitSize;
        TalentBitSize sTalentBitSize;
        for (uint32 i = 1; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
            if (!talentInfo)
            {
                continue;
            }

            TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!talentTabInfo)
            {
                continue;
            }

            uint32 curtalent_maxrank = 0;
            for (uint32 k = 5; k > 0; --k)
            {
                if (talentInfo->RankID[k - 1])
                {
                    curtalent_maxrank = k;
                    break;
                }
            }

            sTalentBitSize[(talentInfo->Row << 24) + (talentInfo->Col << 16) + talentInfo->TalentID] = curtalent_maxrank;
            sTalentTabSizeInInspect[talentInfo->TalentTab] += curtalent_maxrank;
        }

        for (uint32 talentTabId = 1; talentTabId < sTalentTabStore.GetNumRows(); ++talentTabId)
        {
            TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentTabId);
            if (!talentTabInfo)
            {
                continue;
            }

            if ((talentTabInfo->ClassMask & CLASSMASK_ALL_PLAYABLE) == 0)
            {
                continue;
            }

            uint32 cls = 1;
            for (uint32 m = 1; !(m & talentTabInfo->ClassMask) && cls < 12 ; m <<= 1, ++cls)
            {

            }

            sTalentTabPages[cls][talentTabInfo->OrderIndex] = talentTabId;

            uint32 pos = 0;
            for (TalentBitSize::iterator itr = sTalentBitSize.begin(); itr != sTalentBitSize.end(); ++itr)
            {
                uint32 talentId = itr->first & 0xFFFF;
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
                if (!talentInfo)
                {
                    continue;
                }

                if (talentInfo->TalentTab != talentTabId)
                {
                    continue;
                }

                sTalentPosInInspect[talentId] = pos;
                pos += itr->second;
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiNodesStore,           dbcPath, "TaxiNodes.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiPathStore,            dbcPath, "TaxiPath.dbc");
    for (uint32 i = 1; i < sTaxiPathStore.GetNumRows(); ++i)
    {
        if (TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(i))
        {
            sTaxiPathSetBySource[entry->from][entry->to] = TaxiPathBySourceAndDestination(entry->ID, entry->price);
        }
    }
    uint32 pathCount = sTaxiPathStore.GetNumRows();

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiPathNodeStore,        dbcPath, "TaxiPathNode.dbc");

    std::vector<uint32> pathLength;
    pathLength.resize(pathCount);
    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
    {
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
        {
            if (pathLength[entry->PathID] < entry->NodeIndex + 1)
            {
                pathLength[entry->PathID] = entry->NodeIndex + 1;
            }
        }
    }

    sTaxiPathNodesByPath.resize(pathCount);
    for (uint32 i = 1; i < sTaxiPathNodesByPath.size(); ++i)
    {
        sTaxiPathNodesByPath[i].resize(pathLength[i]);
    }

    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
    {
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
        {
            sTaxiPathNodesByPath[entry->PathID].set(entry->NodeIndex, entry);
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTransportAnimationStore, dbcPath, "TransportAnimation.dbc");
    for (uint32 i = 1; i < sTransportAnimationStore.GetNumRows(); ++i)
    {
        if (TransportAnimationEntry const* frame = sTransportAnimationStore.LookupEntry(i))
        {
            sTransportAnimationsByEntry[frame->TransportID].push_back(frame);
        }
    }

    for (auto& lift : sTransportAnimationsByEntry)
    {
        std::sort(lift.second.begin(), lift.second.end(),
                  [](TransportAnimationEntry const* a, TransportAnimationEntry const* b)
                  { return a->TimeIndex < b->TimeIndex; });
    }

    {
        std::set<uint32> spellPaths;
        for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
        {
            if (SpellEntry const* sInfo = sSpellStore.LookupEntry(i))
            {
                for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                {
                    if (sInfo->Effect[j] == 123 )
                    {
                        spellPaths.insert(sInfo->EffectMiscValue[j]);
                    }
                }
            }
        }
        memset(sTaxiNodesMask, 0, sizeof(sTaxiNodesMask));
        for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
        {
            TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i);
            if (!node)
            {
                continue;
            }

            TaxiPathSetBySource::const_iterator src_i = sTaxiPathSetBySource.find(i);
            if (src_i != sTaxiPathSetBySource.end() && !src_i->second.empty())
            {
                bool ok = false;
                for (TaxiPathSetForSource::const_iterator dest_i = src_i->second.begin(); dest_i != src_i->second.end(); ++dest_i)
                {

                    if (spellPaths.find(dest_i->second.ID) == spellPaths.end())
                    {
                        ok = true;
                        break;
                    }
                }

                if (!ok)
                {
                    continue;
                }
            }

            uint8  field   = (uint8)((i - 1) / 32);
            uint32 submask = 1 << ((i - 1) % 32);
            sTaxiNodesMask[field] |= submask;
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWorldMapAreaStore,        dbcPath, "WorldMapArea.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWMOAreaTableStore,        dbcPath, "WMOAreaTable.dbc");
    for (uint32 i = 0; i < sWMOAreaTableStore.GetNumRows(); ++i)
    {
        if (WMOAreaTableEntry const* entry = sWMOAreaTableStore.LookupEntry(i))
        {
            sWMOAreaInfoByTripple.insert(WMOAreaInfoByTripple::value_type(WMOAreaTableTripple(entry->WMOID, entry->NameSetID, entry->WMOGroupID), entry));
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWorldSafeLocsStore,       dbcPath, "WorldSafeLocs.dbc");

    if (bad_dbc_files.size() >= DBCFilesCount)
    {
        sLog.outError("\nIncorrect DataDir value in mangosd.conf or ALL required *.dbc files (%d) not found by path: %sdbc", DBCFilesCount, dataPath.c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }
    else if (!bad_dbc_files.empty())
    {
        std::string str;
        for (std::list<std::string>::iterator i = bad_dbc_files.begin(); i != bad_dbc_files.end(); ++i)
        {
            str += *i + "\n";
        }

        sLog.outError("\nSome required *.dbc files (%u from %d) not found or not compatible:\n%s", (uint32)bad_dbc_files.size(), DBCFilesCount, str.c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    if (!sSpellStore.LookupEntry(33392)            ||
        !sSkillLineAbilityStore.LookupEntry(15030) ||
        !sMapStore.LookupEntry(533)                ||
        !sAreaStore.LookupEntry(3486))
    {
        sLog.outError("\nYou have _outdated_ DBC files. Please re-extract DBC files for one from client build: %s", AcceptableClientBuildsListStr().c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    sLog.outString(">> Initialized %d data stores", DBCFilesCount);
    sLog.outString();
}

SimpleFactionsList const* GetFactionTeamList(uint32 faction)
{
    FactionTeamMap::const_iterator itr = sFactionTeamMap.find(faction);
    if (itr == sFactionTeamMap.end())
    {
        return nullptr;
    }
    return &itr->second;
}

char const* GetPetName(uint32 petfamily, uint32 dbclang)
{
    if (!petfamily)
    {
        return nullptr;
    }
    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(petfamily);
    if (!pet_family)
    {
        return nullptr;
    }
    return pet_family->Name_lang[dbclang] ? pet_family->Name_lang[dbclang] : nullptr;
}

TalentSpellPos const* GetTalentSpellPos(uint32 spellId)
{
    TalentSpellPosMap::const_iterator itr = sTalentSpellPosMap.find(spellId);
    if (itr == sTalentSpellPosMap.end())
    {
        return nullptr;
    }

    return &itr->second;
}

uint32 GetTalentSpellCost(TalentSpellPos const* pos)
{
    if (pos)
    {
        return pos->rank + 1;
    }

    return 0;
}

uint32 GetTalentSpellCost(uint32 spellId)
{
    return GetTalentSpellCost(GetTalentSpellPos(spellId));
}

int32 GetAreaFlagByAreaID(uint32 area_id)
{
    AreaTableEntry const* AreaEntry = sAreaStore.LookupEntry(area_id);
    if (!AreaEntry)
    {
        return -1;
    }

    return AreaEntry->AreaBit;
}

WMOAreaTableEntry const* GetWMOAreaTableEntryByTripple(int32 rootid, int32 adtid, int32 groupid)
{
    WMOAreaInfoByTripple::iterator i = sWMOAreaInfoByTripple.find(WMOAreaTableTripple(rootid, adtid, groupid));
    if (i == sWMOAreaInfoByTripple.end())
    {
        return nullptr;
    }
    return i->second;
}

AreaTableEntry const* GetAreaEntryByAreaID(uint32 area_id)
{
    return sAreaStore.LookupEntry(area_id);
}

AreaTableEntry const* GetAreaEntryByAreaFlagAndMap(uint32 area_flag, uint32 map_id)
{

    static std::map<uint64, AreaTableEntry const*> cache;
    uint64 cacheKey = (static_cast<uint64>(area_flag) << 32) | static_cast<uint64>(map_id);
    auto it = cache.find(cacheKey);
    if (it != cache.end())
    {
        return it->second;
    }

    AreaTableEntry const* aEntry = nullptr;
    for (uint32 i = 0 ; i <= sAreaStore.GetNumRows() ; i++)
    {
        if (area_flag != 0)
        {
            if (AreaTableEntry const* AreaEntry = sAreaStore.LookupEntry(i))
            {
                if (AreaEntry->AreaBit == area_flag)
                {

                    if (AreaEntry->ContinentID == map_id)
                    {
                        cache[cacheKey] = AreaEntry;
                        return AreaEntry;
                    }

                    aEntry = AreaEntry;
                }
            }
        }
    }

    if (aEntry)
    {
        cache[cacheKey] = aEntry;
        return aEntry;
    }

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(map_id))
    {
        AreaTableEntry const* result = GetAreaEntryByAreaID(mapEntry->AreaTableID);
        cache[cacheKey] = result;
        return result;
    }

    cache[cacheKey] = nullptr;
    return nullptr;
}

uint32 GetAreaFlagByMapId(uint32 mapid)
{
    AreaFlagByMapID::iterator i = sAreaFlagByMapID.find(mapid);
    if (i == sAreaFlagByMapID.end())
    {
        return 0;
    }
    else
    {
        return i->second;
    }
}

ChatChannelsEntry const* GetChannelEntryFor(uint32 channel_id)
{

    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        ChatChannelsEntry const* ch = sChatChannelsStore.LookupEntry(i);
        if (ch && ch->ID == channel_id)
        {
            return ch;
        }
    }
    return nullptr;
}

static ChatChannelsEntry worldCh = { 26, 4, "world" };

ChatChannelsEntry const* GetChannelEntryFor(const std::string& name)
{

    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        ChatChannelsEntry const* ch = sChatChannelsStore.LookupEntry(i);
        if (ch)
        {

            std::string entryName(ch->Name_lang[0]);
            std::size_t removeString = entryName.find("%s");

            if (removeString != std::string::npos)
            {
                entryName.replace(removeString, 2, "");
            }

            if (name.find(entryName) != std::string::npos)
            {
                return ch;
            }
        }
    }

    bool compare = true;
    std::string world = "world";
    for (uint8 i = 0; i < name.length(); ++i)
    {
        if (tolower(name[i]) != world[i])
        {
            compare = false;
            break;
        }
    }

    if (compare)
    {
        return &worldCh;
    }

    return nullptr;
}

bool Zone2MapCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    if (!maEntry || maEntry->LocBottom == maEntry->LocTop || maEntry->LocRight == maEntry->LocLeft)
    {
        return false;
    }

    std::swap(x, y);
    x = x * ((maEntry->LocBottom - maEntry->LocTop) / 100) + maEntry->LocTop;
    y = y * ((maEntry->LocRight - maEntry->LocLeft) / 100) + maEntry->LocLeft;

    return true;
}

bool Map2ZoneCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    if (!maEntry || maEntry->LocBottom == maEntry->LocTop || maEntry->LocRight == maEntry->LocLeft)
    {
        return false;
    }

    x = (x - maEntry->LocTop) / ((maEntry->LocBottom - maEntry->LocTop) / 100);
    y = (y - maEntry->LocLeft) / ((maEntry->LocRight - maEntry->LocLeft) / 100);
    std::swap(x, y);

    return true;
}

uint32 GetTalentInspectBitPosInTab(uint32 talentId)
{
    TalentInspectMap::const_iterator itr = sTalentPosInInspect.find(talentId);
    if (itr == sTalentPosInInspect.end())
    {
        return 0;
    }

    return itr->second;
}

uint32 GetTalentTabInspectBitSize(uint32 talentTabId)
{
    TalentInspectMap::const_iterator itr = sTalentTabSizeInInspect.find(talentTabId);
    if (itr == sTalentTabSizeInInspect.end())
    {
        return 0;
    }

    return itr->second;
}

uint32 const* GetTalentTabPages(uint32 cls)
{
    return sTalentTabPages[cls];
}

bool IsPointInAreaTriggerZone(AreaTriggerEntry const* atEntry, uint32 mapid, float x, float y, float z, float delta)
{
    if (mapid != atEntry->mapid)
    {
        return false;
    }

    if (atEntry->radius > 0)
    {

        float dist2 = (x - atEntry->x) * (x - atEntry->x) + (y - atEntry->y) * (y - atEntry->y) + (z - atEntry->z) * (z - atEntry->z);
        if (dist2 > (atEntry->radius + delta) * (atEntry->radius + delta))
        {
            return false;
        }
    }
    else
    {

        double rotation = 2 * M_PI - atEntry->box_yaw;
        double sinVal = sin(rotation);
        double cosVal = cos(rotation);

        float playerBoxDistX = x - atEntry->x;
        float playerBoxDistY = y - atEntry->y;

        float rotPlayerX = float(atEntry->x + playerBoxDistX * cosVal - playerBoxDistY * sinVal);
        float rotPlayerY = float(atEntry->y + playerBoxDistY * cosVal + playerBoxDistX * sinVal);

        float dz = z - atEntry->z;
        float dx = rotPlayerX - atEntry->x;
        float dy = rotPlayerY - atEntry->y;
        if ((fabs(dx) > atEntry->box_length / 2 + delta) ||
            (fabs(dy) > atEntry->box_width / 2 + delta) ||
            (fabs(dz) > atEntry->box_height / 2 + delta))
        {
            return false;
        }
    }

    return true;
}

uint32 GetCreatureModelRace(uint32 model_id)
{
    CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(model_id);
    if (!displayEntry)
    {
        return 0;
    }
    CreatureDisplayInfoExtraEntry const* extraEntry = sCreatureDisplayInfoExtraStore.LookupEntry(displayEntry->ExtendedDisplayInfoID);
    return extraEntry ? extraEntry->Race : 0;
}

DBCStorage <SoundEntriesEntry>  const* GetSoundEntriesStore()   { return &sSoundEntriesStore;   }
DBCStorage <SpellEntry>         const* GetSpellStore()          { return &sSpellStore;          }
DBCStorage <SpellRangeEntry>    const* GetSpellRangeStore()     { return &sSpellRangeStore;     }
DBCStorage <FactionEntry>       const* GetFactionStore()        { return &sFactionStore;        }
DBCStorage <CreatureDisplayInfoEntry> const* GetCreatureDisplayStore()
{
    return &sCreatureDisplayInfoStore;
}

DBCStorage <EmotesEntry>        const* GetEmotesStore()         { return &sEmotesStore;         }
DBCStorage <EmotesTextEntry>    const* GetEmotesTextStore()     { return &sEmotesTextStore;     }
