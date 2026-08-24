# Spell system — optimization plan

**Status: stages 0 through 7 are implemented and building clean on ficom (clang 19).**
Each stage landed as its own commit, in order, with a build between. What each one
actually changed is in its commit message; this file is the plan they were cut from and
is kept because the reasoning outlives the diffs — particularly the ordering argument,
which is the part that is easy to lose.

Baseline is the ledger in `review-spells.md`: **≈198 heap allocations and 198 frees for
one Shadow Bolt** seen by 40 players, of which 5 % is the spell's actual state. The plan
below takes that to **≈13**, and takes an idle channel from 20 allocations per second to
zero. Every stage is separately shippable and separately measurable.

The order is not negotiable in one respect: **ownership is fixed before anything is
pooled or reused.** Pooling an object that still has a lifetime bug converts a
use-after-free — which crashes, and gets reported — into a live `Spell` quietly reading
another `Spell`'s fields. That is the one bug class this codebase cannot afford.

---

## Stage 0 — measure, or none of this is true

Everything in the ledger was derived by reading code. Before changing a line, make the
server say the number itself: a per-cast allocation counter behind a config flag —
increment in a debug `operator new` hooked for the world thread, snapshot in
`Spell::prepare`, print the delta in `~Spell` with the spell id and the observer count.

Without it every stage below is arithmetic. With it, each stage has a before/after that
fits in one line of a log, and a regression that reintroduces churn is visible the day it
lands rather than at the next raid.

**Verify:** the counter reproduces ~198 for the baseline scenario. If it does not, the
ledger is wrong and the plan needs re-cutting before, not after.

---

## Stage 1 — one owner for the spell (correctness, then everything else)

`common/Events/EventProcessor.h` already exists in this tree, already holds
`std::unique_ptr<BasicEvent>`, already refuses insertion during teardown and already
delivers `Abort` exactly once. The game still includes `shared/Utilities/EventProcessor.h`.

1. Point `Unit.h` at `common/Events`, adapt the four `AddEvent` call sites.
2. **Fold `SpellEvent` into `Spell`**: make `Spell` derive from `BasicEvent` directly.
   The two objects have a strict 1:1 lifetime, one pointer chase between them, and a
   separate allocation for a class whose entire state is `Spell* m_Spell`.

Kills, outright: Issue 8 (insertion during teardown), Issue 9 (double `Abort`), Issue 10
(`~SpellEvent` leaking the spell it owns), and the whole `IsDeletable` /
`m_referencedFromCurrentSpell` / `m_executedCurrently` dance becomes a `unique_ptr` plus
one "executing" flag that only guards re-entrancy, not deletion.

**Saves:** 1 allocation per cast, one indirection on every event tick.
**Risk:** medium — it is the ownership model. Do it alone, in its own commit.
**Verify:** a spell cast by a unit that is deleted mid-cast (totem unsummon during its
own cast) must not touch the destroyed unit; the existing crash path is the test.

---

## Stage 2 — stop allocating to wait  *(−60 of 198)*

`SpellEvent::Execute` ends every non-terminal state with `AddEvent(this, e_time + 1)`.
`EventProcessor::Update` erased the multimap node a few instructions earlier. So the
queue frees a node and allocates an identical one 20 times a second per live spell,
carrying no new information — see Issue 18.

Add a tick lane to the processor: an event whose `Execute` returns "again next update"
goes into a `std::vector<BasicEvent*>` that is swapped and walked each `Update`. Amortised
growth, zero per-event allocation. The multimap stays for genuinely scheduled work —
the delayed-impact re-arm, despawn timers, invite timeouts.

**Saves:** 60 allocations on a 3 s cast, 160 on an 8 s channel, and **20 per second per
attacking player, permanently**, for auto-repeat.
**Also:** `Unit::_UpdateAutoRepeatSpell` (Unit.cpp:2110) builds a complete second `Spell`
per shot instead of re-arming the one it holds. Re-arm it.
**Verify:** the Stage 0 counter reports 3 queue operations for a 3 s cast instead of 63,
and a constant count for a channel of any length.

---

## Stage 3 — broadcast once, not once per viewer  *(−120 of 198)*

`ClientConnection::SendPacket` → `PacketCodec::Encode` builds a fresh `std::vector<uint8>`
and returns it by value; the only use is one memcpy into the connection's `SendQueue`,
then it dies (Issue 19). Three broadcast packets × 40 viewers = 120 allocations whose
whole life is a memcpy.

The key fact: **only the 4-byte header is per-connection.** `m_crypt.EncryptSend(header,
len)` encrypts the header alone — the payload is byte-identical for every recipient.

So: `SendQueue::append(header, headerLen, payload, payloadLen)` — build the 4-byte header
on the stack, encrypt it, and copy header + shared payload straight into the pending
buffer under the lock it already takes. Zero allocations, one fewer copy per recipient.

**Saves:** 120 allocations and 120 redundant copies on the baseline cast; it scales with
raid size, which is exactly where the server hurts.
**Risk:** low, and contained to two files.
**Verify:** byte-for-byte comparison of the wire output against the current encoder for a
recorded session, plus the counter dropping by 3 × viewers.

---

## Stage 4 — contiguous target lists  *(−2, and the AoE cliff)*

`std::list<TargetInfo>`, `std::list<GOTargetInfo>`, `std::list<Unit*>`: one allocation per
element, pointer-chased on every one of the four-to-six walks a cast performs over them
(`HandleDelayedSpellLaunch`, `handle_immediate`/`handle_delayed`, `finish` twice,
`IsAliveUnitPresentInTargetList`, `cancel`).

Add `SmallVector<T, N>` to `common/Containers` (C++17, no dependency) and use it with an
inline capacity of 4 — which covers the overwhelming majority of casts with **zero**
allocations, and makes a 40-target AoE one growth instead of 40 nodes.

Two things fall out for free:

- Issue 4 disappears if `TargetInfo` gets default member initializers on the way past.
- The chain-target loop (SpellTargeting.cpp:283-305) currently does
  `sort()` → `erase(begin())` → `sort()` again **per jump** — O(k·n log n) with a
  pointer-chasing comparator on every hop of Chain Lightning and Multi-Shot. On a
  contiguous buffer it is a linear min-scan per jump, k ≤ 5.

**Verify:** a 40-target AoE allocates once for the target buffer, not 80 times.

---

## Stage 5 — compute per-spell facts once, not per cast

`IsPositiveEffect` is **245 lines** of nested switches over a ~700-byte struct, and it is
recomputed from scratch every time it is asked — including once per `Aura` constructed,
i.e. up to 120 times for a 40-target AoE with three aura effects. `IsChanneledSpell`,
`IsPassiveSpell`, `GetSpellSchoolMask` (62 call sites), `GetSpellDuration`,
`IsSpellAppliesAura` are all the same shape: pure functions of immutable DBC data,
evaluated in the hot path.

Build a `SpellRuntime` table once at load, indexed by spell id, holding what the pipeline
actually reads: school mask, cast time, base duration, `positiveEffectMask`,
`auraApplyMask`, `negativeEffectMask`, channeled/passive/AoE/single-target flags, base
power cost, the effect handler pointers, and the custom-behaviour index from Stage 6.

`SpellEntry` then becomes what its name says: a DBC row, read at load and never again.
Today a cast touches ~15 fields scattered across 173 columns — six to eight cache lines
pulled in to read sixty bytes. `SpellRuntime` is one line.

**Saves:** not allocations — cycles and cache. This is the stage that shows up in the
map-update time, not in the allocation counter.
**Verify:** assert at load that `SpellRuntime` agrees with the live helpers for all
~22 000 rows; then delete the helpers' hot-path callers. The assert is the test — if it
never fires, the table is faithful; make it fire on purpose once by corrupting one row.

---

## Stage 6 — 445 hardcoded spells, in two piles

Issue 20. `Spell::cast()` runs a `switch (SpellClassSet)` with per-family sub-switches on
**every cast of every spell**, and almost every spell matches nothing.

Mechanical first cut, no new schema: give each spell a `customMask` in `SpellRuntime`,
computed at load by asking "does any handler have a case for this id?". The common cast
becomes `if (!rt.customMask)` and skips every family switch. That is the cheap 90 %.

Then the real work, which is not a performance task: sort the 445 into
- the ones already expressible as `spell_linked` / `spell_bonus` / `spell_proc_event`
  rows — mechanical, deletable today;
- the residue that needs a per-spell behaviour row, which is what MAI's data model is
  for (see the `mangos-mai` skill).

And do not add the 446th.

---

## Stage 7 — pool the `Spell` object  *(last, and only after Stage 1)*

`Spell` is fixed-size and constructed once per cast, per auto-shot, per triggered chain
link. A per-map-update-thread freelist removes the last two allocations from the common
path.

This is deliberately last. It is the smallest win in the list and the largest risk: a
pool turns any surviving lifetime bug from a crash into silent cross-spell corruption.
It is only safe once Stage 1 has made ownership a `unique_ptr` and the review's Issues 1,
2, 3 and 11 — the raw `Item*` and the dangling `m_spellAuraHolder` — are closed.

---

## Where it lands

| stage | baseline | after | landed |
| --- | ---: | ---: | --- |
| 0 — measure | 198 | 198 | `ALLOC_METRICS`, off by default |
| 1 — one owner | 198 | 197 | `Spell` is the event; queue holds a `unique_ptr` |
| 2 — tick lane | 197 | 137 | `RescheduleNextTick`, no node |
| 3 — broadcast encode | 137 | 17 | `EncodeInto` + one buffer per sending thread |
| 4 — small vectors | 17 | 15 | `SmallVector<T,N>` with a growth guard |
| 5 — precompute | 15 | 15 | cycles, not allocations |
| 6 — cast plans | 15 | 15 | cycles, and one switch became data |
| 7 — pool | 15 | 13 | per-thread freelist for `Spell` |

Stage 7 waited on four review defects, and they went in with it: the cast item and the
item target list became guid-resolved, `m_spellAuraHolder` stopped dangling, and
`m_spellAuraHolder` turned out never to have been initialised in the constructor at all.
That was not diligence for its own sake — a pool over any one of those would have turned
a crash into a live `Spell` reading another `Spell`'s fields.

Plus, not in the column: an 8 s channel stops costing 160 allocations, auto-repeat stops
costing 20 per second per player, a 40-target AoE stops costing 80 list nodes, and the
245-line `IsPositiveEffect` leaves the hot path.

## What not to do

- **Do not touch the effect dispatch table.** `SpellEffects[eff]` is already a jump table;
  the `uint8` narrowing there (Issue 14) is a correctness fix, not a speed one.
- **Do not thread the spell pipeline.** It mutates units, auras, threat and inventory that
  the map update owns single-threaded. The win is in not allocating, not in parallelism.
- **Do not pool before Stage 1.** Stated twice on purpose.
