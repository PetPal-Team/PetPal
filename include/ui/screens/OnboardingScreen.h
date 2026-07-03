#pragma once
#include "ui/Screen.h"
#include "ui/Widgets.h"
#include "core/Types.h"   // kSpeciesCount
#include <array>

namespace petpal {

// First-run flow: pick a species (live preview on top), then name the pet with
// the system software keyboard. Creates the pet and jumps to the main menu.
class OnboardingScreen : public Screen {
public:
    OnboardingScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    void confirmSpecies(Species s);

    std::array<Button, kSpeciesCount> speciesButtons_;
    int     selected_ = 0;
};

} // namespace petpal
