#include "netpass/RedeemTask.h"
#include <cstring>

namespace petpal {

#ifdef __3DS__
void RedeemTask::entry(void* arg) {
    RedeemTask* t = static_cast<RedeemTask*>(arg);
    RedeemResult r = RedeemManager::redeem(t->code_, t->petId_);
    t->result_ = r;                                   // publish before the flag
    t->state_.store(State::Done, std::memory_order_release);
}
#endif

void RedeemTask::start(const char* code, uint64_t petId) {
    if (state_.load(std::memory_order_acquire) == State::Running) return;

    std::strncpy(code_, code ? code : "", sizeof(code_) - 1);
    code_[sizeof(code_) - 1] = '\0';
    petId_ = petId;
    state_.store(State::Running, std::memory_order_relaxed);

#ifdef __3DS__
    // Worker runs one notch below the UI thread so it only uses the CPU while
    // the UI thread is waiting on VBlank - the spinner keeps animating.
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    thread_ = threadCreate(entry, this, 32 * 1024, prio + 1, -1, false);
    if (!thread_) {                                   // fallback: run inline
        result_ = RedeemManager::redeem(code_, petId_);
        state_.store(State::Done, std::memory_order_release);
    }
#else
    result_ = RedeemManager::redeem(code_, petId_);
    state_.store(State::Done, std::memory_order_release);
#endif
}

bool RedeemTask::collect(RedeemResult& out) {
    if (state_.load(std::memory_order_acquire) != State::Done) return false;
#ifdef __3DS__
    if (thread_) {
        threadJoin(thread_, UINT64_MAX);
        threadFree(thread_);
        thread_ = nullptr;
    }
#endif
    out = result_;
    state_.store(State::Idle, std::memory_order_relaxed);
    return true;
}

} // namespace petpal
