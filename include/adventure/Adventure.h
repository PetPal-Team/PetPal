// =============================================================================
//  PetPal - Adventure.h
//  Send-the-pet-away mechanic. An adventure runs in real time: the player picks
//  a location and duration, the pet leaves, and after enough wall-clock time has
//  passed the pet returns with coins, items, and a story line for the journal.
//
//  Only one adventure can be active at a time. State is small and serializable.
// =============================================================================
#pragma once

#include "core/Types.h"
#include "util/Time.h"
#include <vector>
#include <string>

namespace petpal {

class Random;

// One stack of a reward item.
struct RewardItem {
    ItemId   id;
    uint16_t qty;
};

// Everything an adventure yields when the pet returns.
struct AdventureResult {
    bool                    valid = false;
    Location                location = Location::Town;
    uint32_t                coins = 0;
    std::vector<RewardItem> items;
    std::string             story; // e.g. "Ziggy explored the Arcade and found a Rainbow Cookie."
};

class Adventure {
public:
    Adventure() = default;

    bool active() const { return active_; }
    Location          location() const { return location_; }
    AdventureDuration duration() const { return duration_; }
    Timestamp         startTime() const { return startTime_; }

    // Begin an adventure. No-op (returns false) if one is already running.
    bool begin(Location loc, AdventureDuration dur, Timestamp now);

    // Wall-clock seconds remaining; 0 means ready to collect.
    uint32_t secondsRemaining(Timestamp now) const;
    bool     isComplete(Timestamp now) const { return active_ && secondsRemaining(now) == 0; }
    float    progress(Timestamp now) const; // 0..1

    // Roll rewards, clear active state, and return the result. The caller is
    // responsible for applying rewards to Inventory/Pet and logging the story.
    // Returns an invalid result if not complete yet.
    AdventureResult collect(const char* petName, Timestamp now, Random& rng);

    // Abandon the current adventure without rewards (e.g. player cancels).
    void cancel() { active_ = false; }

private:
    bool              active_   = false;
    Location          location_ = Location::Town;
    AdventureDuration duration_ = AdventureDuration::Short;
    Timestamp         startTime_ = 0;

    friend class SaveManager;
};

// Free helper: build a reward set + story for a finished adventure. Exposed for
// unit testing the loot tables independently of timing.
AdventureResult rollAdventureRewards(const char* petName, Location loc,
                                     AdventureDuration dur, Random& rng);

} // namespace petpal
