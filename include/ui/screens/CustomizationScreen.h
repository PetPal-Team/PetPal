#pragma once
#include "ui/Screen.h"

namespace petpal {

// Dress-up screen: change the pet's two colors, equipped accessory, and style.
// Locked cosmetics are shown but can't be equipped until earned.
class CustomizationScreen : public Screen {
public:
    CustomizationScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    // Tabs cycle with L/R. (Accent/3rd color dropped for now.)
    enum Tab { PrimaryColor = 0, SecondaryColor, AccessoryTab, StyleTab, TabCount };
    int  tab_ = 0;
    int  cursor_ = 0;

    int  optionCount() const;       // entries in the current tab
    void applyCurrent();            // equip/set the highlighted option
};

} // namespace petpal
