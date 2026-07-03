// =============================================================================
//  PetPal - PetRenderer.cpp
//  Low-detail procedural pets. Each species is built from primitives and tinted
//  by the pet's two colors (primary = body, secondary = features). Mood drives
//  motion (bob), eyes, and mouth so the pet reads as Idle / Happy / Sad.
// =============================================================================
#include "ui/PetRenderer.h"
#include "ui/AnimationManager.h"
#include "ui/Gfx.h"

#ifdef __3DS__
#include <3ds.h>
#include <citro2d.h>
#include <cmath>

namespace petpal {
namespace petrender {

namespace {
inline u32 dark() { return C2D_Color32(44, 34, 34, 255); }
inline u32 white() { return C2D_Color32(255, 255, 255, 255); }

// Eyes + mouth + cheeks, all additive (no background matching needed).
void drawFace(float fx, float fy, float fr, PetMood mood, float t) {
    const u32 eyeCol = dark();
    const float eyeR = fr * 0.12f, edx = fr * 0.32f, ey = fy - fr * 0.05f;

    // Rosy cheeks.
    const u32 cheek = C2D_Color32(255, 140, 170, 140);
    C2D_DrawCircleSolid(fx - fr * 0.55f, fy + fr * 0.12f, 0.0f, fr * 0.12f, cheek);
    C2D_DrawCircleSolid(fx + fr * 0.55f, fy + fr * 0.12f, 0.0f, fr * 0.12f, cheek);

    const float eyAdj = (mood == PetMood::Sad) ? fr * 0.06f : 0.0f;
    auto eye = [&](float ex) {
        const bool blink = (mood == PetMood::Idle) && std::fmod(t, 4.0f) < 0.12f;
        if (blink) { C2D_DrawRectSolid(ex - eyeR, ey - 1.0f, 0.0f, eyeR * 2.0f, 2.5f, eyeCol); return; }
        const float er = (mood == PetMood::Happy) ? eyeR * 0.85f : eyeR;
        C2D_DrawCircleSolid(ex, ey + eyAdj, 0.0f, er, eyeCol);
        C2D_DrawCircleSolid(ex - er * 0.3f, ey + eyAdj - er * 0.35f, 0.0f, er * 0.4f, white());
    };
    eye(fx - edx);
    eye(fx + edx);

    // Mouth: dots along a parabola (up = smile, down = frown, flat = neutral).
    const float mw = fr * 0.46f, my = fy + fr * 0.34f, amp = fr * 0.16f;
    for (int i = 0; i < 5; ++i) {
        const float u = static_cast<float>(i) / 4.0f - 0.5f;
        const float x = fx + u * mw;
        float yy = my;
        if (mood == PetMood::Happy)     yy = my + amp * (1.0f - 4.0f * u * u);
        else if (mood == PetMood::Sad)  yy = my - amp * (1.0f - 4.0f * u * u) + fr * 0.08f;
        C2D_DrawCircleSolid(x, yy, 0.0f, fr * 0.055f, eyeCol);
    }

    // Sad: a little teardrop.
    if (mood == PetMood::Sad)
        C2D_DrawCircleSolid(fx + edx + fr * 0.1f, ey + fr * 0.22f, 0.0f, fr * 0.06f,
                            C2D_Color32(120, 200, 255, 220));
}

inline void tri(float ax, float ay, float bx, float by, float cx_, float cy_, u32 c) {
    C2D_DrawTriangle(ax, ay, c, bx, by, c, cx_, cy_, c, 0.0f);
}

void drawSpecies(Species sp, float cx, float cy, float r, u32 c1, u32 c2,
                 PetMood mood, float t) {
    switch (sp) {
    case Species::Fox:
        tri(cx - r*0.6f, cy - r*0.6f, cx - r*1.0f, cy - r*1.6f, cx - r*0.1f, cy - r*1.0f, c1);
        tri(cx + r*0.6f, cy - r*0.6f, cx + r*1.0f, cy - r*1.6f, cx + r*0.1f, cy - r*1.0f, c1);
        tri(cx - r*0.55f, cy - r*0.7f, cx - r*0.85f, cy - r*1.35f, cx - r*0.25f, cy - r*0.95f, c2);
        tri(cx + r*0.55f, cy - r*0.7f, cx + r*0.85f, cy - r*1.35f, cx + r*0.25f, cy - r*0.95f, c2);
        C2D_DrawEllipseSolid(cx + r*0.6f, cy - r*0.1f, 0.0f, r*1.1f, r*0.7f, c1);   // tail
        C2D_DrawCircleSolid(cx + r*1.55f, cy + r*0.2f, 0.0f, r*0.3f, c2);            // tail tip
        C2D_DrawEllipseSolid(cx - r, cy - r, 0.0f, r*2.0f, r*2.0f, c1);             // body
        C2D_DrawEllipseSolid(cx - r*0.5f, cy + r*0.05f, 0.0f, r*1.0f, r*1.0f, c2);  // chest/snout
        drawFace(cx, cy, r*0.85f, mood, t);
        break;

    case Species::Cat:
        tri(cx - r*0.55f, cy - r*0.55f, cx - r*0.85f, cy - r*1.3f, cx - r*0.1f, cy - r*0.95f, c1);
        tri(cx + r*0.55f, cy - r*0.55f, cx + r*0.85f, cy - r*1.3f, cx + r*0.1f, cy - r*0.95f, c1);
        tri(cx - r*0.5f, cy - r*0.65f, cx - r*0.72f, cy - r*1.1f, cx - r*0.25f, cy - r*0.9f, c2);
        tri(cx + r*0.5f, cy - r*0.65f, cx + r*0.72f, cy - r*1.1f, cx + r*0.25f, cy - r*0.9f, c2);
        C2D_DrawEllipseSolid(cx + r*0.7f, cy + r*0.2f, 0.0f, r*0.9f, r*0.5f, c1);   // tail
        C2D_DrawEllipseSolid(cx - r, cy - r, 0.0f, r*2.0f, r*2.0f, c1);
        C2D_DrawEllipseSolid(cx - r*0.5f, cy + r*0.1f, 0.0f, r*1.0f, r*0.95f, c2);  // belly
        // whiskers
        C2D_DrawRectSolid(cx - r*0.95f, cy + r*0.18f, 0.0f, r*0.4f, 2.0f, dark());
        C2D_DrawRectSolid(cx + r*0.55f, cy + r*0.18f, 0.0f, r*0.4f, 2.0f, dark());
        drawFace(cx, cy, r*0.85f, mood, t);
        break;

    case Species::Bunny:
        C2D_DrawEllipseSolid(cx - r*0.75f, cy - r*2.1f, 0.0f, r*0.45f, r*1.5f, c1); // ears
        C2D_DrawEllipseSolid(cx + r*0.3f,  cy - r*2.1f, 0.0f, r*0.45f, r*1.5f, c1);
        C2D_DrawEllipseSolid(cx - r*0.66f, cy - r*1.95f, 0.0f, r*0.22f, r*1.15f, c2);
        C2D_DrawEllipseSolid(cx + r*0.42f, cy - r*1.95f, 0.0f, r*0.22f, r*1.15f, c2);
        C2D_DrawEllipseSolid(cx - r, cy - r*0.9f, 0.0f, r*2.0f, r*1.9f, c1);        // body
        C2D_DrawEllipseSolid(cx - r*0.5f, cy + r*0.05f, 0.0f, r*1.0f, r*0.95f, c2); // belly
        C2D_DrawCircleSolid(cx, cy + r*0.95f, 0.0f, r*0.25f, c2);                   // foot puff
        drawFace(cx, cy, r*0.85f, mood, t);
        break;

    case Species::Dragon:
        tri(cx - r*0.5f, cy - r*0.85f, cx - r*0.7f, cy - r*1.5f, cx - r*0.2f, cy - r*0.95f, c2); // horns
        tri(cx + r*0.5f, cy - r*0.85f, cx + r*0.7f, cy - r*1.5f, cx + r*0.2f, cy - r*0.95f, c2);
        tri(cx - r*0.9f, cy - r*0.2f, cx - r*1.9f, cy - r*0.6f, cx - r*1.0f, cy + r*0.6f, c2);    // wings
        tri(cx + r*0.9f, cy - r*0.2f, cx + r*1.9f, cy - r*0.6f, cx + r*1.0f, cy + r*0.6f, c2);
        C2D_DrawEllipseSolid(cx - r, cy - r, 0.0f, r*2.0f, r*2.0f, c1);             // body
        C2D_DrawEllipseSolid(cx - r*0.55f, cy + r*0.05f, 0.0f, r*1.1f, r*1.0f, c2); // belly
        // spikes
        tri(cx - r*0.15f, cy - r*1.0f, cx, cy - r*1.35f, cx + r*0.15f, cy - r*1.0f, c2);
        drawFace(cx, cy, r*0.85f, mood, t);
        break;

    case Species::Slime: {
        // Dome: big circle, flattened bottom, wavy base.
        C2D_DrawCircleSolid(cx, cy - r*0.1f, 0.0f, r*1.05f, c1);
        C2D_DrawRectSolid(cx - r*1.05f, cy - r*0.1f, 0.0f, r*2.1f, r*1.0f, c1);
        for (int i = -2; i <= 2; ++i)
            C2D_DrawCircleSolid(cx + i * r*0.45f, cy + r*0.9f, 0.0f, r*0.26f, c1);
        C2D_DrawEllipseSolid(cx - r*0.7f, cy - r*0.8f, 0.0f, r*0.5f, r*0.35f, white()); // shine
        C2D_DrawEllipseSolid(cx - r*0.45f, cy + r*0.2f, 0.0f, r*0.9f, r*0.6f, c2);      // belly
        drawFace(cx, cy + r*0.05f, r*0.8f, mood, t);
        break;
    }

    case Species::Robot: {
        // Antenna.
        C2D_DrawRectSolid(cx - 1.5f, cy - r*1.6f, 0.0f, 3.0f, r*0.7f, dark());
        C2D_DrawCircleSolid(cx, cy - r*1.6f, 0.0f, r*0.16f, c2);
        // Rounded body (rect + corner circles).
        C2D_DrawRectSolid(cx - r*0.9f, cy - r*0.9f, 0.0f, r*1.8f, r*1.8f, c1);
        C2D_DrawCircleSolid(cx - r*0.9f, cy - r*0.9f, 0.0f, r*0.18f, c1);
        C2D_DrawCircleSolid(cx + r*0.9f, cy - r*0.9f, 0.0f, r*0.18f, c1);
        C2D_DrawCircleSolid(cx - r*0.9f, cy + r*0.9f, 0.0f, r*0.18f, c1);
        C2D_DrawCircleSolid(cx + r*0.9f, cy + r*0.9f, 0.0f, r*0.18f, c1);
        // Face panel (secondary).
        C2D_DrawRectSolid(cx - r*0.7f, cy - r*0.7f, 0.0f, r*1.4f, r*1.2f, c2);
        // Feet.
        C2D_DrawRectSolid(cx - r*0.7f, cy + r*0.9f, 0.0f, r*0.4f, r*0.25f, c1);
        C2D_DrawRectSolid(cx + r*0.3f, cy + r*0.9f, 0.0f, r*0.4f, r*0.25f, c1);
        drawFace(cx, cy - r*0.05f, r*0.7f, mood, t);
        break;
    }

    case Species::Axolotl:
    default:
        for (int i = 0; i < 3; ++i) {                                              // gills
            C2D_DrawCircleSolid(cx - r*1.0f, cy - r*0.5f + i * r*0.45f, 0.0f, r*0.22f, c2);
            C2D_DrawCircleSolid(cx + r*1.0f, cy - r*0.5f + i * r*0.45f, 0.0f, r*0.22f, c2);
        }
        C2D_DrawEllipseSolid(cx - r, cy - r, 0.0f, r*2.0f, r*2.0f, c1);            // body
        C2D_DrawEllipseSolid(cx - r*0.5f, cy + r*0.1f, 0.0f, r*1.0f, r*0.95f, c2); // belly
        drawFace(cx, cy, r*0.85f, mood, t);
        break;
    }
}

// Equipped accessory, drawn on top of the head. Only a subset has art so far;
// others (and None) draw nothing.
void drawAccessory(Accessory a, float cx, float cy, float r) {
    const u32 brown = C2D_Color32(0x8B, 0x5A, 0x2B, 255);
    const u32 gold  = C2D_Color32(0xF2, 0xC1, 0x4E, 255);
    const u32 pink  = C2D_Color32(0xFF, 0x7F, 0xB5, 255);
    const u32 grey  = C2D_Color32(0x44, 0x48, 0x52, 255);
    const float topY = cy - r * 0.85f; // crown of the head
    switch (a) {
        case Accessory::DogHat:
            C2D_DrawEllipseSolid(cx - r*0.9f, topY - r*0.25f, 0.0f, r*1.8f, r*0.7f, brown);
            C2D_DrawEllipseSolid(cx - r*0.6f, topY - r*0.55f, 0.0f, r*1.2f, r*0.6f, brown);
            C2D_DrawEllipseSolid(cx - r*1.2f, topY + r*0.1f, 0.0f, r*0.45f, r*0.95f, brown); // floppy ears
            C2D_DrawEllipseSolid(cx + r*0.75f, topY + r*0.1f, 0.0f, r*0.45f, r*0.95f, brown);
            break;
        case Accessory::TopHat:
            C2D_DrawRectSolid(cx - r*0.8f, topY, 0.0f, r*1.6f, r*0.16f, grey);       // brim
            C2D_DrawRectSolid(cx - r*0.45f, topY - r*0.8f, 0.0f, r*0.9f, r*0.85f, grey);
            break;
        case Accessory::Crown:
            C2D_DrawRectSolid(cx - r*0.6f, topY - r*0.1f, 0.0f, r*1.2f, r*0.22f, gold);
            tri(cx - r*0.6f, topY, cx - r*0.4f, topY - r*0.5f, cx - r*0.2f, topY, gold);
            tri(cx - r*0.1f, topY, cx,          topY - r*0.6f, cx + r*0.1f, topY, gold);
            tri(cx + r*0.2f, topY, cx + r*0.4f, topY - r*0.5f, cx + r*0.6f, topY, gold);
            break;
        case Accessory::Bow:
            tri(cx, topY + r*0.25f, cx - r*0.55f, topY - r*0.05f, cx - r*0.55f, topY + r*0.55f, pink);
            tri(cx, topY + r*0.25f, cx + r*0.55f, topY - r*0.05f, cx + r*0.55f, topY + r*0.55f, pink);
            C2D_DrawCircleSolid(cx, topY + r*0.25f, 0.0f, r*0.13f, pink);
            break;
        case Accessory::Headphones:
            C2D_DrawEllipseSolid(cx - r*0.95f, topY - r*0.15f, 0.0f, r*1.9f, r*0.5f, grey); // band
            C2D_DrawCircleSolid(cx - r*0.9f, cy - r*0.15f, 0.0f, r*0.26f, grey);            // cups
            C2D_DrawCircleSolid(cx + r*0.9f, cy - r*0.15f, 0.0f, r*0.26f, grey);
            break;
        default: break;
    }
}

// BonziBuddy transform (redeem code "Bonzi"): a purple gorilla. Ignores species
// and colors.
void drawBonzi(float cx, float cy, float r) {
    const u32 purple  = C2D_Color32(0x8E, 0x44, 0xAD, 255);
    const u32 dpurple = C2D_Color32(0x6C, 0x33, 0x83, 255);
    const u32 tan     = C2D_Color32(0xE8, 0xC9, 0xA0, 255);
    C2D_DrawCircleSolid(cx - r*0.95f, cy - r*0.5f, 0.0f, r*0.36f, purple);  // ears
    C2D_DrawCircleSolid(cx + r*0.95f, cy - r*0.5f, 0.0f, r*0.36f, purple);
    C2D_DrawCircleSolid(cx - r*0.95f, cy - r*0.5f, 0.0f, r*0.18f, tan);
    C2D_DrawCircleSolid(cx + r*0.95f, cy - r*0.5f, 0.0f, r*0.18f, tan);
    C2D_DrawEllipseSolid(cx - r, cy - r, 0.0f, r*2.0f, r*2.0f, purple);     // head/body
    C2D_DrawEllipseSolid(cx - r*0.6f, cy + r*0.05f, 0.0f, r*1.2f, r*1.05f, dpurple);
    C2D_DrawEllipseSolid(cx - r*0.55f, cy + r*0.05f, 0.0f, r*1.1f, r*0.8f, tan); // muzzle
    C2D_DrawCircleSolid(cx - r*0.32f, cy - r*0.28f, 0.0f, r*0.22f, white());     // eyes
    C2D_DrawCircleSolid(cx + r*0.32f, cy - r*0.28f, 0.0f, r*0.22f, white());
    C2D_DrawCircleSolid(cx - r*0.30f, cy - r*0.25f, 0.0f, r*0.1f, dark());
    C2D_DrawCircleSolid(cx + r*0.30f, cy - r*0.25f, 0.0f, r*0.1f, dark());
    C2D_DrawEllipseSolid(cx - r*0.13f, cy, 0.0f, r*0.26f, r*0.17f, dark());      // nose
    for (int i = 0; i < 5; ++i) {                                               // grin
        const float u = static_cast<float>(i) / 4.0f - 0.5f;
        C2D_DrawCircleSolid(cx + u * r*0.6f, cy + r*0.45f + r*0.12f * (1.0f - 4.0f*u*u),
                            0.0f, r*0.05f, dark());
    }
}
} // namespace

void draw(const Pet& pet, float cx, float cy, AnimationManager& anim,
          float scale, PetMood mood) {
    const float t = anim.time();
    const float stageScale = 1.0f + 0.12f * static_cast<float>(pet.stage());
    const float r = 42.0f * scale * stageScale;

    float bob = 0.0f, yoff = 0.0f;
    switch (mood) {
        case PetMood::Happy: bob = -std::fabs(std::sin(t * 6.0f)) * r * 0.16f; break;
        case PetMood::Sad:   bob = std::sin(t * 1.2f) * r * 0.04f; yoff = r * 0.12f; break;
        default:             bob = std::sin(t * 2.0f) * r * 0.07f; break;
    }
    const float py = cy + yoff + bob;

    // Active redeem-code transform overrides the species look.
    if (pet.currentForm() == TransformForm::Bonzi) { drawBonzi(cx, py, r); return; }

    const u32 c1 = paletteColor(pet.primaryColor());
    const u32 c2 = paletteColor(pet.secondaryColor());
    drawSpecies(pet.species(), cx, py, r, c1, c2, mood, t);
    drawAccessory(pet.equippedAccessory(), cx, py, r);
}

void drawPortrait(Species species, PetColor primary, PetColor secondary,
                  float cx, float cy, float size) {
    const float r = size * 0.5f;
    const u32 c1 = paletteColor(primary), c2 = paletteColor(secondary);
    // Tiny species hint (ears) + head + eyes.
    switch (species) {
        case Species::Fox: case Species::Cat:
            tri(cx - r*0.6f, cy - r*0.5f, cx - r*0.9f, cy - r*1.2f, cx - r*0.1f, cy - r*0.9f, c1);
            tri(cx + r*0.6f, cy - r*0.5f, cx + r*0.9f, cy - r*1.2f, cx + r*0.1f, cy - r*0.9f, c1);
            break;
        case Species::Bunny:
            C2D_DrawEllipseSolid(cx - r*0.6f, cy - r*1.8f, 0.0f, r*0.3f, r*1.2f, c1);
            C2D_DrawEllipseSolid(cx + r*0.3f, cy - r*1.8f, 0.0f, r*0.3f, r*1.2f, c1);
            break;
        case Species::Axolotl:
            C2D_DrawCircleSolid(cx - r*0.95f, cy, 0.0f, r*0.2f, c2);
            C2D_DrawCircleSolid(cx + r*0.95f, cy, 0.0f, r*0.2f, c2);
            break;
        default: break;
    }
    C2D_DrawCircleSolid(cx, cy, 0.0f, r, c1);
    C2D_DrawEllipseSolid(cx - r*0.45f, cy - r*0.05f, 0.0f, r*0.9f, r*0.85f, c2);
    C2D_DrawCircleSolid(cx - r*0.3f, cy - r*0.05f, 0.0f, r*0.1f, dark());
    C2D_DrawCircleSolid(cx + r*0.3f, cy - r*0.05f, 0.0f, r*0.1f, dark());
}

} // namespace petrender
} // namespace petpal
#endif // __3DS__
