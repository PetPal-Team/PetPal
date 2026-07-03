#pragma once
#include "ui/Screen.h"

namespace petpal {

// A simple shop: browse a list of food/materials on the bottom screen, with the
// highlighted item featured (name, blurb, price, owned) on the top screen. Buy
// with A if you can afford it. Coins come from adventures and rewards.
class ShopScreen : public Screen {
public:
    ShopScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    static constexpr int kRowsVisible = 5;
    int   selected_ = 0;
    int   scroll_   = 0;
    float flash_    = 0.0f;   // "bought!" / "too pricey" toast timer
    char  flashMsg_[40] = {0};
    float buyPulse_ = 0.0f;   // little pop when buying
};

} // namespace petpal
