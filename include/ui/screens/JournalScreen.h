#pragma once
#include "ui/Screen.h"

namespace petpal {

// Scrollable diary of automatically-recorded, personality-driven entries.
class JournalScreen : public Screen {
public:
    JournalScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    int scroll_ = 0;
};

} // namespace petpal
