// =============================================================================
//  PetPal - FriendList.h
//  Collection of all pets the player has met. Deduplicates by pet id so meeting
//  the same pet again updates the existing record instead of adding a new one.
// =============================================================================
#pragma once

#include "friends/Friend.h"
#include <vector>

namespace petpal {

struct PetPalPacket;

// Outcome of ingesting a packet, so callers can drive journal + reward logic.
struct MeetingOutcome {
    bool      isNewFriend   = false; // first time meeting this pet id
    bool      levelledUp    = false; // friendship level increased
    uint64_t  friendId      = 0;
    int       friendIndex   = -1;    // index into the list
};

class FriendList {
public:
    // Cap chosen so the friends block stays within a sane save budget.
    static constexpr int kMaxFriends = 512;

    int  size()  const { return static_cast<int>(friends_.size()); }
    bool empty() const { return friends_.empty(); }

    const Friend& at(int i) const { return friends_[i]; }
    Friend&       at(int i)       { return friends_[i]; }

    // Find by pet id; returns nullptr if not met yet.
    const Friend* find(uint64_t id) const;

    // Ingest a received packet, creating or updating a friend record.
    MeetingOutcome ingest(const PetPalPacket& pkt, Timestamp metAt);

    // Counts for achievements / stats.
    int countAtLeast(FriendshipLevel level) const;

    const std::vector<Friend>& all() const { return friends_; }

private:
    std::vector<Friend> friends_;
    friend class SaveManager;
};

} // namespace petpal
