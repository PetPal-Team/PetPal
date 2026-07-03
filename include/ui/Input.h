// =============================================================================
//  PetPal - Input.h
//  Per-frame input snapshot. UIManager fills this once at the top of each frame
//  from libctru's HID, so screens get a clean, testable struct instead of
//  calling hid* directly.
// =============================================================================
#pragma once

#include <cstdint>

namespace petpal {

struct TouchPoint {
    int  x = 0, y = 0;   // bottom-screen pixels
    bool active = false; // true while the stylus is down this frame
};

struct Input {
    uint32_t down = 0;   // keys pressed this frame (KEY_* bitmask)
    uint32_t held = 0;   // keys held
    uint32_t up   = 0;   // keys released this frame

    TouchPoint touch;        // current touch
    bool       touchStart = false; // first frame of a touch
    bool       touchEnd   = false; // released this frame

    // Convenience predicates (use libctru KEY_* values for the masks).
    bool pressed(uint32_t mask) const { return (down & mask) != 0; }
    bool isHeld(uint32_t mask) const  { return (held & mask) != 0; }
    bool released(uint32_t mask) const{ return (up & mask) != 0; }

    // Is (px,py..pw,ph) being tapped this frame?
    bool tappedRect(float x, float y, float w, float h) const {
        return touchStart && touch.x >= x && touch.x < x + w &&
               touch.y >= y && touch.y < y + h;
    }
};

} // namespace petpal
