#include "ui/screens/AdventuresScreen.h"
#include "ui/UIManager.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "core/Game.h"
#include "core/Names.h"

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>

namespace petpal {

AdventuresScreen::AdventuresScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Adventures),
      startButton_(110, 190, 100, 40, "Send!"),
      collectButton_(110, 190, 100, 40, "Collect!") {}

void AdventuresScreen::onEnter() { toastTimer_ = 0.0f; }

void AdventuresScreen::update(float dt, const Input& in) {
    if (toastTimer_ > 0.0f) toastTimer_ -= dt;
    if (in.pressed(KEY_B)) { ui_->goBack(); return; }

    Adventure& adv = game_->adventure();
    startButton_.update(dt);
    collectButton_.update(dt);

    if (adv.active()) {
        if (adv.isComplete(nowSeconds())) {
            if (collectButton_.handle(in, true)) {
                AdventureResult r = game_->collectAdventure();
                if (r.valid) {
                    char b[96];
                    std::snprintf(b, sizeof(b), "Got %lu coins + %u item(s)!",
                                  (unsigned long)r.coins, (unsigned)r.items.size());
                    toast_ = b; toastTimer_ = 3.0f;
                }
            }
        }
        return; // can't start a new one while away
    }

    // Choose location (D-pad up/down) and duration (left/right).
    const int nLoc = kLocationCount;
    if (in.pressed(KEY_DOWN)) selectedLocation_ = (selectedLocation_ + 1) % nLoc;
    if (in.pressed(KEY_UP))   selectedLocation_ = (selectedLocation_ + nLoc - 1) % nLoc;
    if (in.pressed(KEY_RIGHT))selectedDuration_ = (selectedDuration_ + 1) % kAdventureDurationCount;
    if (in.pressed(KEY_LEFT)) selectedDuration_ = (selectedDuration_ + kAdventureDurationCount - 1) % kAdventureDurationCount;

    // Skip locked locations when navigating.
    if (!game_->world().isUnlocked(static_cast<Location>(selectedLocation_))) {
        for (int i = 0; i < nLoc; ++i) {
            int idx = (selectedLocation_ + i) % nLoc;
            if (game_->world().isUnlocked(static_cast<Location>(idx))) { selectedLocation_ = idx; break; }
        }
    }

    if (startButton_.handle(in, true)) {
        const Location loc = static_cast<Location>(selectedLocation_);
        const AdventureDuration dur = static_cast<AdventureDuration>(selectedDuration_);
        if (game_->startAdventure(loc, dur)) {
            toast_ = "Off they go on an adventure!"; toastTimer_ = 2.5f;
        } else {
            toast_ = "Too tired - feed your pet first!"; toastTimer_ = 2.5f;
        }
    }
}

void AdventuresScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    Adventure& adv = game_->adventure();

    // Show the destination's environment behind everything, if we have its art.
    ui_->drawLocationBg(adv.active() ? adv.location()
                                     : static_cast<Location>(selectedLocation_));
    ui_->drawTitleBar(Icon::Adventures, "Adventures", 0x6FCF6FFF);

    if (adv.active()) {
        const uint32_t rem = adv.secondsRemaining(nowSeconds());
        char line[80];
        std::snprintf(line, sizeof(line), "%s is exploring the %s",
                      game_->pet().name(), locationName(adv.location()));
        draw::textCentered(font, buf, line, kTopWidth * 0.5f, 90, 0.5f, toC2D(kPrimaryDk));

        draw::bar(60, 120, kTopWidth - 120, 18, adv.progress(nowSeconds()),
                  toC2D(kBarBack), toC2D(kSecondary));

        if (rem == 0) {
            draw::textCentered(font, buf, "They're back! Tap Collect.", kTopWidth * 0.5f, 160, 0.55f, toC2D(kSuccess));
        } else {
            std::snprintf(line, sizeof(line), "Back in %lu:%02lu",
                          (unsigned long)(rem / 60), (unsigned long)(rem % 60));
            draw::textCentered(font, buf, line, kTopWidth * 0.5f, 160, 0.55f, toC2D(kText));
        }
    } else {
        char line[80];
        std::snprintf(line, sizeof(line), "Where to? %s",
                      locationName(static_cast<Location>(selectedLocation_)));
        draw::textCentered(font, buf, line, kTopWidth * 0.5f, 100, 0.6f, toC2D(kPrimaryDk));
        std::snprintf(line, sizeof(line), "Duration: %s",
                      adventureDurationName(static_cast<AdventureDuration>(selectedDuration_)));
        draw::textCentered(font, buf, line, kTopWidth * 0.5f, 130, 0.55f, toC2D(kText));
        draw::textCentered(font, buf, "Up/Down: place   Left/Right: time",
                           kTopWidth * 0.5f, 165, 0.42f, toC2D(kTextMuted));
    }
}

void AdventuresScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    Adventure& adv = game_->adventure();

    // List unlocked locations so the player sees their world growing.
    draw::textLeft(font, buf, "Unlocked places:", 10, 10, 0.5f, toC2D(kText));
    int shown = 0;
    for (int i = 0; i < kLocationCount && shown < 8; ++i) {
        const Location loc = static_cast<Location>(i);
        if (!game_->world().isUnlocked(loc)) continue;
        const float y = 32.0f + shown * 18.0f;
        const bool sel = (i == selectedLocation_ && !adv.active());
        draw::textLeft(font, buf, locationName(loc), 20, y, 0.45f,
                       toC2D(sel ? kPrimaryDk : kTextMuted));
        ++shown;
    }

    if (adv.active() && adv.isComplete(nowSeconds()))
        collectButton_.draw(font, buf, true);
    else if (!adv.active())
        startButton_.draw(font, buf, true);

    if (toastTimer_ > 0.0f) {
        draw::roundedRect(20, 200, kBottomWidth - 40, 28, 10, toC2D(0x2C3E50EE));
        draw::textCentered(font, buf, toast_.c_str(), kBottomWidth * 0.5f, 214, 0.44f, toC2D(kTextLight));
    } else {
        ui_->drawHint(Btn::B, "Back", 10, 222);
    }
}

} // namespace petpal
