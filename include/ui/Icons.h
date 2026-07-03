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

} // namespace petpal
