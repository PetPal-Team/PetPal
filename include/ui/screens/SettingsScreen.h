#pragma once
#include "ui/Screen.h"
#include "netpass/RedeemTask.h"
#include <string>

namespace petpal {

// Options: music/SFX volume, autosave, rumble, and a manual backup export.
class SettingsScreen : public Screen {
public:
    SettingsScreen(Game* game, UIManager* ui);

    void onEnter() override;
    void update(float dt, const Input& in) override;
    void drawTop() override;
    void drawBottom() override;

private:
    enum Row { MusicVol = 0, SfxVol, AutoSave, Rumble, EnterCode, ExportBackup, RowCount };
    int  cursor_ = 0;
    std::string toast_;
    float toastTimer_ = 0.0f;

    RedeemTask redeemTask_;     // background code redemption
    bool       redeeming_ = false;
};

} // namespace petpal
