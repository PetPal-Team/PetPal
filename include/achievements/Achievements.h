// =============================================================================
//  PetPal - Achievements.h
//  Tracks which achievements the player has earned and what cosmetic each one
//  unlocks. The manager is fed a lightweight snapshot of game progress and
//  reports any newly-unlocked achievements so the UI/journal can celebrate.
//
//  (This is the "Achievement" system from the spec; the manager owns the set
//   of individual achievement records via a unlock bitmask.)
// =============================================================================
#pragma once

#include "core/Types.h"
#include <vector>

namespace petpal {

// The kind of cosmetic an achievement grants.
enum class RewardKind : uint8_t { Accessory, Style };

struct AchievementReward {
    RewardKind kind;
    uint8_t    value; // Accessory or Style enum value
};

// Read-only snapshot of progress used to evaluate unlock conditions. Decouples
// the achievement rules from the rest of the game's classes.
struct AchievementContext {
    int            totalFriends        = 0;
    EvolutionStage petStage            = EvolutionStage::Baby;
    int            locationsUnlocked   = 0; // out of kLocationCount
    int            accessoriesUnlocked = 0; // out of kAccessoryCount-1 (excl. None)
    uint32_t       adventuresCompleted = 0;
};

class Achievements {
public:
    Achievements() = default;

    bool isUnlocked(AchievementId id) const {
        return (mask_ >> static_cast<int>(id)) & 1u;
    }
    int unlockedCount() const;

    // The cosmetic granted by an achievement.
    static AchievementReward rewardFor(AchievementId id);

    // Evaluate all conditions against the snapshot, unlocking any newly-earned
    // achievements. Returns the list of ids unlocked *this call* (may be empty).
    std::vector<AchievementId> evaluate(const AchievementContext& ctx);

    uint32_t rawMask() const { return mask_; }

private:
    void unlock(AchievementId id) { mask_ |= (1u << static_cast<int>(id)); }
    uint32_t mask_ = 0; // bit i set => AchievementId(i) unlocked

    friend class SaveManager;
};

} // namespace petpal
