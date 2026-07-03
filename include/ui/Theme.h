// =============================================================================
//  PetPal - Theme.h
//  Centralized UI palette and metrics so the whole app shares one look. Colors
//  are 0xRRGGBBAA; convert with toC2D() at draw time. The palette aims for the
//  bright, rounded, toy-like feel of StreetPass Mii Plaza / Tomodachi Life.
// =============================================================================
#pragma once

#include <cstdint>

namespace petpal {
namespace theme {

// Backgrounds (soft pastel gradient drawn top->bottom).
constexpr uint32_t kBgTop      = 0xFFF3D6FF; // warm cream
constexpr uint32_t kBgBottom   = 0xFFD89BFF; // peach
constexpr uint32_t kBgPanel    = 0xFFFFFFEE; // translucent white card

// Accents.
constexpr uint32_t kPrimary    = 0xFF7FB5FF; // friendly coral-pink
constexpr uint32_t kPrimaryDk  = 0xE05C95FF;
constexpr uint32_t kSecondary  = 0x5BC0DEFF; // sky blue
constexpr uint32_t kSuccess    = 0x6FCF6FFF; // mint green
constexpr uint32_t kWarning    = 0xF2C14EFF; // sunny yellow

// Text.
constexpr uint32_t kText       = 0x4A3B3BFF; // soft brown-black
constexpr uint32_t kTextLight  = 0xFFFFFFFF;
constexpr uint32_t kTextMuted  = 0x9A8C8CFF;

// Button.
constexpr uint32_t kButtonFill    = 0xFFFFFFFF;
constexpr uint32_t kButtonShadow  = 0x00000033;
constexpr uint32_t kButtonPressed = 0xFFE3EFFF;

// Stat bars.
constexpr uint32_t kBarBack    = 0x00000022;
constexpr uint32_t kBarHappy   = 0xFF8FC7FF; // pink
constexpr uint32_t kBarEnergy  = 0xF2C14EFF; // yellow
constexpr uint32_t kBarXp      = 0x5BC0DEFF; // blue

// Metrics.
constexpr float kCorner     = 12.0f;  // default rounded-corner radius
constexpr float kPad        = 8.0f;
constexpr float kButtonH    = 44.0f;
constexpr float kTextScale  = 0.6f;

} // namespace theme
} // namespace petpal
