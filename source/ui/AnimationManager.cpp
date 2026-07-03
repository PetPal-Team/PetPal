#include "ui/AnimationManager.h"
#include <cmath>

namespace petpal {

// -----------------------------------------------------------------------------
//  Easing
// -----------------------------------------------------------------------------
namespace easing {

static float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }

float linear(float t)       { return clamp01(t); }
float easeInQuad(float t)   { t = clamp01(t); return t * t; }
float easeOutQuad(float t)  { t = clamp01(t); return 1.0f - (1.0f - t) * (1.0f - t); }
float easeInOutQuad(float t) {
    t = clamp01(t);
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float easeOutBack(float t) {
    // Overshoots slightly past 1.0 then settles - the classic UI "pop".
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    const float u = clamp01(t) - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

float easeOutBounce(float t) {
    t = clamp01(t);
    const float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.0f / d1)        return n1 * t * t;
    else if (t < 2.0f / d1) { t -= 1.5f / d1;  return n1 * t * t + 0.75f; }
    else if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
    else                    { t -= 2.625f / d1; return n1 * t * t + 0.984375f; }
}

float easeOutElastic(float t) {
    t = clamp01(t);
    if (t == 0.0f || t == 1.0f) return t;
    const float c4 = (2.0f * 3.14159265f) / 3.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

} // namespace easing

// -----------------------------------------------------------------------------
//  Tween
// -----------------------------------------------------------------------------
float Tween::update(float dt) {
    if (done_ && !loop_) return value_;
    elapsed_ += dt;
    float t = (duration_ > 0.0f) ? elapsed_ / duration_ : 1.0f;
    if (t >= 1.0f) {
        if (loop_) { elapsed_ = std::fmod(elapsed_, duration_); t = elapsed_ / duration_; }
        else       { t = 1.0f; done_ = true; }
    }
    const float e = ease_ ? ease_(t) : t;
    value_ = from_ + (to_ - from_) * e;
    return value_;
}

// -----------------------------------------------------------------------------
//  Manager
// -----------------------------------------------------------------------------
void AnimationManager::update(float dt) {
    globalTime_ += dt;
    for (auto& kv : tweens_) kv.second.update(dt);
}

Tween& AnimationManager::play(const std::string& key, float from, float to,
                              float duration, EaseFn ease) {
    Tween t(from, to, duration, ease);
    auto it = tweens_.insert_or_assign(key, t).first;
    return it->second;
}

float AnimationManager::valueOf(const std::string& key, float fallback) const {
    auto it = tweens_.find(key);
    return it == tweens_.end() ? fallback : it->second.value();
}

bool AnimationManager::has(const std::string& key) const {
    return tweens_.find(key) != tweens_.end();
}

void AnimationManager::remove(const std::string& key) { tweens_.erase(key); }
void AnimationManager::clear() { tweens_.clear(); }

} // namespace petpal
