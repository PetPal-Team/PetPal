#include "ui/screens/PetScreen.h"
#include "ui/UIManager.h"
#include "ui/PetRenderer.h"
#include "ui/Widgets.h"
#include "ui/Gfx.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "core/Game.h"
#include "core/Names.h"
#include "audio/Audio.h"

#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <cstdio>

namespace petpal {

namespace {
const char* kLabels[PetScreen::ActionCount] = { "Feed", "Play", "Rest", "Pet", "Talk", "Evolve" };
const Icon  kIcons[PetScreen::ActionCount]  = {
    Icon::Apple, Icon::Bone, Icon::Energy, Icon::Happy, Icon::Friendship, Icon::Star
};
const uint32_t kColors[PetScreen::ActionCount] = {
    0xF2994AFF, // Feed   - orange
    0x6FCF6FFF, // Play   - mint
    0x9B8CFFFF, // Rest   - indigo
    0xFF7FB5FF, // Pet    - coral
    0x5BC0DEFF, // Talk   - sky
    0x9B59B6FF, // Evolve - purple
};

constexpr float kTileW = 98.0f, kTileH = 66.0f, kGapX = 8.0f, kGapY = 12.0f;
constexpr float kGridTop = 26.0f;

// Choose what to feed: favorite if owned, else any available food.
ItemId pickFood(Game* g) {
    if (g->inventory().has(g->pet().favoriteItem()) &&
        itemCategory(g->pet().favoriteItem()) <= ItemCategory::RareFood)
        return g->pet().favoriteItem();
    for (int i = 0; i < kItemCount; ++i) {
        const ItemId id = static_cast<ItemId>(i);
        if (itemCategory(id) <= ItemCategory::RareFood && g->inventory().has(id))
            return id;
    }
    return ItemId::Apple;
}
} // namespace

PetScreen::PetScreen(Game* game, UIManager* ui) : Screen(game, ui, ScreenId::Pet) {}

void PetScreen::tileRect(int i, float& x, float& y, float& w, float& h) const {
    w = kTileW; h = kTileH;
    const int col = i % 3, row = i / 3;
    const float rowW = 3 * kTileW + 2 * kGapX;
    const float startX = (kBottomWidth - rowW) * 0.5f;
    x = startX + col * (kTileW + kGapX);
    y = kGridTop + row * (kTileH + kGapY);
}

void PetScreen::onEnter() { focus_ = 0; toastTimer_ = 0.0f; }

void PetScreen::showToast(const std::string& msg) { toast_ = msg; toastTimer_ = 2.5f; }

void PetScreen::doAction(int index) {
    ActionFeedback fb;
    switch (index) {
        case ActFeed: fb = game_->feedPet(pickFood(game_)); break;
        case ActPlay: fb = game_->playWithPet(); break;
        case ActRest: fb = game_->restPet();     break;
        case ActPet:  fb = game_->petThePet();  break;
        case ActTalk: fb = game_->talkToPet();  break;
        case ActEvolve:
            if (game_->canEvolveNow()) { game_->confirmEvolution(); showToast("Your pet evolved!"); }
            else showToast("Not ready to evolve yet.");
            return;
    }
    if (!fb.message.empty()) showToast(fb.message);
    if (fb.leveledUp) {
        char b[48];
        std::snprintf(b, sizeof(b), "Level up! Now Lv.%u", fb.newLevel);
        showToast(b);
    }
}

void PetScreen::update(float dt, const Input& in) {
    if (toastTimer_ > 0.0f) toastTimer_ -= dt;
    glow_ += dt;
    for (int i = 0; i < ActionCount; ++i)
        if (press_[i] > 0.0f) { press_[i] -= dt * 4.0f; if (press_[i] < 0) press_[i] = 0; }

    if (in.pressed(KEY_B)) { ui_->goBack(); return; }

    // Y launches the Star Tap minigame (a blocking mode); reward on return.
    if (in.pressed(KEY_Y)) {
        const int sc = ui_->runMinigame();
        ActionFeedback fb = game_->rewardMinigame(sc);
        showToast(fb.message);
        return;
    }

    const int prevFocus = focus_;
    if (in.pressed(KEY_RIGHT) && (focus_ % 3) < 2 && focus_ + 1 < ActionCount) ++focus_;
    if (in.pressed(KEY_LEFT)  && (focus_ % 3) > 0) --focus_;
    if (in.pressed(KEY_DOWN)  && focus_ + 3 < ActionCount) focus_ += 3;
    if (in.pressed(KEY_UP)    && focus_ - 3 >= 0) focus_ -= 3;
    if (focus_ != prevFocus) audio::playSfx(audio::Sfx::Navigate);

    auto activate = [&](int i) {
        if (i == ActEvolve && !game_->canEvolveNow()) {
            audio::playSfx(audio::Sfx::Error);
            showToast("Not ready to evolve yet.");
            return;
        }
        press_[i] = 1.0f;
        if (i != ActFeed) audio::playSfx(audio::Sfx::Select); // Feed has its own eat SFX
        doAction(i);
    };

    for (int i = 0; i < ActionCount; ++i) {
        float x, y, w, h; tileRect(i, x, y, w, h);
        if (in.tappedRect(x, y, w, h)) { focus_ = i; activate(i); return; }
    }
    if (in.pressed(KEY_A)) activate(focus_);
}

void PetScreen::drawTop() {
    using namespace theme;
    Pet& pet = game_->pet();
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();

    ui_->drawLocationBg(Location::Town);
    ui_->drawPet(pet, 120.0f, 130.0f);

    // Title + current mood (derived from needs).
    char line[80];
    std::snprintf(line, sizeof(line), "%s the %s", pet.name(), speciesName(pet.species()));
    draw::textCentered(font, buf, line, 130, 18, 0.56f, toC2D(kText));
    std::snprintf(line, sizeof(line), "%s  -  Lv.%u", evolutionStageName(pet.stage()), pet.level());
    draw::textCentered(font, buf, line, 130, 36, 0.42f, toC2D(kTextMuted));
    std::snprintf(line, sizeof(line), "Feeling %s", moodName(pet.mood()));
    draw::textCentered(font, buf, line, 130, 52, 0.40f, toC2D(kText));

    // Server-assigned profile badges, shown just under the name/mood.
    const std::vector<std::string>& badges = game_->badges();
    if (!badges.empty()) {
        std::string bl;
        for (size_t i = 0; i < badges.size() && i < 3; ++i) { if (i) bl += "  "; bl += badges[i]; }
        draw::textCentered(font, buf, bl.c_str(), 130, 64, 0.34f, toC2D(kSecondary));
    }

    // Stat card on the right with icons: happiness / energy / hunger / xp.
    const float px = 252, pw = 140;
    draw::card(px, 48, pw, 184, 14, toC2D(kBgPanel), toC2D(kButtonShadow));
    auto stat = [&](Icon ic, uint32_t col, float t, float y) {
        ui_->drawIcon(ic, px + 18, y + 8, 18, toC2D(col));
        draw::bar(px + 32, y + 2, pw - 44, 12, t, toC2D(kBarBack), toC2D(col));
    };
    stat(Icon::Happy,  kBarHappy,  pet.happiness() / 100.0f, 58);
    stat(Icon::Energy, kBarEnergy, pet.energy()    / 100.0f, 84);
    stat(Icon::Apple,  kBarHunger, pet.hunger()    / 100.0f, 110);
    stat(Icon::Star,   kBarXp,     pet.levelProgress(),      136);
    ui_->drawIcon(Icon::Friends, px + 18, 170, 18, toC2D(kSecondary));
    std::snprintf(line, sizeof(line), "%lu met", (unsigned long)pet.streetpassEncounters());
    draw::textLeft(font, buf, line, px + 32, 164, 0.40f, toC2D(kText));
    ui_->drawIcon(Icon::Friendship, px + 18, 196, 18, toC2D(kPrimary));
    std::snprintf(line, sizeof(line), "%lu", (unsigned long)pet.friendship());
    draw::textLeft(font, buf, line, px + 32, 190, 0.40f, toC2D(kText));
}

void PetScreen::drawBottom() {
    using namespace theme;
    C2D_Font font = ui_->font();
    C2D_TextBuf buf = ui_->dynBuf();
    const bool canEvolve = game_->canEvolveNow();

    draw::textLeft(font, buf, "Care for your pet", 10, 8, 0.5f, toC2D(kText));
    {
        // Care-streak counter (grows on the first care action of each new day).
        char s[40];
        const unsigned long st = (unsigned long)game_->streak().current;
        std::snprintf(s, sizeof(s), "Streak: %lu day%s", st, st == 1 ? "" : "s");
        draw::textLeft(font, buf, s, kBottomWidth - 128, 10, 0.40f, toC2D(kTextMuted));
    }

    for (int i = 0; i < ActionCount; ++i) {
        float x, y, w, h; tileRect(i, x, y, w, h);
        const bool sel = (i == focus_);
        const bool disabled = (i == ActEvolve && !canEvolve);

        float scale = 1.0f - 0.08f * press_[i];
        if (sel) scale += 0.04f;
        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        const float tw = w * scale, th = h * scale;
        const float tx = cx - tw * 0.5f, ty = cy - th * 0.5f;

        // Evolve tile pulses gold when available.
        if (i == ActEvolve && canEvolve) {
            const float p = 0.5f + 0.5f * std::sin(glow_ * 4.0f);
            draw::roundedRect(tx - 4, ty - 4, tw + 8, th + 8, 16,
                              C2D_Color32(255, 205, 80, (u8)(120 + 110 * p)));
        } else if (sel) {
            draw::roundedRect(tx - 4, ty - 4, tw + 8, th + 8, 16, C2D_Color32(255,255,255,180));
        }

        const uint32_t fill = disabled ? 0xBFC4C9FF : kColors[i];
        draw::card(tx, ty, tw, th, 12, toC2D(fill), toC2D(0x00000040));
        ui_->drawIcon(kIcons[i], cx, cy - 6, th * 0.42f, C2D_Color32(255,255,255,255));
        draw::textCentered(font, buf, kLabels[i], cx, ty + th - 11, 0.40f, toC2D(kTextLight));
    }

    // Toast popup.
    if (toastTimer_ > 0.0f) {
        const float a = toastTimer_ > 0.4f ? 1.0f : toastTimer_ / 0.4f;
        draw::roundedRect(20, 198, kBottomWidth - 40, 30, 10, C2D_Color32(44,62,80,(u8)(220*a)));
        draw::textCentered(font, buf, toast_.c_str(), kBottomWidth * 0.5f, 213, 0.44f,
                           C2D_Color32(255,255,255,(u8)(255*a)));
    } else {
        const float bx = ui_->drawHint(Btn::B, "Back", 10, 222);
        ui_->drawHint(Btn::Y, "Arcade", bx + 14, 222);
    }
}

} // namespace petpal
