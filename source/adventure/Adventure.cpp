#include "adventure/Adventure.h"
#include "core/Names.h"
#include "util/Random.h"
#include <algorithm>
#include <cstdio>

namespace petpal {

bool Adventure::begin(Location loc, AdventureDuration dur, Timestamp now) {
    if (active_) return false;
    active_    = true;
    location_  = loc;
    duration_  = dur;
    startTime_ = now;
    return true;
}

uint32_t Adventure::secondsRemaining(Timestamp now) const {
    if (!active_) return 0;
    const uint32_t total = kAdventureSeconds[static_cast<int>(duration_)];
    const Timestamp end = startTime_ + total;
    if (now >= end) return 0;
    return static_cast<uint32_t>(end - now);
}

float Adventure::progress(Timestamp now) const {
    if (!active_) return 0.0f;
    const uint32_t total = kAdventureSeconds[static_cast<int>(duration_)];
    if (total == 0) return 1.0f;
    const uint32_t remaining = secondsRemaining(now);
    return 1.0f - static_cast<float>(remaining) / static_cast<float>(total);
}

AdventureResult Adventure::collect(const char* petName, Timestamp now, Random& rng) {
    if (!isComplete(now)) return AdventureResult{};
    AdventureResult result = rollAdventureRewards(petName, location_, duration_, rng);
    active_ = false;
    return result;
}

// -----------------------------------------------------------------------------
//  Loot tables
//  Longer adventures award more coins and roll for rarer drops. Each location
//  biases the collectible it yields, giving every place a distinct flavor.
// -----------------------------------------------------------------------------
namespace {

// Coin range per duration.
struct CoinRange { int lo, hi; };
const CoinRange kCoins[kAdventureDurationCount] = {
    { 5, 15 },   // Short
    { 20, 45 },  // Medium
    { 60, 120 }, // Long
};

// Signature collectible most likely to drop at each location.
ItemId locationCollectible(Location loc) {
    switch (loc) {
        case Location::Beach:        return ItemId::Shell;
        case Location::CrystalCave:  return ItemId::Gem;
        case Location::Mountain:     return ItemId::Feather;
        case Location::Castle:
        case Location::HauntedManor: return ItemId::Relic;
        default:                     return ItemId::Sticker;
    }
}

// A common food drop themed loosely by place.
ItemId locationFood(Location loc) {
    switch (loc) {
        case Location::Garden: return ItemId::Berry;
        case Location::Forest: return ItemId::Carrot;
        case Location::Beach:  return ItemId::Fish;
        case Location::Arcade: return ItemId::IceCream;
        default:               return ItemId::Apple;
    }
}

} // namespace

AdventureResult rollAdventureRewards(const char* petName, Location loc,
                                     AdventureDuration dur, Random& rng) {
    AdventureResult r;
    r.valid = true;
    r.location = loc;

    const int d = static_cast<int>(dur);

    // Coins always.
    r.coins = static_cast<uint32_t>(rng.rangeInclusive(kCoins[d].lo, kCoins[d].hi));

    // Headline drop - what the story brags about.
    ItemId headline;
    // Rare food / evolution materials become more likely with longer trips.
    const int rareChance = 5 + d * 12; // Short 5%, Medium 17%, Long 29%
    if (rng.chance(rareChance)) {
        static const ItemId rares[] = {
            ItemId::RainbowCookie, ItemId::GoldenApple, ItemId::CosmicCandy,
            ItemId::EvoShardCommon, ItemId::EvoShardRare, ItemId::EvoShardLegendary,
        };
        // Bias toward higher tiers on long adventures.
        int maxIndex = (d == 0) ? 3 : (d == 1 ? 5 : 6);
        headline = rares[rng.range(maxIndex)];
    } else if (rng.chance(55)) {
        headline = locationCollectible(loc);
    } else {
        headline = locationFood(loc);
    }
    r.items.push_back({ headline, 1 });

    // Bonus filler drops scale with duration.
    const int bonusDrops = rng.rangeInclusive(0, d + 1);
    for (int i = 0; i < bonusDrops; ++i) {
        const ItemId extra = rng.chance(50) ? locationFood(loc) : locationCollectible(loc);
        r.items.push_back({ extra, static_cast<uint16_t>(rng.rangeInclusive(1, 2)) });
    }

    // Story line in the pet's voice.
    static const char* kVerbs[] = { "explored", "wandered through", "adventured in", "discovered" };
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s %s the %s and found a %s.",
                  petName ? petName : "Your pet",
                  kVerbs[rng.range(4)],
                  locationName(loc),
                  itemName(headline));
    r.story.assign(buf);
    return r;
}

} // namespace petpal
