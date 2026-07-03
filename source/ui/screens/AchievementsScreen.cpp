#include "ui/screens/AchievementsScreen.h"
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

AchievementsScreen::AchievementsScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Achievements) {}

void AchievementsScreen::onEnter() { cursor_ = 0; }

void AchievementsScreen::update(float, const Input& in) {
    if (in.pressed(KEY_B)) { ui_->goBack(); return; }
    if (in.pressed(KEY_DOWN) && cursor_ + 1 < kAchievementCount) ++cursor_;
    if (in.pressed(KEY_UP)   && cursor_ > 0) --cursor_;
}

void AchievementsScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    Achievements& ach = game_->achievements();

    char line[64];
    std::snprintf(line, sizeof(line), "Awards  %d / %d",
                  ach.unlockedCount(), kAchievementCount);
    ui_->drawTitleBar(Icon::Awards, line, 0xF2C14EFF);

    const AchievementId id = static_cast<AchievementId>(cursor_);
    const bool got = ach.isUnlocked(id);
    draw::card(40, 70, kTopWidth - 80, 120, 14, toC2D(kBgPanel), toC2D(kButtonShadow));
    draw::textCentered(font, buf, achievementName(id), kTopWidth * 0.5f, 92, 0.6f,
                       toC2D(got ? kSuccess : kTextMuted));
    draw::textCentered(font, buf, achievementDescription(id), kTopWidth * 0.5f, 120, 0.45f, toC2D(kText));

    AchievementReward r = Achievements::rewardFor(id);
    std::snprintf(line, sizeof(line), "Reward: %s %s",
                  r.kind == RewardKind::Accessory ? "Accessory" : "Style",
                  r.kind == RewardKind::Accessory ? accessoryName(static_cast<Accessory>(r.value))
                                                  : styleName(static_cast<Style>(r.value)));
    draw::textCentered(font, buf, line, kTopWidth * 0.5f, 150, 0.45f, toC2D(kPrimaryDk));
    draw::textCentered(font, buf, got ? "UNLOCKED!" : "Keep playing to earn this.",
                       kTopWidth * 0.5f, 172, 0.45f, toC2D(got ? kSuccess : kTextMuted));
}

void AchievementsScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    Achievements& ach = game_->achievements();

    const float rowH = 26.0f, top = 6.0f;
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementId id = static_cast<AchievementId>(i);
        const bool got = ach.isUnlocked(id);
        const float y = top + i * rowH;
        const bool sel = (i == cursor_);
        draw::card(6, y, kBottomWidth - 12, rowH - 2, 8,
                   toC2D(sel ? kButtonPressed : kButtonFill), toC2D(kButtonShadow));
        C2D_DrawCircleSolid(20, y + (rowH - 2) * 0.5f, 0.0f, 6.0f,
                            toC2D(got ? kSuccess : kBarBack));
        draw::textLeft(font, buf, achievementName(id), 34, y + 5, 0.42f,
                       toC2D(got ? kText : kTextMuted));
    }
    ui_->drawHint(Btn::B, "Back", 8, 222);
}

} // namespace petpal
