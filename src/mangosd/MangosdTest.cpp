#include "Utilities/Errors.h"
#include <vector>
#include "MangosdTest.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "Chat.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "AuctionHouseMgr.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "Object/AhUsabilityRef.h"
#include "Item.h"
#include "World.h"
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

/// Self-test for Database::CommitTransactionChecked(): proves the runtime
/// (async-enabled) path is synchronous, durable and returns the REAL result.
/// Returns 0 on pass, non-zero on fail.
static int RunCommitTest()
{
    bool pass = true;

    // Force the runtime async path so the transaction is FIFO-queued through
    // the delay thread and CommitTransactionChecked() blocks until durable.
    CharacterDatabase.AllowAsyncTransactions();

    // The test owns its table outright, so it depends on no schema beyond
    // itself: one column, one primary key, which is all the rollback path needs.
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `mangos_commit_selftest` "
        "(`k` VARCHAR(64) NOT NULL, PRIMARY KEY (`k`))");
    CharacterDatabase.DirectExecute("DELETE FROM `mangos_commit_selftest`");

    // (a) Success path: a valid INSERT must commit and be visible BEFORE return.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `mangos_commit_selftest` (`k`) VALUES ('ok')");
    bool ok = CharacterDatabase.CommitTransactionChecked();
    if (!ok)
    {
        printf("commit FAIL: success-path CommitTransactionChecked returned false\n");
        pass = false;
    }

    // Durability: a synchronous SELECT must see the row immediately after the
    // (blocking) checked commit returned - i.e. it is durable, not deferred.
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT 1 FROM `mangos_commit_selftest` WHERE `k`='ok'"));
        if (!res)
        {
            printf("commit FAIL: committed row not visible after return (not durable)\n");
            pass = false;
        }
    }

    // (b) Rollback path: a second row with the SAME key violates the primary
    // key, so the transaction must fail and CommitTransactionChecked() must
    // return false. The duplicate must NOT land.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `mangos_commit_selftest` (`k`) VALUES ('ok')");
    bool ok2 = CharacterDatabase.CommitTransactionChecked();
    if (ok2)
    {
        printf("commit FAIL: rollback-path CommitTransactionChecked returned true\n");
        pass = false;
    }

    // The duplicate must not have landed: still exactly one row for the key.
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mangos_commit_selftest` WHERE `k`='ok'"));
        if (!res || res->Fetch()[0].GetUInt64() != 1)
        {
            printf("commit FAIL: duplicate row landed after rollback\n");
            pass = false;
        }
    }

    // (c) Post-rollback TSS cleanliness: the false-returning rollback path above
    // must still have detached the transaction from the TSS slot. If it left a
    // residue, this third BeginTransaction() would trip MANGOS_ASSERT(!m_pTrans);
    // in TransHelper::init(); a clean detach lets a fresh checked commit succeed
    // and land its row.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `mangos_commit_selftest` (`k`) VALUES ('after')");
    bool ok3 = CharacterDatabase.CommitTransactionChecked();
    if (!ok3)
    {
        printf("commit FAIL: post-rollback CommitTransactionChecked returned false (TSS not clean?)\n");
        pass = false;
    }
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT 1 FROM `mangos_commit_selftest` WHERE `k`='after'"));
        if (!res)
        {
            printf("commit FAIL: post-rollback committed row not visible (not durable)\n");
            pass = false;
        }
    }

    CharacterDatabase.DirectExecute("DROP TABLE `mangos_commit_selftest`");

    if (pass)
    {
        printf("commit OK\n");
        return 0;
    }

    return 2;
}

/// Self-test for the AH bot forged system owner: proves GetPlayerGuidByName
/// returns the sentinel GUID for the reserved name without a DB row, that the
/// match is case-insensitive, and that ordinary non-existent names still fall
/// through to an empty guid. Returns 0 on pass, non-zero on fail.
static int RunAhOwnerTest()
{
    // Task 1: GetPlayerGuidByName intercepts the forged system name -> sentinel,
    // case-insensitively, with NO dependency on a characters row.
    {
        ObjectGuid g = sObjectMgr.GetPlayerGuidByName(AHBOT_SYSTEM_OWNER_NAME);
        if (!g.IsPlayer() || g.GetCounter() != AHBOT_SYSTEM_OWNER_GUID)
        {
            printf("ahowner FAIL: GetPlayerGuidByName(\"AuctionHouse\") did not return the sentinel\n");
            return 1;
        }
        ObjectGuid gl = sObjectMgr.GetPlayerGuidByName("auctionhouse");
        if (gl.GetCounter() != AHBOT_SYSTEM_OWNER_GUID)
        {
            printf("ahowner FAIL: name match is not case-insensitive\n");
            return 1;
        }
        // a clearly-nonexistent ordinary name must still fall through to 0.
        ObjectGuid none = sObjectMgr.GetPlayerGuidByName("Zzqxnonexistentname");
        if (none)
        {
            printf("ahowner FAIL: non-system name unexpectedly resolved\n");
            return 1;
        }
    }

    // Task 2: the player-GUID allocator never hands out the reserved sentinel.
    if (SkipAhBotSystemOwnerGuid(AHBOT_SYSTEM_OWNER_GUID) != AHBOT_SYSTEM_OWNER_GUID + 1)
    {
        printf("ahowner FAIL: SkipAhBotSystemOwnerGuid did not step past the sentinel\n");
        return 1;
    }
    if (SkipAhBotSystemOwnerGuid(42u) != 42u)
    {
        printf("ahowner FAIL: SkipAhBotSystemOwnerGuid altered a normal GUID\n");
        return 1;
    }

    // Task 3: the forged system name is always reserved from players, even if
    // the reserved_name table is empty (self-healing across wipes).
    sObjectMgr.LoadReservedPlayersNames();
    if (!sObjectMgr.IsReservedName(AHBOT_SYSTEM_OWNER_NAME))
    {
        printf("ahowner FAIL: \"AuctionHouse\" is not reserved after load\n");
        return 1;
    }

    // Task 4: the system-owner guid predicate the mail-gate keys on.
    if (!IsAhBotSystemOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, AHBOT_SYSTEM_OWNER_GUID)))
    {
        printf("ahowner FAIL: IsAhBotSystemOwnerGuid false for the sentinel\n");
        return 1;
    }
    if (IsAhBotSystemOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, 7u)))
    {
        printf("ahowner FAIL: IsAhBotSystemOwnerGuid true for a normal guid\n");
        return 1;
    }

    printf("ahowner OK\n");
    return 0;
}

namespace
{
    struct RefCtx
    {
        const uint16* skills;
        const uint32* skillIds;
        size_t nSkills;
        const uint32* spells;
        size_t nSpells;
        const uint32* repFactions;
        const uint8* repRanks;
        size_t nReps;
    };

    uint16 RefSkill(void* c, uint32 id)
    {
        RefCtx* x = (RefCtx*)c;
        for (size_t i = 0; i < x->nSkills; ++i)
        {
            if (x->skillIds[i] == id)
            {
                return x->skills[i];
            }
        }
        return 0;
    }
    bool RefSpell(void* c, uint32 id)
    {
        RefCtx* x = (RefCtx*)c;
        for (size_t i = 0; i < x->nSpells; ++i)
        {
            if (x->spells[i] == id)
            {
                return true;
            }
        }
        return false;
    }
    uint8 RefRep(void* c, uint32 f)
    {
        RefCtx* x = (RefCtx*)c;
        for (size_t i = 0; i < x->nReps; ++i)
        {
            if (x->repFactions[i] == f)
            {
                return x->repRanks[i];
            }
        }
        return 0;
    }
}

/// Self-test for AhUsabilityRef::Evaluate: drives the production reference
/// evaluator over a battery of synthetic profiles. Returns 0 on pass.
static int RunAhUsabilityRefTest()
{
    uint32 skillIds[1] = { 43u };
    uint16 skills[1]   = { 200u };
    uint32 spells[1]   = { 123u };
    uint32 repF[1]     = { 609u };
    uint8  repR[1]     = { 5u };
    RefCtx ctx;
    ctx.skills      = skills;
    ctx.skillIds    = skillIds;
    ctx.nSkills     = 1;
    ctx.spells      = spells;
    ctx.nSpells     = 1;
    ctx.repFactions = repF;
    ctx.repRanks    = repR;
    ctx.nReps       = 1;

    // warrior (class 1) / human (race 1); level 40; honor rank 2
    const uint32 cm    = 1u << (1u - 1u);
    const uint32 rm    = 1u << (1u - 1u);
    const uint32 level = 40u;
    const uint32 honor = 2u;
    const uint32 MM    = 40u;
    const uint32 EM    = 60u;

    AhRefItem it;
    it.itemClass            = 2u;
    it.allowableClass       = 0xFFFFFFFFu;
    it.allowableRace        = 0xFFFFFFFFu;
    it.requiredLevel        = 30u;
    it.itemId               = 12345u;
    it.requiredSkill        = 43u;
    it.requiredSkillRank    = 150u;
    it.requiredSpell        = 0u;
    it.requiredHonorRank    = 0u;
    it.requiredRepFaction   = 0u;
    it.requiredRepRank      = 0u;
    it.itemProficiencySkill = 43u;

    // baseline: everything satisfied
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, it,
                                 RefSkill, RefSpell, RefRep, &ctx) != AHUSE_OK)
    {
        printf("ahusabilityref FAIL: baseline\n");
        return 1;
    }

    // class gate: wrong class mask
    AhRefItem b = it;
    b.allowableClass = 0x80u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: class gate\n");
        return 1;
    }

    // honor gate fires when direct_action=true and rank is insufficient
    b = it;
    b.requiredHonorRank = 5u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: honor gate (direct_action)\n");
        return 1;
    }
    // D2: honor gate must NOT fire when direct_action=false
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, false, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) != AHUSE_OK)
    {
        printf("ahusabilityref FAIL: honor must be skipped when !direct_action\n");
        return 1;
    }

    // reputation gate: player rep rank 5, item requires 7
    b = it;
    b.requiredRepFaction = 609u;
    b.requiredRepRank    = 7u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: reputation gate\n");
        return 1;
    }

    // epic mount override: item 12302 bumps requiredLevel to EM(60); player is 40
    b = it;
    b.itemId         = 12302u;
    b.requiredLevel  = 30u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: epic-mount override\n");
        return 1;
    }

    printf("ahusabilityref OK\n");
    return 0;
}

int RunMangosdTest(std::string const& name)
{
    if (name == "noop")
    {
        printf("noop OK\n");
        return 0;
    }

    if (name == "commit")
    {
        return RunCommitTest();
    }

    if (name == "ahowner")
    {
        return RunAhOwnerTest();
    }

    if (name == "ahusabilityref")
    {
        return RunAhUsabilityRefTest();
    }

    printf("%s FAIL: unknown test\n", name.c_str());
    return 2;
}
