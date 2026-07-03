// =============================================================================
//  PetPal - RedeemManager.h
//  Redeems a player-entered code against the PetPal server
//  (https://teampetpal.com/api/redeem) and returns the reward the server grants.
//
//  SECURITY: the 3DS holds NO secrets. It only sends the typed code + its public
//  pet id; the server decides validity, rewards, and one-time-use. A leaked or
//  reverse-engineered binary therefore exposes nothing about the site. SSL cert
//  verification is intentionally disabled (no secret is transmitted, so a MITM
//  could at most grant the local player a reward - harmless for a single-player
//  pet game) which also avoids the 3DS's outdated root-CA store rejecting modern
//  Cloudflare certificates.
// =============================================================================
#pragma once

#include "core/Types.h"
#include <cstdint>
#include <string>

namespace petpal {

struct RedeemResult {
    bool        ok = false;        // true only on a successful grant
    std::string message;           // human-readable result (always set)

    enum class Reward { None, Items, Accessory, Transform } reward = Reward::None;

    ItemId        item      = ItemId::Apple;       // Reward::Items
    uint16_t      qty       = 0;
    Accessory     accessory = Accessory::None;     // Reward::Accessory
    TransformForm form      = TransformForm::None; // Reward::Transform
    uint32_t      seconds   = 0;                   // transform duration
};

class RedeemManager {
public:
    // Blocking HTTPS round-trip. Returns a populated RedeemResult; on any
    // network/parse failure, ok=false and message explains why.
    static RedeemResult redeem(const char* code, uint64_t petId);
};

} // namespace petpal
