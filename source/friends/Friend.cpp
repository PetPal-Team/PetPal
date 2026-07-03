#include "friends/Friend.h"
#include "netpass/PetPalPacket.h"

namespace petpal {

Friend Friend::fromPacket(const PetPalPacket& pkt, Timestamp metAt) {
    Friend f;
    f.id_           = pkt.petId;
    f.name_.assign(pkt.name);
    f.species_      = static_cast<Species>(pkt.species);
    f.level_        = pkt.level;
    f.stage_        = static_cast<EvolutionStage>(pkt.stage);
    f.primary_      = static_cast<PetColor>(pkt.primaryColor);
    f.style_        = static_cast<Style>(pkt.style);
    f.favoriteItem_ = static_cast<ItemId>(pkt.favoriteItem);
    f.dateMet_      = metAt;
    f.meetings_     = 1;
    f.friendshipPoints_ = 1;
    return f;
}

FriendshipLevel Friend::friendshipLevel() const {
    // Highest threshold we meet or exceed.
    FriendshipLevel level = FriendshipLevel::Stranger;
    for (int i = 0; i < kFriendshipLevelCount; ++i) {
        if (friendshipPoints_ >= kFriendshipThresholds[i])
            level = static_cast<FriendshipLevel>(i);
    }
    return level;
}

bool Friend::recordMeeting(const PetPalPacket& pkt, Timestamp metAt) {
    const FriendshipLevel before = friendshipLevel();

    // Refresh the appearance snapshot - friends grow over time too.
    name_.assign(pkt.name);
    species_      = static_cast<Species>(pkt.species);
    level_        = pkt.level;
    stage_        = static_cast<EvolutionStage>(pkt.stage);
    primary_      = static_cast<PetColor>(pkt.primaryColor);
    style_        = static_cast<Style>(pkt.style);
    favoriteItem_ = static_cast<ItemId>(pkt.favoriteItem);

    ++meetings_;
    ++friendshipPoints_;
    (void)metAt; // dateMet_ keeps the *first* meeting date

    return friendshipLevel() != before;
}

} // namespace petpal
