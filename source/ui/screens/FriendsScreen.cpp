#include "ui/screens/FriendsScreen.h"
#include "ui/UIManager.h"
#include "ui/PetRenderer.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "core/Game.h"
#include "core/Names.h"
#include "audio/Audio.h"

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>

namespace petpal {

FriendsScreen::FriendsScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Friends) {}

void FriendsScreen::onEnter() { selected_ = 0; scroll_ = 0; }

void FriendsScreen::update(float dt, const Input& in) {
    if (flash_ > 0.0f) flash_ -= dt;

    if (in.pressed(KEY_B)) { ui_->goBack(); return; }

    // Manual StreetPass/NetPass check - drains the inbox on demand. Handy for
    // on-device testing and for NetPass (where passes arrive over the internet).
    if (in.pressed(KEY_X)) {
        const int before = game_->friends().size();
        const int got    = game_->processNetPass();
        const int gained = game_->friends().size() - before;
        if (got > 0)
            std::snprintf(flashMsg_, sizeof(flashMsg_),
                          "Got %d pass(es) - %d new friend(s)!", got, gained);
        else
            std::snprintf(flashMsg_, sizeof(flashMsg_), "No new passes right now.");
        flash_ = 3.0f;
        const int n = game_->friends().size();
        if (selected_ >= n) selected_ = n > 0 ? n - 1 : 0;
        return;
    }

#if PETPAL_DEBUG
    // SELECT = inject a synthetic pass through the real meeting pipeline.
    if (in.pressed(KEY_SELECT)) {
        game_->injectTestPass();
        std::snprintf(flashMsg_, sizeof(flashMsg_), "Injected a test pass!");
        flash_ = 3.0f;
        return;
    }
#endif

    const int n = game_->friends().size();
    if (n == 0) return;

    if (in.pressed(KEY_DOWN) && selected_ < n - 1) { ++selected_; audio::playSfx(audio::Sfx::Navigate); }
    if (in.pressed(KEY_UP)   && selected_ > 0)     { --selected_; audio::playSfx(audio::Sfx::Navigate); }

    // Keep selection within the visible window.
    if (selected_ < scroll_) scroll_ = selected_;
    if (selected_ >= scroll_ + kRowsVisible) scroll_ = selected_ - kRowsVisible + 1;
}

void FriendsScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    FriendList& fl = game_->friends();

    char line[80];
    std::snprintf(line, sizeof(line), "Friends  (%d)", fl.size());
    ui_->drawTitleBar(Icon::Friends, line, 0x5BC0DEFF);

    // Live StreetPass status (or the result of a manual check) along the bottom
    // edge. Works the same with or without NetPass - NetPass just fills the same
    // inbox over the internet.
    {
        const StreetPassStatus sp = game_->netpass().streetpassStatus();
        char st[128];
        u32 col = toC2D(kTextMuted);
        float scale = 0.42f;
        if (flash_ > 0.0f) {
            std::snprintf(st, sizeof(st), "%s", flashMsg_);
            col = toC2D(kPrimaryDk);
            scale = 0.36f;   // manual-check result; shrink a touch to fit one line
        } else if (!sp.serviceUp) {
            std::snprintf(st, sizeof(st), "StreetPass: unavailable");
        } else if (!sp.boxReady) {
            std::snprintf(st, sizeof(st), "StreetPass: setting up...");
        } else {
            std::snprintf(st, sizeof(st), "StreetPass: %s  -  %d waiting",
                          sp.scanning ? "on" : "idle", sp.inboxWaiting);
            col = toC2D(kPrimaryDk);
        }
        draw::textCentered(font, buf, st, kTopWidth * 0.5f, 232, scale, col);
    }

    if (fl.empty()) {
        draw::textCentered(font, buf, "No friends yet!", kTopWidth * 0.5f, 108, 0.6f, toC2D(kTextMuted));
        draw::textCentered(font, buf, "StreetPass other players to meet pets.",
                           kTopWidth * 0.5f, 133, 0.45f, toC2D(kTextMuted));
        draw::textCentered(font, buf, "Press X to check for passes now.",
                           kTopWidth * 0.5f, 153, 0.42f, toC2D(kTextMuted));
        return;
    }

    const Friend& f = fl.at(selected_);
    petrender::drawPortrait(f.species(), f.primaryColor(), PetColor::White,
                            kTopWidth * 0.5f, 110.0f, 80.0f);
    draw::textCentered(font, buf, f.name(), kTopWidth * 0.5f, 158, 0.6f, toC2D(kPrimaryDk));
    std::snprintf(line, sizeof(line), "%s  -  Lv.%u  -  %s",
                  speciesName(f.species()), f.level(), evolutionStageName(f.stage()));
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 178, 0.45f, toC2D(kTextMuted));

    // Friendship tier shown as a 5-star rating (Stranger = 1 star ... Legendary = 5),
    // with the tier name beneath it.
    const int filledStars = static_cast<int>(f.friendshipLevel()) + 1;
    ui_->drawStars(filledStars, kFriendshipLevelCount, kTopWidth * 0.5f, 199.0f, 15.0f);
    draw::textCentered(font, buf, friendshipLevelName(f.friendshipLevel()),
                       kTopWidth * 0.5f, 213, 0.42f, toC2D(kText));

    char date[16];
    formatDate(f.dateMet(), date, sizeof(date));
    std::snprintf(line, sizeof(line), "Met %lu time(s)  -  First met %s",
                  (unsigned long)f.meetings(), date);
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 228, 0.4f, toC2D(kTextMuted));
}

void FriendsScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    FriendList& fl = game_->friends();

    const float rowH = 40.0f, top = 8.0f;
    for (int i = 0; i < kRowsVisible; ++i) {
        const int idx = scroll_ + i;
        if (idx >= fl.size()) break;
        const Friend& f = fl.at(idx);
        const float y = top + i * (rowH + 2.0f);
        const bool sel = (idx == selected_);
        draw::card(6, y, kBottomWidth - 12, rowH, 10,
                   toC2D(sel ? kButtonPressed : kButtonFill), toC2D(kButtonShadow));
        petrender::drawPortrait(f.species(), f.primaryColor(), PetColor::White,
                                26, y + rowH * 0.5f, 28.0f);
        draw::textLeft(font, buf, f.name(), 48, y + 6, 0.46f, toC2D(kText));
        draw::textLeft(font, buf, friendshipLevelName(f.friendshipLevel()),
                       48, y + 22, 0.38f, toC2D(kTextMuted));
    }
    float hx = ui_->drawHint(Btn::B, "Back", 8, 222);
    ui_->drawHint(Btn::X, "Check", hx + 16, 222);
}

} // namespace petpal
