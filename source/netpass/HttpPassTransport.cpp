// =============================================================================
//  PetPal - HttpPassTransport.cpp
//  Internet "StreetPass" over teampetpal.com/api/pass. Homebrew can't create a
//  real CEC/StreetPass box (confirmed dead end), so PetPal runs its own relay:
//  a background worker uploads our pet packet and downloads other players', and
//  the existing NetPassManager pipeline validates + turns them into friends.
//
//  Device-only (__3DS__); the host/test build uses LoopbackTransport. Sockets +
//  curl are already initialized once in Game::init (shared with the redeem
//  feature), so this just uses curl_easy per request on the worker thread.
// =============================================================================
#if defined(__3DS__)

#include "netpass/NetPassManager.h"
#include "util/Log.h"

#include <3ds.h>
#include <curl/curl.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace petpal {

namespace {

const char* kPassUrl = "https://teampetpal.com/api/pass";

size_t writeCb(char* ptr, size_t sz, size_t n, void* ud) {
    const size_t bytes = sz * n;
    static_cast<std::string*>(ud)->append(ptr, bytes);
    return bytes;
}

void toHex(const uint8_t* d, size_t n, std::string& out) {
    static const char* H = "0123456789abcdef";
    out.clear();
    out.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) { out += H[d[i] >> 4]; out += H[d[i] & 0xF]; }
}

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Decode `len` hex chars into bytes. Returns false on any non-hex / odd length.
bool fromHex(const char* s, size_t len, std::vector<uint8_t>& out) {
    if (len == 0 || (len & 1)) return false;
    out.clear();
    out.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2) {
        const int a = hexVal(s[i]), b = hexVal(s[i + 1]);
        if (a < 0 || b < 0) return false;
        out.push_back(static_cast<uint8_t>((a << 4) | b));
    }
    return true;
}

} // namespace

HttpPassTransport::HttpPassTransport() { LightLock_Init(&lock_); }
HttpPassTransport::~HttpPassTransport() { shutdown(); }

bool HttpPassTransport::init() {
    if (running_.load()) return true;
    running_.store(true);
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    // One notch below the UI thread so the exchange only runs while the UI is
    // idle. 48 KiB stack: curl + mbedTLS need headroom for the TLS handshake.
    thread_ = threadCreate(threadEntry, this, 48 * 1024, prio + 1, -1, false);
    if (!thread_) { running_.store(false); PP_ERR("pass worker start failed"); return false; }
    return true;
}

void HttpPassTransport::shutdown() {
    running_.store(false);
    if (thread_) {
        threadJoin(thread_, UINT64_MAX); // waits out any in-flight request
        threadFree(thread_);
        thread_ = nullptr;
    }
}

void HttpPassTransport::threadEntry(void* arg) {
    static_cast<HttpPassTransport*>(arg)->run();
}

void HttpPassTransport::run() {
    exchangeOnce(true);                 // announce our presence right away
    int sinceUpload = 0;
    while (running_.load()) {
        // Sleep ~60s, waking early on a poke (manual check) or shutdown.
        for (int i = 0; i < 60 && running_.load() && !poke_.load(); ++i)
            svcSleepThread(1000000000ULL);
        if (!running_.load()) break;

        const bool poked  = poke_.exchange(false);
        // Re-upload when our packet changed, on a manual poke, or ~every 30 min
        // to refresh the 7-day TTL; otherwise just download.
        const bool upload = dirty_.exchange(false) || poked || (++sinceUpload >= 30);
        if (upload) sinceUpload = 0;
        exchangeOnce(upload);
    }
}

bool HttpPassTransport::exchangeOnce(bool upload) {
    // Snapshot our packet under the lock.
    std::vector<uint8_t> pkt;
    LightLock_Lock(&lock_);
    pkt = outbox_;
    LightLock_Unlock(&lock_);
    if (pkt.size() < 16) return false; // need at least the header + petId

    // petId lives at offset 8 (u64, little-endian).
    unsigned long long id = 0;
    for (int i = 0; i < 8; ++i)
        id |= static_cast<unsigned long long>(pkt[8 + i]) << (8 * i);

    char idhex[20];
    std::snprintf(idhex, sizeof(idhex), "%016llx", id);

    std::string url = kPassUrl;
    url += "?id=";
    url += idhex;
    if (upload) {
        std::string ph;
        toHex(pkt.data(), pkt.size(), ph);
        url += "&pkt=";
        url += ph;
    }

    CURL* c = curl_easy_init();
    if (!c) { lastError_.store(1); lastOk_.store(false); return false; }

    std::string body;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "PetPal-3DS");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L); // no secrets sent (see redeem)
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);

    const CURLcode res = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);

    if (res != CURLE_OK) { lastError_.store((unsigned long)res); lastOk_.store(false); return false; }
    if (http != 200)     { lastError_.store((unsigned long)http); lastOk_.store(false); return false; }
    lastError_.store(0);
    lastOk_.store(true);

    // Response is one packet (hex) per line. Decode valid ones into the queue;
    // NetPassManager::poll() does the real magic/version/CRC/self checks.
    int added = 0;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t eol = body.find('\n', pos);
        if (eol == std::string::npos) eol = body.size();
        size_t len = eol - pos;
        if (len && body[pos + len - 1] == '\r') --len; // trim CRLF
        std::vector<uint8_t> bytes;
        if (len >= 32 && fromHex(body.data() + pos, len, bytes)) {
            LightLock_Lock(&lock_);
            if (inbox_.size() < 64) { inbox_.push_back(std::move(bytes)); ++added; }
            LightLock_Unlock(&lock_);
        }
        pos = eol + 1;
    }
    lastBatch_.store(added);
    recvTotal_.fetch_add(added);
    return true;
}

bool HttpPassTransport::setOutbox(const uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    LightLock_Lock(&lock_);
    outbox_.assign(data, data + len);
    LightLock_Unlock(&lock_);
    dirty_.store(true);
    return true;
}

int HttpPassTransport::drainInbox(std::vector<std::vector<uint8_t>>& out) {
    LightLock_Lock(&lock_);
    const int n = static_cast<int>(inbox_.size());
    for (auto& m : inbox_) out.push_back(std::move(m));
    inbox_.clear();
    LightLock_Unlock(&lock_);
    // A manual poll is also a good moment to ask the worker to refresh soon.
    poke_.store(true);
    return n;
}

StreetPassStatus HttpPassTransport::status() const {
    StreetPassStatus s;
    const bool up = running_.load();
    s.serviceUp    = up;
    s.boxReady     = up;                       // "ready" = relay worker running
    s.scanning     = up;
    s.inboxWaiting = lastBatch_.load();
    s.lastError    = static_cast<uint32_t>(lastError_.load());
    return s;
}

std::string HttpPassTransport::selfTest() {
    const bool ok = exchangeOnce(/*upload=*/true);
    char b[96];
    if (!ok)
        std::snprintf(b, sizeof(b), "Pass FAIL (code %lu)", lastError_.load());
    else if (lastBatch_.load() > 0)
        std::snprintf(b, sizeof(b), "Pass OK: got %d this check (total %d)",
                      lastBatch_.load(), recvTotal_.load());
    else
        std::snprintf(b, sizeof(b), "Connected - no new passes yet");
    return b;
}

} // namespace petpal

#endif // __3DS__
