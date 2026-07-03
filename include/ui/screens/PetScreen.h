#pragma once
#include "ui/Screen.h"
#include <string>

namespace petpal {

// Care for the pet: feed, play, pet, talk, and (when earned) evolve. The top
// screen shows the pet large with full stats; the bottom holds icon action
// tiles.
class PetScreen : public Screen {
public:
    PetScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

    enum Action { ActFeed = 0, ActPlay, ActPet, ActTalk, ActEvolve, ActionCount };

private:
    void doAction(int index);
    void showToast(const std::string& msg);
    void tileRect(int i, float& x, float& y, float& w, float& h) const;

    int   focus_ = 0;
    float press_[ActionCount] = {0};
    std::string toast_;
    float       toastTimer_ = 0.0f;
    float       glow_ = 0.0f; // evolve pulse
};

} // namespace petpal
