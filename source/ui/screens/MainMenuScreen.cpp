#include "ui/screens/MainMenuScreen.h"
#include "ui/UIManager.h"
#include "ui/PetRenderer.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "core/Game.h"
#include "core/Names.h"
#include "audio/Audio.h"

#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <cstdio>

namespace petpal {

namespace {
const char* kLabels[8] = { "Pet", "Friends", "Adventure", "Journal",
                           "Style", "Shop", "Awards", "Settings" };
const Icon kIcons[8] = {
    Icon::Pet, Icon::Friends, Icon::Adventures, Icon::Journal,
    Icon::Customize, Icon::Chest, Icon::Awards, Icon::Settings
};
const ScreenId kTargets[8] = {
    ScreenId::Pet, ScreenId::Friends, ScreenId::Adventures, ScreenId::Journal,
    ScreenId::Customization, ScreenId::Shop, ScreenId::Achievements, ScreenId::Settings
};
// Vibrant, distinct accent per tile (0xRRGGBBAA).
const uint32_t kTileColor[8] = {
    0xFF7FB5FF, // Pet       - coral
    0x5BC0DEFF, // Friends   - sky
    0x6FCF6FFF, // Adventure - mint
    0xF2994AFF, // Journal   - orange
    0x9B59B6FF, // Style     - purple
    0xE0863CFF, // Shop      - amber
    0xF2C14EFF, // Awards    - gold
    0x4FB0C6FF, // Settings  - teal
};

constexpr float kTileW = 66.0f, kTileH = 66.0f;
constexpr float kGap   = 12.0f;
constexpr float kRow1Y = 24.0f;
constexpr float kRowPitch = 90.0f; // tile + label + gap
} // namespace

MainMenuScreen::MainMenuScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::MainMenu) {}

// 4x2 grid: row 1 = tiles 0-3, row 2 = tiles 4-7.
void MainMenuScreen::tileRect(int i, float& x, float& y, float& w, float& h) const {
    w = kTileW; h = kTileH;
    const int col = i % 4, row = i / 4;
    const float rowW = 4 * kTileW + 3 * kGap;
    const float startX = (kBottomWidth - rowW) * 0.5f;
    x = startX + col * (kTileW + kGap);
    y = kRow1Y + row * kRowPitch;
}

void MainMenuScreen::onEnter() { focus_ = 0; }

void MainMenuScreen::update(float dt, const Input& in) {
    // Grid navigation over a clean 4x2 grid.
    const int prevFocus = focus_;
    if (in.pressed(KEY_RIGHT)) focus_ = (focus_ + 1) % kCount;
    if (in.pressed(KEY_LEFT))  focus_ = (focus_ + kCount - 1) % kCount;
    if (in.pressed(KEY_DOWN) && focus_ < 4) focus_ += 4; // drop into row 2
    if (in.pressed(KEY_UP)   && focus_ >= 4) focus_ -= 4; // rise into row 1
    if (focus_ != prevFocus) audio::playSfx(audio::Sfx::Navigate);

    // Decay press bounces.
    for (int i = 0; i < kCount; ++i)
        if (press_[i] > 0.0f) { press_[i] -= dt * 4.0f; if (press_[i] < 0) press_[i] = 0; }

    auto activate = [&](int i) {
        press_[i] = 1.0f;
        ui_->navigateTo(kTargets[i]);
    };

    // Touch.
    for (int i = 0; i < kCount; ++i) {
        float x, y, w, h; tileRect(i, x, y, w, h);
        if (in.tappedRect(x, y, w, h)) { focus_ = i; activate(i); return; }
    }
    // A button on the focused tile.
    if (in.pressed(KEY_A)) activate(focus_);
}

void MainMenuScreen::drawTop() {
    using namespace theme;
    Pet& pet = game_->pet();
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    // Town environment behind the pet, then the pet itself.
    ui_->drawLocationBg(Location::Town);
    ui_->drawPet(pet, kTopWidth * 0.5f, 150.0f);

    // Name + stage + level header card.
    char line[64];
    draw::card(96, 12, 208, 38, 16, toC2D(kBgPanel), toC2D(kButtonShadow));
    std::snprintf(line, sizeof(line), "%s", pet.name());
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 24, 0.62f, toC2D(kText));
    std::snprintf(line, sizeof(line), "%s  -  Lv.%u", evolutionStageName(pet.stage()), pet.level());
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 40, 0.42f, toC2D(kTextMuted));

    // Coins, top-right with a coin icon.
    ui_->drawIcon(Icon::Coin, 350, 24, 22, toC2D(kWarning));
    std::snprintf(line, sizeof(line), "%lu", (unsigned long)game_->inventory().coins());
    draw::textLeft(font, buf, line, 362, 16, 0.55f, toC2D(kText));

    // Stat bars bottom-left with icons.
    ui_->drawIcon(Icon::Happy,  18, 206, 18, toC2D(kBarHappy));
    draw::bar(32, 200, 150, 12, pet.happiness() / 100.0f, toC2D(kBarBack), toC2D(kBarHappy));
    ui_->drawIcon(Icon::Energy, 18, 224, 18, toC2D(kBarEnergy));
    draw::bar(32, 218, 150, 12, pet.energy() / 100.0f, toC2D(kBarBack), toC2D(kBarEnergy));
}

void MainMenuScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    const float t = ui_->anim().time();

    for (int i = 0; i < kCount; ++i) {
        float x, y, w, h; tileRect(i, x, y, w, h);
        const bool sel = (i == focus_);

        // Scale: press bounce (squash) + a gentle hover pop on the focused tile.
        float scale = 1.0f - 0.08f * press_[i];
        if (sel) scale += 0.05f + 0.02f * std::sin(t * 4.0f);

        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        const float tw = w * scale, th = h * scale;
        const float tx = cx - tw * 0.5f, ty = cy - th * 0.5f;

        // Selected tile gets a soft white halo behind it.
        if (sel)
            draw::roundedRect(tx - 4, ty - 4, tw + 8, th + 8, 18, C2D_Color32(255, 255, 255, 180));

        draw::card(tx, ty, tw, th, 14, toC2D(kTileColor[i]), toC2D(0x00000040));

        // White icon glyph centered in the tile.
        ui_->drawIcon(kIcons[i], cx, cy, th * 0.56f, C2D_Color32(255, 255, 255, 255));

        // Label below the tile.
        draw::textCentered(font, buf, kLabels[i], cx, y + h + 9,
                           sel ? 0.44f : 0.40f, toC2D(sel ? kText : kTextMuted));
    }

    draw::textCentered(font, buf, "START: exit", kBottomWidth * 0.5f, 228, 0.40f,
                       toC2D(kTextMuted));
}

} // namespace petpal
