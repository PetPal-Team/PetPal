#pragma once
#include "ui/Screen.h"
#include "ui/Widgets.h"
#include <array>

namespace petpal {

// Send the pet on an adventure: pick an unlocked location and a duration, watch
// the timer on the top screen, then collect rewards when it returns.
class AdventuresScreen : public Screen {
public:
    AdventuresScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    int  selectedLocation_ = 0;
    int  selectedDuration_ = 0;
    Button startButton_;
    Button collectButton_;
    std::string toast_;
    float toastTimer_ = 0.0f;
};

} // namespace petpal
