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
};

} // namespace petpal
