#include "ui/screens/AnnouncementsScreen.h"
#include "ui/UIManager.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "core/Game.h"
#include "audio/Audio.h"

#include <3ds.h>
#include <citro2d.h>
#include <algorithm>
#include <cstdio>
#include <ctime>

namespace petpal {

namespace {
constexpr float kBodyLeft   = 14.0f;
constexpr float kBodyTop    = 48.0f;
constexpr float kBodyBottom = 232.0f;
constexpr float kBodyWidth  = 372.0f;   // kTopWidth (400) - 2*14

float styleScale(md::Style s) {
    switch (s) {
        case md::Style::H1:   return 0.60f;
        case md::Style::H2:   return 0.52f;
        case md::Style::H3:   return 0.47f;
        case md::Style::Code: return 0.40f;
        default:              return 0.42f;
    }
}
float lineHeight(md::Style s) {
    switch (s) {
        case md::Style::H1:    return 26.0f;
        case md::Style::H2:    return 22.0f;
        case md::Style::H3:    return 20.0f;
        case md::Style::Blank: return 9.0f;
        case md::Style::Rule:  return 12.0f;
        case md::Style::Code:  return 15.0f;
        default:               return 16.0f;
    }
}

void fmtAnnDate(long long ms, char* out, size_t n) {
    out[0] = '\0';
    if (ms <= 0) return;
    const time_t t = static_cast<time_t>(ms / 1000);
    struct tm* tm = gmtime(&t);
    if (tm) std::strftime(out, n, "%b %d, %Y", tm);
}
} // namespace

AnnouncementsScreen::AnnouncementsScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Announcements) {}

void AnnouncementsScreen::onEnter() {
    selected_ = 0; scrollList_ = 0; topLine_ = 0; lastStart_ = 0;
    items_.clear(); lines_.clear();
    error_ = false;
    loading_ = true;
    task_.start();   // background fetch; polled in update()
}

void AnnouncementsScreen::clampScroll() {
    const float viewport = kBodyBottom - kBodyTop;
    float h = 0.0f;
    lastStart_ = static_cast<int>(lines_.size());
    for (int k = static_cast<int>(lines_.size()) - 1; k >= 0; --k) {
        h += lineHeight(lines_[k].style);
        if (h > viewport) { lastStart_ = k + 1; break; }
        lastStart_ = k;
    }
    if (lastStart_ < 0) lastStart_ = 0;
    if (topLine_ > lastStart_) topLine_ = lastStart_;
    if (topLine_ < 0) topLine_ = 0;
}

void AnnouncementsScreen::relayout() {
    lines_.clear();
    topLine_ = 0;
    if (items_.empty()) { lastStart_ = 0; return; }

    C2D_TextBuf mb = C2D_TextBufNew(2048);
    C2D_Font font = ui_->font();
    auto measure = [&](const std::string& s, md::Style st) -> float {
        C2D_TextBufClear(mb);
        C2D_Text t;
        C2D_TextFontParse(&t, font, mb, s.c_str());
        float w = 0.0f, h = 0.0f;
        C2D_TextGetDimensions(&t, styleScale(st), styleScale(st), &w, &h);
        return w;
    };
    lines_ = md::layout(items_[selected_].body, kBodyWidth, measure);
    C2D_TextBufDelete(mb);
    clampScroll();
}

void AnnouncementsScreen::update(float dt, const Input& in) {
    (void)dt;

    // Poll the background fetch (fires true exactly once when it finishes).
    {
        std::vector<Announcement> got;
        bool ok = false;
        if (task_.collect(got, ok)) {
            items_ = std::move(got);
            loading_ = false;
            error_ = !ok;
            selected_ = 0; scrollList_ = 0;
            relayout();
        }
    }

    if (in.pressed(KEY_B)) { ui_->goBack(); return; }
    if (loading_) return;

    const int n = static_cast<int>(items_.size());
    if (n == 0) return;

    const int prev = selected_;
    if (in.pressed(KEY_DOWN) && selected_ < n - 1) ++selected_;
    if (in.pressed(KEY_UP)   && selected_ > 0)     --selected_;
    if (selected_ != prev) {
        audio::playSfx(audio::Sfx::Navigate);
        if (selected_ < scrollList_) scrollList_ = selected_;
        if (selected_ >= scrollList_ + kRowsVisible) scrollList_ = selected_ - kRowsVisible + 1;
        relayout();
        return;
    }

    // L/R scroll the post body on the top screen (a few lines per press).
    const int step = 3;
    if (in.pressed(KEY_R)) topLine_ = std::min(topLine_ + step, lastStart_);
    if (in.pressed(KEY_L)) topLine_ = std::max(topLine_ - step, 0);
}

void AnnouncementsScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    if (items_.empty()) {
        const char* msg = loading_ ? "Loading news..."
                        : error_   ? "Couldn't load news. Try again later."
                                   : "No announcements yet.";
        draw::textCentered(font, buf, "News", kTopWidth * 0.5f, 96, 0.7f, toC2D(kPrimaryDk));
        draw::textCentered(font, buf, msg, kTopWidth * 0.5f, 128, 0.46f, toC2D(kTextMuted));
        return;
    }

    const Announcement& a = items_[selected_];

    // Header: title + date + divider.
    draw::textLeft(font, buf, md::stripInline(a.title).c_str(), kBodyLeft, 8, 0.52f, toC2D(kPrimaryDk));
    char date[24];
    fmtAnnDate(a.date, date, sizeof(date));
    if (date[0]) draw::textLeft(font, buf, date, kBodyLeft, 30, 0.36f, toC2D(kTextMuted));
    draw::roundedRect(kBodyLeft, 44, kBodyWidth, 2, 1, toC2D(kButtonShadow));

    // Body (scrolled by whole lines from topLine_).
    float y = kBodyTop;
    for (int k = topLine_; k < static_cast<int>(lines_.size()); ++k) {
        const md::Line& ln = lines_[k];
        const float lh = lineHeight(ln.style);
        if (y > kBodyBottom) break;
        if (ln.style == md::Style::Rule) {
            draw::roundedRect(kBodyLeft, y + lh * 0.5f - 1.0f, kBodyWidth, 2, 1, toC2D(kTextMuted));
        } else if (ln.style != md::Style::Blank && !ln.text.empty()) {
            u32 col = toC2D(kText);
            if (ln.style == md::Style::H1 || ln.style == md::Style::H2 || ln.style == md::Style::H3)
                col = toC2D(kPrimaryDk);
            else if (ln.style == md::Style::Quote || ln.style == md::Style::Code)
                col = toC2D(kTextMuted);
            draw::textLeft(font, buf, ln.text.c_str(), kBodyLeft, y, styleScale(ln.style), col);
        }
        y += lh;
    }

    // Scroll affordances.
    if (topLine_ > 0)
        draw::textLeft(font, buf, "L up", kTopWidth - 84, kBodyTop - 12, 0.34f, toC2D(kTextMuted));
    if (topLine_ < lastStart_)
        draw::textLeft(font, buf, "R more", kTopWidth - 50, kBodyBottom - 2, 0.34f, toC2D(kPrimaryDk));
}

void AnnouncementsScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    char head[32];
    std::snprintf(head, sizeof(head), "News  (%d)", static_cast<int>(items_.size()));
    draw::textLeft(font, buf, head, 10, 8, 0.5f, toC2D(kText));

    if (loading_) {
        ui_->drawSpinner(kBottomWidth * 0.5f, 110, 16, theme::kPrimary);
        draw::textCentered(font, buf, "Loading news...", kBottomWidth * 0.5f, 140, 0.46f, toC2D(kTextMuted));
        ui_->drawHint(Btn::B, "Back", 10, 222);
        return;
    }

    if (items_.empty()) {
        draw::textCentered(font, buf, error_ ? "Couldn't reach the server." : "No announcements yet.",
                           kBottomWidth * 0.5f, 108, 0.48f, toC2D(kTextMuted));
        draw::textCentered(font, buf, error_ ? "Check your connection and reopen News." : "Check back soon!",
                           kBottomWidth * 0.5f, 130, 0.42f, toC2D(kTextMuted));
        ui_->drawHint(Btn::B, "Back", 10, 222);
        return;
    }

    const float top = 30.0f, rowH = 34.0f;
    for (int i = 0; i < kRowsVisible; ++i) {
        const int idx = scrollList_ + i;
        if (idx >= static_cast<int>(items_.size())) break;
        const Announcement& a = items_[idx];
        const float y = top + i * (rowH + 2.0f);
        const bool sel = (idx == selected_);
        draw::card(6, y, kBottomWidth - 12, rowH, 8,
                   toC2D(sel ? kButtonPressed : kButtonFill), toC2D(kButtonShadow));
        draw::textLeft(font, buf, md::stripInline(a.title).c_str(), 16, y + 6, 0.44f, toC2D(kText));
        char date[24];
        fmtAnnDate(a.date, date, sizeof(date));
        if (date[0]) draw::textLeft(font, buf, date, 16, y + 20, 0.34f, toC2D(kTextMuted));
    }

    float hx = ui_->drawHint(Btn::B, "Back", 10, 222);
    hx = ui_->drawHint(Btn::L, "", hx + 12, 222);
    hx = ui_->drawHint(Btn::R, "Scroll", hx + 2, 222);
}

} // namespace petpal
