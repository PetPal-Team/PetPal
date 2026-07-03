#pragma once
#include "ui/Screen.h"

namespace petpal {

// Grid of achievements showing which are earned and what cosmetic each grants.
class AchievementsScreen : public Screen {
public:
    AchievementsScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    int cursor_ = 0;
};

} // namespace petpal
