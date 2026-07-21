// =============================================================================
//  PetPal - AccountClient.h
//  Talks to teampetpal.com's account endpoints so the 3DS gets a server-minted
//  pet ID, can ask "am I banned?" at boot, and can link to a phone account.
//
//  Device build uses 3ds-curl (like RedeemManager); the host build stubs every
//  call out (returns "unknown"/false), so gameplay/tests still compile off-device.
//  Nothing here holds a compile-time secret - the per-device token is minted at
//  runtime and lives only in the SD save.
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace petpal {

struct RegisterResult {
    bool        ok = false;
    std::string id;      // "PP-XXXX-XXXX"
    std::string token;   // per-device credential
};

// Tri-state so the boot check can FAIL OPEN: only an explicit Yes blocks play;
// any network/parse problem is Unknown (treated as "let them in").
enum class BanState { Unknown, No, Yes };

// A minimal cross-device pet snapshot (identity + stats). species/stage are enum
// ordinals shared with the Android app and the save format. -1 == "unset".
struct ServerPet {
    std::string        name;
    int                species = -1, stage = -1, level = -1;
    int                happiness = -1, energy = -1, hunger = -1, coins = -1, encounters = -1;
    unsigned long long updatedAt = 0; // server ms; 0 == no pet on the server yet
};

class AccountClient {
public:
    // POST /api/register  {device} -> {id, token}. device e.g. "3ds".
    static RegisterResult registerDevice(const char* device);

    // GET /api/petstatus?id=&token=&device=[&name=] -> banned? Unknown on any error.
    // petName is optional (nullptr/"" to skip); when the token matches, the server
    // stores it so the admin panel can show the 3DS pet's name. If outBadges is
    // non-null it's filled with the pet's profile badge labels (for display).
    static BanState checkBanned(const char* id, const char* token, const char* device,
                               const char* petName = nullptr,
                               std::vector<std::string>* outBadges = nullptr);

    // POST /api/link {id, token, targetId}. True when the phone account accepted
    // the link (caller should then adopt targetId locally).
    static bool link(const char* id, const char* token, const char* targetId);

    // --- Cross-device continuity (linked accounts) --------------------------
    // GET /api/account -> fill `out` from the stored pet. True on HTTP 200 + ok;
    // out.updatedAt==0 means the server has no pet yet.
    static bool fetchPet(const char* id, const char* token, ServerPet& out);

    // POST /api/account {pet:{...}} -> push our pet snapshot. True on success.
    // When outUpdatedAt is non-null it receives the server's authoritative write
    // timestamp, which the caller must store as its sync marker (never its own
    // clock — the 3DS RTC is often wrong). See Game::pushPetToServer.
    static bool savePet(const char* id, const char* token, const ServerPet& in,
                        unsigned long long* outUpdatedAt = nullptr);
};

} // namespace petpal
