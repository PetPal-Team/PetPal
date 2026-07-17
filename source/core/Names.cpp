// =============================================================================
//  PetPal - Names.cpp
//  Lookup tables backing core/Names.h. All arrays are indexed by the enum's
//  integer value, so they MUST stay in sync with Types.h ordering.
// =============================================================================
#include "core/Names.h"

namespace petpal {

// -----------------------------------------------------------------------------
//  Species
// -----------------------------------------------------------------------------
static const char* kSpeciesNames[kSpeciesCount] = {
    "Fox", "Cat", "Bunny", "Dragon", "Slime", "Robot", "Axolotl"
};

static const char* kSpeciesDesc[kSpeciesCount] = {
    "A clever little fox with a fluffy tail.",
    "An independent cat who naps in sunbeams.",
    "A gentle bunny with boundless curiosity.",
    "A proud dragon hatchling chasing legends.",
    "A bouncy slime that wobbles with delight.",
    "A tidy robot cataloguing the whole world.",
    "A smiley axolotl who loves cool water.",
};

static const char* kSpeciesPersonality[kSpeciesCount] = {
    "Curious, energetic, and loves exploring.",
    "Calm, picky, and secretly affectionate.",
    "Sweet, shy, and always making friends.",
    "Brave, adventurous, and loves treasure.",
    "Playful, squishy, and endlessly cheerful.",
    "Logical, collects gadgets, loves technology.",
    "Relaxed, friendly, and a little mischievous.",
};

const char* speciesName(Species s)        { return kSpeciesNames[static_cast<int>(s)]; }
const char* speciesDescription(Species s) { return kSpeciesDesc[static_cast<int>(s)]; }
const char* speciesPersonality(Species s) { return kSpeciesPersonality[static_cast<int>(s)]; }

// -----------------------------------------------------------------------------
//  Colors
// -----------------------------------------------------------------------------
static const char* kColorNames[kColorCount] = {
    "Red", "Orange", "Yellow", "Green", "Blue",
    "Purple", "Pink", "Black", "White", "Brown"
};

// 0xRRGGBBAA. Bright, toy-like palette to match the playful art direction.
static const uint32_t kColorRGBA[kColorCount] = {
    0xE74C3CFF, // Red
    0xF39C12FF, // Orange
    0xF1C40FFF, // Yellow
    0x2ECC71FF, // Green
    0x3498DBFF, // Blue
    0x9B59B6FF, // Purple
    0xFF8FC7FF, // Pink
    0x2C3E50FF, // Black (soft charcoal so it never reads as a hole)
    0xF7F9FBFF, // White
    0x9B6B43FF, // Brown
};

const char* colorName(PetColor c) { return kColorNames[static_cast<int>(c)]; }
uint32_t    colorRGBA(PetColor c) { return kColorRGBA[static_cast<int>(c)]; }

// -----------------------------------------------------------------------------
//  Accessories & Styles
// -----------------------------------------------------------------------------
static const char* kAccessoryNames[kAccessoryCount] = {
    "None", "Bow", "Top Hat", "Crown", "Scarf", "Glasses",
    "Flower", "Wizard Hat", "Cape", "Backpack", "Headphones", "Dog Hat"
};
const char* accessoryName(Accessory a) { return kAccessoryNames[static_cast<int>(a)]; }

static const char* kStyleNames[kStyleCount] = {
    "Classic", "Fantasy", "Cyber", "Royal", "Space",
    "Pirate", "Retro Pixel", "Halloween", "Winter", "Spring"
};
const char* styleName(Style s) { return kStyleNames[static_cast<int>(s)]; }

// -----------------------------------------------------------------------------
//  Stages / friendship / locations / durations
// -----------------------------------------------------------------------------
static const char* kStageNames[kEvolutionStageCount] = {
    "Baby", "Teen", "Adult", "Rare Form", "Legendary Form"
};
const char* evolutionStageName(EvolutionStage e) { return kStageNames[static_cast<int>(e)]; }

static const char* kFriendshipNames[kFriendshipLevelCount] = {
    "Stranger", "Acquaintance", "Friend", "Best Friend", "Legendary Friend"
};
const char* friendshipLevelName(FriendshipLevel f) { return kFriendshipNames[static_cast<int>(f)]; }

static const char* kMoodNames[kMoodCount] = {
    "Happy", "Content", "Hungry", "Tired", "Sad"
};
const char* moodName(Mood m) { return kMoodNames[static_cast<int>(m)]; }

static const char* kLocationNames[kLocationCount] = {
    "Town", "Forest", "Beach", "Arcade", "Castle",
    "Space Station", "Mountain", "Garden", "Haunted Manor", "Crystal Cave"
};
const char* locationName(Location l) { return kLocationNames[static_cast<int>(l)]; }

static const char* kDurationNames[kAdventureDurationCount] = {
    "Short", "Medium", "Long"
};
const char* adventureDurationName(AdventureDuration d) { return kDurationNames[static_cast<int>(d)]; }

// -----------------------------------------------------------------------------
//  Items
// -----------------------------------------------------------------------------
static const char* kItemNames[kItemCount] = {
    "Apple", "Cookie", "Cake", "Fish", "Berry", "Carrot", "Ice Cream",
    "Rainbow Cookie", "Golden Apple", "Cosmic Candy",
    "Shell", "Gem", "Feather", "Relic", "Sticker",
    "Common Shard", "Rare Shard", "Legendary Shard"
};

static const char* kItemDesc[kItemCount] = {
    "A crisp apple. A reliable snack.",
    "A sweet cookie everyone loves.",
    "A fluffy slice of cake for special days.",
    "A fresh fish. Cats approve.",
    "A juicy forest berry.",
    "A crunchy garden carrot.",
    "A cold treat for warm afternoons.",
    "A shimmering cookie. Tastes like joy.",
    "A radiant apple of legend.",
    "Candy from the stars. Very rare.",
    "A pretty seashell from the beach.",
    "A glittering gemstone.",
    "A soft, iridescent feather.",
    "An ancient relic full of mystery.",
    "A collectible sticker for your journal.",
    "A common shard humming with energy.",
    "A rare shard pulsing with light.",
    "A legendary shard of pure power.",
};

static const ItemCategory kItemCategories[kItemCount] = {
    ItemCategory::Food, ItemCategory::Food, ItemCategory::Food, ItemCategory::Food,
    ItemCategory::Food, ItemCategory::Food, ItemCategory::Food,
    ItemCategory::RareFood, ItemCategory::RareFood, ItemCategory::RareFood,
    ItemCategory::Collectible, ItemCategory::Collectible, ItemCategory::Collectible,
    ItemCategory::Collectible, ItemCategory::Collectible,
    ItemCategory::EvolutionMaterial, ItemCategory::EvolutionMaterial, ItemCategory::EvolutionMaterial,
};

const char*  itemName(ItemId id)        { return kItemNames[static_cast<int>(id)]; }
const char*  itemDescription(ItemId id) { return kItemDesc[static_cast<int>(id)]; }
ItemCategory itemCategory(ItemId id)    { return kItemCategories[static_cast<int>(id)]; }

// -----------------------------------------------------------------------------
//  Achievements
// -----------------------------------------------------------------------------
static const char* kAchievementNames[kAchievementCount] = {
    "First Friend", "10 Friends", "100 Friends", "First Evolution",
    "Legendary Pet", "World Traveler", "Accessory Collector", "Explorer"
};

static const char* kAchievementDesc[kAchievementCount] = {
    "Meet your very first pet.",
    "Make 10 friends.",
    "Make 100 friends.",
    "Evolve your pet for the first time.",
    "Reach the Legendary evolution form.",
    "Unlock every location.",
    "Unlock every accessory.",
    "Complete 25 adventures.",
};

const char* achievementName(AchievementId id)        { return kAchievementNames[static_cast<int>(id)]; }
const char* achievementDescription(AchievementId id) { return kAchievementDesc[static_cast<int>(id)]; }

} // namespace petpal
