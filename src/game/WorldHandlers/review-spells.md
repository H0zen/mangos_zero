# Review — the spell system

**Scope:** `src/game/WorldHandlers/Spell*.{cpp,h}` (the cast pipeline, targeting, hit,
effects, auras), plus the parts of `src/game/Object` the pipeline owns memory through:
`UnitAura.cpp`, `Unit.cpp` (current-spell slots, deferred aura deletion),
`PlayerSave.cpp` (`AddSpellMod`), `Item.cpp` (item lifetime), and
`src/shared/Utilities/EventProcessor.{cpp,h}` — the queue that owns every `Spell`.

**Opened 2026-08-24 with 20 issues. Open now: 2.**

A live worklist, not an archive. A resolved issue is DELETED from this file rather
than annotated — git keeps the history, and a review whose entries mostly no longer
apply is one nobody can act on without re-reading all of it. Eighteen entries are
gone; the two below are the ones that are not a pointer bug or an allocation and do
not have a mechanical fix.

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

### Issue 13 -- Severity: suggestion
- File: SpellAuras.cpp:2280-2283, SpellAuras.cpp:1333-1341
- Description: `SpellAuraHolder::AddAura` is `m_auras[index] = aura;` with no check for an
  occupied slot. A caller that hits one leaks the previous `Aura` and, if that aura had
  been registered by `AddAuraToModList`, leaves a dangling entry in `Unit::m_modAuras`.
  `Aura::HandleModDetectRange` (Soothe Animal, spells 9901/8955/2908) does exactly this
  kind of blind add at a hardcoded `EFFECT_INDEX_1` on every apply and — as its own
  comment asks — never removes it on unapply. The aura it creates is also never added to
  the mod list, so `GetTotalAuraModifier(SPELL_AURA_MOD_DETECT_RANGE)` cannot see it:
  the hack appears to be both leaky and inert.
- Suggestion: assert or free-and-replace in `AddAura`; then decide whether Soothe Animal
  is supposed to do anything at all, because at the moment it is not clear that it does.
- Status: open.

### Issue 20 -- Severity: bug
- File: all of `Spell*.cpp`; worst: SpellEffectDummy.cpp (77 `case` labels),
  SpellEffectScript.cpp (47), SpellAuras.cpp (35), SpellAuraDummy.cpp (33)
- Description: **445 distinct spell IDs are hardcoded into the effect and aura handlers.**
  342 distinct IDs across 392 `case <id>:` / `ID == <id>` dispatch sites; 139 further
  lines passing a literal spell id to `CastSpell`, `RemoveAurasDueToSpell` and friends;
  and 35 distinct hardcoded `SpellFamily` masks across 47 sites.
  This is not incidental: `SpellEntry` is read-only DBC data with one 3-field patch, and
  the 17 `SpellMgr` side tables cover only the parameters someone thought to make into a
  table. Anything else — one spell that needs a different target, a different duration,
  an extra removal — has no data slot, so it becomes a `case` in a 1243-line handler.
  Every one is a recompile, is invisible to the DB, and is unreachable from any script.
- Progress: the `switch` in `Spell::cast` is gone — it, and the two `spell_linked`
  lookups beside it, now run once per spell into `SpellMgr::BuildSpellCastPlans` and the
  cast reads one byte. That is the pattern the rest should follow: move the rules to
  load time as DATA, do not copy them.
- Suggestion: sort the remainder into the ones already expressible as `spell_linked` /
  `spell_bonus` / `spell_proc_event` rows — mechanical, deletable today — and the residue
  that needs a real per-spell behaviour table, which is what MAI's data model is for (see
  the `mangos-mai` skill). And do not add the 446th.
- Status: open.
