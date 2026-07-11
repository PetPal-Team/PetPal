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

namespace petpal {

struct RegisterResult {
    bool        ok = false;
    std::string id;      // "PP-XXXX-XXXX"
    std::string token;   // per-device credential
};

// Tri-state so the boot check can FAIL OPEN: only an explicit Yes blocks play;
// any network/parse problem is Unknown (treated as "let them in").
enum class BanState { Unknown, No, Yes };

class AccountClient {
public:
    // POST /api/register  {device} -> {id, token}. device e.g. "3ds".
    static RegisterResult registerDevice(const char* device);

    // GET /api/petstatus?id=&token=&device=[&name=] -> banned? Unknown on any error.
    // petName is optional (nullptr/"" to skip); when the token matches, the server
    // stores it so the admin panel can show the 3DS pet's name (there's no
    // separate 3DS pet-sync).
    static BanState checkBanned(const char* id, const char* token, const char* device,
                               const char* petName = nullptr);

    // POST /api/link {id, token, targetId}. True when the phone account accepted
    // the link (caller should then adopt targetId locally).
    static bool link(const char* id, const char* token, const char* targetId);
};

} // namespace petpal
