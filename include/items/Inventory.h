// =============================================================================
//  PetPal - Inventory.h
//  Owns the player's item counts and coin balance. Item metadata (names,
//  categories) lives in core/Names.h; this class only tracks quantities.
// =============================================================================
#pragma once

#include "core/Types.h"
#include <array>

namespace petpal {

class Inventory {
public:
    Inventory() { counts_.fill(0); }

    // --- Items --------------------------------------------------------------
    uint16_t count(ItemId id) const { return counts_[static_cast<int>(id)]; }
    bool     has(ItemId id)   const { return count(id) > 0; }

    void add(ItemId id, uint16_t qty = 1);
    // Remove up to qty; returns how many were actually removed.
    uint16_t remove(ItemId id, uint16_t qty = 1);

    // Total number of items in a category (handy for journal/UI summaries).
    uint32_t totalInCategory(ItemCategory cat) const;
    int      distinctCollectibles() const; // unique collectible types owned

    // --- Coins --------------------------------------------------------------
    uint32_t coins() const { return coins_; }
    void     addCoins(uint32_t amount) { coins_ += amount; }
    // Spend coins if affordable. Returns false (and changes nothing) if broke.
    bool     spendCoins(uint32_t amount);

    const std::array<uint16_t, kItemCount>& rawCounts() const { return counts_; }

private:
    std::array<uint16_t, kItemCount> counts_;
    uint32_t coins_ = 0;

    friend class SaveManager;
};

} // namespace petpal
