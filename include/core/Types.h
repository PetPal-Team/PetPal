// =============================================================================
//  PetPal - Types.h
//  Central definition of every enum, constant, and lightweight POD used across
//  the project. Keeping these in one place guarantees that the save format,
//  the NetPass packet layout, and the UI all agree on the same numeric values.
//
//  IMPORTANT: The integer value of every enumerator below is part of the
//  on-disk save format AND the over-the-wire NetPass format. Never reorder or
//  remove enumerators. Only append new values at the end of each enum.
// =============================================================================
#pragma once

#include <cstdint>
#include <array>

namespace petpal {

// -----------------------------------------------------------------------------
//  Versioning
// -----------------------------------------------------------------------------
constexpr uint32_t kAppVersion      = 0x00000104; // 0.1.4  (major.minor.patch, one byte each)
constexpr uint32_t kSaveMagic       = 0x4C415050; // 'PPAL' little-endian "PPAL"
constexpr uint16_t kSaveVersion     = 4; // v4: hunger need + decay clock + care streak
constexpr uint32_t kNetPassMagic    = 0x50455450; // 'PTEP' -> "PTEP" PetPal tag
constexpr uint16_t kNetPassVersion  = 1;

// -----------------------------------------------------------------------------
//  Species
// -----------------------------------------------------------------------------
enum class Species : uint8_t {
    Fox = 0,
    Cat,
    Bunny,
    Dragon,
    Slime,
    Robot,
    Axolotl,
    Count
};
constexpr int kSpeciesCount = static_cast<int>(Species::Count);

// -----------------------------------------------------------------------------
//  Colors (shared by primary / secondary / accent slots)
// -----------------------------------------------------------------------------
enum class PetColor : uint8_t {
    Red = 0,
    Orange,
    Yellow,
    Green,
    Blue,
    Purple,
    Pink,
    Black,
    White,
    Brown,
    Count
};
constexpr int kColorCount = static_cast<int>(PetColor::Count);

// -----------------------------------------------------------------------------
//  Accessories (cosmetic, unlockable). Stored as a bitmask of unlocks plus the
//  single currently-equipped accessory.
// -----------------------------------------------------------------------------
enum class Accessory : uint8_t {
    None = 0,
    Bow,
    TopHat,
    Crown,
    Scarf,
    Glasses,
    Flower,
    WizardHat,
    Cape,
    Backpack,
    Headphones,
    DogHat,      // redeem code "Belzer"
    Count
};
constexpr int kAccessoryCount = static_cast<int>(Accessory::Count);

// Pet transform forms granted by redeem codes (0 = normal species rendering).
enum class TransformForm : uint8_t {
    None  = 0,
    Bonzi = 1,   // redeem code "Bonzi" (temporary)
};

// -----------------------------------------------------------------------------
//  Styles (full-body theme). Unlockable.
// -----------------------------------------------------------------------------
enum class Style : uint8_t {
    Classic = 0,
    Fantasy,
    Cyber,
    Royal,
    Space,
    Pirate,
    RetroPixel,
    Halloween,
    Winter,
    Spring,
    Count
};
constexpr int kStyleCount = static_cast<int>(Style::Count);

// -----------------------------------------------------------------------------
//  Evolution stages
// -----------------------------------------------------------------------------
enum class EvolutionStage : uint8_t {
    Baby = 0,
    Teen,
    Adult,
    RareForm,
    LegendaryForm,
    Count
};
constexpr int kEvolutionStageCount = static_cast<int>(EvolutionStage::Count);

// Requirements per stage (see PROMPT.md). Index by the *target* stage.
// Baby is the starting stage so its requirement is 0.
struct EvolutionRequirement {
    uint32_t streetpassEncounters; // total StreetPass/NetPass encounters
    uint32_t uniquePetsMet;        // distinct pets met (used by Legendary)
};
constexpr std::array<EvolutionRequirement, kEvolutionStageCount> kEvolutionTable = {{
    /* Baby          */ {   0,    0 },
    /* Teen          */ {  50,    0 },
    /* Adult         */ { 200,    0 },
    /* RareForm      */ { 500,    0 },
    /* LegendaryForm */ {1000, 1000 },
}};

// -----------------------------------------------------------------------------
//  Friendship levels
// -----------------------------------------------------------------------------
enum class FriendshipLevel : uint8_t {
    Stranger = 0,
    Acquaintance,
    Friend,
    BestFriend,
    LegendaryFriend,
    Count
};
constexpr int kFriendshipLevelCount = static_cast<int>(FriendshipLevel::Count);

// Friendship points required to reach each level (cumulative meetings/points).
constexpr std::array<uint32_t, kFriendshipLevelCount> kFriendshipThresholds = {
    /* Stranger        */ 0,
    /* Acquaintance    */ 3,
    /* Friend          */ 10,
    /* BestFriend      */ 30,
    /* LegendaryFriend */ 100,
};

// -----------------------------------------------------------------------------
//  Mood
//  Derived at runtime from the pet's needs (never serialized), so a save can
//  never disagree with how the pet looks. Order is display order, not stored.
// -----------------------------------------------------------------------------
enum class Mood : uint8_t {
    Happy = 0,   // all needs healthy
    Content,     // doing fine
    Hungry,      // fullness is the pressing need
    Tired,       // energy is the pressing need
    Sad,         // happiness is the pressing need / generally neglected
    Count
};
constexpr int kMoodCount = static_cast<int>(Mood::Count);

// -----------------------------------------------------------------------------
//  Locations (unlockable)
// -----------------------------------------------------------------------------
enum class Location : uint8_t {
    Town = 0,
    Forest,
    Beach,
    Arcade,
    Castle,
    SpaceStation,
    Mountain,
    Garden,
    HauntedManor,
    CrystalCave,
    Count
};
constexpr int kLocationCount = static_cast<int>(Location::Count);

// -----------------------------------------------------------------------------
//  Adventure
// -----------------------------------------------------------------------------
enum class AdventureDuration : uint8_t {
    Short = 0,   // minutes
    Medium,
    Long,
    Count
};
constexpr int kAdventureDurationCount = static_cast<int>(AdventureDuration::Count);

// Real-time seconds each duration takes. Tuned short for a handheld pick-up game.
constexpr std::array<uint32_t, kAdventureDurationCount> kAdventureSeconds = {
    /* Short  */  5 * 60,
    /* Medium */ 20 * 60,
    /* Long   */ 60 * 60,
};

// -----------------------------------------------------------------------------
//  Items
//  Item ids are a flat namespace so they pack into save & inventory cleanly.
// -----------------------------------------------------------------------------
enum class ItemCategory : uint8_t {
    Food = 0,
    RareFood,
    Collectible,
    EvolutionMaterial,
    Count
};

enum class ItemId : uint16_t {
    // --- Food ---
    Apple = 0,
    Cookie,
    Cake,
    Fish,
    Berry,
    Carrot,
    IceCream,
    // --- Rare Food ---
    RainbowCookie,
    GoldenApple,
    CosmicCandy,
    // --- Collectibles ---
    Shell,
    Gem,
    Feather,
    Relic,
    Sticker,
    // --- Evolution Materials ---
    EvoShardCommon,
    EvoShardRare,
    EvoShardLegendary,
    Count
};
constexpr int kItemCount = static_cast<int>(ItemId::Count);

// -----------------------------------------------------------------------------
//  Achievements
// -----------------------------------------------------------------------------
enum class AchievementId : uint16_t {
    FirstFriend = 0,
    TenFriends,
    HundredFriends,
    FirstEvolution,
    LegendaryPet,
    WorldTraveler,       // unlock all locations
    AccessoryCollector,  // unlock all accessories
    Explorer,            // complete N adventures
    Count
};
constexpr int kAchievementCount = static_cast<int>(AchievementId::Count);

// -----------------------------------------------------------------------------
//  Core stat caps / tuning
// -----------------------------------------------------------------------------
constexpr uint8_t  kStatMax        = 100;   // happiness, energy & hunger are 0..100
constexpr uint16_t kMaxLevel       = 99;
constexpr uint32_t kXpPerLevelBase = 100;   // see Pet::xpForLevel()

// A need at/under this fraction of full is what drives the pet's Mood.
constexpr uint8_t  kNeedLowThreshold = 30;  // <=30/100 => that need is "pressing"

// Needs decay while the app is closed (and slowly while open). Per real hour, so
// a fully-tended pet drifts toward hungry/tired over ~a day of neglect.
constexpr uint8_t  kHungerDecayPerHour    = 5;  // fullness: ~20h to empty
constexpr uint8_t  kEnergyDecayPerHour    = 4;  // ~25h to empty
constexpr uint8_t  kHappinessDecayPerHour = 3;  // ~33h to empty

// Care streak: a chain of days on which the player tended the pet at least once.
// Hitting a multiple of kStreakMilestoneDays pays a coin bonus.
constexpr uint32_t kStreakMilestoneDays  = 7;
constexpr uint32_t kStreakMilestoneCoins = 50;

// -----------------------------------------------------------------------------
//  Fixed-size string limits (kept small for save & packet predictability)
// -----------------------------------------------------------------------------
constexpr int kMaxPetNameLen   = 12;  // not counting null terminator
constexpr int kMaxNameBuffer   = kMaxPetNameLen + 1;

// Convenience: count helper for any "Count"-terminated enum.
template <typename E>
constexpr int enumCount() { return static_cast<int>(E::Count); }

} // namespace petpal
