// =============================================================================
//  PetPal - RedeemTask.h
//  Runs RedeemManager::redeem() on a background thread so the UI thread keeps
//  rendering (animated spinner) instead of freezing during the network call.
//
//  Usage (on the UI thread):
//    task.start(code, petId);            // kicks off the worker
//    ... each frame: draw spinner while task.busy() ...
//    RedeemResult r;
//    if (task.collect(r)) { apply r; }   // true exactly once when finished
// =============================================================================
#pragma once

#include "netpass/RedeemManager.h"
#include <atomic>
#include <cstdint>

#ifdef __3DS__
#include <3ds.h>
#endif

namespace petpal {

class RedeemTask {
public:
    bool busy() const { return state_.load(std::memory_order_acquire) == State::Running; }

    // Begin a redeem on a worker thread. No-op if one is already running.
    void start(const char* code, uint64_t petId);

    // If the worker has finished, fill `out`, reset to idle, and return true
    // (exactly once). Otherwise return false. Call every frame while busy().
    bool collect(RedeemResult& out);

private:
    enum class State { Idle, Running, Done };

#ifdef __3DS__
    static void entry(void* arg);
    Thread thread_ = nullptr;
#endif
    std::atomic<State> state_{State::Idle};
    RedeemResult result_;
    char     code_[40] = {0};
    uint64_t petId_ = 0;
};

} // namespace petpal
