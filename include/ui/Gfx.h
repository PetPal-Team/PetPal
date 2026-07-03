// =============================================================================
//  PetPal - Gfx.h
//  Small helpers bridging our palette/types to citro2d, plus shared screen
//  metrics. Keeping these here means screens don't each re-derive constants.
// =============================================================================
#pragma once

#include "core/Types.h"
#include "core/Names.h"   // colorRGBA()

#ifdef __3DS__
#include <citro2d.h>
#endif

namespace petpal {

// 3DS screen geometry (pixels).
constexpr float kTopWidth     = 400.0f;
constexpr float kTopWidthWide = 800.0f; // when stereoscopic-wide, unused in 2D
constexpr float kBottomWidth  = 320.0f;
constexpr float kScreenHeight = 240.0f;

#ifdef __3DS__
// Convert our 0xRRGGBBAA palette value to a citro2d color (0xAABBGGRR).
inline u32 toC2D(uint32_t rgba) {
    const u8 r = (rgba >> 24) & 0xFF;
    const u8 g = (rgba >> 16) & 0xFF;
    const u8 b = (rgba >> 8)  & 0xFF;
    const u8 a = (rgba >> 0)  & 0xFF;
    return C2D_Color32(r, g, b, a);
}

// citro2d color from a pet palette entry.
inline u32 paletteColor(PetColor c) { return toC2D(colorRGBA(c)); }
#endif // __3DS__

} // namespace petpal
