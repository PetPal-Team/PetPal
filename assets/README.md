# PetPal Assets

This folder holds the **source** art and audio for PetPal. Binary assets are not
committed as code; drop the real files here and the build/runtime will pick them
up. Until then the game renders procedurally (see `source/ui/PetRenderer.cpp`)
so it is fully playable without any art.

```
assets/
  sprites/   Pet body/animation frames, per species & evolution stage
  ui/        Icons, buttons, backgrounds, menu chrome; icon.png / banner.png
  fonts/     Optional custom .bcfnt (the system font is used by default)
  sfx/       Sound effects (.wav / .bcwav): taps, level-up, evolution, coins
  music/     Background tracks & the evolution jingle
```

## How assets reach the device

* **Sprites / UI atlas** → authored as a citro2d texture atlas. Put a `*.t3s`
  spec in `gfx/` (see `gfx/sprites.t3s.example`); the Makefile runs `tex3ds`
  to produce `romfs:/gfx/sprites.t3x`, which `UIManager` loads at startup. If
  the atlas is missing, the procedural renderer is used instead.
* **Audio** → place `.wav`/`.bcwav` under `romfs/` and load via `ndsp`. Audio
  playback is stubbed at `UIManager::celebrate()` and the Settings volume
  fields; wire `ndspChnWaveBufAdd` there when assets land.
* **Homebrew icon / banner** → `assets/ui/icon.png` (48×48, generated from the
  `assets/ui/logo.png` brand art) is already wired into the `Makefile` via
  `ICON`. Replace either file to rebrand. A `BANNER` is only needed for CIA
  builds.

## Naming convention (suggested)

```
sprites/pet_<species>_<stage>_<frame>.png   e.g. pet_fox_baby_idle0.png
ui/icon_<menu>.png                          e.g. icon_friends.png
sfx/sfx_<event>.wav                         e.g. sfx_levelup.wav
music/bgm_<scene>.wav                       e.g. bgm_main.wav
```
