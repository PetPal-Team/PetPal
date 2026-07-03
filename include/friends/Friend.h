// =============================================================================
//  PetPal - Friend.h
//  A record of another player's pet we have met via StreetPass / NetPass.
//  Friends are derived from the data exchanged in a PetPalPacket plus our own
//  bookkeeping (when/how often we've met, accumulated friendship).
// =============================================================================
#pragma once

#include "core/Types.h"
#include "util/Time.h"
#include <string>

namespace petpal {

struct PetPalPacket; // forward decl (netpass/PetPalPacket.h)

class Friend {
public:
    Friend() = default;

    // Build a friend record from a freshly-received packet.
    static Friend fromPacket(const PetPalPacket& pkt, Timestamp metAt);

    // --- Identity / appearance (snapshot from last meeting) -----------------
    uint64_t       id()       const { return id_; }
    const char*    name()     const { return name_.c_str(); }
    Species        species()  const { return species_; }
    uint16_t       level()    const { return level_; }
    EvolutionStage stage()    const { return stage_; }
    PetColor       primaryColor() const { return primary_; }
    Style          style()    const { return style_; }
    ItemId         favoriteItem() const { return favoriteItem_; }

    // --- Relationship -------------------------------------------------------
    Timestamp dateMet()   const { return dateMet_; }
    uint32_t  meetings()  const { return meetings_; }
    uint32_t  friendshipPoints() const { return friendshipPoints_; }
    FriendshipLevel friendshipLevel() const;

    // Register another meeting, refreshing the appearance snapshot from a new
    // packet and bumping counters. Returns true if the friendship level rose.
    bool recordMeeting(const PetPalPacket& pkt, Timestamp metAt);

private:
    uint64_t       id_ = 0;
    std::string    name_;
    Species        species_ = Species::Fox;
    uint16_t       level_   = 1;
    EvolutionStage stage_   = EvolutionStage::Baby;
    PetColor       primary_ = PetColor::Orange;
    Style          style_   = Style::Classic;
    ItemId         favoriteItem_ = ItemId::Cookie;

    Timestamp dateMet_   = 0;
    uint32_t  meetings_  = 0;
    uint32_t  friendshipPoints_ = 0;

    friend class SaveManager;
};

} // namespace petpal
