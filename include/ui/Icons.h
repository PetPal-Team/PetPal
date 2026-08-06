// =============================================================================
//  PetPal - Icons.h
//  Sub-image indices into romfs:/gfx/sprites.t3x. Order MUST match the line
//  order in gfx/sprites.t3s. UI icons are white silhouettes (drawn tinted via
//  UIManager::drawIcon). Button glyphs are full-color Switch prompts (drawn
//  untinted via UIManager::drawButtonGlyph).
// =============================================================================
#pragma once

namespace petpal {

enum class Icon : int {
    Pet = 0,
    Friends,
    Adventures,
    Journal,
    Customize,
    Awards,
    Settings,
    Coin,
    Happy,       // heart
    Energy,      // lightning
    Star,
    Friendship,  // hearts
    Chest,
    Apple,
    Bone,
    Count
};
constexpr int kIconCount = static_cast<int>(Icon::Count); // 15

// Nintendo Switch button glyphs (A/B/X/Y/L/R). Their atlas indices continue
// right after the icons in gfx/sprites.t3s.
enum class Btn : int {
    A = 15,
    B = 16,
    X = 17,
    Y = 18,
    L = 19,
    R = 20,
};

// Full-color decorative sprites (from the itch.io "Pixel UI pack 3"). Unlike the
// UI icons these are NOT white silhouettes, so they're drawn UNTINTED via
// UIManager::drawDeco. Indices continue after the button glyphs in sprites.t3s.
enum class Deco : int {
    StarFull  = 21,  // gold star  - filled friendship pip / unlocked award
    StarEmpty = 22,  // gray star  - empty pip / locked award
    BarTrack  = 23,  // empty dark stat-bar track (stretched to width)
    BarPink   = 24,  // happiness fill
    BarYellow = 25,  // energy fill
    BarOrange = 26,  // hunger fill
    BarBlue   = 27,  // xp / progress fill
    // Evolution-stage badges (gem shields + a winged emblem for Legendary).
    EmblemBaby      = 28,  // green gem
    EmblemTeen      = 29,  // blue gem
    EmblemAdult     = 30,  // orange gem
    EmblemRare      = 31,  // silver gem
    EmblemLegend    = 32,  // winged emblem
};

} // namespace petpal
