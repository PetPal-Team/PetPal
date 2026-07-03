#include "friends/FriendList.h"
#include "netpass/PetPalPacket.h"

namespace petpal {

const Friend* FriendList::find(uint64_t id) const {
    for (const auto& f : friends_)
        if (f.id() == id) return &f;
    return nullptr;
}

MeetingOutcome FriendList::ingest(const PetPalPacket& pkt, Timestamp metAt) {
    MeetingOutcome out;
    out.friendId = pkt.petId;

    // Update an existing record if we've met this pet before.
    for (int i = 0; i < static_cast<int>(friends_.size()); ++i) {
        if (friends_[i].id() == pkt.petId) {
            out.levelledUp  = friends_[i].recordMeeting(pkt, metAt);
            out.friendIndex = i;
            out.isNewFriend = false;
            return out;
        }
    }

    // New friend. Respect the cap by dropping the least-recently-met stranger
    // when full (keeps the social graph fresh without unbounded save growth).
    if (static_cast<int>(friends_.size()) >= kMaxFriends) {
        int victim = 0;
        for (int i = 1; i < static_cast<int>(friends_.size()); ++i)
            if (friends_[i].friendshipPoints() < friends_[victim].friendshipPoints())
                victim = i;
        friends_[victim] = Friend::fromPacket(pkt, metAt);
        out.friendIndex = victim;
    } else {
        friends_.push_back(Friend::fromPacket(pkt, metAt));
        out.friendIndex = static_cast<int>(friends_.size()) - 1;
    }
    out.isNewFriend = true;
    out.levelledUp  = true; // Stranger -> Acquaintance happens at points>=... handled by level
    return out;
}

int FriendList::countAtLeast(FriendshipLevel level) const {
    int n = 0;
    for (const auto& f : friends_)
        if (static_cast<int>(f.friendshipLevel()) >= static_cast<int>(level)) ++n;
    return n;
}

} // namespace petpal
