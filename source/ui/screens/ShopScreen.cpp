#include "ui/screens/ShopScreen.h"
#include "ui/UIManager.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "core/Game.h"
#include "core/Names.h"
#include "audio/Audio.h"

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>

namespace petpal {

namespace {
struct ShopEntry { ItemId id; uint32_t price; };
const ShopEntry kShop[] = {
    { ItemId::Apple,          5 },
    { ItemId::Berry,          6 },
    { ItemId::Carrot,         7 },
    { ItemId::Cookie,        10 },
    { ItemId::Fish,          14 },
    { ItemId::IceCream,      18 },
    { ItemId::Cake,          25 },
    { ItemId::RainbowCookie,  60 },
    { ItemId::GoldenApple,   120 },
    { ItemId::CosmicCandy,   180 },
    { ItemId::EvoShardCommon,250 },
    { ItemId::EvoShardRare,  600 },
};
constexpr int kShopCount = static_cast<int>(sizeof(kShop) / sizeof(kShop[0]));

Icon iconFor(ItemId id) {
    switch (itemCategory(id)) {
        case ItemCategory::Food:     return Icon::Apple;
        case ItemCategory::RareFood: return Icon::Star;
        default:                     return Icon::Chest;
    }
}
} // namespace

ShopScreen::ShopScreen(Game* game, UIManager* ui) : Screen(game, ui, ScreenId::Shop) {}

void ShopScreen::onEnter() { selected_ = 0; scroll_ = 0; flash_ = 0.0f; }

void ShopScreen::update(float dt, const Input& in) {
    if (flash_ > 0.0f)     flash_ -= dt;
    if (buyPulse_ > 0.0f)  buyPulse_ -= dt * 3.0f;

    if (in.pressed(KEY_B)) { ui_->goBack(); return; }

    if (in.pressed(KEY_DOWN) && selected_ < kShopCount - 1) { ++selected_; audio::playSfx(audio::Sfx::Navigate); }
    if (in.pressed(KEY_UP)   && selected_ > 0)              { --selected_; audio::playSfx(audio::Sfx::Navigate); }
    if (selected_ < scroll_) scroll_ = selected_;
    if (selected_ >= scroll_ + kRowsVisible) scroll_ = selected_ - kRowsVisible + 1;

    if (in.pressed(KEY_A)) {
        const ShopEntry& e = kShop[selected_];
        if (game_->buyItem(e.id, e.price, 1)) {
            audio::playSfx(audio::Sfx::Coin);
            std::snprintf(flashMsg_, sizeof(flashMsg_), "Bought %s!", itemName(e.id));
            flash_ = 1.6f; buyPulse_ = 1.0f;
        } else {
            audio::playSfx(audio::Sfx::Error);
            std::snprintf(flashMsg_, sizeof(flashMsg_), "Not enough coins!");
            flash_ = 1.6f;
        }
    }
}

void ShopScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    ui_->drawTitleBar(Icon::Chest, "Shop", 0xF2C14EFF);

    const ShopEntry& e = kShop[selected_];
    const float pop = 1.0f + 0.15f * (buyPulse_ > 0.0f ? buyPulse_ : 0.0f);
    ui_->drawIcon(iconFor(e.id), kTopWidth * 0.5f, 96, 64 * pop, toC2D(kPrimaryDk));

    draw::textCentered(font, buf, itemName(e.id), kTopWidth * 0.5f, 140, 0.68f, toC2D(kPrimaryDk));
    draw::textCentered(font, buf, itemDescription(e.id), kTopWidth * 0.5f, 164, 0.44f, toC2D(kTextMuted));

    char line[72];
    std::snprintf(line, sizeof(line), "Price %lu     Owned %u",
                  (unsigned long)e.price, game_->inventory().count(e.id));
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 190, 0.5f, toC2D(kText));

    // Coin balance, top-right.
    ui_->drawIcon(Icon::Coin, 350, 24, 22, toC2D(kWarning));
    std::snprintf(line, sizeof(line), "%lu", (unsigned long)game_->inventory().coins());
    draw::textLeft(font, buf, line, 362, 16, 0.55f, toC2D(kText));

    if (flash_ > 0.0f)
        draw::textCentered(font, buf, flashMsg_, kTopWidth * 0.5f, 218, 0.52f, toC2D(kPrimaryDk));
}

void ShopScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    const float rowH = 40.0f, top = 8.0f;
    for (int i = 0; i < kRowsVisible; ++i) {
        const int idx = scroll_ + i;
        if (idx >= kShopCount) break;
        const ShopEntry& e = kShop[idx];
        const float y = top + i * (rowH + 2.0f);
        const bool sel = (idx == selected_);
        const bool afford = game_->inventory().coins() >= e.price;

        draw::card(6, y, kBottomWidth - 12, rowH, 10,
                   toC2D(sel ? kButtonPressed : kButtonFill), toC2D(kButtonShadow));
        ui_->drawIcon(iconFor(e.id), 26, y + rowH * 0.5f, 24, toC2D(sel ? kPrimaryDk : kTextMuted));
        draw::textLeft(font, buf, itemName(e.id), 48, y + 12, 0.46f, toC2D(kText));

        char pr[16];
        std::snprintf(pr, sizeof(pr), "%lu", (unsigned long)e.price);
        ui_->drawIcon(Icon::Coin, kBottomWidth - 66, y + rowH * 0.5f, 18,
                      toC2D(afford ? kWarning : 0x9A9A9AFF));
        draw::textLeft(font, buf, pr, kBottomWidth - 52, y + 12, 0.44f,
                       toC2D(afford ? kText : kTextMuted));
    }

    float hx = ui_->drawHint(Btn::A, "Buy", 8, 222);
    ui_->drawHint(Btn::B, "Back", hx + 16, 222);
}

} // namespace petpal
