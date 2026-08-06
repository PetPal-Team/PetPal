// =============================================================================
//  PetPal - Markdown.h
//  A tiny Markdown -> styled, width-wrapped "display lines" engine for the 3DS
//  News page. citro2d has a single proportional font (no bold/italic), so inline
//  markers are stripped to plain text and emphasis is conveyed by BLOCK style
//  (bigger headings, bullet/number prefixes, quote indent, a rule line). Mirrors
//  the Android renderer + admin preview so authored posts read the same way.
//
//  Pure C++ (no citro2d) so the wrapping logic is unit-tested on the host build.
//  Wrapping is measured via a caller-supplied `measure(text, style)` that returns
//  the pixel width of `text` at that style's font scale.
// =============================================================================
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace petpal {
namespace md {

enum class Style : unsigned char {
    Normal,
    H1, H2, H3,
    Bullet,
    Ordered,
    Quote,
    Code,
    Rule,    // horizontal rule (text is empty)
    Blank,   // paragraph spacer (text is empty)
};

struct Line {
    Style       style;
    std::string text;   // already inline-stripped + prefixed (bullets/numbers)
};

// Pixel width of `text` rendered at `style`'s scale.
using Measure = std::function<float(const std::string&, Style)>;

// Remove inline markers, keeping the visible text: **b**, *i*/_i_, `c`, and
// [label](url) -> label. Only paired markers are stripped (so snake_case and a
// lone * survive as literals).
std::string stripInline(const std::string& s);

// Parse `body` into styled, width-wrapped display lines fitting `maxWidth` px.
std::vector<Line> layout(const std::string& body, float maxWidth, const Measure& measure);

} // namespace md
} // namespace petpal
