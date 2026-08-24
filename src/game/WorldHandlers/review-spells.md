# Review — the spell system

**Scope:** `src/game/WorldHandlers/Spell*.{cpp,h}` (the cast pipeline, targeting, hit,
effects, auras), plus the parts of `src/game/Object` the pipeline owns memory through:
`UnitAura.cpp`, `Unit.cpp` (current-spell slots, deferred aura deletion),
`PlayerSave.cpp` (`AddSpellMod`), `Item.cpp` (item lifetime), and
`src/shared/Utilities/EventProcessor.{cpp,h}` — the queue that owns every `Spell`.

**Pass of 2026-08-24:** 13 bugs, 5 suggestions, 2 nits. **Open now: 20.**

A live worklist, not an archive. A resolved issue is DELETED from this file rather
than annotated — git keeps the history, and a review whose entries mostly no longer
apply is one nobody can act on without re-reading all of it.

The through-line of the first four: **the pipeline was designed around GUIDs and then
three raw pointers were left in it.** Unit and GO targets are stored as `ObjectGuid`
and re-resolved on every `UpdatePointers()` — the header says why, "prevent access to
already nonexistent object". The cast item, the item target list, and the item inside
`SpellCastTargets` never got that treatment, and `Item::SetState` frees an `ITEM_NEW`
item **synchronously**, so the window is not a save-tick wide, it is one packet wide.

## Measurements

### One cast, counted

Scenario: a player casts Shadow Bolt — 3.0 s cast, `Speed` 21 so it flies, one unit
target — with 40 players inside visibility range. World tick is 50 ms
(`WORLD_SLEEP_CONST`, mangosd/Master.cpp:59), so 20 `Unit::Update` calls per second.

| what | allocations | where |
| --- | ---: | --- |
| `new Spell` | 1 | Unit.cpp:1443 / SpellHandler.cpp:401 |
| `new SpellEvent` | 1 | Spell.cpp:599 |
| event-queue node, initial | 1 | EventProcessor.cpp:135 |
| **event-queue node, re-queued once per world tick while PREPARING** | **60** | Spell.cpp:1066 |
| event-queue node, delay start + flight | 2 | Spell.cpp:1040, 1052 |
| `UnitList` node (target fill) | 1 | SpellTargetList.cpp:88 |
| `m_UniqueTargetInfo` node | 1 | SpellTargetList.cpp:604 |
| SMSG_SPELL_START: buffer + 1 encode per viewer | 1 + 40 | SpellPackets.cpp:169, 191 |
| SMSG_SPELL_GO: buffer + 1 encode per viewer | 1 + 40 | SpellPackets.cpp:213, 237 |
| SMSG_SPELL_COOLDOWN: buffer + 1 encode | 1 + 1 | SpellCast.cpp:551 |
| SMSG_SPELLNONMELEEDAMAGELOG: buffer + 1 encode per viewer | 1 + 40 | SpellHit.cpp:264 |
| cooldown map node | 1 | SpellCooldownMgr |
| `new HostileReference` (first threat from this attacker) | 1 | ThreatManager.cpp:648 |
| `ProcTriggeredList` nodes, attacker + victim | ~4 | Unit.cpp:4834, 4993 |
| **total** | **≈ 198** | and the same number of frees |

Read the shape, not the total:

- **125 of 198 (63 %) are the packet fan-out.** `ClientConnection::SendPacket` calls
  `PacketCodec::Encode`, which builds a fresh `std::vector<uint8>` and returns it by
  value (PacketCodec.cpp:176-188) — **one heap allocation per packet per recipient**,
  freed microseconds later. The bytes are then memcpy'd into the connection's
  `SendQueue` (SendQueue.hpp:64-81), which is the buffer that actually amortises. The
  encode vector exists only to be copied and thrown away, 40 times over.
- **63 of 198 (32 %) are the spell waiting.** See Issue 18.
- **~10 of 198 (5 %) are the spell itself** — the `Spell`, its event, its two target
  list nodes, the cooldown and threat entries. Everything that is actually *state*.

Two variations, same code:

- A channel (Blizzard, 8 s) re-queues **160** event nodes, and its periodic trigger
  builds a whole new `Spell` + `SpellEvent` + target list on top, per tick.
- Auto Shot never leaves the queue: while auto-repeat is on, the current spell
  re-queues **20 nodes per second, indefinitely** (SpellEvent::Execute's `default`
  arm), and `Unit::_UpdateAutoRepeatSpell` (Unit.cpp:2110) constructs a **complete
  second `Spell` object per shot** rather than re-arming the one it already holds.

### `SpellEntry` — where it is filled, and how much C++ works around it

Filled **once, at startup, in one place**: `LoadDBC(..., sSpellStore, dbcPath,
"Spell.dbc")` (DBCStores.cpp:350) → `DBCStorage<SpellEntry>::Load`
(shared/DataStores/DBCStore.h:106-125) → `DBCFileLoader::AutoProduceData`, laid out by
the 173-column format string `SpellEntryfmt` (DBCfmt.h:66): 113 ints, 16 floats,
16 strings, **27 columns skipped outright**. Two allocations for the entire table —
one data block, one string pool.

Written afterwards in **exactly one place**: `SpellMgr::ModDBCSpellAttributes()`
(SpellMgr.cpp:1518-1548) const-casts the DBC row and patches 3 fields on 2 spells
(20647 Execute, 16870 Clearcasting). That is the whole mutation surface.

Read everywhere: **816** `m_spellInfo` references in the pipeline alone (688 of them
field accesses), 207 `GetSpellProto()`, 228 `sSpellStore.LookupEntry`, 595 `SpellEntry`
tokens in `src/game` and 851 in `src/`. **118 distinct field names** are read across the
server; the top four are `ID` (380), `Effect` (111), `HasAttribute` (97) and
`ImplicitTargetA` (90).

The struct is read-only DBC data with nowhere to put server behaviour, so behaviour went
into two other places: **17 `SpellMgr::Load*` side tables** (chains, affects, bonuses,
threats, proc events, elixirs, pet auras, script targets, linked, areas, target
positions, learn-skills, learn-spells, item-enchant procs, facing flags, skill-line,
skill-race-class) — and, for everything those cannot express, into `switch` statements.
That is Issue 20.

## Issues

### Issue 1 -- Severity: bug
- File: Spell.h:454, Spell.cpp:883, SpellPower.cpp:84, Object/Item.cpp:504
- Description: `m_CastItem` is captured once and used for the whole cast — `SendSpellGo`,
  `TakeReagents`, `TakeCastItem`, `CreateSpellAuraHolder(..., m_CastItem)`, every
  `CreateAura(..., m_CastItem)` — and `Spell::UpdatePointers()` never re-resolves it.
  `Item::SetState(ITEM_REMOVED)` does `delete this` on the spot when the item is still
  `ITEM_NEW` (Item.cpp:504-512), i.e. anything looted, crafted or conjured since the last
  inventory save. `Player::RemoveItemDependentAurasAndCasts` (Player.cpp:6074-6084) does
  not help: it interrupts a cast only when the spell's *equipment requirements* stop
  holding, never because the destroyed item is the cast item, and it skips
  `SPELL_STATE_DELAYED` outright.
- Failure: use a freshly looted item with a cast time, destroy or sell it before the cast
  completes → `TakeCastItem()` reads `m_CastItem->GetProto()` on freed memory. Same for a
  delayed spell still in flight.
- Suggestion: add `ObjectGuid m_CastItemGuid` next to `m_CastItem` and re-resolve it in
  `UpdatePointers()` the way `m_originalCaster` and `m_targets` already are.
- Status: open.

### Issue 2 -- Severity: bug
- File: Spell.cpp:181-186, Spell.cpp:1252, SpellPower.cpp:267
- Description: `SpellCastTargets::setItemTarget(NULL)` early-returns on a null argument, so
  it clears nothing. Both callers pass exactly NULL and both exist to break a dangling
  link: `ClearCastItem()` (Spell.cpp:1248-1256) and `TakeReagents()` (SpellPower.cpp:265-268),
  the latter under the comment "prevent crash at access to deleted m_targets.getItemTarget"
  (SpellPower.cpp:146). `m_itemTarget` survives both, still pointing at the item that was
  just destroyed. `m_targets.Update()` would repair it, but nothing calls it between
  `TakeReagents()` and `_handle_immediate_phase()` — they are in the same `cast()` frame.
- Suggestion: give the clearing job its own method (`clearItemTarget()`), or drop the
  early return and handle NULL by resetting guid, entry and the mask bit.
- Status: open.

### Issue 3 -- Severity: bug
- File: SpellTargetList.cpp:723-729, SpellHit.cpp:614-628
- Description: `ItemTargetInfo` stores a raw `Item*`. `TargetInfo` and `GOTargetInfo` — the
  two lists next to it, written by the same three functions — store `ObjectGuid`
  deliberately. Nothing clears an entry when the item dies, and the order inside `cast()`
  makes the collision routine: `TakeReagents()` destroys reagents, then
  `_handle_immediate_phase()` walks `m_UniqueItemInfo` and hands each stored pointer to
  `HandleEffects(NULL, target->item, ...)`. If the reagent was also the item target and was
  `ITEM_NEW`, that pointer is freed memory.
- Suggestion: store `ObjectGuid` and resolve at use, exactly like the other two lists.
- Status: open.

### Issue 4 -- Severity: bug
- File: SpellTargetList.cpp:539, SpellHit.cpp:159
- Description: `TargetInfo target;` is a POD left partly uninitialized — `AddUnitTarget`
  assigns `targetGUID`, `effectMask`, `processed`, `missCondition`, `reflectResult` and
  `timeDelay`, but never `damage` or `HitInfo`. Those two are written only by
  `HandleDelayedSpellLaunch` (SpellHit.cpp:695-696), which runs only when `speed > 0`. Two
  of the three readers guard on `speed > 0.0f` (SpellHit.cpp:143, 251). The third does not.
- Failure: a creature casts an instant (speed 0) spell, the target reflects it via
  `SPELL_AURA_REFLECT_SPELLS`, and SpellHit.cpp:159 feeds
  `LowerPlayerDamageReq(target->damage)` a stack value that was never written — the
  creature's player-damage requirement, which gates loot and XP, is set from junk.
- Suggestion: `TargetInfo target = {};`, or default member initializers on the struct.
- Status: open.

### Issue 5 -- Severity: bug
- File: SpellAuras.cpp:2266-2268
- Description: the `SpellAuraHolder` constructor treats `caster` as nullable at line 2216
  (`if (!caster) m_casterGuid = target->GetObjectGuid();`) and then dereferences it
  unconditionally 50 lines later: `... && caster != target && caster->GetTypeId() ==
  TYPEID_PLAYER && ...`. A null caster passes `caster != target` and crashes on
  `GetTypeId()`. `Spell::DoSpellHitOnUnit` passes `GetAffectiveCaster()` (SpellHit.cpp:499),
  which the same function guards with `if (realCaster)` at lines 371, 380, 405, 462 and 476
  because it genuinely can be NULL.
- Failure: a spell carrying `SPELL_ATTR_HEARTBEAT_RESIST_CHECK` (fear, polymorph and most
  of the CC set) lands after its original caster has left the map — trap, delayed cast,
  logged-off caster → null dereference.
- Suggestion: `caster && caster != target && caster->GetTypeId() == ...`.
- Status: open.

### Issue 6 -- Severity: bug
- File: SpellCast.cpp:797-826
- Description: `Spell::finish` iterates the live list —
  `Unit::AuraList const& targetTriggers = m_caster->GetAurasByType(SPELL_AURA_ADD_TARGET_TRIGGER)`
  is a reference to `m_modAuras[type]` — and calls `m_caster->CastSpell(unit, ..., (*i))`
  inside the loop. `Unit::RemoveAura` erases from that same list (UnitAura.cpp:1264-1267)
  before doing anything else, and the triggered spell can remove the very aura being
  iterated: charge consumption, dispel, the caster dying. `std::list::remove` frees the
  node, so the following `++i` walks freed memory.
- Suggestion: the tree already has the right pattern — `Unit::ProcDamageAndSpell`
  (Unit.cpp:4975-4993) snapshots into a local list and pins each holder with
  `SetInUse(true)` first. Do the same here.
- Status: open.

### Issue 7 -- Severity: bug
- File: SpellAuraAddModifier.cpp:119, Object/UnitAura.cpp:1285-1299, Object/Player.cpp:680
- Description: every active `SpellModifier` leaks when a player logs out.
  `Unit::CleanupsBeforeDelete` calls `RemoveAllAuras(AURA_REMOVE_BY_DELETE)`
  (Unit.cpp:4498); `Unit::RemoveAura` skips `ApplyModifier(false, true)` for every aura
  type except the two POSSESS ones when the mode is `AURA_REMOVE_BY_DELETE`; so
  `Aura::HandleAddModifier(false)` never runs, so `Player::AddSpellMod(mod, false)` never
  runs — and that is the only `delete mod` in the tree (PlayerSave.cpp:1499-1506).
  `~Aura()` is empty (SpellAuras.cpp:333) and `~Player()` never touches `m_spellMods`
  (Player.cpp:680-723). One leaked `SpellModifier` per modifier aura per logout, forever.
- Suggestion: free `m_spellmod` in `~Aura()` (and null it in `HandleAddModifier` after the
  unapply, which also closes the double-free if that handler is ever entered twice), or
  drain `m_spellMods` in `~Player`.
- Status: open.

### Issue 8 -- Severity: bug
- File: shared/Utilities/EventProcessor.cpp:87-90 and 125-136, Object/Unit.cpp:4481-4499
- Description: `KillAllEvents` sets `m_aborting = true` under the comment "prevent event
  insertions" and `AddEvent` never reads it. The insertion path is live during teardown:
  `Unit::CleanupsBeforeDelete` calls `KillAllEvents(false)` and *then*
  `RemoveAllAuras(AURA_REMOVE_BY_DELETE)`, whose removal handlers cast spells, and
  `Spell::prepare` (Spell.cpp:599-600) pushes a fresh `SpellEvent` into the processor that
  is already tearing down. That event survives to `~EventProcessor` →
  `KillAllEvents(true)` → `~SpellEvent` → `m_Spell->cancel()`, which makes virtual calls on
  a `Unit` whose derived destructor has already returned.
- Suggestion: `common/Events/EventProcessor.h` — the replacement already sitting in the
  tree — refuses insertion while aborting, aborts and destroys the event instead, and
  reports the refusal. Port that behaviour, or migrate the game to it.
- Status: open.

### Issue 9 -- Severity: bug
- File: shared/Utilities/EventProcessor.cpp:57-77 and 92-105
- Description: `KillAllEvents(false)` aborts every event and then leaves the non-deletable
  ones queued with `to_Abort = true`. The next `Update` takes the abort branch and calls
  `Abort()` a second time before deleting; `~EventProcessor`'s `KillAllEvents(true)` makes
  it a third. Nothing in an event's own code hints this can happen. Spells survive it only
  by accident — `Spell::cancel` early-returns on `SPELL_STATE_FINISHED` — but any event
  whose `Abort` refunds a cost or notifies a player pays twice.
- Suggestion: the `m_aborted` flag in `common/Events/EventProcessor.h` exists for exactly
  this; deliver `Abort` once.
- Status: open.

### Issue 10 -- Severity: bug
- File: Spell.cpp:956-971
- Description: `~SpellEvent` refuses to delete a non-deletable `Spell` — correct — and then
  drops it on the floor with an error line that says so ("causes memory leak"). It is the
  only owner. The message itself calls `m_Spell->GetCaster()->GetTypeId()` and
  `GetGUIDLow()`, and the path that reaches it from `~EventProcessor` runs while that
  caster is mid-destruction.
- Suggestion: the only reason a `Spell` is undeletable at this point is
  `m_executedCurrently`; a `Spell` cannot be executing inside its own event's destructor
  unless the owner is being destroyed underneath it, which Issues 8 and 9 are about. Fix
  those, then make this branch an assertion rather than a leak.
- Status: open.

### Issue 11 -- Severity: bug
- File: SpellHit.cpp:545, 556, 560
- Description: `Spell::m_spellAuraHolder` is left dangling by all three exits — the two
  explicit `delete m_spellAuraHolder` calls do not null it, and
  `unit->AddSpellAuraHolder(m_spellAuraHolder)` returns a bool that is discarded although
  the callee deletes the holder on four of its paths (UnitAura.cpp:368, 377, 396, 458).
  Today no read reaches a stale value because every entry into `DoSpellHitOnUnit`
  reassigns the member before the effect loop — but that is a property of the current
  call graph, not of the code, and `Spell::EffectApplyAura` (SpellEffectHealPower.cpp:111)
  and `Spell::EffectApplyAreaAura` (SpellEffectSummonLock.cpp:309) dereference it with no
  null check at all.
- Suggestion: `m_spellAuraHolder = NULL;` after each delete and after `AddSpellAuraHolder`
  returns false; null-check it in the two effect handlers.
- Status: open.

### Issue 12 -- Severity: suggestion
- File: Spell.h:523-527 and 544, Spell.cpp:404, Object/Unit.cpp:2201
- Description: `m_selfContainer` is written (by `SetCurrentCastedSpell`) and read nowhere.
  It exists so a `Spell` destroyed while `Unit::m_currentSpells[type]` still points at it
  can null that slot from its own destructor; `Spell::~Spell()` (Spell.cpp:504-506) is
  empty, so the net is gone. The flag discipline currently makes the case unreachable —
  every site that clears `m_referencedFromCurrentSpell` also nulls the slot — which is
  precisely the kind of invariant that a defence like this is meant to outlive.
- Suggestion: restore the destructor guard (`if (m_selfContainer && *m_selfContainer ==
  this) *m_selfContainer = NULL;` plus the error log), or delete the member, the setter
  and the getter. Do not leave it half-wired.
- Status: open.

### Issue 13 -- Severity: suggestion
- File: SpellAuras.cpp:2280-2283, SpellAuras.cpp:1333-1341
- Description: `SpellAuraHolder::AddAura` is `m_auras[index] = aura;` with no check for an
  occupied slot. A caller that hits one leaks the previous `Aura` and, if that aura had
  been registered by `AddAuraToModList`, leaves a dangling entry in `Unit::m_modAuras`.
  `Aura::HandleModDetectRange` (Soothe Animal, spells 9901/8955/2908) does exactly this
  kind of blind add at a hardcoded `EFFECT_INDEX_1` on every apply, and — as its own
  comment asks — never removes it on unapply.
- Suggestion: assert or free-and-replace in `AddAura`; fix the Soothe Animal hack to be
  symmetric, or move it into the holder's own effect table.
- Status: open.

### Issue 14 -- Severity: suggestion
- File: SpellEffectDispatch.cpp:96
- Description: `uint8 eff = m_spellInfo->Effect[i];` narrows a `uint32` DBC field before the
  `eff < TOTAL_SPELL_EFFECTS` bound check, so a value of 256 + n passes the check as n and
  dispatches the wrong handler through `SpellEffects[]`. 1.12 data never reaches 256, which
  is the only thing keeping it safe.
- Suggestion: keep the field's own type; check the bound on the unnarrowed value.
- Status: open.

### Issue 15 -- Severity: suggestion
- File: Spell.cpp:168-172, Spell.cpp:209-212
- Description: of the four `SpellCastTargets` setters, `setUnitTarget` and `setItemTarget`
  guard against NULL and `setGOTarget` / `setCorpseTarget` dereference immediately. The
  inconsistency is the trap, not either behaviour on its own — see Issue 2 for what the
  guard in `setItemTarget` cost.
- Suggestion: pick one contract for all four and make the callers honour it.
- Status: open.

### Issue 16 -- Severity: nit
- File: SpellCast.cpp:278
- Description: `if (m_spellInfo->ID == 12042) m_targets.getUnitTarget()->RemoveAurasDueToSpell(10060);`
  dereferences the unit target without a check, in a function that checks it three lines
  earlier and four lines later. Arcane Power is self-targeted from the client, but any
  script or DB trigger that casts 12042 with empty targets crashes the world.
- Suggestion: `if (Unit* t = m_targets.getUnitTarget())`.
- Status: open.

### Issue 17 -- Severity: nit
- File: Spell.cpp:523-538, 690-720, 739-748; SpellAuras.cpp:1298-1315, 1348-1370, 1374-1383,
  1387-1397, 1425-1441, 1464-1470, 1474-1479, 1483-1490, 1500-1506;
  SpellEffects.cpp:223-247, 363-370, 621-641, 784-800, 895-906
- Description: seventeen runs of six or more consecutive blank lines, up to 31 at a time —
  the holes left where functions were cut out when the monolith was split into
  `Spell*.cpp`. They make the files read as if something is missing at each gap.
- Suggestion: close them.
- Status: open.

### Issue 18 -- Severity: bug
- File: Spell.cpp:1066, shared/Utilities/EventProcessor.cpp:57-77
- Description: a spell that is merely *waiting* allocates. `SpellEvent::Execute` ends every
  non-terminal state with `AddEvent(this, e_time + 1, false)` — due 1 ms later, i.e. the
  next `EventProcessor::Update`, i.e. the next world tick. `EventProcessor::Update` erases
  the `std::multimap` node before executing and `AddEvent` inserts a new one, so **every
  live spell costs one node allocation and one node free per tick, 20 times a second**,
  for the whole time it is preparing, delayed or channelling. A 3 s cast: 60. An 8 s
  channel: 160. Auto Shot: 20 per second for as long as the player keeps attacking —
  forever, on every attacking player on the server, in the hottest loop there is.
  The re-queue carries no information: nothing about the spell changed, the event is
  simply saying "call me again". It is 32 % of the allocation traffic of a plain cast
  (see Measurements) and 100 % of the traffic of an idle channel.
- Suggestion: an event that wants the next tick should not have to leave and re-enter the
  queue. Either keep a due-time field the processor updates in place for the common
  `now + 1` case, or give the processor a "tick list" for events that run every update and
  reserve the multimap for genuinely scheduled ones. `Unit::_UpdateAutoRepeatSpell`
  (Unit.cpp:2110) should also re-arm the spell it already holds instead of constructing a
  second `Spell` per shot.
- Status: open.

### Issue 19 -- Severity: suggestion
- File: proto/PacketCodec.cpp:176-188, proto/ClientConnection.cpp:148-165
- Description: `ClientConnection::SendPacket` calls `PacketCodec::Encode`, which reserves
  and returns a `std::vector<uint8>` by value, and the only thing done with it is
  `m_sender(frame.data(), frame.size())` → `SendQueue::append`, which memcpys it into the
  connection's amortised pending buffer and drops it. So each packet costs **one heap
  allocation per recipient** whose entire life is one memcpy. A spell cast seen by 40
  players spends 120 of its ~198 allocations here (3 broadcast packets × 40).
- Suggestion: encode straight into the `SendQueue`'s pending buffer — `append` already
  holds the lock and already knows the length; the header is 4 bytes and can be written
  ahead of the payload. Failing that, a thread-local scratch vector reused across calls
  removes the allocation without touching the queue.
- Status: open.

### Issue 20 -- Severity: bug
- File: all of `Spell*.cpp` (46 `switch` blocks); worst: SpellEffectDummy.cpp,
  SpellEffectScript.cpp, SpellAuraDummy.cpp, SpellEffectObjectCombat.cpp
- Description: **445 distinct spell IDs are hardcoded into the effect and aura handlers.**
  Breakdown: 342 distinct IDs across 392 `case <id>:` / `ID == <id>` dispatch sites; 139
  further lines that pass a literal spell id to `CastSpell`, `RemoveAurasDueToSpell`,
  `SendSpellMiss` and friends; 46 `switch` blocks keyed on `m_spellInfo->ID` or `GetId()`;
  and 35 distinct hardcoded `SpellFamily` masks across 47 sites. `SpellEffectDummy.cpp`
  alone carries 77 `case` labels, `SpellEffectScript.cpp` 47, `SpellAuras.cpp` 35,
  `SpellAuraDummy.cpp` 33.
  This is not incidental: `SpellEntry` is read-only DBC data with one 3-field patch
  (see Measurements), and the 17 `SpellMgr` side tables cover only the parameters someone
  thought to make into a table. Anything else — one spell that needs a different target, a
  different duration, an extra removal — has no data slot, so it becomes a `case` in a
  1243-line handler. Every one of them is a recompile, is invisible to the DB, and is
  unreachable from any script.
- Suggestion: this is what MAI's data model exists for (see the `mangos-mai` skill). The
  actionable first cut is to sort the 445 into the ones already expressible as
  `spell_linked` / `spell_bonus` / `spell_proc_event` rows (mechanical, no new schema) and
  the residue that needs a real per-spell behaviour table. Do not add the 446th.
- Status: open.
