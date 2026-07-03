# PetPal Implementation Roadmap

Status legend: ✅ done in this starter · 🔨 partial / stubbed · ⬜ planned.

## Phase 0 — Foundation ✅
* ✅ Project structure, devkitPro Makefile, romfs/gfx pipeline.
* ✅ Core type system & tuning tables (`core/Types.h`) — append-only enums shared
  by save + wire formats.
* ✅ Utility layer: `Random`, `Time`, `Crc32`, `ByteBuffer`, `Log`.
* ✅ Host test harness for the model layer.

## Phase 1 — Gameplay model ✅
* ✅ `Pet`: identity, appearance, stats, XP/levels, evolution, daily activities,
  decay.
* ✅ `Inventory` + item database (`Names`), coins.
* ✅ `Friend` / `FriendList`: dedupe, friendship levels, roster cap.
* ✅ `Journal`: auto-written, capped, typed helpers.
* ✅ `Adventure`: real-time trips, per-location loot tables, story generation.
* ✅ `Achievements`: condition evaluation + cosmetic rewards.

## Phase 2 — Persistence ✅
* ✅ `SaveManager`: versioned, CRC'd serialization; atomic write + backup
  rotation; portable export/import.
* ✅ Time-passed decay applied on load.
* ✅ Round-trip + corruption tests.

## Phase 3 — StreetPass / NetPass ✅ / 🔨
* ✅ `PetPalPacket` wire format + CRC validation.
* ✅ `NetPassManager` build/validate/poll + meeting pipeline in `Game`.
* ✅ `LoopbackTransport` for offline testing.
* ✅ `CecdTransport` (device): real `cecd:u` IPC client (Open/ReadMessage/
  WriteMessage/Delete/OpenAndRead/GetCecdState), box provisioning, inbox
  enumeration. Compiles ABI-correct.
* 🔨 On-device CEC box *exchange* verification (needs a second console or
  NetPass); box provisioning may need iteration on real hardware.

## Phase 4 — UI ✅ / 🔨
* ✅ `UIManager` frame loop, screen stack, navigation, background.
* ✅ `AnimationManager` (easing + tweens + global clock).
* ✅ `Widgets` (rounded button w/ bounce, cards, bars, text).
* ✅ All 9 screens functional with touch + D-pad.
* ✅ Procedural `PetRenderer` (animated) as art placeholder.
* 🔨 Slide transitions are hooked (tween) but screens don't yet offset by it.
* ⬜ Particle bursts for celebrations.

## Phase 5 — Audio ⬜
* ⬜ `ndsp` init + a small `AudioManager` (BGM + SFX channels).
* ⬜ Wire SFX/jingle into `UIManager::celebrate()` and button taps; honor the
  Settings volume sliders.
* ⬜ Evolution sequence music.

## Phase 6 — Art & polish ⬜
* ⬜ Author species sprites per stage + animation frames; build the citro2d
  atlas and swap `PetRenderer` to sprites.
* ⬜ Menu icons, backgrounds, environment art per location.
* ⬜ Homebrew icon/banner (`icon.png`, `banner.png`) + SMDH metadata.

## Phase 7 — Content depth ⬜
* ⬜ Shop screen (spend coins on food/accessories/decorations).
* ⬜ Food picker UI for feeding (today it auto-picks the favorite/first food).
* ⬜ Per-location adventure flavor text & rarer drop tables.
* ⬜ More journal templates per event.

---

## Future features (architecture is ready for these)

These map to the spec's "Future Features"; the data model was shaped to absorb
them with minimal churn:

* **More species** — add a `Species` value (append) + rows in `Names` + sprites.
  Save/packet already store species as an int.
* **More locations** — add a `Location` value (append) + a loot bias in
  `Adventure`. `World` mask is 16-bit (room to grow; widen if > 16).
* **Seasonal events** — gate content on `Time` (date checks); event flags fit in
  reserved fields or a new save section (bump `kSaveVersion`).
* **Minigames** — new `Screen` subclasses; rewards flow through `Game` verbs.
* **Trading** — extend `PetPalPacket` (bump `kNetPassVersion`) with an offer
  block; the transport + validation path is unchanged.
* **Community events** — aggregate via NetPass packets; tally in a new save
  section.
* **Pet housing / decorations** — items already have a `Collectible` category;
  add a `Decoration` category + a room `Screen`.
* **Multiplayer activities** — local wireless (`udds`/`uds`) as a third
  transport behind `INetPassTransport`-style abstraction.

## Known gaps / TODO markers in code
* `CecdTransport` CEC box exchange is unverified on hardware (see Phase 3).
* `celebrate()` does not yet spawn particles or play sound.
* Slide transition value is computed but not applied to screen draw offsets.
* Several accessories/styles render as no-ops in `PetRenderer` until art lands.
* Feeding auto-selects food; a picker is planned.
