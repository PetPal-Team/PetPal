# PetPal Architecture

This document describes how PetPal is structured, who owns what, and how data
flows through a frame and through a StreetPass meeting.

## Design goals

1. **Separate model from device.** All gameplay rules (pet stats, evolution,
   loot, save format, packet validation) live in classes that do *not* include
   `<3ds.h>` or citro2d, so they compile and unit-test on a PC. Only the UI and
   the CECD transport are device-bound.
2. **One mutation path.** Screens never poke the model directly. They call
   high-level verbs on `Game` ("feed", "send on adventure"), and `Game` applies
   the side effects consistently (journal, achievements, autosave, celebration).
3. **Append-only enums.** Every enum's integer values are baked into the save
   and the wire format. New values append at the end; nothing is reordered.
4. **Built for expansion.** New species/locations/items are table entries; new
   screens are a subclass + one array slot.

## Layered view

```
                +-------------------------------------------------+
   device  -->  |  UI layer (citro2d)                             |
                |  UIManager · Screen[9] · Widgets · PetRenderer  |
                |  AnimationManager                               |
                +-------------------------+-----------------------+
                                          | high-level verbs
                +-------------------------v-----------------------+
   core    -->  |  Game  (orchestrator, main loop, side effects)  |
                +----+-------------+-------------+----------------+
                     |             |             |
        +------------v--+   +------v------+  +---v-------------------+
 model  | PersistentState|  | SaveManager |  | NetPassManager        |
 (POD-  | Pet Inventory  |  | (serialize, |  | (build/validate pkt,  |
  ish)  | FriendList     |  |  CRC, files |  |  poll inbox)          |
        | Journal        |  |  + backup)  |  |   |                   |
        | Adventure      |  +-------------+  |   v                   |
        | Achievements   |                   | INetPassTransport     |
        | World Settings |                   | Loopback | Cecd(3DS)  |
        | SaveMeta       |                   +-----------------------+
        +----------------+
                     ^
        util: Random · Time · Crc32 · ByteBuffer · Log
```

## Ownership

* **`Game`** owns everything: a single `PersistentState`, a `SaveManager`, a
  `NetPassManager`, a `UIManager`, and an RNG. Lifetime = the app.
* **`PersistentState`** is the entire saveable game in one struct. Every
  subsystem object is a member by value, so "what gets saved" is one type.
* **`UIManager`** owns the citro2d render targets, the system font, a per-frame
  text buffer, the optional sprite sheet, the `AnimationManager`, and all nine
  `Screen` instances (built once, kept alive, switched via a nav stack).
* **`SaveManager`/`NetPassManager`** are `friend`s of the model classes so they
  can serialize private fields compactly without widening the public API.

## The frame loop

`Game::run()` (device) drives time with `svcGetSystemTick()`:

```
loop while aptMainLoop() && !quit:
    dt = clamp(tick delta, 0.1s)
    every ~10s: NetPassManager.poll() -> Game.processNetPass()
    accumulate playtime
    UIManager.tick(dt):
        scanInput()                       # HID -> Input struct
        if START on hub/onboarding: quit
        currentScreen.update(dt, input)   # gameplay verbs via Game
        AnimationManager.update(dt)        # advance tweens + global clock
        C3D_FrameBegin
          draw top  (background + currentScreen.drawTop)
          draw bottom (currentScreen.drawBottom)
        C3D_FrameEnd
        clear per-frame text buffer
    autosaveTick(dt)                       # every 30s if enabled
on exit: final save
```

Target is 60 FPS; `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` paces to VSync.

## The meeting pipeline (StreetPass / NetPass)

```
CECD/NetPass inbox
   -> INetPassTransport.drainInbox()           # raw bytes per message
   -> NetPassManager.validate()                # magic, version, size, CRC
   -> filter out our own echo (petId == ours)
   -> Game.processNetPass(): for each ReceivedPet
        FriendList.ingest()        # new friend or update + dedupe
        Pet.recordEncounter()      # XP + friendship, evolution eligibility
        Journal.logMetFriend()     # first meetings only
        apply visitor's gift -> Inventory + Journal
        maybeUnlockLocation()      # 20% chance to grow the world
        celebrate() hooks
   -> runAchievementChecks()       # unlock cosmetics + journal + celebrate
   -> publishSelf()                # refresh our outbox (stats changed)
   -> requestSave()
```

Outbound, `NetPassManager::buildPacket()` snapshots the pet into a packed
`PetPalPacket`, stamps a CRC, and the transport publishes it to the outbox.

## Module responsibilities

| Module | Key types | Responsibility |
|--------|-----------|----------------|
| `core` | `Types.h`, `Names`, `Game` | Enums/tuning tables; display strings; orchestration & main loop |
| `pets` | `Pet` | Identity, appearance, stats, XP/level, evolution, daily-activity effects |
| `items` | `Inventory` | Item counts + coins, category queries |
| `friends` | `Friend`, `FriendList` | Met-pet records, dedupe, friendship levels |
| `journal` | `Journal` | Auto-written, capped, newest-first diary |
| `adventure` | `Adventure` | Real-time trips, per-location loot tables, story text |
| `achievements` | `Achievements` | Unlock bitmask, condition evaluation, cosmetic rewards |
| `netpass` | `NetPassManager`, `PetPalPacket`, transports | Wire format, validation, CECD bridge |
| `save` | `SaveManager`, `SaveData` | Versioned/CRC'd serialization, atomic file I/O, backups |
| `ui` | `UIManager`, `Screen`+9, `Widgets`, `AnimationManager`, `PetRenderer` | Rendering, input, navigation, animation |
| `util` | `Random`, `Time`, `Crc32`, `ByteBuffer`, `Log` | Dependency-light helpers |

## Why these choices

* **Manual binary serialization** (not raw struct dumps) because the model uses
  `std::string`/`std::vector`; `ByteWriter`/`ByteReader` keep it compact and
  endian-explicit, and tolerate table growth (e.g. more item types) on load.
* **Transport interface** so the same meeting logic runs against a real CECD box
  on device and an in-memory loopback in tests.
* **Procedural `PetRenderer`** so the project is runnable and the animation
  system is demonstrable before any art exists; swapping in `C2D_Sprite` art is
  isolated to that one file.
