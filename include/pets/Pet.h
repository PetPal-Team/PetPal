// =============================================================================
//  PetPal - Pet.h
//  The player's pet: identity, appearance, stats, and progression. The Pet owns
//  no rendering or save logic itself - it is a pure gameplay model so it can be
//  unit-tested off-device and serialized by SaveManager / NetPassManager.
// =============================================================================
#pragma once

#include "core/Types.h"
#include "util/Time.h"
#include <string>

namespace petpal {

// Result of an action that may level up or change a stat, so the UI knows what
// to celebrate (sound, particles, popups).
struct ActionResult {
    bool     leveledUp     = false;
    uint16_t newLevel      = 0;
    bool     statChanged   = false;
    bool     canEvolveNow  = false; // evolution requirement newly satisfied
};

class Pet {
public:
    Pet();

    // --- Creation -----------------------------------------------------------
    // Build a fresh starter pet. Generates a random 64-bit id and sensible
    // default colors. The name is copied (truncated to kMaxPetNameLen).
    static Pet createStarter(Species species, const char* name);

    // --- Identity -----------------------------------------------------------
    uint64_t    id()      const { return id_; }
    Species     species() const { return species_; }
    const char* name()    const { return name_.c_str(); }
    void        setName(const char* name);

    // --- Appearance ---------------------------------------------------------
    PetColor primaryColor()   const { return primary_; }
    PetColor secondaryColor() const { return secondary_; }
    PetColor accentColor()    const { return accent_; }
    void setColors(PetColor primary, PetColor secondary, PetColor accent);

    Accessory equippedAccessory() const { return equippedAccessory_; }
    Style     equippedStyle()     const { return equippedStyle_; }
    bool      equipAccessory(Accessory a); // false if not unlocked
    bool      equipStyle(Style s);         // false if not unlocked

    bool isAccessoryUnlocked(Accessory a) const;
    bool isStyleUnlocked(Style s) const;
    void unlockAccessory(Accessory a);
    void unlockStyle(Style s);
    int  unlockedAccessoryCount() const;   // excludes None

    // --- Core stats ---------------------------------------------------------
    uint16_t level()         const { return level_; }
    uint32_t experience()    const { return experience_; }
    uint8_t  happiness()     const { return happiness_; }
    uint8_t  energy()        const { return energy_; }
    uint8_t  hunger()        const { return hunger_; } // fullness: 100=full, 0=starving
    uint32_t friendship()    const { return friendship_; }
    uint16_t adventureRank() const { return adventureRank_; }

    // Current mood, derived from the needs above (never serialized).
    Mood mood() const;

    // XP needed to advance *from* a given level to the next.
    static uint32_t xpForLevel(uint16_t level);
    // Progress 0..1 through the current level (for UI bars).
    float levelProgress() const;

    // --- Progression counters ----------------------------------------------
    uint32_t streetpassEncounters() const { return streetpassEncounters_; }
    uint32_t uniquePetsMet()        const { return uniquePetsMet_; }
    uint32_t adventuresCompleted()  const { return adventuresCompleted_; }
    EvolutionStage stage()          const { return stage_; }

    ItemId favoriteItem() const { return favoriteItem_; }
    void   setFavoriteItem(ItemId id) { favoriteItem_ = id; }

    // --- Transform (temporary form granted by redeem codes) -----------------
    // Apply a transform that lasts `seconds` from now. seconds==0 clears it.
    void          setTransform(TransformForm f, uint32_t seconds);
    // The form to render right now (None once the timer has expired).
    TransformForm currentForm() const;
    Timestamp     transformExpiry() const { return transformExpiry_; }

    // --- Daily activities ---------------------------------------------------
    // Each returns what changed so the UI can react. Stats are clamped 0..100.
    ActionResult feed(ItemId food);   // restores fullness (+a little energy); favorite gives bonus
    ActionResult play();              // +happiness, -energy, small XP
    ActionResult rest();              // +energy (a nap)
    ActionResult petPet();            // +happiness
    ActionResult talk();              // +happiness, +friendship trickle

    // --- Experience & evolution --------------------------------------------
    ActionResult addExperience(uint32_t xp);

    // Record one StreetPass/NetPass encounter. `unique` flags first-time pets.
    // Awards XP/friendship and may flag a new evolution as available.
    ActionResult recordEncounter(bool unique);

    // True if the *next* stage's requirements are met but not yet applied.
    bool canEvolve() const;
    // Advance to the next stage. Returns the new stage, or current if maxed.
    EvolutionStage evolve();

    // --- Adventure hooks ----------------------------------------------------
    void onAdventureCompleted() { ++adventuresCompleted_; ++adventureRank_; }
    void addFriendship(uint32_t amount);
    void addHappiness(int delta); // clamp to 0..100 (used by minigame rewards)

    // --- Real-time needs decay ---------------------------------------------
    // Advance the pet's needs to wall-clock `now`, draining fullness/energy/
    // happiness for whole hours elapsed since the last catch-up. Safe to call
    // every frame (a no-op until an hour passes) and on load. lastDecayAt_ is
    // the internal clock; a brand-new pet anchors it on the first call.
    void catchUpTo(Timestamp now);

    // --- Cross-device continuity -------------------------------------------
    // Overwrite identity + basic stats from a linked account's server snapshot.
    // Appearance (colors/accessories) and progression counters stay local.
    // A negative int or empty name means "leave this field alone".
    void applyServerSnapshot(const char* name, int species, int stage, int level,
                             int happiness, int energy, int hunger);

private:
    void recomputeStage(); // clamps stage_ upward to whatever is earned

    // Identity
    uint64_t id_;
    Species  species_;
    std::string name_;

    // Appearance
    PetColor  primary_;
    PetColor  secondary_;
    PetColor  accent_;
    Accessory equippedAccessory_;
    Style     equippedStyle_;
    uint16_t  accessoryUnlockMask_; // bit i set => Accessory(i) unlocked
    uint16_t  styleUnlockMask_;     // bit i set => Style(i) unlocked

    // Stats
    uint16_t level_;
    uint32_t experience_;     // XP accumulated *within* the current level
    uint8_t  happiness_;
    uint8_t  energy_;
    uint8_t  hunger_;         // fullness/satiety: 100=full, 0=starving (v4)
    uint32_t friendship_;
    uint16_t adventureRank_;

    // Needs clock (v4): wall-clock seconds at which decay was last applied.
    Timestamp lastDecayAt_ = 0;

    // Progression
    uint32_t streetpassEncounters_;
    uint32_t uniquePetsMet_;
    uint32_t adventuresCompleted_;
    EvolutionStage stage_;
    ItemId   favoriteItem_;

    // Transform (redeem codes). None unless an unexpired code form is active.
    TransformForm transformForm_   = TransformForm::None;
    Timestamp     transformExpiry_ = 0;

    friend class SaveManager; // serialization reaches into raw fields
};

} // namespace petpal
