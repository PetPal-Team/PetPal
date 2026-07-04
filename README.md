# Join the Discord: https://discord.gg/6hwsE4WNDj

# 🐾 PetPal — a StreetPass-style Virtual Pet for the Nintendo 3DS

PetPal is a homebrew virtual-pet game in the spirit of the StreetPass era. You
raise one pet — feed it, play with it, send it on adventures, dress it up — and
every other player you "pass" becomes a friend who brings your pet XP, gifts, and
progress toward evolution. Passing happens **over the internet** through PetPal's
own relay, so it works from anywhere without needing another console nearby.

Bright, bouncy dual-screen UI, procedurally-drawn pets, a music track and sound
effects, an auto-written journal, a coin shop, and a redeemable code system.

> **Built with** C++17 · devkitPro · libctru · citro2d/citro3d · ndsp · 3ds-curl.
> Runs on Old 3DS, New 3DS/2DS (any modded console).

---

## 📥 Install & play

Requires a **modded 3DS** (Luma3DS / custom firmware).

**CIA (recommended)** — grab `PetPal.cia` from the
[Releases](../../releases) page and install it with a CIA manager (e.g. FBI). It
appears on your HOME Menu with its own icon and banner.

**3DSX** — copy `PetPal.3dsx` to `sdmc:/3ds/` and launch it from the Homebrew
Launcher, or push it over the network with `3dslink PetPal.3dsx`.

Save data lives at `sdmc:/3ds/PetPal/` (`save.bin` + a rotating `save.bak`, plus
portable exports under `backups/`). It's created automatically on first run.

### How to play
- **Onboarding** picks and names your pet. From then on the **hub** (bottom
  screen) is a grid of tiles: Pet, Friends, Adventure, Journal, Style, **Shop**,
  Awards, Settings. D-pad + A to navigate, B to go back, touch works too.
- **Pet** screen: Feed / Play / Pet / Talk raise stats; **Evolve** when your pet
  has met enough friends. Buy food and evolution shards in the **Shop** with coins
  earned from adventures and rewards.
- **Friends**: everyone you've passed. Press **X** to check for new passes now.
- **Codes**: Settings → **Enter code** redeems codes from
  [teampetpal.com/codes](https://teampetpal.com/codes) for one-off rewards.

---

## 🌐 Passing over the internet (PetPal's own "StreetPass")

Real 3DS StreetPass runs on the system's CECD service, which a homebrew title
can't register a message box with (a confirmed dead end — see
[docs/NETPASS_INTEGRATION.md](docs/NETPASS_INTEGRATION.md)). Instead, PetPal runs
its **own relay**: a background thread uploads a tiny packet describing your pet
to `teampetpal.com/api/pass` and downloads other players', and the same meeting
pipeline (friends, journal, gifts, celebrations) turns them into friends. No CECD,
no NetPass app, no second console required — just an internet connection.

The packet is a fixed 49-byte `PetPalPacket` (magic + version + pet snapshot +
CRC). The server is a tiny Cloudflare Pages Function backed by Workers KV; it just
stores and hands back recent packets — the 3DS validates everything, and no
secrets live on the device.

> **Note for self-hosters:** the online features (passes + codes) point at
> `teampetpal.com`. To run your own, change the URLs in
> `source/netpass/HttpPassTransport.cpp` and `source/netpass/RedeemManager.cpp`
> and deploy the functions in the companion site.

---

## ✨ Features

- **7 procedural species** (Fox, Cat, Bunny, Dragon, Slime, Robot, Axolotl), each
  drawn from shapes with **two customizable colors** and Idle / Happy / Sad
  animations — no sprite art needed.
- **5-stage evolution** driven by how many friends you've met.
- **Core loop**: feed, play, pet, talk, dress-up, adventures with per-location
  loot and a generated story line.
- **Coin shop** to buy foods, rare treats, and evolution shards.
- **Internet passes** — meet other players worldwide via PetPal's relay.
- **Redeemable codes** with server-side rewards (items / accessories / a 24-hour
  BonziBuddy transform).
- **Audio** — streamed looping BGM plus WAV and synthesized chip-blip SFX, with
  live music/SFX volume sliders.
- **Polish** — slide transitions between screens, celebration confetti, an
  auto-written journal, achievements, and a versioned, CRC-checked, atomic save.

---

## 🔨 Build from source

Requires **devkitPro** with the 3DS toolchain plus the curl portlib:
```sh
# libctru, citro2d/citro3d, tools:
dkp-pacman -S 3ds-dev
# HTTPS client used for passes + code redemption:
dkp-pacman -S 3ds-curl        # pulls 3ds-mbedtls
```

```sh
make            # -> PetPal.3dsx (+ .smdh). Bundles romfs (gfx atlas + audio).
make -C tests run   # host-side model tests (no 3DS needed)
```

### Build the CIA
`make cia` also needs two community tools that aren't part of devkitPro —
[**makerom**](https://github.com/3DSGuy/Project_CTR/releases) and
[**bannertool**](https://github.com/Steveice10/bannertool/releases). Drop the
executables under `tools/cia/` (or point the Makefile at them:
`make cia MAKEROM=/path/to/makerom BANNERTOOL=/path/to/bannertool`). The banner is
built from `banner.png` + `audio.wav` in the repo root.
```sh
make cia        # -> PetPal.cia
```

The music/SFX WAVs live in `assets/` and are copied into `romfs/audio/` at build
time (the ~30 MB BGM is why the artifacts are large). Swap in your own audio by
replacing the files in `assets/sfx/` and `assets/music/`.

---

## 🗂️ Project layout
```
PetPal/
├── Makefile                 devkitPro 3DS build (make, make cia)
├── README.md
├── petpal.rsf               CIA exheader/banner config
├── banner.png / audio.wav   CIA banner art + jingle
├── docs/                    architecture & format specs
├── include/  source/        headers + implementation (mirrored trees):
│     core/ pets/ items/ friends/ journal/ adventure/ achievements/
│     netpass/ save/ audio/ util/ ui/ ui/screens/
├── gfx/                     citro2d .t3s atlas specs (-> romfs:/gfx/*.t3x)
├── assets/                  source art & audio (sfx/, music/, ui/)
├── romfs/                   mounted at romfs:/ (gfx + audio are generated)
├── tools/make_pass.py       generate a valid pass packet / seed URL
└── tests/                   host-side model tests (no 3DS needed)
```

## 📚 Documentation
| Doc | What it covers |
|-----|----------------|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Class responsibilities, ownership, data flow, the frame loop |
| [SAVE_FORMAT.md](docs/SAVE_FORMAT.md) | On-disk byte layout, versioning, backup strategy |
| [NETPASS_INTEGRATION.md](docs/NETPASS_INTEGRATION.md) | Packet format, the CECD dead end, the internet relay |
| [UI_SYSTEM.md](docs/UI_SYSTEM.md) | Screen stack, widgets, animation engine, rendering |
| [ROADMAP.md](docs/ROADMAP.md) | Implementation phases and future features |
| [PROGRESS.md](PROGRESS.md) | Running dev log / build & deploy notes |

## 🚧 Status
First public release (**v0.1.0**). The full core loop is implemented and verified
on real hardware. Known rough edges / planned: a food-picker when feeding, an
evolution cutscene, rendering the remaining accessories/styles, and a few more
nav sounds. Contributions and bug reports welcome.

## 📄 License & credits
Released under the [MIT License](LICENSE). Homebrew and non-commercial in spirit —
**not affiliated with Nintendo**. "Nintendo 3DS" and "StreetPass" are trademarks
of Nintendo. Thanks to the devkitPro, libctru, and 3DS homebrew communities, and
to the folks behind [NetPass](https://gitlab.com/3ds-netpass/netpass) and the
[CECD](https://github.com/NarcolepticK/CECDocs) documentation.
