// =============================================================================
//  PetPal - Widgets.h
//  Reusable bottom-screen controls drawn with citro2d: rounded buttons with a
//  bounce-on-press effect, progress/stat bars, and panel/card backgrounds.
//  Drawing is device-only; the geometry + hit-testing is plain data so it can
//  be reasoned about anywhere.
// =============================================================================
#pragma once

#include "ui/Input.h"
#include <string>

#ifdef __3DS__
#include <citro2d.h>
#endif

namespace petpal {

// -----------------------------------------------------------------------------
//  Low-level draw helpers (no-ops off device so headers stay portable).
// -----------------------------------------------------------------------------
namespace draw {
#ifdef __3DS__
    // Filled rounded rectangle (rect body + 4 corner circles).
    void roundedRect(float x, float y, float w, float h, float r, u32 color);
    // Rounded rect with a soft drop shadow underneath.
    void card(float x, float y, float w, float h, float r, u32 fill, u32 shadow);
    // Horizontal value bar (0..1) with rounded ends.
    void bar(float x, float y, float w, float h, float t, u32 back, u32 fill);
    // Centered text within a box using a shared text buffer.
    void textCentered(C2D_Font font, C2D_TextBuf buf, const char* str,
                      float cx, float cy, float scale, u32 color);
    void textLeft(C2D_Font font, C2D_TextBuf buf, const char* str,
                  float x, float y, float scale, u32 color);
#endif
} // namespace draw

// -----------------------------------------------------------------------------
//  Button
// -----------------------------------------------------------------------------
class Button {
public:
    Button() = default;
    Button(float x, float y, float w, float h, std::string label)
        : x_(x), y_(y), w_(w), h_(h), label_(std::move(label)) {}

    void setBounds(float x, float y, float w, float h) { x_=x; y_=y; w_=w; h_=h; }
    void setLabel(std::string s) { label_ = std::move(s); }
    void setIcon(int iconIndex)  { icon_ = iconIndex; }
    void setEnabled(bool e)      { enabled_ = e; }
    bool enabled() const         { return enabled_; }

    float x() const { return x_; } float y() const { return y_; }
    float w() const { return w_; } float h() const { return h_; }

    // Returns true if activated this frame (tapped, or A-pressed while focused).
    bool handle(const Input& in, bool focused);

    // Advance the press-bounce animation.
    void update(float dt);

#ifdef __3DS__
    void draw(C2D_Font font, C2D_TextBuf buf, bool focused) const;
#endif

private:
    float x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    std::string label_;
    int   icon_ = -1;
    bool  enabled_ = true;
    float pressAnim_ = 0.0f; // 1.0 right after press, decays toward 0
};

} // namespace petpal
