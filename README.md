# 🐾 PetPal — a StreetPass Virtual Pet for the Nintendo 3DS

PetPal is a homebrew virtual-pet game built around **StreetPass / NetPass**. You
raise one pet; every player you pass (locally via StreetPass or worldwide via
[NetPass](https://github.com/Sorunome/netpass) ) becomes a friend, feeds your
pet experience and gifts, unlocks new places, and pushes it toward evolution.

It's designed to feel like an official StreetPass-era title: bright, rounded,
bouncy UI across both screens, a charming auto-written journal, and a pet that
grows the more of the world it meets.

> Built with **C++17 · devkitPro · libctru · citro2d (+ citro3d)**.
> Targets Old 3DS, New 3DS, and New 3DS XL.

---

## Quick start

### Build the 3DS app
```sh
# Requires devkitPro with the 3ds-dev group installed:
#   pacman -S 3ds-dev          (libctru, citro2d, citro3d, tools)
make            # produces PetPal.3dsx (+ .smdh)
```
Copy `PetPal.3dsx` to `sdmc:/3ds/` and launch from the Homebrew Launcher, or run
over the network with `3dslink PetPal.3dsx`.

Save data lives at `sdmc:/3ds/PetPal/` (`save.bin`, a rotating `save.bak`, and
portable exports under `backups/`). The directory is created automatically.

### Run the model tests on your PC
The gameplay/save/netpass logic is decoupled from the device, so it runs on a
desktop compiler:
```sh
make -C tests run
```
See [tests/README.md](tests/README.md).

---

## What's implemented

* **7 species** (Fox, Cat, Bunny, Dragon, Slime, Robot, Axolotl) with distinct
  personalities, default palettes, and procedural rendering.
* **Customization**: 3 color slots × 10 colors, 10 unlockable accessories, 10
  unlockable styles.
* **5-stage evolution** driven by encounter milestones (50 / 200 / 500 / 1000).
* **Core stats**: level, XP, happiness, energy, friendship, adventure rank.
* **Daily activities**: feed, play, pet, talk — plus dress-up & journal.
* **Adventure system**: real-time short/medium/long trips with per-location loot
  tables and a generated story line.
* **StreetPass / NetPass**: CRC-checked packet exchange, friend dedupe,
  gifts, encounter rewards, friendship levels.
* **Friendship, Journal, Items, Achievements, Currency** systems.
* **Save system**: versioned, CRC-validated, atomic writes with backup rotation
  and portable export.
* **Dual-screen UI**: animated hub + 8 screens, easing/tween engine, touch +
  D-pad navigation, 60 FPS target.

The art and audio are placeholders by design — the game renders pets
procedurally so it is fully playable before any sprite or sound is authored. See
[assets/README.md](assets/README.md) for how to drop real assets in.

---

## Project layout
```
PetPal/
├── Makefile                 devkitPro 3DS build
├── README.md
├── docs/                    architecture & format specs (start here)
│   ├── ARCHITECTURE.md
│   ├── SAVE_FORMAT.md
│   ├── NETPASS_INTEGRATION.md
│   ├── UI_SYSTEM.md
│   └── ROADMAP.md
├── include/                 public headers (mirrors source/)
│   ├── core/  pets/  friends/  journal/  adventure/
│   ├── items/ achievements/  netpass/  save/  util/
│   └── ui/    ui/screens/
├── source/                  implementation (mirrors include/)
├── gfx/                     citro2d .t3s atlas specs (-> romfs:/gfx/*.t3x)
├── romfs/                   files mounted at romfs:/ on device
├── assets/                  source art & audio (sprites/ui/fonts/sfx/music)
└── tests/                   host-side model tests (no 3DS needed)
```

## Documentation map
| Doc | What it covers |
|-----|----------------|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Class responsibilities, ownership, data flow, the frame loop |
| [SAVE_FORMAT.md](docs/SAVE_FORMAT.md) | On-disk byte layout, versioning, backup strategy |
| [NETPASS_INTEGRATION.md](docs/NETPASS_INTEGRATION.md) | Packet format, CECD/NetPass transport, meeting pipeline |
| [UI_SYSTEM.md](docs/UI_SYSTEM.md) | Screen stack, widgets, animation engine, rendering |
| [ROADMAP.md](docs/ROADMAP.md) | Implementation phases and future features |

## License & credits
Homebrew, non-commercial. Not affiliated with Nintendo. "Nintendo 3DS" and
"StreetPass" are trademarks of Nintendo. NetPass is a community project.
