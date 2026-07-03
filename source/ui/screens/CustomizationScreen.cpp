#include "ui/screens/CustomizationScreen.h"
#include "ui/UIManager.h"
#include "ui/PetRenderer.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "core/Game.h"
#include "core/Names.h"

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>

namespace petpal {

CustomizationScreen::CustomizationScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Customization) {}

void CustomizationScreen::onEnter() { tab_ = 0; cursor_ = 0; }

int CustomizationScreen::optionCount() const {
    switch (tab_) {
        case PrimaryColor:
        case SecondaryColor: return kColorCount;
        case AccessoryTab:   return kAccessoryCount; // includes None
        case StyleTab:       return kStyleCount;
    }
    return 0;
}

void CustomizationScreen::applyCurrent() {
    Pet& pet = game_->pet();
    switch (tab_) {
        case PrimaryColor:
            pet.setColors(static_cast<PetColor>(cursor_), pet.secondaryColor(), pet.accentColor());
            break;
        case SecondaryColor:
            pet.setColors(pet.primaryColor(), static_cast<PetColor>(cursor_), pet.accentColor());
            break;
        case AccessoryTab:
            pet.equipAccessory(static_cast<Accessory>(cursor_)); // no-op if locked
            break;
        case StyleTab:
            pet.equipStyle(static_cast<Style>(cursor_)); // no-op if locked
            break;
    }
    game_->publishSelf();   // appearance changed -> refresh StreetPass outbox
    game_->requestSave();
}

void CustomizationScreen::update(float, const Input& in) {
    if (in.pressed(KEY_B)) { ui_->goBack(); return; }

    if (in.pressed(KEY_R)) { tab_ = (tab_ + 1) % TabCount; cursor_ = 0; }
    if (in.pressed(KEY_L)) { tab_ = (tab_ + TabCount - 1) % TabCount; cursor_ = 0; }

    const int n = optionCount();
    const int cols = 5;
    if (in.pressed(KEY_RIGHT) && cursor_ + 1 < n) ++cursor_;
    if (in.pressed(KEY_LEFT)  && cursor_ > 0)     --cursor_;
    if (in.pressed(KEY_DOWN)  && cursor_ + cols < n) cursor_ += cols;
    if (in.pressed(KEY_UP)    && cursor_ - cols >= 0) cursor_ -= cols;

    if (in.pressed(KEY_A)) applyCurrent();
}

void CustomizationScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    ui_->drawTitleBar(Icon::Customize, "Dress Up", 0x9B59B6FF);
    ui_->drawPet(game_->pet(), kTopWidth * 0.5f, 135.0f);

    static const char* kTabNames[TabCount] = {
        "Color 1", "Color 2", "Accessory", "Style"
    };
    draw::textCentered(font, buf, kTabNames[tab_], kTopWidth * 0.5f, 215, 0.55f, toC2D(kPrimaryDk));
    float hx = 96.0f;
    hx = ui_->drawHint(Btn::L, "",         hx, 226);
    hx = ui_->drawHint(Btn::R, "Category", hx, 226);
    hx = ui_->drawHint(Btn::A, "Apply",    hx + 6.0f, 226);
}

void CustomizationScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    Pet& pet = game_->pet();

    const int n = optionCount();
    const int cols = 5;
    const float cell = 56.0f, gx = 6.0f, gy = 6.0f, left = 12.0f, top = 12.0f;

    for (int i = 0; i < n; ++i) {
        const int col = i % cols, row = i / cols;
        const float x = left + col * (cell + gx);
        const float y = top + row * (cell + gy);
        const bool sel = (i == cursor_);

        bool locked = false;
        u32 swatch = toC2D(kButtonFill);
        const char* label = "";
        if (tab_ <= SecondaryColor) {
            swatch = paletteColor(static_cast<PetColor>(i));
            label  = colorName(static_cast<PetColor>(i));
        } else if (tab_ == AccessoryTab) {
            locked = !pet.isAccessoryUnlocked(static_cast<Accessory>(i));
            label  = accessoryName(static_cast<Accessory>(i));
        } else {
            locked = !pet.isStyleUnlocked(static_cast<Style>(i));
            label  = styleName(static_cast<Style>(i));
        }

        draw::card(x, y, cell, cell - 16, 8,
                   tab_ <= SecondaryColor ? swatch : toC2D(locked ? 0xCCCCCCFF : kButtonFill),
                   toC2D(kButtonShadow));
        if (locked)
            draw::textCentered(font, buf, "LOCK", x + cell * 0.5f, y + (cell - 16) * 0.5f, 0.4f, toC2D(kTextMuted));

        if (sel)
            draw::roundedRect(x - 2, y - 2, cell + 4, 3, 2, toC2D(kPrimary));

        draw::textCentered(font, buf, label, x + cell * 0.5f, y + cell - 8, 0.32f, toC2D(kText));
    }

    ui_->drawHint(Btn::B, "Back", 8, 222);
}

} // namespace petpal
