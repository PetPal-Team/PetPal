// =============================================================================
//  PetPal - NameFilter.h
//  Client-side pet-name profanity filter. Mirrors the server filter
//  (SITE_exclude .../functions/api/_namefilter.js): it folds leetspeak, symbol,
//  separator, repeated-letter and common accent tricks to a canonical form
//  before matching, so "B!tch", "n1gg@", "f.u.c.k" and "fuuuck" are all caught.
//
//  Used in three places so a bad name never enters the game locally:
//    * Pet::setName          - the player's own pet name
//    * NetPassManager::build  - our outgoing packet (belt & suspenders)
//    * Game::processOneEncounter - names received from other players
// =============================================================================
#pragma once

#include <cstddef>

namespace petpal {

// True if `name` (a UTF-8 C string) reads as profanity after de-obfuscation.
bool isBadName(const char* name);

// If `buf` holds a bad name, overwrite it with a clean placeholder ("Pal"),
// null-padded to `bufSize`. No-op when the name is clean. `buf` must be a
// writable, null-terminated buffer of `bufSize` bytes.
void cleanName(char* buf, std::size_t bufSize);

} // namespace petpal
