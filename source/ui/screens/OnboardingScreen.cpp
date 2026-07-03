#include "ui/screens/OnboardingScreen.h"
#include "ui/UIManager.h"
#include "ui/PetRenderer.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "core/Game.h"
#include "core/Names.h"

#include <3ds.h>
#include <citro2d.h>
#include <cstring>

namespace petpal {

OnboardingScreen::OnboardingScreen(Game* game, UIManager* ui)
    : Screen(game, ui, ScreenId::Onboarding) {
    // 7 species buttons in a 4x2 grid (last cell empty).
    const float bw = 72.0f, bh = 44.0f, gx = 6.0f, gy = 10.0f, top = 70.0f, left = 8.0f;
    for (int i = 0; i < kSpeciesCount; ++i) {
        const int col = i % 4, row = i / 4;
        speciesButtons_[i] = Button(left + col * (bw + gx), top + row * (bh + gy),
                                    bw, bh, speciesName(static_cast<Species>(i)));
    }
}

void OnboardingScreen::onEnter() { selected_ = 0; }

// Open the system software keyboard to get a name, then create the pet.
void OnboardingScreen::confirmSpecies(Species s) {
    char name[kMaxNameBuffer] = {0};

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, kMaxPetNameLen);
    swkbdSetHintText(&swkbd, "Name your pet");
    swkbdSetValidation(&swkbd, SWKBD_NOTBLANK_NOTEMPTY, 0, 0);
    SwkbdButton btn = swkbdInputText(&swkbd, name, sizeof(name));

    if (btn != SWKBD_BUTTON_RIGHT) return; // cancelled
    if (name[0] == '\0') std::strcpy(name, speciesName(s));

    game_->createPet(s, name);
    ui_->navigateTo(ScreenId::MainMenu);
}

void OnboardingScreen::update(float dt, const Input& in) {
    if (in.pressed(KEY_RIGHT) && (selected_ % 4) < 3 && selected_ + 1 < kSpeciesCount) ++selected_;
    if (in.pressed(KEY_LEFT)  && (selected_ % 4) > 0) --selected_;
    if (in.pressed(KEY_DOWN)  && selected_ + 4 < kSpeciesCount) selected_ += 4;
    if (in.pressed(KEY_UP)    && selected_ - 4 >= 0) selected_ -= 4;

    for (int i = 0; i < kSpeciesCount; ++i) {
        speciesButtons_[i].update(dt);
        if (speciesButtons_[i].handle(in, i == selected_))
            confirmSpecies(static_cast<Species>(i));
    }
}

void OnboardingScreen::drawTop() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    const Species s = static_cast<Species>(selected_);

    draw::textCentered(font, buf, "Choose your PetPal!", kTopWidth * 0.5f, 24, 0.7f, toC2D(kText));

    // Live preview using species default colors.
    Pet preview = Pet::createStarter(s, speciesName(s));
    ui_->drawPet(preview, kTopWidth * 0.5f, 132.0f);

    draw::card(40, 195, kTopWidth - 80, 40, 12, toC2D(kBgPanel), toC2D(kButtonShadow));
    draw::textCentered(font, buf, speciesName(s), kTopWidth * 0.5f, 206, 0.5f, toC2D(kPrimaryDk));
    draw::textCentered(font, buf, speciesPersonality(s), kTopWidth * 0.5f, 224, 0.4f, toC2D(kTextMuted));
}

void OnboardingScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    draw::textCentered(font, buf, "Pick a friend to begin", kBottomWidth * 0.5f, 30, 0.5f, toC2D(kText));
    for (int i = 0; i < kSpeciesCount; ++i)
        speciesButtons_[i].draw(font, buf, i == selected_);
    ui_->drawHint(Btn::A, "Choose", 122, 218);
}

} // namespace petpal
