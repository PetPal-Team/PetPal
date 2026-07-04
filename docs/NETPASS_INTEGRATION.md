# PetPal NetPass / StreetPass Integration

> **CURRENT STATE (important):** real StreetPass over CECD turned out to be a
> **dead end for homebrew** — a homebrew title cannot create a registered CEC
> message box (confirmed on hardware, and by the NetPass author). PetPal now
> exchanges pets over its **own internet relay** (`HttpPassTransport` →
> `teampetpal.com/api/pass`), which needs no CECD and no NetPass. The CECD code
> (`CecdTransport`) is kept for reference but is **not used**. See
> "Why CECD was abandoned" and "Internet relay" below. Everything above the
> transport (packet format, meeting pipeline, friends/journal) is unchanged.

PetPal exchanges a small, fixed packet describing your pet with other PetPal
players. That packet is transport-agnostic — historically it was meant to ride
the system's **CECD** StreetPass boxes (local) / **NetPass** relay (internet),
but since homebrew can't register a CEC box, it now rides PetPal's own HTTPS
relay. One wire format, one meeting pipeline; only the transport differs.

## The packet — `PetPalPacket`

Packed (`#pragma pack(1)`), little-endian, 49 bytes (≤ 64 to fit a StreetPass
slot). Defined in `include/netpass/PetPalPacket.h`.

| Off | Type | Field | Notes |
|----:|------|-------|-------|
| 0   | u32  | magic | `0x50455450` (`kNetPassMagic`) — identifies a PetPal payload |
| 4   | u16  | version | `kNetPassVersion` (1) |
| 6   | u16  | reserved0 | flags / future |
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
  a from-scratch **`cecd:u` IPC client** (libctru ships no CECD bindings, so the
  raw service IPC is implemented directly). Command IDs, parameter layouts, and
  the CEC structures come from 3dbrew + the Citra/Azahar HLE. PetPal's box is
  keyed by a fixed CEC program id (`kCecProgramId = 0x000F00D5`, derived from the
  RSF UniqueId) so all installs match.
  * `init()` →
    1. `srvGetServiceHandle("cecd:u")` + `GetCecdState` handshake.
    2. `Open(MBoxInfo, Read)`, falling back to `Open(MBoxInfo, Create|Read|Write)`
       so cecd provisions the box (dir tree + `MBoxInfo` with a `private_id` and
       `hmac_key`) if it doesn't exist yet.
    3. **Enable the box for StreetPass** — read the `MBoxInfo` blob, set its
       `enabled` byte (offset `0x0D`), and write it back with `OpenAndWrite`
       (read-modify-write preserves the `private_id`/`hmac_key`).
    4. **`Start(CEC_COMMAND_START)`** — nudge the CEC daemon into its scan/
       exchange cycle so the system swaps our box.
  * `setOutbox()` → `WriteMessage(OutboxMsg)` with a fixed message id.
  * `drainInbox()` → `OpenAndRead(InboxInfo)` to enumerate waiting messages
    (`CecBoxInfoHeader` + `CecMessageHeader`×N, header `0x70`, id at `0x20`),
    then `ReadMessage` + `Delete` for each (bounded, clamped to 32/poll).
  * `status()` → cached `StreetPassStatus` (service up, box ready, scanning,
    inbox waiting, raw CEC state, last error) for the UI. Never issues IPC.
  * `shutdown()` → `svcCloseHandle` **only** (CEC is intentionally left running so
    the system keeps exchanging while the console sleeps).

  Every call is bounded and error-checked, so a missing service or an
  unprovisioned box degrades to "no exchange" rather than hanging.

`NetPassManager` is constructed with whichever transport is appropriate; the
rest of the game is identical. It also exposes `streetpassStatus()` which the
Friends screen renders as a live status line, with **X = check now** (an on-
demand `drainInbox`) for testing.

## Internet relay — `HttpPassTransport` (the active path)

Since homebrew can't register a CEC box, PetPal runs its own relay:

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

## Why CECD was abandoned (reference)

The sections below document the CECD/StreetPass attempt. It is **not used** — kept
because the IPC layer is correct and reusable if box creation is ever solved.

On hardware, `OpenAndRead(MBOX_INFO)` for our own title returned `0xC8A10BF0`
(CEC / InvalidState) for **both** id forms — i.e. our box is not a valid,
registered CEC box. `Open(MBoxInfo, Create)` reports success but does not produce
a registered box (it never appears in *System Settings → StreetPass Management*).
Confirmed unsolved by the NetPass author, and the dedicated `libctru-cecd`
library has no box-creation function either. Conclusion: **a homebrew title cannot
create an exchange-eligible CEC StreetPass box** with any known method.

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

To receive StreetPass/NetPass traffic, PetPal must own a **CEC message box**
keyed by a stable, unique program id (`kCecProgramId = 0x000F00D5` in
`CecdTransport.cpp`, derived from the RSF UniqueId `0xf00d5`). `init()` now does
this automatically each launch:

1. Open the box, creating it if missing (cecd fills in `private_id`/`hmac_key`).
2. Set the `MBoxInfo.enabled` flag so the system will scan/relay it.
3. `Start(CEC_COMMAND_START)` to run the exchange cycle.

`init()` also writes a **box title + icon** (paths `110`/`101`) best-effort, since
retail StreetPass boxes carry these and the system/relay may require them before
scanning a homebrew box. The exact on-disk formats are the least-documented part
of CECD, so these are educated guesses (title = UTF-16LE; icon = 48×48 RGB565).
Their write `Result`s are surfaced by `selfTest()` (`title XXXX icon XXXX`) so we
can iterate from real-hardware feedback.

Notes / things that may still need on-device iteration:

* Keep the CEC id **constant across versions** — changing it orphans the box and
  every existing friend mapping.
* NetPass forwards boxes generically, so no coordination is required beyond
  having the box exist and be enabled (steps above).
* If exchange still doesn't happen, the title/icon **format** (size/layout) is the
  prime suspect — adjust it using the `Result` codes from the on-device self-test.

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

> **Open caveat (box creation):** the proven reference (NetPass) never *creates*
> boxes — it only works with boxes the system/retail titles already made. Creating
> a brand-new, exchange-eligible CEC box for a homebrew title is not something any
> known code does, so whether the system/NetPass will actually relay PetPal's
> self-made box remains the real unknown, even once the local write/read round-trip
> (the **Y** self-test) passes.

## On-device diagnostics

Because real exchange needs a *second* PetPal box (another console or a NetPass
peer), two hooks let you verify everything else on one unit, from the Friends
screen:

* **Y — CECD self-test.** `CecdTransport::selfTest()` writes a known 64-byte
  pattern to our **own outbox**, reads it back, compares, and restores the real
  outbox. Proves `WriteMessage`/`ReadMessage` + the CEC I/O path work on hardware
  (isolating that from "does the system relay"). Shows e.g.
  `CECD I/O OK | title 00000000 icon 00000000`.
* **SELECT — inject a test pass** *(debug builds only,* `make EXTRA_DEFS=-DPETPAL_DEBUG=1`*)*.
  `Game::injectTestPass()` runs a synthetic visitor through the **real** meeting
  pipeline (friend + journal + celebration), bypassing CECD, so you can confirm
  the receive→friend flow renders correctly on the console.

> libctru's CECD coverage is nonexistent, so `CecdTransport` centralizes all
> CECD IPC in one place; box management can be adjusted here without touching
> gameplay code.

## Versioning

If the packet must change, bump `kNetPassVersion` and teach `validate()` to
accept and up-convert the older layout. As with saves, enum values are
append-only so a v1 and v2 client still agree on what `Species::Fox` means.
