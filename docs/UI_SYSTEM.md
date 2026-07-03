# PetPal UI System

The UI aims for the bright, rounded, bouncy feel of StreetPass Mii Plaza /
Tomodachi Life across both screens at 60 FPS. It is built on **citro2d** (with
citro3d underneath) and is the only device-bound part of the codebase.

## Pieces

| Type | File | Role |
|------|------|------|
| `UIManager` | `ui/UIManager.*` | Owns render targets, font, text buffer, sprite sheet, the screen stack, and the animation engine. Drives one frame. |
| `Screen` | `ui/Screen.h` | Abstract base: `update`, `drawTop`, `drawBottom`, `onEnter/onExit`. |
| 9 screens | `ui/screens/*` | Onboarding, MainMenu, Pet, Friends, Adventures, Journal, Customization, Achievements, Settings. |
| `Widgets` | `ui/Widgets.*` | `Button` (rounded + press-bounce) and `draw::` helpers (rounded rect, card+shadow, value bar, centered/left text). |
| `AnimationManager` | `ui/AnimationManager.*` | Easing functions + named `Tween`s + a global clock for idle motion. |
| `PetRenderer` | `ui/PetRenderer.*` | Draws the pet (procedural today, sprite-ready). |
| `Theme` | `ui/Theme.h` | Central palette + metrics. |
| `Gfx` | `ui/Gfx.h` | Screen geometry + palette → citro2d color conversion. |
| `Input` | `ui/Input.h` | Per-frame HID snapshot (keys + touch), hit-testing. |

## Screen split

* **Top screen (400×240)** — display: the pet, environment, stats, status. No
  touch.
* **Bottom screen (320×240)** — interaction: buttons, lists, menus. Touch +
  D-pad/A both work everywhere; a focus ring shows the D-pad selection.

## Navigation

`UIManager` keeps a `navStack_` of `ScreenId`. `navigateTo(id)` pushes (fires
`onExit`/`onEnter` and a slide tween); `goBack()` pops (never past the root).
The root is **Onboarding** on first run, otherwise **MainMenu**. `START` on the
hub or onboarding quits.

All nine screens are built once at startup and kept alive, so switching is
instant and screens retain scroll/cursor state.

## Frame rendering

```cpp
bool UIManager::tick(float dt) {
    scanInput();                       // HID -> Input
    if (START on hub/onboarding) return false;
    current()->update(dt, input_);     // gameplay verbs go through Game
    anim_.update(dt);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
      C2D_TargetClear(top);    C2D_SceneBegin(top);    drawBackground(); current()->drawTop();
      C2D_TargetClear(bottom); C2D_SceneBegin(bottom); current()->drawBottom();
    C3D_FrameEnd(0);
    C2D_TextBufClear(dynBuf_);          // text buffer is per-frame
    return true;
}
```

`C3D_FRAME_SYNCDRAW` paces to VSync for a stable 60 FPS. The shared `dynBuf_`
text buffer is filled during drawing and cleared each frame; the system font is
used (no custom font load required).

## Animation engine

`AnimationManager` advances every registered `Tween` and a global clock.

* **Easing**: `linear`, `easeIn/Out/InOutQuad`, `easeOutBack` (the UI "pop"),
  `easeOutBounce`, `easeOutElastic`.
* **Named tweens**: `play("key", from, to, dur, ease)` then read `valueOf("key")`
  each frame. Used for nav slides and the celebration flourish.
* **Idle motion**: screens read `anim().time()` and use `sinf` for continuous
  effects (pet bob, blink, evolve-button pulse) without storing a tween.

`Button` has its own decaying `pressAnim_` so taps squash-and-spring
independently of any manager state.

## Drawing helpers

citro2d has no rounded-rect primitive, so `draw::roundedRect()` composes a cross
of rectangles + four corner circles. `draw::card()` adds a soft offset shadow;
`draw::bar()` is a rounded progress/stat bar; `draw::textCentered/textLeft()`
parse → optimize → draw text from the shared buffer.

Colors live in `Theme.h` as `0xRRGGBBAA` and convert via `toC2D()` (which packs
to citro2d's `0xAABBGGRR`). The pet palette converts with `paletteColor()`.

## Pet rendering (procedural → sprites)

`PetRenderer` currently draws each species from primitives (body, ears/horns/
antenna/gills, eyes with blink, cheeks, smile, accessory), tinted by the pet's
three color slots, scaled up per evolution stage, and bobbing via the animation
clock. This keeps the game fully playable and the animation system visible
before any art exists.

To switch to real art: author a citro2d atlas (`gfx/sprites.t3s` →
`romfs:/gfx/sprites.t3x`, loaded by `UIManager`), then replace the primitive
calls in `PetRenderer::draw()` with `C2D_DrawSprite` using a sprite index keyed
by `{species, stage, frame}`. Nothing else changes.

## Celebrations & audio (hook)

`UIManager::celebrate(kind)` is the single entry point for level-up / evolution
/ new-friend / achievement / reward feedback. It currently kicks a tween;
particle bursts and `ndsp` jingle playback wire in here once SFX/music assets
land (see `assets/README.md`). Settings expose music/SFX volume for that path.

## Adding a screen

1. Subclass `Screen` in `ui/screens/`.
2. Add an enum value before `Count` in `ScreenId`.
3. Instantiate it in `UIManager::init()` at that index.
4. Point a `MainMenuScreen` tile (or another screen) at it via `navigateTo`.
