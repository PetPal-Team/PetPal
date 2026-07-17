// =============================================================================
//  PetPal - SaveData.h
//  The complete, persistable game state. Game owns one PersistentState; every
//  gameplay system mutates it, and SaveManager serializes it. Bundling state in
//  one struct keeps "what gets saved" obvious and in one place.
// =============================================================================
#pragma once

#include "pets/Pet.h"
#include "items/Inventory.h"
#include "friends/FriendList.h"
#include "journal/Journal.h"
#include "adventure/Adventure.h"
#include "achievements/Achievements.h"
#include "util/Time.h"
#include <string>

namespace petpal {

// Player-facing options.
struct Settings {
    uint8_t musicVolume = 100; // 0..100
    uint8_t sfxVolume   = 100; // 0..100
    bool    autoSave    = true;
    bool    rumble      = true; // New 3DS / circle-pad-pro style feedback hooks
};

// Bookkeeping that isn't owned by a specific subsystem.
struct SaveMeta {
    Timestamp createdAt    = 0; // when the save was first made
    Timestamp lastSavedAt  = 0; // updated on every write
    uint64_t  playSeconds  = 0; // total time in-app
    bool      onboarded    = false; // has the player created their pet yet?
};

// Server-side identity (minted by teampetpal.com on first online boot). The id
// is the human-typeable PP-XXXX-XXXX shown for support + used for the boot
// ban-check; the token is this console's own credential (not a shared secret).
// `linked` is set once this console has joined a phone account via /api/link.
struct Account {
    std::string id;       // "PP-XXXX-XXXX" (empty until first online boot)
    std::string token;    // per-device credential (empty after linking)
    bool        linked = false; // adopted a phone account's id
    // Cross-device continuity (v4): the server pet `updatedAt` (ms) this console
    // last reconciled with, so a linked device knows if the server copy is newer.
    uint64_t    petSyncAt = 0;
};

// What a streak check-in produced, so the UI can celebrate.
struct StreakResult {
    bool     advanced    = false; // the chain grew today (first care of a new day)
    bool     milestone   = false; // landed on a kStreakMilestoneDays multiple
    uint32_t current     = 0;     // streak length after this check-in
    uint32_t rewardCoins = 0;     // bonus paid on a milestone
};

// Care streak (v4): consecutive real days on which the pet was tended at least
// once. Any care action calls checkIn(now); it is idempotent within a day.
struct Streak {
    uint32_t  current     = 0; // days in the current chain
    uint32_t  best        = 0; // longest chain ever reached
    Timestamp lastCareDay = 0; // epoch-day of the last tend (0 = never tended)

    StreakResult checkIn(Timestamp now) {
        StreakResult res;
        const Timestamp today = now / 86400ULL; // UTC day index
        res.current = current;
        if (lastCareDay == today) return res;            // already counted today
        if (lastCareDay != 0 && today == lastCareDay + 1) current += 1; // continued
        else current = 1;                                // started fresh / chain broke
        lastCareDay = today;
        if (current > best) best = current;
        res.advanced = true;
        res.current  = current;
        if (current % kStreakMilestoneDays == 0) {
            res.milestone   = true;
            res.rewardCoins = kStreakMilestoneCoins;
        }
        return res;
    }
};

// World progression (locations the pet has unlocked). Town is free.
struct World {
    uint16_t locationUnlockMask = (1u << static_cast<int>(Location::Town));

    bool isUnlocked(Location l) const { return (locationUnlockMask >> (int)l) & 1u; }
    void unlock(Location l)           { locationUnlockMask |= (1u << (int)l); }
    int  unlockedCount() const {
        int n = 0;
        for (int i = 0; i < kLocationCount; ++i)
            if (isUnlocked(static_cast<Location>(i))) ++n;
        return n;
    }
};

// The whole game in one struct.
struct PersistentState {
    Pet           pet;
    Inventory     inventory;
    FriendList    friends;
    Journal       journal;
    Adventure     adventure;
    Achievements  achievements;
    World         world;
    Settings      settings;
    SaveMeta      meta;
    Account       account;   // v3: server identity / 3DS-link
    Streak        streak;    // v4: daily care streak
};

} // namespace petpal
