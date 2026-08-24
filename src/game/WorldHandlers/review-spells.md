# Review — the spell system

**Scope:** `src/game/WorldHandlers/Spell*.{cpp,h}` (the cast pipeline, targeting, hit,
effects, auras), plus the parts of `src/game/Object` the pipeline owns memory through:
`UnitAura.cpp`, `Unit.cpp` (current-spell slots, deferred aura deletion),
`PlayerSave.cpp` (`AddSpellMod`), `Item.cpp` (item lifetime), and
`src/shared/Utilities/EventProcessor.{cpp,h}` — the queue that owns every `Spell`.

**Opened 2026-08-24 with 20 issues. Open now: 1.**

A live worklist, not an archive. A resolved issue is DELETED from this file rather
than annotated — git keeps the history, and a review whose entries mostly no longer
apply is one nobody can act on without re-reading all of it. Nineteen entries are
gone. The one below is not a defect with a fix; it is a direction, and it has its own
burn-down list in `hardcoded-spells.md`.

What the eighteen had in common is worth keeping when they are deleted: **the
pipeline was designed around GUIDs and then three raw pointers were left in it** —
the cast item, the item target list, and the aura holder — and **the event queue owned
its events through raw pointers and decided at deletion time whether it was allowed to
delete them.** Both are gone. `Spell` is now the queued event, the queue holds it in a
`unique_ptr`, and every object the cast keeps across a tick is resolved from a guid.

## Measurements

### One cast, counted

Scenario: a player casts Shadow Bolt — 3.0 s cast, `Speed` 21 so it flies, one unit
target — with 40 players inside visibility range. World tick is 50 ms
(`WORLD_SLEEP_CONST`, mangosd/Master.cpp:59), so 20 `Unit::Update` calls per second.

| what | before | after |
| --- | ---: | ---: |
| `Spell` object | 1 | 0 — pooled per thread after the first |
| `SpellEvent` object | 1 | 0 — folded into `Spell` |
| event-queue node, initial | 1 | 1 |
| event-queue node, re-queued once per world tick while PREPARING | 60 | 0 — tick lane |
| event-queue node, delay start + flight | 2 | 2 |
| `UnitList` node (target fill) | 1 | 1 |
| target-list node | 1 | 0 — inline capacity |
| SMSG_SPELL_START: buffer + one encode per viewer | 41 | 1 |
| SMSG_SPELL_GO: buffer + one encode per viewer | 41 | 1 |
| SMSG_SPELL_COOLDOWN: buffer + encode | 2 | 1 |
| SMSG_SPELLNONMELEEDAMAGELOG: buffer + one encode per viewer | 41 | 1 |
| cooldown map node | 1 | 1 |
| `new HostileReference` (first threat from this attacker) | 1 | 1 |
| `ProcTriggeredList` nodes, attacker + victim | ~4 | ~4 |
| **total** | **≈ 198** | **≈ 13** |

The three that mattered:

- **120 of the 198 were the packet fan-out.** `PacketCodec::Encode` built a fresh
  `std::vector<uint8>` per packet **per recipient**, whose entire life was one memcpy
  into that connection's `SendQueue`. Only the four-byte header differs between
  recipients — it is the header, and only the header, that the per-connection cipher
  touches — so the frame is now built in one buffer per sending thread.
- **60 were the spell waiting.** `Spell::Execute` re-queued itself for `now + 1`,
  which erased a `std::multimap` node and allocated an identical one, 20 times a
  second, carrying no information beyond "call me again". Those events are due every
  tick, so they are no longer in a time-ordered map.
- **~10 were the spell itself**, and most of that is now a per-thread freelist.

Not in the column: an 8 s channel stopped costing 160 queue nodes, auto-repeat stopped
costing 20 per second per attacking player, a 40-target AoE stopped costing 80 list
nodes, and the 245-line `IsPositiveEffect` left the hot path.

Reproduce any of it with `-DALLOC_METRICS=1`: the build replaces the global
`operator new`/`delete` and `~Spell` logs the per-cast delta, split by site.

### `SpellEntry` — where it is filled, and how much C++ works around it

Filled **once, at startup, in one place**: `LoadDBC(..., sSpellStore, dbcPath,
"Spell.dbc")` (DBCStores.cpp:350) → `DBCStorage<SpellEntry>::Load`
(shared/DataStores/DBCStore.h:106-125) → `DBCFileLoader::AutoProduceData`, laid out by
the 173-column format string `SpellEntryfmt` (DBCfmt.h:66): 113 ints, 16 floats,
16 strings, **27 columns skipped outright**. Two allocations for the entire table —
one data block, one string pool.

Written afterwards in **exactly one place**: `SpellMgr::ModDBCSpellAttributes()`
const-casts the DBC row and patches 3 fields on 2 spells (20647 Execute, 16870
Clearcasting). That is the whole mutation surface.

Read everywhere: **816** `m_spellInfo` references in the pipeline alone (688 of them
field accesses), 207 `GetSpellProto()`, 228 `sSpellStore.LookupEntry`, 595 `SpellEntry`
tokens in `src/game` and 851 in `src/`. **118 distinct field names** are read across the
server; the top four are `ID` (380), `Effect` (111), `HasAttribute` (97) and
`ImplicitTargetA` (90).

The struct is read-only DBC data with nowhere to put server behaviour, so behaviour went
into two other places: **17 `SpellMgr::Load*` side tables** — and, for everything those
cannot express, into `switch` statements. That is Issue 20.

## Issues

### Issue 20 -- Severity: bug
- File: `hardcoded-spells.md`, regenerated by `hardcoded-spells.py`
- Description: **505 sites, 426 distinct spell ids, nailed into the effect and aura
  handlers.** (An earlier ad-hoc grep said 445 distinct; the generator is the number to
  trust — it is reproducible and it does not count commented-out lines.) By shape:
  316 `case <id>:` dispatch, 138 literal ids passed to `CastSpell` and friends, 51
  `ID == <id>` comparisons.
  This is not incidental. `SpellEntry` is read-only DBC data with one 3-field patch, and
  the 17 `SpellMgr` side tables cover only the parameters someone thought to make into a
  table. Anything else — one spell that needs a different target, a different duration,
  an extra removal — has no data slot, so it becomes a `case` in a 1243-line handler.
  Every one is a recompile, is invisible to the DB, and is unreachable from any script.
- **The shape of it, which the inventory made visible:** it is not spread thin. Four
  functions hold 311 of the 505 sites — `Spell::EffectDummy` (137),
  `Spell::EffectScriptEffect` (93), `Aura::HandleAuraDummy` (51), `Aura::TriggerSpell`
  (30). That is four tables waiting to be written, not four hundred arguments, and it is
  a far better prospect than the raw count suggests.
- Progress: two of these switches are already gone. `Spell::cast`'s family switch and
  `SpellAuraHolder::HandleSpellSpecificBoosts` — the latter running on every aura
  application AND removal — now resolve at load into `SpellMgr::BuildSpellPlans`, and the
  hot path reads one byte. Both were MOVED, not copied: there is one implementation of
  each rule and it is in `SpellMgr`. That is the pattern for the rest.
- Suggestion: work `hardcoded-spells.md` top-down. Check the `reference` shape against
  the existing `spell_linked` / `spell_bonus` / `spell_proc_event` tables first — some of
  those literals duplicate rows that already exist. The `dispatch` bulk is what MAI's
  data model is for (see the `mangos-mai` skill). The 51 `comparison` sites are last and
  hardest, because what they do is not "cast X" but "behave differently here".
  And do not add the 427th.
- Status: open.
