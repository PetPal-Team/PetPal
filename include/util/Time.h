// =============================================================================
//  PetPal - Time.h
//  Thin wrapper over wall-clock time. Stored timestamps are Unix seconds so the
//  same value is meaningful in saves and journal entries. On the 3DS this uses
//  libctru's time(); on a PC build it falls back to std::time for unit testing.
// =============================================================================
#pragma once

#include <cstdint>
#include <ctime>

namespace petpal {

using Timestamp = uint64_t; // Unix seconds (UTC)

inline Timestamp nowSeconds() {
    return static_cast<Timestamp>(::time(nullptr));
}

// Whole days between two timestamps (b - a), clamped at 0.
inline uint32_t daysBetween(Timestamp a, Timestamp b) {
    if (b <= a) return 0;
    return static_cast<uint32_t>((b - a) / 86400ULL);
}

// Format as "YYYY-MM-DD" into a caller-provided buffer (>=11 bytes).
void formatDate(Timestamp t, char* out, int outSize);

} // namespace petpal
