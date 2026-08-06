#pragma once
#include "ui/Screen.h"
#include "ui/Markdown.h"
#include "netpass/AnnouncementsClient.h"
#include <vector>

namespace petpal {

// News page: dev-team announcements fetched from teampetpal.com and rendered as
// Markdown. Bottom screen = the list (pick with Up/Down); top screen = the
// selected post, rendered + scrolled with L/R. Fetch runs on a background task.
class AnnouncementsScreen : public Screen {
public:
    AnnouncementsScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    static constexpr int kRowsVisible = 5;

    // Re-wrap the selected announcement's body for the top screen.
    void relayout();
    void clampScroll();

    AnnouncementsTask         task_;
    std::vector<Announcement> items_;
    std::vector<md::Line>     lines_;    // wrapped body of the selected post
    bool loading_ = false;
    bool error_   = false;
    int  selected_   = 0;   // list cursor
    int  scrollList_ = 0;   // first visible list row
    int  topLine_    = 0;   // first visible body line (top screen scroll)
    int  lastStart_  = 0;   // max topLine_ so the tail still fills the viewport
};

} // namespace petpal
