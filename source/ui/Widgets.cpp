#include "ui/Widgets.h"
#include "ui/Theme.h"
#include "ui/Gfx.h"

#ifdef __3DS__
#include <3ds.h>
#endif

namespace petpal {

// -----------------------------------------------------------------------------
//  Hit-testing / animation (portable - no citro2d needed)
// -----------------------------------------------------------------------------
bool Button::handle(const Input& in, bool focused) {
    if (!enabled_) return false;
    bool activated = in.tappedRect(x_, y_, w_, h_);
#ifdef __3DS__
    if (focused && in.pressed(KEY_A)) activated = true;
#else
    (void)focused;
#endif
    if (activated) pressAnim_ = 1.0f;
    return activated;
}

void Button::update(float dt) {
    if (pressAnim_ > 0.0f) {
        pressAnim_ -= dt * 4.0f; // ~0.25s decay
        if (pressAnim_ < 0.0f) pressAnim_ = 0.0f;
    }
}

// -----------------------------------------------------------------------------
//  Drawing (device only)
// -----------------------------------------------------------------------------
#ifdef __3DS__
namespace draw {

void roundedRect(float x, float y, float w, float h, float r, u32 color) {
    if (r * 2.0f > w) r = w * 0.5f;
    if (r * 2.0f > h) r = h * 0.5f;
    // Cross of rectangles + four corner circles.
    C2D_DrawRectSolid(x + r, y,       0.0f, w - 2*r, h,        color);
    C2D_DrawRectSolid(x,     y + r,   0.0f, r,       h - 2*r,  color);
    C2D_DrawRectSolid(x+w-r, y + r,   0.0f, r,       h - 2*r,  color);
    C2D_DrawCircleSolid(x + r,     y + r,     0.0f, r, color);
    C2D_DrawCircleSolid(x + w - r, y + r,     0.0f, r, color);
    C2D_DrawCircleSolid(x + r,     y + h - r, 0.0f, r, color);
    C2D_DrawCircleSolid(x + w - r, y + h - r, 0.0f, r, color);
}

void card(float x, float y, float w, float h, float r, u32 fill, u32 shadow) {
    roundedRect(x + 2.0f, y + 3.0f, w, h, r, shadow); // soft offset shadow
    roundedRect(x, y, w, h, r, fill);
}

void bar(float x, float y, float w, float h, float t, u32 back, u32 fill) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    const float r = h * 0.5f;
    roundedRect(x, y, w, h, r, back);
    if (t > 0.001f) {
        float fw = w * t;
        if (fw < h) fw = h; // keep the rounded cap visible at low values
        roundedRect(x, y, fw, h, r, fill);
    }
}

void textCentered(C2D_Font font, C2D_TextBuf buf, const char* str,
                  float cx, float cy, float scale, u32 color) {
    C2D_Text text;
    C2D_TextFontParse(&text, font, buf, str);
    C2D_TextOptimize(&text);
    float tw = 0, th = 0;
    C2D_TextGetDimensions(&text, scale, scale, &tw, &th);
    C2D_DrawText(&text, C2D_WithColor, cx - tw * 0.5f, cy - th * 0.5f, 0.5f,
                 scale, scale, color);
}

void textLeft(C2D_Font font, C2D_TextBuf buf, const char* str,
              float x, float y, float scale, u32 color) {
    C2D_Text text;
    C2D_TextFontParse(&text, font, buf, str);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

} // namespace draw

void Button::draw(C2D_Font font, C2D_TextBuf buf, bool focused) const {
    using namespace theme;

    // Press bounce: shrink slightly toward the press, then spring back.
    const float squash = 1.0f - 0.06f * pressAnim_;
    const float dw = w_ * (1.0f - squash) * 0.5f;
    const float dh = h_ * (1.0f - squash) * 0.5f;
    const float bx = x_ + dw, by = y_ + dh;
    const float bw = w_ - dw * 2.0f, bh = h_ - dh * 2.0f;

    u32 fill = enabled_ ? (focused ? toC2D(kButtonPressed) : toC2D(kButtonFill))
                        : toC2D(0xDDDDDDFF);
    draw::card(bx, by, bw, bh, kCorner, fill, toC2D(kButtonShadow));

    if (focused) {
        // Bright outline ring when selected via D-pad.
        draw::roundedRect(bx - 2, by - 2, bw + 4, 3, 2, toC2D(kPrimary));
    }

    const u32 textColor = enabled_ ? toC2D(kText) : toC2D(kTextMuted);
    draw::textCentered(font, buf, label_.c_str(), bx + bw * 0.5f, by + bh * 0.5f,
                       kTextScale, textColor);
}
#endif // __3DS__

} // namespace petpal
