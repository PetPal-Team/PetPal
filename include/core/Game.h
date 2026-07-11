// =============================================================================
//  PetPal - Game.h
//  The application core. Owns the persistent model and every manager, exposes
//  the high-level gameplay verbs the UI calls ("feed", "send on adventure",
//  "process StreetPass"), and runs the main loop. Screens never mutate the
//  model directly - they go through Game so side effects (journal, achievements,
//  autosave, celebrations) happen consistently in one place.
// =============================================================================
#pragma once

#include "save/SaveData.h"
#include "save/SaveManager.h"
#include "netpass/NetPassManager.h"
#include "netpass/RedeemManager.h"
#include "ui/UIManager.h"
#include "util/Random.h"
#include <string>
#include <vector>

namespace petpal {

// What a high-level action produced, so the UI knows how to react.
struct ActionFeedback {
    bool        ok            = true;
    bool        leveledUp     = false;
    uint16_t    newLevel      = 0;
    bool        canEvolve     = false;
    std::string message;       // short toast text, may be empty
};

class Game {
public:
    explicit Game(std::string baseDir);
    ~Game();

    // Boot: graphics, load-or-create save, NetPass. Returns false on fatal init
    // failure (graphics unavailable).
    bool init();
    void run();        // blocks until the player quits
    void shutdown();

    // ----- Model access -----------------------------------------------------
    PersistentState&       state()       { return state_; }
    const PersistentState& state() const { return state_; }
    Pet&        pet()        { return state_.pet; }
    Inventory&  inventory()  { return state_.inventory; }
    FriendList& friends()    { return state_.friends; }
    Journal&    journal()    { return state_.journal; }
    Adventure&  adventure()  { return state_.adventure; }
    Achievements& achievements() { return state_.achievements; }
    World&      world()      { return state_.world; }
    Settings&   settings()   { return state_.settings; }

    Random&     rng()        { return rng_; }
    UIManager&  ui()         { return ui_; }
    NetPassManager& netpass(){ return netpass_; }

    bool onboardingComplete() const { return state_.meta.onboarded; }

    // ----- Server identity / ban ------------------------------------------
    const std::string& accountId() const { return state_.account.id; }
    bool               banned() const { return banned_; }
    // Link this console to a phone account by its PP-XXXX-XXXX id. On success the
    // console adopts targetId locally + saves. Returns false on any failure.
    bool linkToPhone(const char* targetId);

    // ----- Onboarding -------------------------------------------------------
    void createPet(Species species, const char* name);

    // ----- Daily activities (each may level up / unlock evolution) ----------
    ActionFeedback feedPet(ItemId food);
    ActionFeedback playWithPet();
    ActionFeedback petThePet();
    ActionFeedback talkToPet();

    // ----- Adventures -------------------------------------------------------
    bool            startAdventure(Location loc, AdventureDuration dur);
    AdventureResult collectAdventure(); // applies rewards + logs story

    // ----- Evolution --------------------------------------------------------
    bool canEvolveNow() const { return state_.pet.canEvolve(); }
    void confirmEvolution();   // performs the evolution sequence + side effects

    // ----- StreetPass / NetPass --------------------------------------------
    // Poll the inbox and process every pet we met: friendship, rewards, gifts,
    // journal entries, possible location unlocks. Returns number processed.
    int processNetPass();

    // Re-publish our outbox (after appearance/level changes).
    void publishSelf();

#if PETPAL_DEBUG
    // Debug-only: run a synthetic visitor through the REAL meeting pipeline
    // (friend/journal/celebration) without needing CECD or a second console.
    // Build with -DPETPAL_DEBUG=1 to enable the SELECT hook on the Friends screen.
    void injectTestPass();
#endif

    // ----- Redeem codes -----------------------------------------------------
    // Apply a reward already fetched from the server (the network call runs on a
    // background RedeemTask). No-op if the result wasn't a successful grant.
    void applyRedeem(const RedeemResult& res);

    // ----- Currency shop ----------------------------------------------------
    bool buyItem(ItemId id, uint32_t price, uint16_t qty = 1);

    // ----- Persistence ------------------------------------------------------
    void requestSave();           // force a save now
    void exportBackup(std::string& outPath);

    bool wantsQuit() const { return quit_; }
    void requestQuit()     { quit_ = true; }

private:
    // Roll achievements against current progress; apply cosmetic unlocks +
    // journal + celebration for any newly earned.
    void runAchievementChecks();

    // Apply one received pet (real or simulated) to all the social systems.
    void processOneEncounter(const PetPalPacket& pkt, Timestamp when);

    // Maybe unlock a new location after an encounter (keeps the world growing).
    bool maybeUnlockLocation();

    void autosaveTick(float dt);
    AchievementContext buildAchievementContext() const;

    // Boot version check (device only). True if the server's latest version is
    // newer than kAppVersion. Fails open (returns false) on any network error.
    bool fetchUpdateStatus();

    std::string      baseDir_;
    PersistentState  state_;
    SaveManager      save_;
    NetPassManager   netpass_;
    UIManager        ui_;
    Random           rng_;

    Timestamp lastTick_   = 0;
    float     autosaveAcc_ = 0.0f;
    bool      quit_        = false;
    bool      updateAvailable_ = false; // server reported a newer version at boot
    bool      banned_      = false;     // server reported this pet is banned at boot
};

} // namespace petpal
