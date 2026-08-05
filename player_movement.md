# Player Movement — audit MaNGOS Zero (1.12.x)

Document de investigație: pachete de mișcare ale jucătorului, fluxuri, movement flags,
validare, opcode-uri, defecte latente, asimetrii, efecte client și sugestii de reparare.

**Scope:** Vanilla 1.12.1–1.12.3 only. Surse principale:

| Zonă | Fișiere |
|------|---------|
| Handlere mișcare | `src/game/WorldHandlers/MovementHandler.cpp` |
| ACK-uri auxiliare | `src/game/WorldHandlers/MiscHandler.cpp` |
| Taxi / spline done | `src/game/WorldHandlers/TaxiHandler.cpp` |
| Structură + flags | `src/game/Object/Unit.h` (`MovementFlags`, `MovementInfo`) |
| Serializare wire | `src/game/Object/Unit.cpp` (`MovementInfo::Read/Write`, `WriteMovementInfo`) |
| Root / fall / fly | `src/game/Object/PlayerMovement.cpp`, `Player.cpp` (`HandleFall`) |
| Speed force | `src/game/Object/UnitSpeed.cpp` |
| Opcode table | `src/game/Server/OpcodeTable.cpp`, `src/proto/Opcodes.h` |
| Update create | `src/game/Object/ObjectUpdate.cpp` (`BuildMovementUpdate`) |
| Transport / deck | `TransportMap.*`, ramura ONTRANSPORT din `HandleMoverRelocation` |

---

## 1. Model de autoritate

**Clientul este autoritatea de poziție pentru jucătorul controlat.** Serverul:

1. Citește `MovementInfo` din pachet.
2. Validează superficial (coordonate finite, extent pe punte).
3. Aplică poziția pe unitatea mover (`SetPosition` / `CreatureRelocation`).
4. Rebroadcast-uiește același opcode + info către ceilalți clienți (`SendMessageToSetExcept`).

Nu există interpolare server-side a traiectoriei jucătorului, nu există verificare de viteză
între pachete, nu există collision/navmesh pe calea de mișcare a playerului. Anti-cheat-ul
efectiv se reduce la:

- `IsValidMapCoord` pe poziție (și o sumă greșită transport+world — vezi §7);
- extent pe offset de punte;
- ACK de viteză forțată (kick dacă clientul raportează viteză *mai mare* decât serverul);
- kick/admin pe `CMSG_WORLD_TELEPORT` / `CMSG_MOVE_SET_RAW_POSITION` (doar SEC_ADMINISTRATOR).

**Implicație:** orice client modificat poate teleporta, speedhack, flyhack, ignore root pe
partea de poziție, atâta timp cât trimite coordonate „valide” ca float-uri pe hartă.

---

## 2. Fluxul principal (MSG_MOVE_*)

```
Client
  │  MSG_MOVE_START_FORWARD / HEARTBEAT / FALL_LAND / …
  ▼
WorldSession (STATUS_LOGGEDIN, PROCESS_THREADSAFE → Map::Update)
  │
  ├─ plMover = _player->GetMover()  (self sau unit possessat)
  ├─ dacă IsBeingTeleported() → discard pachet (rpos=wpos)
  ├─ MovementInfo::Read(recv_data)
  ├─ AdjustMovementInfoTime  // client clock + offset fix pe sesiune + MovementPacketDelay
  ├─ VerifyMovementInfo(movementInfo)  // fără guid pe path-ul normal
  │     └─ pe eșec: ResyncMover() (rate-limit 1s, no-op la bord)
  ├─ dacă FALL_LAND → HandleFall (damage)
  ├─ HandleMoverRelocation(movementInfo)
  │     ├─ m_movementInfo = movementInfo  (înainte de embark)
  │     ├─ ONTRANSPORT embark / leave disembark
  │     ├─ SetInWater toggle
  │     ├─ SetPosition (deck-local dacă TransportMap, altfel world)
  │     ├─ m_movementInfo = movementInfo  (din nou)
  │     ├─ UpdateLiftMinions()
  │     ├─ cancel loot
  │     └─ void fall z < -500
  ├─ UpdateFallInformationIfNeed
  └─ rebroadcast opcode + PackGUID + Write(movementInfo) → set except self
```

### 2.1 Threading

| Mod | Unde rulează | Mișcare |
|-----|--------------|---------|
| `PROCESS_THREADSAFE` | `Map::Update` (thread-ul hărții) | aproape toate MSG_MOVE_*, ACK-uri speed/root/knock |
| `PROCESS_THREADUNSAFE` | `World::UpdateSessions` | `MSG_MOVE_WORLDPORT_ACK`, raw pos, worldport GM |
| `PROCESS_INPLACE` | thread rețea | doar NULL / ServerSide stubs |

Mișcarea pe thread-ul hărții e corectă pentru `SetPosition` pe aceeași mapă. **Problema:**
`TransportMap::Embark` / `Disembark` schimbă mapa jucătorului din handler THREADSAFE. Orice
race cu update-ul altei hărți sau cu sesiunea world e o zonă de interes (vezi §8).

### 2.2 Mover (charm / possess)

`_player->GetMover()` poate fi:

- jucătorul însuși (default `m_mover = this`);
- creatură / jucător possessat (`SetMover` din auras / spells).

Pe path-ul **creature charmed**, `HandleMoverRelocation` face doar:

```cpp
mover->GetMap()->CreatureRelocation((Creature*)mover, x, y, z, o);
```

**nu** actualizează `mover->m_movementInfo`. Create/heartbeat ulterioare pot scrie poziție
vechi din `m_movementInfo` (mitigat parțial de `WriteMovementInfo` care preferă `Where()` pe
uscat — vezi §6).

`HandleMovementOpcodes` **nu** verifică că pachetul aparține mover-ului curent prin guid
(în vanilla, `MovementInfo` din MSG_MOVE_* nu poartă guid-ul unității; guid-ul e adăugat la
rebroadcast de server). Controlul e doar „sesiunea care a trimis pachetul controlează
GetMover()”.

---

## 3. Structura `MovementInfo` (wire 1.12)

Serializare (`Unit.cpp`):

```
uint32 moveFlags
uint32 time
float x, y, z, o
[if ONTRANSPORT]  guid, t_x, t_y, t_z, t_o, t_time
[if SWIMMING]     float s_pitch
[if !ONTRANSPORT] uint32 fallTime     // ← intenționat omis pe transport/taxi
[if FALLING]      float velocity, sinAngle, cosAngle, xyspeed
[if SPLINE_ELEVATION] float u_unk1
```

### 3.1 Movement flags (enum `MovementFlags`)

| Bit | Flag | Rol 1.12 |
|-----|------|----------|
| 0x00000001 | `FORWARD` | mișcare înainte |
| 0x00000002 | `BACKWARD` | înapoi |
| 0x00000004 | `STRAFE_LEFT` | |
| 0x00000008 | `STRAFE_RIGHT` | |
| 0x00000010 | `TURN_LEFT` | |
| 0x00000020 | `TURN_RIGHT` | |
| 0x00000040 | `PITCH_UP` | pitch (înot / fly residual) |
| 0x00000080 | `PITCH_DOWN` | |
| 0x00000100 | `WALK_MODE` | walk vs run |
| 0x00000400 | `LEVITATING` | levitate / hover-ish |
| 0x00000800 | `FLYING` | **[-ZERO] valoare dubioasă** |
| 0x00002000 | `FALLING` | salt / cădere (are jump block) |
| 0x00004000 | `FALLINGFAR` | cădere lungă |
| 0x00200000 | `SWIMMING` | înot (+ pitch) |
| 0x00400000 | `SPLINE_ENABLED` | spline NPC / taxi |
| 0x00800000 | `CAN_FLY` | **[-ZERO] dubios pe vanilla** |
| 0x01000000 | `FLYING_OLD` | **[-ZERO] dubios** |
| 0x02000000 | `ONTRANSPORT` | pe navă / lift / elevator |
| 0x04000000 | `SPLINE_ELEVATION` | taxi path elevation |
| 0x08000000 | `ROOT` | root flag pe wire |
| 0x10000000 | `WATERWALKING` | water walk |
| 0x20000000 | `SAFE_FALL` | safe fall aura |
| 0x40000000 | `HOVER` | hover |

`movementFlagsMask` (verificări cast spell) include FORWARD/BACK/STRAFE/PITCH/ROOT/FALLING/
FALLINGFAR/SPLINE_ELEVATION — **nu** include TURN, SWIMMING, ONTRANSPORT, WALK_MODE.

Comentariul din `Unit.h` recunoaște explicit: *„[-ZERO] Need check and update used in most
movement packets”* — flags de fly sunt moștenite din core-uri multi-expansion și pot fi
greșite pentru 1.12.

---

## 4. Catalog opcode-uri mișcare

### 4.1 Implementate (handler real)

| Opcode | Handler | Procesare | Note |
|--------|---------|-----------|------|
| `MSG_MOVE_START_FORWARD` … `SET_WALK_MODE` | `HandleMovementOpcodes` | THREADSAFE | bulk path |
| `MSG_MOVE_FALL_LAND` | same + `HandleFall` | THREADSAFE | fall damage |
| `MSG_MOVE_START/STOP_SWIM` | same | THREADSAFE | |
| `MSG_MOVE_SET_FACING` / `SET_PITCH` | same | THREADSAFE | |
| `MSG_MOVE_HEARTBEAT` | same | THREADSAFE | ~periodice |
| `CMSG_MOVE_FALL_RESET` | same | THREADSAFE | |
| `CMSG_MOVE_CHNG_TRANSPORT` | same | THREADSAFE | **@TODO vanilla usage** |
| `MSG_MOVE_TELEPORT_ACK` | `HandleMoveTeleportAckOpcode` | THREADSAFE | near TP |
| `MSG_MOVE_WORLDPORT_ACK` | `HandleMoveWorldportAckOpcode` | THREADUNSAFE + STATUS_TRANSFER | far TP |
| `CMSG_FORCE_*_SPEED_CHANGE_ACK` | `HandleForceSpeedChangeAckOpcodes` | THREADSAFE | anti speed |
| `CMSG_FORCE_MOVE_ROOT/UNROOT_ACK` | `HandleMoveRoot/UnRootAck` | THREADSAFE | **no-op** |
| `CMSG_MOVE_KNOCK_BACK_ACK` | `HandleMoveKnockBackAck` | THREADSAFE | relocate + rebroadcast |
| `CMSG_MOVE_HOVER_ACK` | `HandleMoveHoverAck` | THREADSAFE | parse only |
| `CMSG_MOVE_WATER_WALK_ACK` | `HandleMoveWaterWalkAck` | THREADSAFE | parse only |
| `CMSG_MOVE_FEATHER_FALL_ACK` | `HandleFeatherFallAck` | THREADSAFE | **discard** |
| `CMSG_MOVE_TIME_SKIPPED` | `HandleMoveTimeSkippedOpcode` | INPLACE | **log only** |
| `CMSG_MOVE_NOT_ACTIVE_MOVER` | `HandleMoveNotActiveMoverOpcode` | THREADSAFE | stash mi |
| `CMSG_SET_ACTIVE_MOVER` | `HandleSetActiveMoverOpcode` | THREADUNSAFE | log mismatch |
| `CMSG_MOVE_SPLINE_DONE` | `HandleMoveSplineDoneOpcode` | THREADSAFE | taxi multi-node |
| `CMSG_MOVE_SET_RAW_POSITION` | `HandleMoveSetRawPosition` | THREADUNSAFE | GM only |
| `CMSG_WORLD_TELEPORT` | `HandleWorldTeleportOpcode` | THREADUNSAFE | GM only |

### 4.2 STATUS_NEVER / Handle_NULL (client→server blocate sau nefolosite)

Cheat / debug (corect blocate pe server public):

- `MSG_MOVE_TOGGLE_LOGGING`, `TOGGLE_FALL_LOGGING`, `TOGGLE_COLLISION_CHEAT`, `TOGGLE_GRAVITY_CHEAT`
- `MSG_MOVE_TELEPORT_CHEAT`, `SET_*_SPEED_CHEAT`, `SET_ALL_SPEED_CHEAT`
- `CMSG_MOVE_START/STOP_SWIM_CHEAT`, `MSG_MOVE_START/STOP_SWIM_CHEAT`

Server→client only (STATUS_NEVER + ServerSide stub pe path client — normal):

- `SMSG_MOVE_WATER_WALK`, `LAND_WALK`, `KNOCK_BACK`, `FEATHER_FALL`, `NORMAL_FALL`, `SET/UNSET_HOVER`
- `SMSG_FORCE_*_SPEED_CHANGE`, `SMSG_FORCE_MOVE_ROOT/UNROOT`
- `SMSG_MOVE_SET/UNSET_FLIGHT` (TBC+; pe Zero e moștenire)

**NULL dar pot fi relevante pe wire 1.12 (asimetrie / neimplementare):**

| Opcode | Severitate | Observație |
|--------|------------|------------|
| `MSG_MOVE_TELEPORT` | Medie | Server ar putea vrea să-l emită; near TP folosește `MSG_MOVE_TELEPORT_ACK` pe canal client. |
| `MSG_MOVE_ROOT` / `MSG_MOVE_UNROOT` | Medie | Broadcast alternativ la `SMSG_FORCE_MOVE_*`; root pe Zero folosește doar SMSG_FORCE. |
| `MSG_MOVE_KNOCK_BACK` | Joasă | Emis din ACK handler ca broadcast; table status NEVER pe inbound. |
| `MSG_MOVE_HOVER` | Joasă | Similar hover. |
| `MSG_MOVE_FEATHER_FALL` / `MSG_MOVE_WATER_WALK` | Joasă | Variante MSG vs SMSG. |
| `MSG_MOVE_SET_*_SPEED` (non-cheat) | Medie | Server forțează prin SMSG_FORCE_*; MSG_MOVE_SET_* e NULL. |
| `MSG_MOVE_TIME_SKIPPED` | **Ridicată** | Server **nu** retransmite time-skip către alții (vezi §7). |
| `CMSG_MOVE_FLIGHT_ACK` | N/A Zero | Flight TBC; corect NULL pe 1.12. |
| `CMSG_MOVE_SET_RUN_SPEED` (0x3AB) | Joasă | @TODO în OpcodeTable. |
| `MSG_MOVE_SET_RAW_POSITION_ACK` | Joasă | GM path nu trimite ACK. |

### 4.3 Server → client (emise din cod, nu din table inbound)

| API | Opcode | Unde |
|-----|--------|------|
| `Player::SetRoot` | `SMSG_FORCE_MOVE_ROOT` / `UNROOT` | `PlayerMovement.cpp` |
| `Player::SetWaterWalk` | `SMSG_MOVE_WATER_WALK` / `LAND_WALK` | doar către self (`GetSession()->SendPacket`) — **asimetrie** |
| `Player::SetFeatherFall` | `SMSG_MOVE_FEATHER_FALL` / `NORMAL_FALL` | `SendMessageToSet` |
| `Player::SetHover` | `SMSG_MOVE_SET/UNSET_HOVER` | set |
| `Player::SetCanFly` | flags + `SendHeartBeat` | **fără opcode fly 1.12 real** |
| `Player::SetLevitate` | empty | TODO stub |
| `Unit::SetSpeedRate(forced)` | `SMSG_FORCE_*` + spline set to others | `UnitSpeed.cpp` |
| `WorldSession::SendKnockBack` | `SMSG_MOVE_KNOCK_BACK` | |
| `Player::BuildTeleportAckMsg` | `MSG_MOVE_TELEPORT_ACK` | near teleport |
| `Unit::SendHeartBeat` | `MSG_MOVE_HEARTBEAT` | |

---

## 5. Fluxuri speciale

### 5.1 Near teleport

1. `Player::TeleportTo` same map → `SetSemaphoreTeleportNear(true)`, `BuildTeleportAckMsg` → client.
2. Mișcarea normală e **ignorată** cât timp `IsBeingTeleported()`.
3. Client trimite `MSG_MOVE_TELEPORT_ACK` (guid, counter, time).
4. Server verifică guid == plMover, `SetPosition` din `GetTeleportDest`, zone update, pet resummon.

**Goluri:**

- `counter` din ACK **nu e validat** (orice counter acceptat).
- Nu se re-scrie `m_movementInfo` poziția în `HandleMoveTeleportAckOpcode` (doar `SetPosition`);
  mitigat de `WriteMovementInfo` care citește `Where()` pe uscat.
- Dacă ACK nu vine niciodată, jucătorul rămâne cu semaphore near → mișcare blocată până la
  logout / alt TP.

### 5.2 Far teleport (worldport)

1. `SMSG_TRANSFER_PENDING` + `SMSG_NEW_WORLD` (în `TeleportTo`).
2. Client loading → `MSG_MOVE_WORLDPORT_ACK` (STATUS_TRANSFER, THREADUNSAFE).
3. `HandleMoveWorldportAckOpcode`: validare coord, BG instance, `SetMap`, `Place().MoveTo`,
   **`m_movementInfo.ChangePosition`** (fix recent: pose veche pe hartă nouă),
   `m_clientGUIDs.clear()`, `BoardingMap()->Add`, packets, flight resume, mount check, honorless,
   pet, delayed ops.

**Efecte client dacă `m_movementInfo` nu e seed-uit:** unitatea apare pe noua hartă cu
coordonate/facing de pe harta veche (ex. tele de pe punte → Icecrown pose pe EK). Fix parțial
deja în cod.

### 5.3 Knockback

1. Server: `SendKnockBack` → `SMSG_MOVE_KNOCK_BACK` (PackGUID + sequence + cos/sin + speeds).
2. Client ACK: `CMSG_MOVE_KNOCK_BACK_ACK` (guid + unused + MovementInfo).
3. `VerifyMovementInfo(mi, guid)`, relocate, rebroadcast `MSG_MOVE_KNOCK_BACK` cu **full GUID
   ne-packed** + jump fields.

**Asimetrie:** SMSG folosește PackGUID; broadcast MSG folosește `GetObjectGuid()` raw. Client
vanilla acceptă de obicei ambele pe MSG, dar e o discrepanță de format față de restul
rebroadcast-urilor (care folosesc PackGUID).

### 5.4 Speed force ACK

- Server incrementează `m_forced_speed_changes[mtype]` la fiecare `SetSpeedRate(..., forced)`.
- Client ACK: skip pe counter-uri intermediare; pe ultimul compară `newspeed` cu `GetSpeed`.
- Dacă server > client (client mai lent): force re-set.
- Dacă client > server: **KickPlayer**.
- **Except:** pe transport (`GetTransport()`), verificarea e ocolită complet.

**Goluri:**

- Nu se validează counter-ul din pachet.
- Nu se aplică relocate din MovementInfo din ACK (doar viteză).
- Mount/run share pe același bucket (comentariu în cod) — fragile la spam de update speed.

### 5.5 Root / stun

`Aura::HandleAuraModRoot` → `UNIT_STAT_ROOT` + `SetRoot(true)` + **clear toate movement flags**.

`HandleMoveRootAck` / `UnRootAck`: **consumă pachetul, zero logică**.

**Efect:** client-ul vanilla se oprește pe SMSG_FORCE_MOVE_ROOT. Client modificat poate
continua să trimită MSG_MOVE_*; serverul **acceptă și rebroadcast-uiește** poziția nouă.
Root pe server e „cosmetico-combat” (cast interrupt, AI), nu un hard lock pe poziție.

### 5.6 Taxi / spline

`CMSG_MOVE_SPLINE_DONE`:

- far multi-map: interrupt flight, teleport la nod, continue;
- next destination: `SendDoFlight`;
- end: clear taxi.

Mișcarea normală în zbor taxi: fall damage ignorat (`IsTaxiFlying()`). `fallTime` omis când
`ONTRANSPORT` (comentariu: „never sent when on taxi”).

### 5.7 Transport / deck map (zonă critică)

Filozofia din cod (și din regulile proiectului): **puntea e o hartă**; coordonatele deck-local
sunt adevărul; nu se compune `hull_world ⊗ offset`.

La `HandleMoverRelocation`:

1. Assign `m_movementInfo` **înainte** de embark (fix pet-through-walls: Add citea offset
   vechi fără transport).
2. ONTRANSPORT + fără `m_transport`: caută vessel în `sMapMgr.m_Transports`, `Embark`.
3. fără ONTRANSPORT + avea transport: `Disembark` doar dacă world pos e lângă navă
   (`HullRadius + NodeSlack + DECK_EDGE_MARGIN`); altfel disembark la pose-ul navelor
   (evită Lordamere Lake din world (0,0,0) echo).
4. Poziție: pe `TransportMap` → `SetPosition(transport offset)`; altfel world pos.
5. `UpdateLiftMinions` pentru elevator type-11 (nu vessel list).

Validare ONTRANSPORT (diff recent): extent simetric pe |x|,|y|,|z| cu `HullRadius +
DECK_EDGE_MARGIN` (sau `MAX_DECK_EXTENT=250`), nu vechiul „doar x,y > 50 pozitiv”.

---

## 6. Zone de interes (cod cald)

| Zonă | De ce |
|------|--------|
| `HandleMovementOpcodes` + `HandleMoverRelocation` | orice bug de poziție/transport/root |
| `VerifyMovementInfo` | singura barieră anti-cheat de poziție |
| `MovementInfo::Read` | format wire; desync pe flags greșite = read shift |
| `Unit::WriteMovementInfo` | create/heartbeat: deck vs shore, sync Where() |
| `Player::HandleFall` | combat fairness |
| `HandleForceSpeedChangeAckOpcodes` | anti-speed; false positive kick |
| `TransportMap::Embark/Disembark` | map change mid-packet |
| `UpdateLiftMinions` | pet pe lift / leave |
| `HandleMoveWorldportAckOpcode` | far TP, BG, flight, clientGUID clear |
| `Player::SetRoot` / root ACK no-op | control vs hack |
| `CMSG_MOVE_TIME_SKIPPED` | lag compensation observers |

---

## 7. Defecte, buguri latente, erori logice

Severități: **S0** catastrofic / exploit trivial · **S1** high · **S2** medium · **S3** low · **S4** cosmetic / debt.

### 7.0 Stare curentă — ce e reparat în arbore

Verificat direct în surse. **Nimic nu a fost compilat sau jucat.**

| ID | Stare | Unde |
|----|-------|------|
| 7.4 TIME_SKIPPED doar log | **REPARAT** | `MovementHandler.cpp` — validează mover, avansează `m_movementInfo`, rebroadcast `MSG_MOVE_TIME_SKIPPED` (0x319, există pe 1.12); opcode mutat `PROCESS_INPLACE` → `PROCESS_THREADSAFE` |
| 7.6 compunere world+transport | **REPARAT** | `VerifyMovementInfo` — suma ștearsă, rămâne doar bound-ul pe punte |
| 7.10 charm fără `m_movementInfo` | **REPARAT** | ramura charmed relocează deck-aware **și** scrie `mover->m_movementInfo` |
| 7.11 `NotActiveMover` pe player | **REPARAT** | se aplică pe old mover găsit prin guid, sau pe nimeni |
| 7.15 `SetWaterWalk` doar self | **REPARAT** | `SendMessageToSet` + seed `MOVEFLAG_WATERWALKING` |
| 7.3 ACK-uri no-op | **PARȚIAL** | hover + water walk trec prin `ApplyStateAck` (verify + relocate); root/unroot/feather rămân drenate — vezi 7.0.2 |
| — resync la reject | **NOU** | `ResyncMover()`, rate-limit 1 s, no-op la bord — acoperă golul din §7.1/§8 „alții te văd înghețat" |

**Trei defecte care nu erau în acest document**, găsite comparând cu auditul lui 01 și confirmate în sursele 00:

| Defect | Unde | Stare |
|--------|------|-------|
| `Player::TeleportTo` compune `final_* += transportPosition->*` | `Player.cpp` | **REPARAT** — compunerea ștearsă; înălțimea de cădere devine Z-ul de pe punte când e la bord |
| `PlayerLoad` validează `Where() + transportPosition` la login | `PlayerLoad.cpp` | **REPARAT** — trimitea la homebind un caracter delogat pe navă |
| `UpdateFallInformationIfNeed` folosește Z-ul world la bord | `PlayerTalent.cpp` | **REPARAT** — folosește Z-ul de pe punte cât timp `ONTRANSPORT` |

Ultimele două explică direct „fall damage după zeppelin" din §8, împreună cu 7.8.

#### 7.0.1 Divergență față de 01, deliberată — `SetRoot` NU seed-ează flag-ul

Pe 01 toate cele cinci setter-e forțate scriu flag-ul în `m_movementInfo` înainte să trimită.
Pe 00 s-au făcut doar trei: `WATERWALKING`, `SAFE_FALL`, `HOVER`.

`MOVEFLAG_ROOT` pe 1.12 este `0x08000000` — **alt bit** decât `0x00000800` de pe 2.4.3 — și
poartă în `Unit.h` comentariul `// used for flight paths`, exact același comentariu care stă pe
`MOVEFLAG_SPLINE_ELEVATION` deasupra lui. Seamănă cu un copy-paste rămas de când
`SPLINE_ENABLED` s-a mutat pe `0x00400000`, dar „seamănă" nu e dovadă: wowdev.wiki a răspuns
403, iar `MOVEFLAG_ROOT` e în `movementFlagsMask`, deci un bit greșit schimbă tăcut cast-ul de
vrăji și rutele de taxi. `Creature::SetRoot` din 00 folosește bitul ca root, dar e aceeași sursă
— nu o confirmare independentă. Se seed-ează după o captură sau o referință 1.12.

#### 7.0.3 Modelul de ceas (7.5 / 7.23) — portat pe jumătate, și jumătatea lipsă e o limită de client

`HandleMoverRelocation` făcea `movementInfo.UpdateTime(GetTime() + GetLatency())`, cu comentariul
„carry the client's own clock forward by the round trip; do not translate it into a server time
base". Jumătatea a doua a comentariului merită păstrată, prima e defectul: **`GetLatency()` e
revizuit de fiecare PING**, deci două pachete consecutive pot fi împinse cu valori diferite și pot
ajunge **în ordine inversă** în timeline-ul observatorului, care repoziționează unitatea înapoi.

**Ce s-a portat din 01:**

| Piesă | Unde |
|-------|------|
| `m_clientTimeDelay` + `AdjustMovementInfoTime()` — offset **fix pe sesiune**, latch-uit din primul pachet de mișcare | `WorldSession.{h,cpp}` |
| `MovementPacketDelay` (default 500 ms) — playout buffer | `World.h`, `WorldConfig.cpp`, `mangosd.conf.dist.in` |
| `MovementStreamTime()` — aceeași ștampilă pentru heartbeat **și** create block | `Unit.{h,cpp}`, `ObjectUpdate.cpp` |
| `ResetClientTimeDelay()` la login (nu la ping — un reset pe ping ar re-latch-ui și ar da salt) | `CharacterHandler.cpp` |
| Apel în cele 3 locuri care citesc un pachet de pe fir: mișcare, knockback ACK, `ApplyStateAck` | `MovementHandler.cpp` |

Re-bazarea printr-un offset constant **păstrează exact delta-urile clientului** — deci respectă
ce voia comentariul vechi — și nu le poate inversa.

**Ce NU se poate porta: TIME_SYNC. Nu e o alegere, e o limită de client.**

`SMSG_TIME_SYNC_REQ` / `CMSG_TIME_SYNC_RESP` (0x390/0x391 pe 2.4.3 și 3.3.5) **nu există pe
1.12**. Trei surse: lipsesc din `src/proto/Opcodes.h` al acestui arbore; lipsesc din tabela
1.12.1 a lui VMaNGOS (core specializat pe vanilla), unde opcode-urile vecine se potrivesc exact
cu ale noastre (`CMSG_MOVE_TIME_SKIPPED = 718` = 0x2CE, `MSG_MOVE_TIME_SKIPPED = 793` = 0x319);
și apar pentru prima dată în 01/02. wowdev.wiki a dat 403 și nu a fost folosit.

Consecința: pe 00 offset-ul e **latch-uit o dată și nerevizuit niciodată**. Pe 01 filtrul RTT +
dead-band îl rafinează continuu. Asta e o divergență corectă, nu o restanță — **`PushTimeSyncSample`
și filtrul lui nu au ce rafina pe 1.12 și nu trebuie adăugate.** Drift-ul de ceas pe durata unei
sesiuni deplasează toate pachetele egal, iar clientul îl absoarbe (urmărește skew per mover).

**Efect secundar reparat pe drum:** `BuildMovementUpdate` din 00 **nu ștampila deloc** timpul în
create block — ducea ce rămăsese în `m_movementInfo` de la ultimul pachet relay-at, sau zero
pentru orice nu se mișcase vreodată. 01 îl ștampila. Acum ambele folosesc `MovementStreamTime()`.

**De verificat în joc, e zona care a rupt mișcarea o dată:** `MovementPacketDelay = 0` vs `500`
trebuie să difere doar ca netezime, niciodată ca poziție finală; `wire` nu trebuie să meargă
înapoi pentru același mover (`LogFilter_PlayerMoves = 0`).

#### 7.0.2 Ce NU s-a atins, și de ce

- **7.1 / 7.2 anti-cheat și root gate (S0/S1).** Subsistem lipsă, nu defect în cod existent. Un
  check de viteză pe jumătate dă kick la fiecare mount, blink și knockback. Merită PR propriu cu
  model de toleranță — exact cum spune §11.2.
- **7.3 root/unroot/feather ACK.** Corpul lor există doar ca ghicitură comentată, identică în
  toate cele patru core-uri și verificată de nimeni. Un layout greșit aruncă
  `ByteBufferException` și pică sesiunea — mai rău decât a ignora pachetul.
- **7.16 `SetCanFly`.** Face `SetMovementFlags(MOVEFLAG_NONE)` pe dezactivare, ceea ce șterge
  **toate** flag-urile, inclusiv `ONTRANSPORT`. E GM-only și bitii de fly sunt marcați `[-ZERO]`
  ca nesiguri; lăsat neatins intenționat, dar e un defect real.
- **7.21 lookup transport global, 7.22 threading Embark.** Neatinse: cer audit de ciclu de viață
  pe hărți, nu un diff local.
- Restul S3/S4 (7.17 counter TP, 7.19 format knockback, 7.24 opcode-uri moarte, 7.27 loot) —
  datorie tehnică, neatinse.

**Notă de igienă:** `CharacterHandler.cpp`, `Master.{cpp,h}` și `IocpServer.cpp` apar modificate
în working tree din altă lucrare; nu au fost atinse aici.

### 7.1 Client-authoritative fără speed/distance check — **S0**

Nu se compară Δpos / Δtime cu `GetSpeed` × margin. Speedhack, blink, wallhack de poziție
funcționează. Speed ACK prinde doar nepotrivirea pe **forțarea de viteză server**, nu viteza
reală de deplasare.

**Reparare:** istoric de ultimele N pachete; max distance = speed * dt * lag_factor;
soft-correct (teleport back) + escalate kick; opțional MMaps raycast pe Δ.

### 7.2 Root / stun nu blochează relocate — **S1**

Server acceptă mișcare sub `UNIT_STAT_ROOT` / `STUNNED` / `NOT_MOVE`.

**Reparare:** în `HandleMovementOpcodes`, dacă `plMover->hasUnitState(UNIT_STAT_CAN_NOT_MOVE)`
(și nu knockback activ), discard sau clamp la ultima poziție + optional re-root packet.

### 7.3 Root/Unroot/FeatherFall/Hover/WaterWalk ACK no-op — **S2**

Fără counter, fără validare, fără apply MovementInfo. Desync posibil dacă client-ul ACKs
târziu sau deloc; serverul crede că aura e aplicată, clientul încă mișcă.

**Reparare:** pattern ca la speed: counter per force; pe ACK final aplică flags / ignore
mișcare contradictorie; pe timeout re-send force packet.

### 7.4 `CMSG_MOVE_TIME_SKIPPED` doar log — **S1**

Vanilla: client anunță timp sărit (lag); server ar trebui să propage `MSG_MOVE_TIME_SKIPPED`
către set ca alții să ajusteze interpolarea. Fără asta: **rubber-band / desync vizual** pe
lag spikes pentru observers (nu neapărat pentru self).

**Reparare:** după validare guid+mover, rebroadcast `MSG_MOVE_TIME_SKIPPED` (guid + time);
opțional ajustează baza de timp a mișcării.

### 7.5 Rewrite timp: `time + GetLatency()` — **S2**

Diff local a înlocuit formula veche (`m_clientTimeDelay + 300ms`). Noua formă:

```cpp
movementInfo.UpdateTime(movementInfo.GetTime() + GetLatency());
```

- `GetLatency()` e de obicei RTT din ping — poate **dubla** compensarea.
- Se aplică pe **toate** pachetele relocate, inclusiv knockback.
- Valoarea rescrisă e ce văd ceilalți clienți.

**Reparare:** documentează modelul de ceas (client clock + one-way estimate); măsoară one-way
≈ RTT/2; nu amesteca server GameTime în câmpul client; teste pe lag 50/200/500ms.

### 7.6 `VerifyMovementInfo` încă compune world + transport offset — **S2**

```cpp
IsValidMapCoord(pos.x + tpos.x, pos.y + tpos.y, pos.z + tpos.z, pos.o + tpos.o)
```

Contradictoriu cu modelul „deck-local only / nu compune”. Aboard, world e adesea (0,0,0) →
suma = offset (trece). La leave cu garbage, poate respinge pachete valide sau accepta sume
nonsens.

**Reparare:** pe ONTRANSPORT validează **doar** deck pos + guid transport cunoscut; pe leave
validează world pos separat; **șterge** suma compusă.

### 7.7 fallTime absent pe ONTRANSPORT — **S3**

Sări pe punte / elevator: fără fallTime în wire. `HandleFall` oricum skip pe
`MOVEFLAG_ONTRANSPORT`. Jump pe platformă fără flag transport ar putea avea fallTime greșit
dacă flag-ul oscillates.

### 7.8 Fall damage: doar ΔZ față de `m_lastFallZ` — **S2**

Nu folosește `fallTime` din pachet pentru damage. Client poate:

- reseta fall cu pachete intermediare care ridică `m_lastFallZ` (`UpdateFallInformationIfNeed`);
- evita damage cu water flag / feather aura (corect) sau cu ONTRANSPORT spurios (**S1** dacă
  client setează ONTRANSPORT pe uscat fără vessel — extent 250 poate trece cu t_pos mic).

**Reparare:** cere vessel/GO valid pentru ONTRANSPORT; altfel strip flag. Fall: max( cumulative
fall, formula Z).

### 7.9 Swimming / water toggle confuz — **S3**

```cpp
if (SWIMMING flag != IsInWater())
    SetInWater(!IsInWater() || IsUnderWater(pos));
```

Logica e un toggle special-case pentru jump sub apă, nu o setare directă din flag.
`IsInWater` poate rămâne desincronizat de liquid map → breath / spell / zone effects greșite.

**Reparare:** `SetInWater(terrain liquid check la pos)`; flag-ul client e hint, nu autoritate.

### 7.10 Charm: CreatureRelocation fără `m_movementInfo` — **S2**

Posess creature: poziția live e în `Where()`, dar `m_movementInfo` rămâne vechi până la alt
writer. Orice cod care citește flags/pos din `m_movementInfo` pe creatură e greșit.

**Reparare:** `mover->m_movementInfo = movementInfo` (+ ChangePosition) și pe branch-ul
creature.

### 7.11 `HandleMoveNotActiveMover` scrie pe `_player->m_movementInfo` — **S2**

Când old mover ≠ current mover, stochează mi pe **player**, nu pe unitatea care a fost
mover. La possess end, starea jucătorului poate fi suprascrisă cu mi de pe creatură (flags
transport/falling greșite).

### 7.12 `HandleSetActiveMover` doar loghează mismatch — **S3**

Nu corectează client control, nu resync. Client confuz rămâne confuz.

### 7.13 Double assign `m_movementInfo` + ClearTransportData pe leave — **S3**

După disembark, `movementInfo.ClearTransportData()` apoi re-assign pe player — OK.
Între cele două assign-uri, codul de minion citește starea post-first-assign — intenționat
pentru embark. Fragile la reorder viitor.

### 7.14 `CMSG_MOVE_CHNG_TRANSPORT` pe path generic — **S3**

Marcat `@TODO need to check usage in vanilla`. Pe 1.12 elevators adesea trimit mișcare
normală cu ONTRANSPORT. Path-ul generic e probabil OK, dar fără test dedicat.

### 7.15 `SetWaterWalk` trimite doar la self — **S2 asimetrie**

`SetFeatherFall` / `SetHover` / `SetRoot` → `SendMessageToSet`.
`SetWaterWalk` → `GetSession()->SendPacket` only.

**Efect client (alții):** jucătorul pare că înoată / se scufundă greșit pe water-walk până la
următorul update full.

**Reparare:** `SendMessageToSet` ca celelalte.

### 7.16 `SetCanFly` / `SetLevitate` pe 1.12 — **S2**

`SetLevitate`: stub gol.
`SetCanFly`: setează LEVITATING|SWIMMING|CAN_FLY|FLYING și heartbeat. Flags fly sunt marcate
[-ZERO] ca nesigure. GM fly poate arăta bizar altor clienți (swim anim în aer).

### 7.17 Near TP counter invalidat — **S3**

Counter din `MSG_MOVE_TELEPORT_ACK` citit și logat, niciodată comparat cu un secvențial
server. Replay/stale ACK teoretic posibil în race multi-TP.

### 7.18 Void fall z < -500 — **S2**

Aplicat de multe ori cât timp pachetele continuă (TODO în cod: discard după root).
Secvență: damage void → kill → repop graveyard → resurrect 50% → corpse bones — agresiv și
reintrant.

### 7.19 Knockback broadcast format — **S3**

Vezi §5.3 PackGUID vs full GUID.

### 7.20 Speed ACK pe transport ocolit — **S3**

Pe navă, client poate ACK-ui viteze arbitrare fără kick. Combinat cu 7.1 e secondary.

### 7.21 Embark din lista globală `m_Transports` — **S2**

Caută vessel doar în setul global, nu pe mapa curentă / instance. Instance transport sau
timing greșit → ONTRANSPORT acceptat dar `m_transport` null → poziție world din pachet pe
când client e pe punte (desync).

### 7.22 `PROCESS_THREADSAFE` + map transfer transport — **S1** (latent)

`Embark`/`Disembark` mută player între hărți din Map::Update al hărții curente. Necesită
garanții stricte că obiectul e scos/adăugat fără a fi tick-uit pe ambele hărți. Orice
regresie aici = crash use-after-free sau dublu update.

### 7.23 Heartbeat server `GameTime` vs client time — **S3**

`SendHeartBeat` pune `GameTime::GetGameTimeMS()` în câmpul time, pe când pachetele client
poartă client clock + latency. Observerii văd baze de timp amestecate → micro-jitter.

### 7.24 Opcode-uri fly TBC în table Zero — **S4**

`SMSG_MOVE_SET_FLIGHT`, `CMSG_MOVE_FLIGHT_ACK` etc. STATUS_NEVER — dead weight, confuzie
la audit.

### 7.25 `HandleFall` și Gust of Wind (43621) — **S4**

Hardcoded dummy aura check; documentează ca special-case vanilla.

### 7.26 Movement în timpul `IsBeingTeleported` discard silent — **S4**

Corect pentru anti-desync; pe lag lung la worldport, clientul „îngheață” server-side până
ACK (așteptat).

### 7.27 Loot cancel pe orice relocate — **S4**

Inclusiv micro-heartbeat / set facing poate închide loot — UX enervant dacă facing e
considerat relocate (da, trece prin HandleMoverRelocation).

**Reparare opțională:** cancel loot doar dacă Δpos > epsilon.

### 7.28 Flags fly greșite în create update — **S3**

Dacă `m_movementInfo` reține CAN_FLY/FLYING de la GM sau bug, `BuildMovementUpdate` le trimite
la toți la create object.

### 7.29 Playerbots setează FLYING|CAN_FLY — **S3** (modul)

`playerbot` forțează flags non-vanilla; pe server Zero afectează vizibilitatea bots.

---

## 8. Efecte probabile la client

| Simptom | Cauză probabilă în server |
|---------|---------------------------|
| Rubber-banding la lag (alții te văd sări) | TIME_SKIPPED nepropagat; time rewrite greșit |
| Pet iese prin punte / sub keel | m_movementInfo assign order (fixat); sau minion fără lift update |
| Teleport de pe navă în Lordamere Lake | disembark pe world (0,0,0) — mitigat de reach check |
| Îngheț pe punte înaintea catargului | vechi extent +50 only — mitigat de extent simetric |
| Pose greșită după `.tele` / worldport | m_movementInfo neactualizat la far TP — mitigat ChangePosition |
| Jucător „înoată” pe uscat pentru alții | waterwalk nebroadcast; flags SWIMMING rămase |
| Root vizibil dar se mișcă (hack) | root ACK no-op + relocate acceptat |
| Kick la mount/dismount spam | false positive speed ACK |
| Knockback desincronizat observers | format MSG vs SMSG; jump fields |
| Fall damage 0 de pe stâncă | ONTRANSPORT spurios; feather; safe fall; z_diff < 14.57 |
| Void death loop | z<-500 reentrant până iese din zonă |
| Charm target „sare” la release | mi pe player din NotActiveMover |
| Facing update închide fereastra de loot | loot release pe orice relocate |

---

## 9. Asimetrii rezumate

| Aspect | Self / SMSG | Others / rebroadcast | Problemă |
|--------|-------------|----------------------|----------|
| Water walk | packet self only | — | alții nu văd waterwalk |
| Root | set | set | OK, dar ACK ignorat |
| Feather / hover | set | set | ACK ignorat |
| Knockback out | PackGUID | MSG full GUID | format split |
| Speed force | SMSG_FORCE self | SMSG_SPLINE set | dual path intentional |
| Movement time | client+latency | same rewritten | vs heartbeat GameTime |
| Transport write | deck in WriteMovementInfo | client MSG raw | server rewrite pe create |
| SetCanFly | flags+heartbeat | same | no real 1.12 fly opcode |

---

## 10. Matrice severități + prioritate fix

| ID | Issue | Sev | Efort | Prioritate |
|----|-------|-----|-------|------------|
| 7.1 | Fără anti-speed pe Δpos | S0 | L | P0 dacă PvP/competitiv |
| 7.2 | Root nu blochează move | S1 | S | P0 |
| 7.4 | TIME_SKIPPED dead | S1 | S | P1 |
| 7.22 | Embark pe map thread | S1 | M | P1 audit + assert |
| 7.6 | Compose world+transport | S2 | S | P1 |
| 7.5 | Latency time model | S2 | M | P1 measure |
| 7.15 | WaterWalk broadcast | S2 | S | P1 |
| 7.10 | Charm m_movementInfo | S2 | S | P1 |
| 7.11 | NotActiveMover target | S2 | S | P2 |
| 7.3 | Force ACK no-ops | S2 | M | P2 |
| 7.8 | Fall / fake ONTRANSPORT | S2 | M | P2 |
| 7.21 | Transport lookup global | S2 | M | P2 |
| 7.18 | Void fall reentrant | S2 | S | P2 |
| 7.9 | Water toggle | S3 | S | P3 |
| 7.19 | Knockback GUID format | S3 | S | P3 |
| 7.17 | TP counter | S3 | S | P3 |
| 7.23 | Heartbeat clock mix | S3 | S | P3 |
| 7.24 | Opcode dead TBC | S4 | S | cleanup |

S = small, M = medium, L = large.

---

## 11. Sugestii de reparare (concrete)

### 11.1 Hardening minim (diff mic)

1. **Root/stun gate** la începutul `HandleMovementOpcodes` / `HandleMoverRelocation` (except
   knockback ACK și teleport state).
2. **ONTRANSPORT** obligatoriu cu guid rezolvabil (`Transport` sau `GAMEOBJECT_TYPE_TRANSPORT`);
   altfel strip flag + reject sau treat as ground.
3. **Șterge** validarea `pos + tpos` compusă; validează câmpurile separat.
4. **`SetWaterWalk`**: `SendMessageToSet`.
5. **Charm branch**: copiază `movementInfo` pe creature.
6. **`CMSG_MOVE_TIME_SKIPPED`**: rebroadcast validat.
7. **Void / rooted**: nu procesa relocate după death void până la repop stabil.
8. **Loot cancel**: threshold pe distanță.

### 11.2 Anti-cheat incremental

```
last_pos, last_time (client clock)
dt = clamp(client_time - last_time, 0, MAX_DT)
maxDist = GetSpeed(activeMoveType) * (dt/1000) * (1 + lag_slack)
if (dist3d > maxDist && !IsTaxiFlying && !IsBeingTeleported)
    → reverse to last good / strike counter / kick
```

Nu folosi server wall-clock ca sursă unică (lag spikes). Nu pedepsi primul pachet după
teleport/knockback/embark (grace window).

### 11.3 Force ACK framework unificat

Un `m_pendingMovementForces` cu tip (ROOT, UNROOT, WATERWALK, FEATHER, HOVER, SPEED_*) +
counter + timeout. ACK consumă; mișcare care contrazice force-ul se ignoră până ACK sau
timeout re-send.

### 11.4 Time model

- Un singur ceas pe wire pentru player-driven MSG: **client movement time**.
- Compensare one-way documentată.
- Heartbeat-urile server-generate: fie nu rescrie time-ul client pentru unități player-driven,
  fie marchează sursa.

### 11.5 Transport

- Lookup vessel pe mapa relevantă + instance.
- Embark/Disembark: API serializată pe map mgr (nu half-state pe două map threads).
- Minion/pet: deja event-driven pe lift; extinde aceleași garanții pe vessel map enter.

### 11.6 Flags 1.12 purge

- Elimină sau izolează CAN_FLY / FLYING / FLYING_OLD din path-urile player vanilla.
- `SetCanFly` GM: documentează ca non-blizzlike; nu polua flags persistente pe save.

### 11.7 Teste de regresie recomandate

| Test | Așteptat |
|------|----------|
| Walk full deck length pe ship (x negativ și pozitiv) | poziție continuă, fără freeze |
| Step off ship lângă mal | disembark pe mal, nu (0,0,0) |
| Step off mid-ocean cu world zero echo | disembark la coarse ship pose |
| Far tele de pe punte | pose dest pe harta nouă; vessel create din nou |
| Root + client force move packets | server ignoră / clamp |
| Lag spike + TIME_SKIPPED | observers netezi |
| Fall 20y pe uscat | damage ≈ formulă |
| Fall pe elevator ONTRANSPORT | 0 damage |
| Possess creature move + release | player pose intactă |
| Water walk apply | alți clienți văd pe apă |
| Speed hack ×3 | detect / reverse (după 11.2) |
| Knockback off cliff | land + fall damage corect |

---

## 12. Diagramă stare teleport

```
                    ┌──────────────────┐
                    │   In World       │
                    │  normal move     │
                    └────┬────────┬────┘
              near TP    │        │ far TP
                         ▼        ▼
              ┌──────────────┐  ┌────────────────┐
              │ SemaphoreNear│  │ SemaphoreFar   │
              │ move ignored │  │ STATUS_TRANSFER│
              └──────┬───────┘  └───────┬────────┘
                     │ ACK              │ WORLDPORT_ACK
                     ▼                  ▼
              SetPosition dest    SetMap+Place+seed mi
              zone/pet            Add map, packets
                     │                  │
                     └────────┬─────────┘
                              ▼
                         In World
```

---

## 13. Fișiere de atins la un fix planificat

Ordine sugerată pentru un PR mic, single-purpose (conform etiquette repo):

1. **P0 safety:** root gate + ONTRANSPORT guid require + drop compose check  
   → `MovementHandler.cpp` only
2. **P1 UX/network:** TIME_SKIPPED rebroadcast, WaterWalk set, charm mi  
   → `MovementHandler.cpp`, `PlayerMovement.cpp`
3. **P2 fall/transport hardening:** vessel resolve, fall spoof  
   → `MovementHandler.cpp`, `Player.cpp`
4. **P3 anti-cheat:** speed history  
   → nou helper + `MovementHandler.cpp` + config world
5. **Force ACK framework:** PR separat  
   → `MiscHandler.cpp`, `PlayerMovement.cpp`, `UnitSpeed.cpp`

---

## 14. Concluzie

Pipeline-ul de mișcare Zero este un **reflector client→server→set** cu validare minimă și
logică de transport/deck relativ matură (embark order, disembark reach, WriteMovementInfo
deck-local, far-TP seed). Punctele slabe structurale sunt:

1. **lipsa autorității pe viteză/distanță** (hack-uri triviale);
2. **force movement ACK-uri goale** (root/water/feather/time-skip);
3. **asimetrii de broadcast** (water walk, time clocks, knockback format);
4. **rămășițe multi-expansion** pe flags fly și opcode table;
5. **compunerea residuală** world+transport în verify, în conflict cu modelul deck-map.

Fix-urile de transport recente (extent, time rewrite, mi seed, embark order) adresează
simptome reale de punte/pet/teleport; nu înlocuiesc un model de securitate a mișcării.

---

*Generat din sursele tree-ului local `00-mangos_zero` (branch `updates` + diff MovementHandler).
Nu include runtime packet captures din client live.*
