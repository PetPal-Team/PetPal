// =============================================================================
//  PetPal - AnnouncementsClient.h
//  Fetches the dev-team announcements (News page) from teampetpal.com. Uses the
//  plaintext feed (/api/announcements?format=text) so the 3DS doesn't need a JSON
//  parser - the body is raw Markdown, rendered by ui/Markdown.h.
//
//  SECURITY: read-only, no secrets sent. SSL verification is intentionally off
//  (same rationale as RedeemManager/AccountClient: nothing secret is transmitted
//  and the 3DS's root-CA store is too old for modern Cloudflare certs).
// =============================================================================
#pragma once

#include <atomic>
#include <string>
#include <vector>

#ifdef __3DS__
#include <3ds.h>
#endif

namespace petpal {

struct Announcement {
    std::string id;
    std::string title;
    std::string body;       // Markdown
    long long   date = 0;   // epoch ms (0 if unknown)
};

class AnnouncementsClient {
public:
    // Blocking HTTPS GET + parse. Returns true on success (out filled; may be
    // empty if there are no announcements). Host build: returns false (stub).
    static bool fetch(std::vector<Announcement>& out);
};

// Background wrapper so the News screen keeps rendering (spinner) during the
// fetch. Mirrors RedeemTask: start() on the UI thread, poll collect() each frame.
class AnnouncementsTask {
public:
    bool busy() const { return state_.load(std::memory_order_acquire) == State::Running; }

    // Begin a fetch on a worker thread. No-op if one is already running.
    void start();

    // When finished, fill `out`, set `ok` to whether the fetch succeeded, reset to
    // idle, and return true (exactly once). Otherwise return false.
    bool collect(std::vector<Announcement>& out, bool& ok);

private:
    enum class State { Idle, Running, Done };

#ifdef __3DS__
    static void entry(void* arg);
    Thread thread_ = nullptr;
#endif
    std::atomic<State> state_{State::Idle};
    std::vector<Announcement> result_;
    bool ok_ = false;
};

} // namespace petpal
