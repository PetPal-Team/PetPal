// =============================================================================
//  PetPal - UIManager.cpp
//  citro2d/citro3d host for the screen stack. Device-specific calls are guarded
//  with __3DS__ so the header stays portable; on a host build the methods are
//  inert (a host test harness drives the model layer, not the UI).
// =============================================================================
#include "ui/UIManager.h"
#include "ui/Theme.h"
#include "ui/Gfx.h"
#include "ui/Widgets.h"
#include "ui/PetRenderer.h"
#include "core/Game.h"
#include "audio/Audio.h"
#include <cmath>

#include "ui/screens/OnboardingScreen.h"
#include "ui/screens/MainMenuScreen.h"
#include "ui/screens/PetScreen.h"
#include "ui/screens/FriendsScreen.h"
#include "ui/screens/AdventuresScreen.h"
#include "ui/screens/JournalScreen.h"
#include "ui/screens/CustomizationScreen.h"
#include "ui/screens/AchievementsScreen.h"
#include "ui/screens/ShopScreen.h"
#include "ui/screens/SettingsScreen.h"

#ifdef __3DS__
#include <3ds.h>
#include <citro2d.h>
#endif

namespace petpal {

#ifdef __3DS__
// -----------------------------------------------------------------------------
//  Confetti burst for celebrations (a single UIManager instance, so file-scope
//  storage is fine). Spawned by celebrate(), drawn on top of the top screen.
// -----------------------------------------------------------------------------
namespace {
struct Particle { float x, y, vx, vy, life, maxLife, size; u32 color; };
constexpr int kMaxParticles = 64;
Particle g_parts[kMaxParticles];
int      g_partCount = 0;
uint32_t g_rngState  = 0x1234abcd;

float frand() { // 0..1
    g_rngState = g_rngState * 1664525u + 1013904223u;
    return ((g_rngState >> 8) & 0xFFFFFF) / static_cast<float>(0x1000000);
}

void spawnConfetti(float cx, float cy, u32 accentRGBA) {
    static const u32 palette[6] = {
        0xFF5FA8FF, 0x5FE0FFFF, 0x6FCF6FFF, 0xF2C14EFF, 0xB89CFFFF, 0xFF8A4CFF,
    };
    const int n = 26;
    for (int i = 0; i < n && g_partCount < kMaxParticles; ++i) {
        Particle& p = g_parts[g_partCount++];
        const float ang = frand() * 6.2831853f;
        const float spd = 60.0f + frand() * 150.0f;
        p.x = cx + (frand() - 0.5f) * 30.0f;
        p.y = cy + (frand() - 0.5f) * 10.0f;
        p.vx = std::cos(ang) * spd;
        p.vy = std::sin(ang) * spd - 120.0f; // bias upward
        p.maxLife = p.life = 0.9f + frand() * 0.6f;
        p.size = 3.0f + frand() * 3.0f;
        p.color = (frand() < 0.3f) ? accentRGBA : palette[(int)(frand() * 6) % 6];
    }
}

void updateParticles(float dt) {
    int w = 0;
    for (int i = 0; i < g_partCount; ++i) {
        Particle& p = g_parts[i];
        p.life -= dt;
        if (p.life <= 0.0f) continue;
        p.vy += 340.0f * dt;            // gravity
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        g_parts[w++] = p;
    }
    g_partCount = w;
}

void drawParticles() {
    for (int i = 0; i < g_partCount; ++i) {
        const Particle& p = g_parts[i];
        const float a = p.life / p.maxLife;
        const u8 alpha = static_cast<u8>(255 * (a > 1 ? 1 : a));
        const u32 c = (toC2D(p.color) & 0x00FFFFFF) | (static_cast<u32>(alpha) << 24);
        C2D_DrawCircleSolid(p.x, p.y, 0.0f, p.size * (0.5f + 0.5f * a), c);
    }
}
} // namespace
#endif

UIManager::UIManager(Game* game) : game_(game) {
    screens_.resize(static_cast<int>(ScreenId::Count));
}

UIManager::~UIManager() { shutdown(); }

bool UIManager::init() {
    if (started_) return true;
#ifdef __3DS__
    gfxInitDefault();
    romfsInit(); // for optional asset packs; harmless if no romfs

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) return false;
    C2D_Prepare();

    top_    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom_ = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    dynBuf_ = C2D_TextBufNew(8192);
    font_   = nullptr; // nullptr == system font in C2D_TextFontParse

    // Icon + button-glyph atlas (gfx/sprites.t3s). May be absent (icons simply
    // won't draw), so this load can fail safely.
    sprites_ = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
#endif

    // Build every screen up front (cheap, keeps navigation instant).
    screens_[(int)ScreenId::Onboarding]    .reset(new OnboardingScreen(game_, this));
    screens_[(int)ScreenId::MainMenu]      .reset(new MainMenuScreen(game_, this));
    screens_[(int)ScreenId::Pet]           .reset(new PetScreen(game_, this));
    screens_[(int)ScreenId::Friends]       .reset(new FriendsScreen(game_, this));
    screens_[(int)ScreenId::Adventures]    .reset(new AdventuresScreen(game_, this));
    screens_[(int)ScreenId::Journal]       .reset(new JournalScreen(game_, this));
    screens_[(int)ScreenId::Customization] .reset(new CustomizationScreen(game_, this));
    screens_[(int)ScreenId::Achievements]  .reset(new AchievementsScreen(game_, this));
    screens_[(int)ScreenId::Shop]          .reset(new ShopScreen(game_, this));
    screens_[(int)ScreenId::Settings]      .reset(new SettingsScreen(game_, this));

    // Start on onboarding for new players, otherwise the hub.
    const ScreenId start = game_->onboardingComplete() ? ScreenId::MainMenu
                                                       : ScreenId::Onboarding;
    navStack_.push_back(start);
    if (screens_[(int)start]) screens_[(int)start]->onEnter();

    started_ = true;
    return true;
}

void UIManager::shutdown() {
    if (!started_) return;
    screens_.clear();
    navStack_.clear();
#ifdef __3DS__
    if (sprites_) { C2D_SpriteSheetFree(sprites_); sprites_ = nullptr; }
    if (dynBuf_)  { C2D_TextBufDelete(dynBuf_); dynBuf_ = nullptr; }
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
#endif
    started_ = false;
}

Screen* UIManager::current() {
    if (navStack_.empty()) return nullptr;
    return screens_[(int)navStack_.back()].get();
}

ScreenId UIManager::currentId() const {
    return navStack_.empty() ? ScreenId::Count : navStack_.back();
}

void UIManager::navigateTo(ScreenId id) {
    if (id == ScreenId::Count) return;
    if (Screen* c = current()) c->onExit();
    navStack_.push_back(id);
    if (Screen* n = current()) n->onEnter();
    transitionDir_ = 1;                                        // push: slide from right
    anim_.play("nav", 0.0f, 1.0f, 0.24f, easing::easeOutQuad);
    audio::playSfx(audio::Sfx::Select);
}

void UIManager::goBack() {
    if (navStack_.size() <= 1) return; // never pop the root
    if (Screen* c = current()) c->onExit();
    navStack_.pop_back();
    if (Screen* n = current()) n->onEnter();
    transitionDir_ = -1;                                       // pop: slide from left
    anim_.play("nav", 0.0f, 1.0f, 0.2f, easing::easeOutQuad);
    audio::playSfx(audio::Sfx::Back);
}

void UIManager::celebrate(Celebration kind) {
    anim_.play("celebrate", 1.0f, 0.0f, 0.8f, easing::easeOutQuad);

    uint32_t accent = 0xF2C14EFF; // gold
    switch (kind) {
        case Celebration::LevelUp:     audio::playSfx(audio::Sfx::LevelUp);   accent = 0xF2C14EFF; break;
        case Celebration::Evolution:   audio::playSfx(audio::Sfx::LevelUp);   accent = 0xB89CFFFF; break;
        case Celebration::NewFriend:   audio::playSfx(audio::Sfx::NewFriend); accent = 0x5FE0FFFF; break;
        case Celebration::Achievement: audio::playSfx(audio::Sfx::Coin);      accent = 0xF2C14EFF; break;
        case Celebration::Reward:      audio::playSfx(audio::Sfx::Coin);      accent = 0x6FCF6FFF; break;
    }
#ifdef __3DS__
    spawnConfetti(kTopWidth * 0.5f, 70.0f, accent);
#endif
}

void UIManager::scanInput() {
#ifdef __3DS__
    hidScanInput();
    input_.down = hidKeysDown();
    input_.held = hidKeysHeld();
    input_.up   = hidKeysUp();

    const bool wasActive = input_.touch.active;
    touchPosition tp;
    hidTouchRead(&tp);
    const bool nowActive = (input_.held & KEY_TOUCH) != 0;
    input_.touch.x = tp.px;
    input_.touch.y = tp.py;
    input_.touch.active = nowActive;
    input_.touchStart = nowActive && !wasActive;
    input_.touchEnd   = !nowActive && wasActive;
#endif
}

bool UIManager::tick(float dt) {
    if (!started_) return false;
    scanInput();

#ifdef __3DS__
    // START quits from the hub / onboarding.
    if (input_.pressed(KEY_START) &&
        (currentId() == ScreenId::MainMenu || currentId() == ScreenId::Onboarding))
        return false;
#endif

    if (Screen* c = current()) c->update(dt, input_);
    anim_.update(dt);

#ifdef __3DS__
    updateParticles(dt);

    // Screen-slide transition: the incoming screen's content slides in from the
    // side (right on push, left on pop) over a static background. `nav` goes
    // 0 -> 1; at 1 the offset is 0 (settled).
    const float navT  = anim_.valueOf("nav", 1.0f);
    const float slide = (1.0f - navT);
    const bool  sliding = slide > 0.002f;

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(top_, toC2D(theme::kBgTop));
    C2D_SceneBegin(top_);
    C2D_ViewReset();
    drawBackground();
    if (sliding) C2D_ViewTranslate(slide * kTopWidth * transitionDir_, 0.0f);
    if (Screen* c = current()) c->drawTop();
    C2D_ViewReset();
    drawParticles();

    C2D_TargetClear(bottom_, toC2D(theme::kBgBottom));
    C2D_SceneBegin(bottom_);
    C2D_ViewReset();
    if (sliding) C2D_ViewTranslate(slide * kBottomWidth * transitionDir_, 0.0f);
    if (Screen* c = current()) c->drawBottom();
    C2D_ViewReset();

    C3D_FrameEnd(0);
    C2D_TextBufClear(dynBuf_);
#endif
    return true;
}

#ifdef __3DS__
void UIManager::drawIcon(Icon id, float cx, float cy, float size, u32 tint) {
    if (!sprites_) return;
    const int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(C2D_SpriteSheetCount(sprites_))) return;

    C2D_Image img = C2D_SpriteSheetGetImage(sprites_, idx);
    if (!img.tex || !img.subtex) return;

    const float sx = size / img.subtex->width;
    const float sy = size / img.subtex->height;

    C2D_ImageTint t;
    C2D_PlainImageTint(&t, tint, 1.0f); // blend=1 -> icon takes the tint color
    C2D_DrawImageAt(img, cx - size * 0.5f, cy - size * 0.5f, 0.5f, &t, sx, sy);
}

void UIManager::drawTitleBar(Icon icon, const char* title, uint32_t accentRGBA) {
    const float w = 300.0f, h = 34.0f;
    const float x = (kTopWidth - w) * 0.5f, y = 8.0f;
    draw::card(x, y, w, h, 16.0f, toC2D(accentRGBA), toC2D(theme::kButtonShadow));
    drawIcon(icon, x + 26.0f, y + h * 0.5f, 22.0f, C2D_Color32(255, 255, 255, 255));
    draw::textCentered(font_, dynBuf_, title, kTopWidth * 0.5f + 12.0f, y + h * 0.5f,
                       0.58f, C2D_Color32(255, 255, 255, 255));
}

void UIManager::drawPet(const Pet& pet, float cx, float cy, float scale) {
    // Mood from current stats: happy when content, sad when neglected.
    PetMood mood = PetMood::Idle;
    if (pet.happiness() >= 70)                              mood = PetMood::Happy;
    else if (pet.happiness() <= 30 || pet.energy() <= 20)   mood = PetMood::Sad;
    petrender::draw(pet, cx, cy, anim_, scale, mood);
}

float UIManager::drawButtonGlyph(Btn b, float x, float cy, float height) {
    if (!sprites_) return 0.0f;
    const int idx = static_cast<int>(b);
    if (idx < 0 || idx >= static_cast<int>(C2D_SpriteSheetCount(sprites_))) return 0.0f;
    C2D_Image img = C2D_SpriteSheetGetImage(sprites_, idx);
    if (!img.tex || !img.subtex) return 0.0f;
    const float aspect = static_cast<float>(img.subtex->width) / img.subtex->height;
    const float w = height * aspect;
    const float s = height / img.subtex->height;
    C2D_DrawImageAt(img, x, cy - height * 0.5f, 0.5f, nullptr, s, s); // untinted
    return w;
}

float UIManager::drawHint(Btn b, const char* label, float x, float y) {
    const float h = 16.0f;
    const float w = drawButtonGlyph(b, x, y + h * 0.5f, h);
    const float tx = x + w + 4.0f;
    C2D_Text txt;
    C2D_TextFontParse(&txt, font_, dynBuf_, label);
    C2D_TextOptimize(&txt);
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&txt, 0.40f, 0.40f, &tw, &th);
    C2D_DrawText(&txt, C2D_WithColor, tx, y + 2.0f, 0.5f, 0.40f, 0.40f, toC2D(theme::kTextMuted));
    return tx + tw + 12.0f; // next hint x
}

void UIManager::drawSpinner(float cx, float cy, float radius, uint32_t colorRGBA) {
    const u32 base = toC2D(colorRGBA);
    const int N = 8;
    const float t = anim_.time();
    const int head = static_cast<int>(t * 8.0f) % N;
    for (int i = 0; i < N; ++i) {
        const float ang = static_cast<float>(i) / N * 6.2831853f - 1.5708f;
        const float dx = cx + std::cos(ang) * radius;
        const float dy = cy + std::sin(ang) * radius;
        const int behind = (head - i + N) % N;
        const float a = 1.0f - static_cast<float>(behind) / N;
        const u8 alpha = static_cast<u8>(40 + 215 * a);
        const u32 c = (base & 0x00FFFFFF) | (static_cast<u32>(alpha) << 24); // citro = 0xAABBGGRR
        C2D_DrawCircleSolid(dx, dy, 0.0f, radius * 0.20f, c);
    }
}

// -----------------------------------------------------------------------------
//  Simple procedural location backgrounds (sky + ground + a few shapes).
// -----------------------------------------------------------------------------
namespace {
inline u32 col(uint32_t rgba) { return toC2D(rgba); }
inline void tri_(float ax, float ay, float bx, float by, float cx, float cy, u32 c) {
    C2D_DrawTriangle(ax, ay, c, bx, by, c, cx, cy, c, 0.0f);
}
void bgSky(u32 top, u32 bottom) {
    C2D_DrawRectangle(0, 0, 0.0f, kTopWidth, kScreenHeight, top, top, bottom, bottom);
}
void bgGround(float y, u32 c) { C2D_DrawRectSolid(0, y, 0.0f, kTopWidth, kScreenHeight - y, c); }
} // namespace

void UIManager::drawLocationBg(Location loc) {
    const float G = 200.0f; // ground line
    switch (loc) {
    case Location::Town:
        bgSky(0x9FD8F0FF, 0xD8F0FAFF);
        for (int i = 0; i < 6; ++i) {                       // buildings
            float bw = 40 + (i * 13 % 30), bh = 60 + (i * 37 % 70);
            C2D_DrawRectSolid(10 + i * 66, G - bh, 0.0f, bw, bh, col(0x6E8CA8FF));
        }
        bgGround(G, col(0x7DBE6BFF));                        // grass
        C2D_DrawRectSolid(0, G + 18, 0.0f, kTopWidth, 6, col(0x8A6B4AFF));
        break;
    case Location::Forest:
    case Location::Garden:
        bgSky(0xBFE3A8FF, 0xE6F2D0FF);
        for (int i = 0; i < 5; ++i) {                       // trees
            float tx = 30 + i * 90;
            C2D_DrawRectSolid(tx - 7, G - 70, 0.0f, 14, 70, col(0x7A5230FF)); // trunk
            C2D_DrawCircleSolid(tx, G - 80, 0.0f, 34, col(0x4Fae5cFF));       // canopy
        }
        bgGround(G, col(0x6FB45AFF));
        break;
    case Location::Beach:
        bgSky(0x7FD0E8FF, 0xCFF0F7FF);
        C2D_DrawCircleSolid(330, 50, 0.0f, 26, col(0xFFE27AFF));            // sun
        C2D_DrawRectSolid(0, G - 30, 0.0f, kTopWidth, 30, col(0x3FB0CFFF)); // sea
        bgGround(G, col(0xF2DDA0FF));                                       // sand
        break;
    case Location::SpaceStation:
        bgSky(0x1B1740FF, 0x3A2C66FF);
        for (int i = 0; i < 14; ++i)
            C2D_DrawCircleSolid((i * 53 % 390) + 5, (i * 31 % 180) + 10, 0.0f, 1.6f, col(0xFFFFFFFF));
        C2D_DrawCircleSolid(320, 60, 0.0f, 28, col(0xC9A0FFFF));            // planet
        bgGround(G + 10, col(0x4A4A66FF));
        break;
    case Location::Castle:
        bgSky(0xB7C8E8FF, 0xE3ECF7FF);
        C2D_DrawRectSolid(120, G - 110, 0.0f, 160, 110, col(0x9AA6B8FF));   // keep
        for (int i = 0; i < 4; ++i)                                         // battlements
            C2D_DrawRectSolid(122 + i * 42, G - 122, 0.0f, 22, 14, col(0x9AA6B8FF));
        C2D_DrawRectSolid(185, G - 60, 0.0f, 30, 60, col(0x5C4A3AFF));      // door
        bgGround(G, col(0x7DBE6BFF));
        break;
    case Location::Mountain:
        bgSky(0xAFE0F0FF, 0xE6F4FAFF);
        tri_(60, G, 170, G - 150, 280, G, col(0x8A93A6FF));
        tri_(220, G, 320, G - 120, 400, G, col(0x9AA3B6FF));
        tri_(140, G - 80, 170, G - 150, 200, G - 80, col(0xFFFFFFFF));      // snow cap
        bgGround(G, col(0x7AA85AFF));
        break;
    case Location::HauntedManor:
        bgSky(0x2A2440FF, 0x4A3A5AFF);
        C2D_DrawCircleSolid(330, 50, 0.0f, 22, col(0xF2EFC0FF));            // moon
        C2D_DrawRectSolid(120, G - 100, 0.0f, 160, 100, col(0x3A3350FF));   // manor
        tri_(110, G - 100, 200, G - 150, 290, G - 100, col(0x2A2440FF));    // roof
        C2D_DrawRectSolid(185, G - 45, 0.0f, 30, 45, col(0x1A1730FF));      // door
        bgGround(G, col(0x3A4A3AFF));
        break;
    case Location::CrystalCave:
        bgSky(0x241A3AFF, 0x3A2A55FF);
        for (int i = 0; i < 5; ++i)                                         // crystals
            tri_(40 + i * 80, G, 60 + i * 80, G - (60 + i * 12), 80 + i * 80, G,
                 col(i % 2 ? 0x7FE0D0FF : 0xB89CFFFF));
        bgGround(G, col(0x2E2440FF));
        break;
    case Location::Arcade:
        bgSky(0x2A1740FF, 0x55307AFF);
        for (int i = 0; i < 8; ++i)                                         // neon lights
            C2D_DrawCircleSolid(25 + i * 50, 40 + (i % 3) * 20, 0.0f, 8,
                                col(i % 2 ? 0xFF5FA8FF : 0x5FE0FFFF));
        bgGround(G, col(0x3A2A55FF));
        break;
    default:
        // Other locations keep the pastel gradient from drawBackground().
        break;
    }
}

void UIManager::drawBackground() {
    // Vertical pastel gradient using a single gradient quad.
    C2D_DrawRectangle(0, 0, 0.0f, kTopWidth, kScreenHeight,
                      toC2D(theme::kBgTop), toC2D(theme::kBgTop),
                      toC2D(theme::kBgBottom), toC2D(theme::kBgBottom));
}
#endif

} // namespace petpal
