#pragma once
#include "ui/Screen.h"

namespace petpal {

// The hub: a grid of seven colored icon tiles (StreetPass-Plaza style) on the
// bottom screen, with the pet and a status read-out on the top screen.
class MainMenuScreen : public Screen {
public:
    MainMenuScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    static constexpr int kCount = 8;
    void tileRect(int i, float& x, float& y, float& w, float& h) const;

    int   focus_ = 0;
    float press_[kCount] = {0};  // per-tile press-bounce (decays to 0)
};

} // namespace petpal
