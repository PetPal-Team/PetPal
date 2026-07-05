// =============================================================================
//  PetPal - UIManager.h
//  Owns the citro2d/citro3d render state, the shared font and text buffers, the
//  pet/UI sprite sheets, the per-frame input snapshot, and the stack of
//  Screens. It drives one frame at a time: scan input -> update -> render top
//  and bottom -> swap. Screens ask the UIManager to navigate; the UIManager
//  handles slide transitions between them.
// =============================================================================
#pragma once

#include "ui/Screen.h"
#include "ui/Input.h"
#include "ui/AnimationManager.h"
#include "ui/Icons.h"
#include "core/Types.h"   // Location, etc.
#include <memory>
#include <vector>

#ifdef __3DS__
#include <citro2d.h>
#include <citro3d.h>
#endif

namespace petpal {

class Game;
class Pet;

class UIManager {
public:
    explicit UIManager(Game* game);
    ~UIManager();

    // Bring up graphics, load assets, and build all screens. Returns false if
    // citro2d init fails. Safe to call once.
    bool init();
    void shutdown();

    // Run exactly one frame: input -> update -> draw. Returns false when the
    // app should quit (e.g. START on the main menu, or system requests exit).
    bool tick(float dt);

    // --- Navigation ---------------------------------------------------------
    void navigateTo(ScreenId id);  // push with a slide-in transition
    void goBack();                 // pop to previous (usually MainMenu)
    ScreenId currentId() const;

    // --- Shared resources for screens --------------------------------------
    AnimationManager& anim() { return anim_; }
    const Input&      input() const { return input_; }

#ifdef __3DS__
    C2D_Font    font()    const { return font_; }
    C2D_TextBuf dynBuf()  const { return dynBuf_; }   // cleared every frame
    C2D_SpriteSheet sprites() const { return sprites_; }

    // Draw a white-silhouette atlas icon centered at (cx,cy), scaled to `size`
    // pixels, multiplied by `tint` (0xAABBGGRR). No-op if the atlas is missing.
    void drawIcon(Icon id, float cx, float cy, float size, u32 tint);
    bool hasIcons() const { return sprites_ != nullptr; }

    // Draw a consistent section header (accent pill + icon + title) centered at
    // the top of the *top* screen. `accentRGBA` is 0xRRGGBBAA. Call from drawTop.
    void drawTitleBar(Icon icon, const char* title, uint32_t accentRGBA);

    // Draw the pet centered at (cx,cy) as procedural shapes (PetRenderer),
    // choosing the Idle/Happy/Sad animation from the pet's current stats.
    void drawPet(const Pet& pet, float cx, float cy, float scale = 1.0f);

    // Fill the top screen with a simple procedural background for `loc`
    // (sky/ground + a few shapes). Call first in a screen's drawTop().
    void drawLocationBg(Location loc);

    // Draw a Switch button glyph (untinted) with its native aspect, `height`
    // tall, left edge at (x), vertically centered on (cy). Returns drawn width.
    float drawButtonGlyph(Btn b, float x, float cy, float height);

    // Draw a "[glyph] label" hint at (x, y-top). Returns the x after the label.
    float drawHint(Btn b, const char* label, float x, float y);

    // Draw an animated loading spinner (ring of fading dots) centered at (cx,cy).
    void drawSpinner(float cx, float cy, float radius, uint32_t colorRGBA);
#endif

    // Trigger a celebratory flourish (particles + jingle) - used after level
    // ups, evolutions, and new friends. `kind` selects the effect.
    enum class Celebration { LevelUp, Evolution, NewFriend, Achievement, Reward };
    void celebrate(Celebration kind);

    // Run a blocking full-screen message on the top screen until the player
    // presses START. Used for the boot "please update" prompt. No-op off device.
    void showModalMessage(const char* line1, const char* line2);

private:
    void scanInput();
    Screen* current();

#ifdef __3DS__
    void drawBackground(); // shared pastel gradient
#endif

    Game* game_;

    // Screen stack (back navigation pops).
    std::vector<std::unique_ptr<Screen>> screens_; // indexed by ScreenId
    std::vector<ScreenId> navStack_;
    ScreenId pending_ = ScreenId::Count; // transition target, or Count if none
    float    transition_ = 0.0f;         // 0..1 slide progress
    int      transitionDir_ = 1;         // +1 = forward (push), -1 = back (pop)

    AnimationManager anim_;
    Input input_;
    bool  started_ = false;

#ifdef __3DS__
    C3D_RenderTarget* top_    = nullptr;
    C3D_RenderTarget* bottom_ = nullptr;
    C2D_Font          font_     = nullptr;
    C2D_TextBuf       dynBuf_   = nullptr;
    C2D_SpriteSheet   sprites_  = nullptr; // UI icons + button glyphs
#endif
};

} // namespace petpal
