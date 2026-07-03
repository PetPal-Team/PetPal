#include "ui/screens/JournalScreen.h"
#include "ui/UIManager.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "core/Game.h"
#include "core/Names.h"

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>

namespace petpal {

JournalScreen::JournalScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Journal) {}

void JournalScreen::onEnter() { scroll_ = 0; }

namespace {
// Category -> icon + tint, so entries are scannable at a glance.
uint32_t categoryColor(JournalCategory c) {
    switch (c) {
        case JournalCategory::Friend:      return theme::kPrimary;
        case JournalCategory::Adventure:   return theme::kSecondary;
        case JournalCategory::Item:        return theme::kWarning;
        case JournalCategory::Evolution:   return 0x9B59B6FF;
        case JournalCategory::Achievement: return theme::kSuccess;
        default:                           return theme::kTextMuted;
    }
}
Icon categoryIcon(JournalCategory c) {
    switch (c) {
        case JournalCategory::Friend:      return Icon::Friends;
        case JournalCategory::Adventure:   return Icon::Adventures;
        case JournalCategory::Item:        return Icon::Chest;
        case JournalCategory::Evolution:   return Icon::Star;
        case JournalCategory::Achievement: return Icon::Awards;
        default:                           return Icon::Pet;
    }
}
} // namespace

void JournalScreen::update(float, const Input& in) {
    if (in.pressed(KEY_B)) { ui_->goBack(); return; }
    const int n = game_->journal().size();
    if (in.pressed(KEY_DOWN) && scroll_ < n - 1) ++scroll_;
    if (in.pressed(KEY_UP)   && scroll_ > 0)     --scroll_;
}

void JournalScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    Journal& j = game_->journal();

    ui_->drawTitleBar(Icon::Journal, "Pet Journal", 0xF2994AFF);

    if (j.size() == 0) {
        draw::textCentered(font, buf, "The story starts soon...", kTopWidth * 0.5f, 120, 0.5f, toC2D(kTextMuted));
        return;
    }

    // Show a window of entries (newest first) on the top screen.
    const int perScreen = 7;
    for (int i = 0; i < perScreen; ++i) {
        const int idx = scroll_ + i;
        if (idx >= j.size()) break;
        const JournalEntry& e = j.at(idx);
        const float y = 50.0f + i * 26.0f;

        ui_->drawIcon(categoryIcon(e.category), 16, y + 8, 18, toC2D(categoryColor(e.category)));
        char date[16];
        formatDate(e.when, date, sizeof(date));
        draw::textLeft(font, buf, date, 26, y, 0.36f, toC2D(kTextMuted));
        draw::textLeft(font, buf, e.text.c_str(), 26, y + 11, 0.42f, toC2D(kText));
    }
}

void JournalScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    draw::textCentered(font, buf, "Up / Down to scroll", kBottomWidth * 0.5f, 80, 0.5f, toC2D(kText));
    char line[48];
    std::snprintf(line, sizeof(line), "Entry %d of %d", game_->journal().size() ? scroll_ + 1 : 0,
                  game_->journal().size());
    draw::textCentered(font, buf, line, kBottomWidth * 0.5f, 110, 0.45f, toC2D(kTextMuted));
    ui_->drawHint(Btn::B, "Back", 10, 222);
}

} // namespace petpal
