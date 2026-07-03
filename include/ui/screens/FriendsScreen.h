#pragma once
#include "ui/Screen.h"

namespace petpal {

// Scrollable roster of every pet met via StreetPass / NetPass, with the
// selected friend featured on the top screen.
class FriendsScreen : public Screen {
public:
    FriendsScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    static constexpr int kRowsVisible = 5;
    int   selected_ = 0;
    int   scroll_   = 0;
    float flash_    = 0.0f;   // seconds remaining to show the check result
    char  flashMsg_[64] = {0};
};

} // namespace petpal
