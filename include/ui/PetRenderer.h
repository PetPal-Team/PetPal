// =============================================================================
//  PetPal - PetRenderer.h
//  Draws the pet as simple, low-detail hardcoded shapes (no sprite assets).
//  Each species has a distinct silhouette built from circles/ellipses/triangles
//  and is colored by the pet's two customizable colors (primary + secondary).
//  Three moods drive simple animation: Idle, Happy, Sad.
// =============================================================================
#pragma once

#include "pets/Pet.h"

namespace petpal {

class AnimationManager;

enum class PetMood : uint8_t { Idle = 0, Happy, Sad };

namespace petrender {

#ifdef __3DS__
// Draw `pet` centered at (cx, cy). `anim` drives idle motion; `scale` lets
// callers grow the pet (evolution flourish); `mood` selects the animation.
void draw(const Pet& pet, float cx, float cy, AnimationManager& anim,
          float scale = 1.0f, PetMood mood = PetMood::Idle);

// Small head-only portrait for menus / friend lists.
void drawPortrait(Species species, PetColor primary, PetColor secondary,
                  float cx, float cy, float size);
#else
inline void draw(const Pet&, float, float, AnimationManager&, float = 1.0f,
                 PetMood = PetMood::Idle) {}
inline void drawPortrait(Species, PetColor, PetColor, float, float, float) {}
#endif

} // namespace petrender
} // namespace petpal
