#include "achievements/Achievements.h"

namespace petpal {

int Achievements::unlockedCount() const {
    int n = 0;
    for (int i = 0; i < kAchievementCount; ++i)
        if (isUnlocked(static_cast<AchievementId>(i))) ++n;
    return n;
}

AchievementReward Achievements::rewardFor(AchievementId id) {
    switch (id) {
        case AchievementId::FirstFriend:         return { RewardKind::Accessory, (uint8_t)Accessory::Bow };
        case AchievementId::TenFriends:          return { RewardKind::Accessory, (uint8_t)Accessory::Scarf };
        case AchievementId::HundredFriends:      return { RewardKind::Accessory, (uint8_t)Accessory::Crown };
        case AchievementId::FirstEvolution:      return { RewardKind::Style,     (uint8_t)Style::Fantasy };
        case AchievementId::LegendaryPet:        return { RewardKind::Style,     (uint8_t)Style::Royal };
        case AchievementId::WorldTraveler:       return { RewardKind::Accessory, (uint8_t)Accessory::Backpack };
        case AchievementId::AccessoryCollector:  return { RewardKind::Style,     (uint8_t)Style::Cyber };
        case AchievementId::Explorer:            return { RewardKind::Accessory, (uint8_t)Accessory::Glasses };
        default:                                 return { RewardKind::Accessory, (uint8_t)Accessory::Bow };
    }
}

std::vector<AchievementId> Achievements::evaluate(const AchievementContext& ctx) {
    std::vector<AchievementId> newly;

    auto tryUnlock = [&](AchievementId id, bool condition) {
        if (condition && !isUnlocked(id)) {
            unlock(id);
            newly.push_back(id);
        }
    };

    tryUnlock(AchievementId::FirstFriend,    ctx.totalFriends >= 1);
    tryUnlock(AchievementId::TenFriends,     ctx.totalFriends >= 10);
    tryUnlock(AchievementId::HundredFriends, ctx.totalFriends >= 100);
    tryUnlock(AchievementId::FirstEvolution, static_cast<int>(ctx.petStage) >= static_cast<int>(EvolutionStage::Teen));
    tryUnlock(AchievementId::LegendaryPet,   ctx.petStage == EvolutionStage::LegendaryForm);
    tryUnlock(AchievementId::WorldTraveler,  ctx.locationsUnlocked >= kLocationCount);
    tryUnlock(AchievementId::AccessoryCollector, ctx.accessoriesUnlocked >= (kAccessoryCount - 1));
    tryUnlock(AchievementId::Explorer,       ctx.adventuresCompleted >= 25);

    return newly;
}

} // namespace petpal
