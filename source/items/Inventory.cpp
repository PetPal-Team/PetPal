#include "items/Inventory.h"
#include "core/Names.h"
#include <algorithm>

namespace petpal {

void Inventory::add(ItemId id, uint16_t qty) {
    const int i = static_cast<int>(id);
    // Saturate rather than overflow the uint16_t count.
    uint32_t v = static_cast<uint32_t>(counts_[i]) + qty;
    counts_[i] = static_cast<uint16_t>(std::min<uint32_t>(v, 0xFFFFu));
}

uint16_t Inventory::remove(ItemId id, uint16_t qty) {
    const int i = static_cast<int>(id);
    const uint16_t removed = std::min(counts_[i], qty);
    counts_[i] = static_cast<uint16_t>(counts_[i] - removed);
    return removed;
}

uint32_t Inventory::totalInCategory(ItemCategory cat) const {
    uint32_t total = 0;
    for (int i = 0; i < kItemCount; ++i)
        if (itemCategory(static_cast<ItemId>(i)) == cat)
            total += counts_[i];
    return total;
}

int Inventory::distinctCollectibles() const {
    int n = 0;
    for (int i = 0; i < kItemCount; ++i) {
        const ItemId id = static_cast<ItemId>(i);
        if (itemCategory(id) == ItemCategory::Collectible && counts_[i] > 0) ++n;
    }
    return n;
}

bool Inventory::spendCoins(uint32_t amount) {
    if (coins_ < amount) return false;
    coins_ -= amount;
    return true;
}

} // namespace petpal
