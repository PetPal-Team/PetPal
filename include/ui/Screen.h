// =============================================================================
//  PetPal - Screen.h
//  Base class for every full-screen view. A Screen owns its own widgets and
//  knows how to draw the top (display) and bottom (interactive) screens and how
//  to react to input. Screens are created and switched by UIManager.
// =============================================================================
#pragma once

#include "ui/Input.h"

namespace petpal {

class Game;
class UIManager;

enum class ScreenId : uint8_t {
    Onboarding = 0, // first-run: choose & name your pet
    MainMenu,
    Pet,
    Friends,
    Adventures,
    Journal,
    Customization,
    Achievements,
    Shop,
    Settings,
    Count
};

class Screen {
public:
    virtual ~Screen() = default;

    // Lifecycle hooks fired by UIManager on transitions.
    virtual void onEnter() {}
    virtual void onExit()  {}

    // Per-frame logic. `dt` is seconds since last frame.
    virtual void update(float dt, const Input& in) = 0;

    // Rendering. Called between C2D_SceneBegin for each target.
    virtual void drawTop()    = 0;
    virtual void drawBottom() = 0;

    ScreenId id() const { return id_; }

protected:
    Screen(Game* game, UIManager* ui, ScreenId id)
        : game_(game), ui_(ui), id_(id) {}

    Game*      game_;
    UIManager* ui_;
    ScreenId   id_;
};

} // namespace petpal
