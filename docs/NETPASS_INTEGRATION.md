# PetPal NetPass / StreetPass Integration

> **CURRENT STATE (important):** real StreetPass over CECD **works** — a homebrew
> title *can* create a registered CEC message box after all. Confirmed on hardware
> (2026-08): PetPal's box is created, enabled, given a title/icon + shared HMAC key,
> and listed in the global `MBoxList`; it shows in *System Settings → Data
> Management → StreetPass Management* and in cectool (`Enabled: Yes`, title id
> `0F00D500`). The last piece — verifying console-to-console *exchange* — needs a
> second console/NetPass and is **in the works**. On device, `CecdTransport` (real
> StreetPass) and `HttpPassTransport` (internet relay) run **together** via
> `DualTransport`; the relay stays the reliable path meanwhile. Real StreetPass
> requires the installed **`.cia`** (the exheader is only granted `cecd:s` when the
> title is installed to the HOME Menu). See "CECD StreetPass — how box creation was
> solved" and "Internet relay" below. Everything above the transport (packet
> format, meeting pipeline, friends/journal) is unchanged.

PetPal exchanges a small, fixed packet describing your pet with other PetPal
players. That packet is transport-agnostic — it rides the system's **CECD**
StreetPass boxes (local StreetPass, which **NetPass** also relays over the internet)
**and** PetPal's own HTTPS relay, at the same time. One wire format, one meeting
pipeline; only the transport differs.

## The packet — `PetPalPacket`

Packed (`#pragma pack(1)`), little-endian, 49 bytes (≤ 64 to fit a StreetPass
slot). Defined in `include/netpass/PetPalPacket.h`.

| Off | Type | Field | Notes |
|----:|------|-------|-------|
| 0   | u32  | magic | `0x50455450` (`kNetPassMagic`) — identifies a PetPal payload |
| 4   | u16  | version | `kNetPassVersion` (1) |
| 6   | u16  | reserved0 | sender's app-version stamp `kAppVersion & 0xFFFF` (minor<<8 \| patch); `0` on builds that predate this stamp. Read by the server pass gate; ignored by 3DS receivers |
| 8   | u64  | petId | sender's unique pet id |
| 16  | char[13] | name | null-terminated, ≤ 12 chars |
| 29  | u8   | species | `Species` |
| 30  | u8   | stage | `EvolutionStage` |
| 31  | u16  | level | 1–99 |
| 33  | u8   | primaryColor | `PetColor` |
| 34  | u8   | secondaryColor | `PetColor` |
| 35  | u8   | accentColor | `PetColor` |
| 36  | u8   | style | `Style` |
| 37  | u16  | favoriteItem | `ItemId` |
| 39  | u16  | reserved1 | flags / future |
| 41  | u16  | giftItem | `ItemId`, or `0xFFFF` (`kNoGift`) for none |
| 43  | u16  | giftQty | quantity of the gift |
| 45  | u32  | crc32 | CRC-32 over bytes `[0,45)` (i.e. all fields except this) |

This satisfies the spec's exchange set — **Pet ID, Species, Name, Level,
Evolution Stage, Colors, Style, Favorite Item** — and adds an optional **gift**
the visiting pet leaves behind.

### Build & validate
* `NetPassManager::buildPacket(pet, gift, qty)` snapshots the pet, zeroes the
  CRC field, then computes and stores the CRC.
* `NetPassManager::validate(bytes, len, out)` rejects anything that isn't
  exactly our magic + version, has an out-of-range species/stage, or whose CRC
  doesn't match. It also re-terminates `name` defensively.

## Transport abstraction

```cpp
class INetPassTransport {
    bool init();
    void shutdown();
    bool setOutbox(const uint8_t* data, size_t len);   // what others receive
    int  drainInbox(std::vector<std::vector<uint8_t>>& out); // messages waiting
    bool isAvailable() const;
};
```

Two implementations ship:

* **`LoopbackTransport`** (all platforms) — in-memory; `injectInbox()` feeds it
  test messages. Used by the host tests and as the default when no device
  transport is supplied.
* **`CecdTransport`** (`__3DS__` only, `source/netpass/CecdTransport.cpp`) —
  a from-scratch **`cecd:s` IPC client** (libctru ships no CECD bindings, so the
  raw service IPC is implemented directly). Command IDs, parameter layouts, and
  the CEC structures come from 3dbrew, the Citra/Azahar HLE, and NetPass's own
  `cecd.c`. PetPal's box is keyed by its **CEC program id**
  `kCecProgramId = 0x0F00D500` — the low 32 bits of the title id
  (`uniqueId 0xf00d5 << 8`), which is how CEC keys every box and what `cecd:u`
  authorizes a title to touch. (The old `0x000F00D5` was wrong — it's why box
  calls returned `0xC8A10BF0`.)
  * `init()` →
    1. Open **`cecd:s`** (falls back to `cecd:u`) + `GetCecdState` handshake.
    2. `registerBox()` — create the FULL box the way the system requires (see
       "how box creation was solved" below), or early-out if already registered.
    3. `ensureBoxProvisioned()` — keep it enabled, right type flag, and the shared
       HMAC key (upgrades boxes made by older builds).
    4. Rewrite the box **title + icon** each boot (heals older/garbled icons).
    5. **`Start(CEC_COMMAND_START)`** — nudge the CEC daemon into its scan cycle.
  * `setOutbox()` → `writeBoxMessage()` — a full HMAC-signed CEC message + BoxInfo
    refresh (not a raw body).
  * `drainInbox()` → `OpenAndRead(InboxInfo)` to enumerate waiting messages
    (`CecBoxInfoHeader` + `CecMessageHeader`×N, header `0x70`, id at `0x20`),
    then `ReadMessage` + `Delete` for each (bounded, clamped to 32/poll).
  * `shutdown()` → `svcCloseHandle` **only** (CEC is intentionally left running so
    the system keeps exchanging while the console sleeps).

  Every call is bounded and error-checked, so a missing service or an
  unprovisioned box degrades to "no exchange" rather than hanging.
* **`DualTransport`** (`__3DS__` only) — runs `CecdTransport` (real StreetPass) and
  `HttpPassTransport` (relay) together: outbox writes go to both and the drained
  inbox is their union (deduped in `NetPassManager::poll`). This is what `Game`
  constructs on device, so passing works whether or not a given path does.

`NetPassManager` is constructed with whichever transport is appropriate; the
rest of the game is identical. It also exposes `streetpassStatus()` which the
Friends screen renders as a live status line, with **X = check now** (an on-
demand `drainInbox`) for testing.

## Internet relay — `HttpPassTransport` (the reliable path)

PetPal also runs its own relay — today the dependable everyday path, and the
fallback while console-to-console StreetPass exchange is being finished:

* **Client** (`source/netpass/HttpPassTransport.cpp`, `__3DS__` only). A background
  worker thread (so the UI never blocks) periodically calls
  `GET teampetpal.com/api/pass?id=<petId hex>&pkt=<packet hex>`:
  * `setOutbox()` just caches our packet (+ marks it dirty); the worker uploads it.
  * the worker uploads when the packet changes / on a manual check / ~every 30 min
    to refresh the 7-day TTL, and **downloads** other players' packets every ~60 s.
  * downloaded packets are queued; `drainInbox()` hands them to `NetPassManager`,
    which runs the same magic/version/CRC/self-id validation as always.
  * reuses the `soc`/curl stack already set up in `Game::init` for redeem.
* **Server** (`functions/api/pass.js`, Cloudflare Pages Function + KV `PETPAL_KV`).
  Stores `pass:<id>` = packet hex (7-day TTL) and returns up to 12 random *other*
  players' packets, one hex line each. Dumb by design — no secrets, the 3DS
  validates. Needs the **`PETPAL_KV`** binding (same namespace as redeem); without
  it the endpoint returns empty (no passes) rather than erroring.

Write-budget note (Cloudflare free KV = 1k writes/day): the client uploads only
on change / hourly refresh, so each player is a handful of writes/day; downloads
are reads (a `list` + a few `get`s per exchange).

Friends screen: **X** drains whatever the worker has fetched (instant); **Y** runs
one synchronous exchange and reports the result (`Pass OK: got N…`).

> **Verified end-to-end on real hardware:** a seeded pet is uploaded, downloaded,
> validated, and becomes a friend (with journal + gift + celebration).

### Seeding the pool / generating packets — `tools/make_pass.py`
To inject a pet (an official/event pet, or a test friend) run e.g.
`python tools/make_pass.py --id 0xB0B0CAFE12345678 --name Pixel --species 0 --level 7`
and open the printed URL.

> **CRC gotcha (cost us a long debug):** PetPal's `crc32` (`source/util/Crc32.cpp`)
> is **NOT zlib-compatible**, even though the code looks like standard CRC‑32. The
> console's own packets are self-consistent, so console-to-console works — but any
> **externally generated** packet must use the app's exact algorithm or it fails
> validation. `tools/make_pass.py` already does this (verified against hardware:
> the app computes `D2904EE3` where zlib gives `ABCDF720` for the same bytes). Do
> not use `zlib.crc32` to build packets.

## CECD StreetPass — how box creation was solved

Box creation *is* possible; it just needs the exact recipe the system uses (from
NetPass's `cecd.c`). Two things had blocked it earlier:

1. **Wrong service + id.** PetPal opened `cecd:u` (which only lets a title touch
   its *own* box) with the wrong program id `0x000F00D5`, so every call returned
   `0xC8A10BF0` (CEC / InvalidState). Fix: open **`cecd:s`** (the system CEC
   service, granted to an installed CIA) and use the real CEC id **`0x0F00D500`**
   (title-id low 32 bits). With the correct id even `cecd:u` accepts the box.
2. **Wrong create call.** CEC box files must be **created** with
   `OpenRawFile`(0x01, specific open flags) **+** a separate `WriteRawFile`(0x05) —
   *not* `OpenAndWrite`(0x11). `registerBox()` mirrors NetPass exactly: make the
   three directories (open flag `8`), then `MBoxInfo` (magic 0x6363, flag `6`),
   `InboxInfo` (0x6262, flag `0x14`), `OutboxInfo` (0x6262, flag `6`) +
   `OutBoxIndex` (0x6767), a UTF-16 title + a 48×48 tiled RGB565 icon, and finally
   an entry in the global `/CEC/MBoxList____`. `OpenAndWrite` is correct only to
   *update* files that already exist (message I/O, the enable flag, the mbox list).

Result (confirmed on hardware): the box shows in *System Settings → Data
Management → StreetPass Management* and in cectool, `Enabled: Yes`, title id
`0F00D500`. Requires the installed **`.cia`** — a `.3dsx` under the Homebrew
Launcher inherits HBL's services (no `cecd:s`) and HBL's title id, so it can't
register the box. Console-to-console *exchange* verification (a second unit or a
NetPass peer) is the remaining "in the works" step.

## Meeting pipeline

`Game::run()` polls every ~10 s (and on resume). For each validated, non-self
packet, `Game::processNetPass()`:

1. `FriendList.ingest()` — create a new `Friend` or update an existing one
   (dedupe by `petId`, refresh appearance snapshot, bump meeting count). When
   the roster is full (512) the lowest-friendship stranger is evicted.
2. `Pet.recordEncounter(unique)` — award XP (25 new / 10 repeat) and friendship,
   advance `streetpassEncounters` / `uniquePetsMet`, flag evolution eligibility.
3. `Journal.logMetFriend()` — only for first meetings, in the pet's voice.
4. Apply any `giftItem` → `Inventory` + a "found item" journal line.
5. `maybeUnlockLocation()` — 20% chance to open the next locked location.
6. Fire celebration hooks (new friend / level up).

Afterwards: `runAchievementChecks()` (cosmetic unlocks), `publishSelf()`
(refresh the outbox since stats changed), and `requestSave()`.

Friendship levels are derived from accumulated points
(`kFriendshipThresholds`): Stranger → Acquaintance → Friend → Best Friend →
Legendary Friend.

## Provisioning the CEC box (device)

`init()` registers the box automatically each launch via `registerBox()` (see
"how box creation was solved" for the exact file recipe). Key details:

* **Service + id:** `cecd:s`, program id `kCecProgramId = 0x0F00D500` (title-id
  low 32 bits). Keep this id **constant across versions** — changing it orphans the
  box and every friend mapping. `petpal.rsf`'s service list includes `cecd:s`.
* **Shared HMAC key:** all installs bake in the same 32-byte `kHmacKey`, so a passed
  message validates against the receiver's box (a per-title/derived key would leave
  consoles unable to read each other's pets). Not a server secret — it's a
  StreetPass integrity key, on every device by design.
* **Box sizing:** tuned to PetPal's ≤64-byte packet (`kCecMaxMessageSize = 512`,
  inbox 25 msgs, outbox 1) rather than NetPass's relay-everything defaults.
* **Title + icon:** paths `110`/`101`. The icon is 48×48 RGB565 in the **3DS tiled
  layout** (8×8 tiles, Morton order — verified against cectool's `tileToBuf`); a
  linear buffer renders as garbage. `writeBoxNameAndIcon()` rewrites both each boot
  so older boxes self-heal.
* **Must run the `.cia`.** `cecd:s` and the correct title id are only granted to the
  installed title; a `.3dsx` under HBL can't register the box.

## Hardware gotchas found while bringing this up

Two real-hardware-only bugs the emulators don't surface (both fixed):

1. **`0xD9001830`** (kernel / OS / WrongArgument) on `WriteMessage`. Cause: the
   message-id buffer was a `const` global in read-only `.rodata`, but 3dbrew
   specifies `WriteMessage`'s msgId buffer is mapped **read/write**. The kernel
   won't create a writable IPC mapping of read-only memory, so it rejected the
   whole request. Fix: copy the msgId into a writable stack buffer and use the
   `IPC_BUFFER_RW` descriptor. (Also moved the box-title buffer out of `.rodata`.)

2. **`0xC8A10BF0`** (CEC / InvalidState) on writing the outbox. Real cause (found
   by reading the proven NetPass source, `gitlab.com/3ds-netpass/netpass`
   `source/cecd.c`): PetPal was writing a **raw body** via plain `WriteMessage`,
   but the system expects a **full, HMAC-signed CEC message**. The correct write
   (see `writeBoxMessage`) is:
   - read `MBoxInfo` to get the box's `hmac_key`;
   - build `[CecMessageHeader (magic 0x6060) | body]`;
   - `WriteMessageWithHMAC` (0x07) passing that buffer + the hmac key (the system
     computes the signature);
   - refresh the box's `BoxInfo` (append the header, bump `message_num` /
     `box_info_size` / `box_size`) so the message is listed for exchange.
   Two ABI bugs were fixed alongside this: `OpenAndRead`/`OpenAndWrite` must put
   **`0`** in `cmd[4]` (a nonzero flags value is rejected — this had been breaking
   even the `MBoxInfo` read), and `ReadMessage` returns `[header | body]`, so
   `drainInbox` now skips the `0x70` header before validating the packet.

> **Remaining unknown (exchange):** box *creation* is solved and confirmed on
> hardware (the box registers, enables, and appears in System Settings + cectool).
> What still needs a second console — or a NetPass peer — to verify is the actual
> *exchange*: that the system/NetPass swaps PetPal's self-made box the same way it
> does retail boxes. That's the "in the works" piece; the internet relay covers
> passing until it's confirmed.

## On-device diagnostics

From the Friends screen:

* **X — check for passes now.** Drains the inbox on demand and reports how many
  passes/new friends arrived. A normal user-facing control (also handy for
  on-device testing); the bottom-edge status line shows live StreetPass state.
* **SELECT — inject a test pass** *(debug builds only,* `make EXTRA_DEFS=-DPETPAL_DEBUG=1`*)*.
  `Game::injectTestPass()` runs a synthetic visitor through the **real** meeting
  pipeline (friend + journal + celebration), bypassing CECD, so you can confirm
  the receive→friend flow renders correctly on the console.

> The earlier **Y — CECD self-test** diagnostic (a raw `svc=… step=… reg=…`
> registration dump) has been removed now that box creation is confirmed working
> on hardware; box registration results are still tracked internally in
> `CecdTransport` for debugging via a debugger/log.

> libctru's CECD coverage is nonexistent, so `CecdTransport` centralizes all
> CECD IPC in one place; box management can be adjusted here without touching
> gameplay code.

## Versioning

If the packet must change, bump `kNetPassVersion` and teach `validate()` to
accept and up-convert the older layout. As with saves, enum values are
append-only so a v1 and v2 client still agree on what `Species::Fox` means.
