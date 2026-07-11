#include "ui/screens/SettingsScreen.h"
#include "ui/UIManager.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "core/Game.h"
#include "audio/Audio.h"

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>

namespace petpal {

SettingsScreen::SettingsScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Settings) {}

void SettingsScreen::onEnter() { cursor_ = 0; toastTimer_ = 0.0f; }

void SettingsScreen::update(float dt, const Input& in) {
    if (toastTimer_ > 0.0f) toastTimer_ -= dt;

    // While a code is being checked, the worker thread does the network call;
    // we just poll for the result and ignore input (spinner keeps animating).
    if (redeeming_) {
        RedeemResult r;
        if (redeemTask_.collect(r)) {
            redeeming_ = false;
            game_->applyRedeem(r);
            toast_ = r.message.empty() ? (r.ok ? "Redeemed!" : "Code not accepted.")
                                       : r.message;
            toastTimer_ = 4.0f;
        }
        return;
    }

    if (in.pressed(KEY_B)) { game_->requestSave(); ui_->goBack(); return; }

    if (in.pressed(KEY_DOWN) && cursor_ + 1 < RowCount) { ++cursor_; audio::playSfx(audio::Sfx::Navigate); }
    if (in.pressed(KEY_UP)   && cursor_ > 0)            { --cursor_; audio::playSfx(audio::Sfx::Navigate); }

    Settings& s = game_->settings();
    auto adjust = [&](uint8_t& v, int delta) {
        int nv = static_cast<int>(v) + delta;
        if (nv < 0) nv = 0;
        if (nv > 100) nv = 100;
        v = static_cast<uint8_t>(nv);
    };

    switch (cursor_) {
        case MusicVol:
            if (in.pressed(KEY_RIGHT)) adjust(s.musicVolume, +10);
            if (in.pressed(KEY_LEFT))  adjust(s.musicVolume, -10);
            if (in.pressed(KEY_RIGHT) || in.pressed(KEY_LEFT))
                audio::setMusicVolume(s.musicVolume / 100.0f);
            break;
        case SfxVol:
            if (in.pressed(KEY_RIGHT)) adjust(s.sfxVolume, +10);
            if (in.pressed(KEY_LEFT))  adjust(s.sfxVolume, -10);
            if (in.pressed(KEY_RIGHT) || in.pressed(KEY_LEFT)) {
                audio::setSfxVolume(s.sfxVolume / 100.0f);
                audio::playSfx(audio::Sfx::Coin); // audible preview of the new level
            }
            break;
        case AutoSave:
            if (in.pressed(KEY_A) || in.pressed(KEY_LEFT) || in.pressed(KEY_RIGHT))
                { s.autoSave = !s.autoSave; audio::playSfx(audio::Sfx::Select); }
            break;
        case Rumble:
            if (in.pressed(KEY_A) || in.pressed(KEY_LEFT) || in.pressed(KEY_RIGHT))
                { s.rumble = !s.rumble; audio::playSfx(audio::Sfx::Select); }
            break;
        case EnterCode:
            if (in.pressed(KEY_A)) {
                char code[32] = {0};
                SwkbdState kb;
                swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, sizeof(code) - 1);
                swkbdSetHintText(&kb, "Enter a code");
                swkbdSetValidation(&kb, SWKBD_NOTBLANK_NOTEMPTY, 0, 0);
                if (swkbdInputText(&kb, code, sizeof(code)) == SWKBD_BUTTON_RIGHT) {
                    redeemTask_.start(code, game_->pet().id()); // background call
                    redeeming_ = true;
                    toastTimer_ = 0.0f;
                }
            }
            break;
        case LinkPhone:
            if (in.pressed(KEY_A)) {
                // Type the phone's Link ID (shown in the Android app's Settings)
                // to tie this console to that account. Brief pause while it calls.
                char code[16] = {0};
                SwkbdState kb;
                swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, sizeof(code) - 1);
                swkbdSetHintText(&kb, "Phone Link ID: PP-XXXX-XXXX");
                swkbdSetValidation(&kb, SWKBD_NOTBLANK_NOTEMPTY, 0, 0);
                if (swkbdInputText(&kb, code, sizeof(code)) == SWKBD_BUTTON_RIGHT) {
                    const bool ok = game_->linkToPhone(code);
                    toast_ = ok ? "Linked to phone!" : "Link failed - check the ID.";
                    toastTimer_ = 4.0f;
                }
            }
            break;
        case ExportBackup:
            if (in.pressed(KEY_A)) {
                std::string path;
                game_->exportBackup(path);
                toast_ = "Backup saved!"; toastTimer_ = 3.0f;
            }
            break;
    }
}

void SettingsScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    ui_->drawTitleBar(Icon::Settings, "Settings", 0x4FB0C6FF);

    char line[80];
    const SaveMeta& m = game_->state().meta;
    std::snprintf(line, sizeof(line), "Play time: %lu min", (unsigned long)(m.playSeconds / 60));
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 100, 0.5f, toC2D(kText));
    char date[16];
    formatDate(m.lastSavedAt, date, sizeof(date));
    std::snprintf(line, sizeof(line), "Last saved: %s", date);
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 124, 0.45f, toC2D(kTextMuted));
    const std::string& acct = game_->accountId();
    std::snprintf(line, sizeof(line), "Pet ID: %s", acct.empty() ? "(offline)" : acct.c_str());
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 142, 0.45f, toC2D(kPrimaryDk));

    std::snprintf(line, sizeof(line), "PetPal v%lu.%lu.%lu",
                  (unsigned long)((kAppVersion >> 16) & 0xFF),
                  (unsigned long)((kAppVersion >> 8) & 0xFF),
                  (unsigned long)(kAppVersion & 0xFF));
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 164, 0.45f, toC2D(kTextMuted));
}

void SettingsScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    Settings& s = game_->settings();

    const float top = 10.0f, rowH = 28.0f;   // 7 rows fit above the hint line
    char val[32];

    auto row = [&](Row r, const char* label, const char* value) {
        const float y = top + r * rowH;
        const bool sel = (cursor_ == r);
        draw::card(8, y, kBottomWidth - 16, rowH - 6, 8,
                   toC2D(sel ? kButtonPressed : kButtonFill), toC2D(kButtonShadow));
        draw::textLeft(font, buf, label, 20, y + 8, 0.46f, toC2D(kText));
        draw::textLeft(font, buf, value, 210, y + 8, 0.46f, toC2D(kPrimaryDk));
    };

    std::snprintf(val, sizeof(val), "%u%%", s.musicVolume); row(MusicVol, "Music", val);
    std::snprintf(val, sizeof(val), "%u%%", s.sfxVolume);   row(SfxVol,   "Sound FX", val);
    row(AutoSave, "Auto-save", s.autoSave ? "On" : "Off");
    row(Rumble,   "Rumble",    s.rumble ? "On" : "Off");
    row(EnterCode, "Enter code", "Press A");
    row(LinkPhone, "Link to phone", game_->state().account.linked ? "Linked" : "Press A");
    row(ExportBackup, "Export backup", "Press A");

    // Modal loading spinner while a code is being checked over the network.
    if (redeeming_) {
        C2D_DrawRectSolid(0, 0, 0.0f, kBottomWidth, kScreenHeight, C2D_Color32(20, 24, 34, 170));
        draw::card(kBottomWidth * 0.5f - 70, 78, 140, 84, 14, toC2D(0xFFFFFFEE), toC2D(theme::kButtonShadow));
        ui_->drawSpinner(kBottomWidth * 0.5f, 110, 18, theme::kPrimary);
        draw::textCentered(font, buf, "Checking code...", kBottomWidth * 0.5f, 146, 0.46f, toC2D(kText));
        return;
    }

    if (toastTimer_ > 0.0f) {
        draw::roundedRect(20, 205, kBottomWidth - 40, 26, 10, toC2D(0x2C3E50EE));
        draw::textCentered(font, buf, toast_.c_str(), kBottomWidth * 0.5f, 218, 0.44f, toC2D(kTextLight));
    } else {
        ui_->drawHint(Btn::B, "Back", 10, 222);
    }
}

} // namespace petpal
