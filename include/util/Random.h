// =============================================================================
//  PetPal - Random.h
//  A tiny, dependency-free PRNG (xoshiro128**-style splitmix seed). We avoid
//  <random> to keep code size down on the 3DS and to make results reproducible
//  for adventure rolls. Use a single shared instance via Random::global().
// =============================================================================
#pragma once

#include <cstdint>

namespace petpal {

class Random {
public:
    explicit Random(uint64_t seed = 0x9E3779B97F4A7C15ULL) { reseed(seed); }

    void reseed(uint64_t seed) {
        // SplitMix64 to fill the state from a single seed.
        for (auto& s : state_) {
            seed += 0x9E3779B97F4A7C15ULL;
            uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            s = static_cast<uint32_t>((z ^ (z >> 31)) & 0xFFFFFFFFu);
        }
    }

    // Raw 32-bit value.
    uint32_t next() {
        const uint32_t result = rotl(state_[1] * 5u, 7) * 9u;
        const uint32_t t = state_[1] << 9;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = rotl(state_[3], 11);
        return result;
    }

    // Uniform integer in [0, bound). bound==0 returns 0.
    uint32_t range(uint32_t bound) { return bound ? (next() % bound) : 0; }

    // Uniform integer in [lo, hi] inclusive.
    int rangeInclusive(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + static_cast<int>(range(static_cast<uint32_t>(hi - lo + 1)));
    }

    // True with probability percent/100.
    bool chance(int percent) { return static_cast<int>(range(100)) < percent; }

    // Pick a random index < count.
    template <typename Enum>
    Enum pickEnum(int count) { return static_cast<Enum>(range(static_cast<uint32_t>(count))); }

    static Random& global() {
        static Random instance;
        return instance;
    }

private:
    static uint32_t rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }
    uint32_t state_[4];
};

} // namespace petpal
