// =============================================================================
//  PetPal - AnimationManager.h
//  Time-based tweening + easing for the bouncy, lively UI the spec calls for.
//  Screens create named Tweens (button presses, menu slides, pet idle bob) and
//  the manager advances them every frame. Pure math - no rendering - so it is
//  testable and reusable.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace petpal {

// -----------------------------------------------------------------------------
//  Easing functions (t in [0,1] -> eased [0,1], some overshoot >1 for bounce).
// -----------------------------------------------------------------------------
namespace easing {
    float linear(float t);
    float easeInQuad(float t);
    float easeOutQuad(float t);
    float easeInOutQuad(float t);
    float easeOutBack(float t);    // overshoots then settles (bouncy buttons)
    float easeOutBounce(float t);  // multi-bounce landing
    float easeOutElastic(float t); // springy
}

using EaseFn = float(*)(float);

// -----------------------------------------------------------------------------
//  A single value animating from->to over a duration.
// -----------------------------------------------------------------------------
class Tween {
public:
    Tween() = default;
    Tween(float from, float to, float duration, EaseFn ease = easing::easeOutQuad)
        : from_(from), to_(to), duration_(duration), ease_(ease) {}

    void   restart()           { elapsed_ = 0.0f; done_ = false; }
    void   setLoop(bool loop)  { loop_ = loop; }
    bool   done() const        { return done_; }
    float  value() const       { return value_; }

    // Advance by dt seconds; returns the current eased value.
    float update(float dt);

private:
    float  from_ = 0, to_ = 0, value_ = 0;
    float  duration_ = 0.25f, elapsed_ = 0.0f;
    EaseFn ease_ = easing::linear;
    bool   loop_ = false, done_ = false;
};

// -----------------------------------------------------------------------------
//  Manager: owns named tweens and a global animation time for idle effects.
// -----------------------------------------------------------------------------
class AnimationManager {
public:
    // Advance all registered tweens and the global clock.
    void update(float dt);

    // Create/replace a named tween. Returns a reference you can read each frame.
    Tween& play(const std::string& key, float from, float to, float duration,
                EaseFn ease = easing::easeOutBack);

    // Look up a tween's current value, or `fallback` if it doesn't exist.
    float valueOf(const std::string& key, float fallback = 0.0f) const;
    bool  has(const std::string& key) const;
    void  remove(const std::string& key);
    void  clear();

    // Seconds since the manager started - drive looping idle motion off this
    // (e.g. sinf(time*speed) for a gentle pet bob) without a stored tween.
    float time() const { return globalTime_; }

private:
    std::unordered_map<std::string, Tween> tweens_;
    float globalTime_ = 0.0f;
};

} // namespace petpal
