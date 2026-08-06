// =============================================================================
//  PetPal - AnnouncementsClient.cpp
//  Device build: 3ds-curl GET of the plaintext announcements feed + a small
//  line-based parser. Host build: inert stub. curl_global_init / soc are already
//  set up once in Game::init (shared with redeem/pass/account).
// =============================================================================
#include "netpass/AnnouncementsClient.h"

#ifdef __3DS__

#include <curl/curl.h>
#include <cstdlib>
#include <cstring>
#include "util/Log.h"

namespace petpal {
namespace {

const char* kUrl = "https://teampetpal.com/api/announcements?format=text";

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* ud) {
    const size_t bytes = size * nmemb;
    static_cast<std::string*>(ud)->append(ptr, bytes);
    return bytes;
}

// Strip a trailing \r (feed uses \n, but be tolerant of \r\n).
void rstripCr(std::string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

// Parse the plaintext feed (see announcements.js -> toText):
//   status=ok / count=N, then repeating:
//     ##ANN## / id=.. / title=.. / date=.. / ##BODY## / <body lines...>
void parseFeed(const std::string& body, std::vector<Announcement>& out) {
    Announcement cur;
    bool have = false, inBody = false;
    size_t i = 0;
    const size_t n = body.size();
    while (i <= n) {
        // Extract one line [i, eol).
        size_t eol = body.find('\n', i);
        if (eol == std::string::npos) eol = n;
        std::string line = body.substr(i, eol - i);
        i = eol + 1;
        rstripCr(line);

        if (line == "##ANN##") {
            if (have) { if (!cur.body.empty() && cur.body.back() == '\n') cur.body.pop_back(); out.push_back(cur); }
            cur = Announcement{};
            have = true;
            inBody = false;
        } else if (!have) {
            // status=/count= preamble before the first record: ignore.
        } else if (inBody) {
            cur.body += line;
            cur.body += '\n';
        } else if (line == "##BODY##") {
            inBody = true;
        } else if (line.rfind("id=", 0) == 0) {
            cur.id = line.substr(3);
        } else if (line.rfind("title=", 0) == 0) {
            cur.title = line.substr(6);
        } else if (line.rfind("date=", 0) == 0) {
            cur.date = std::atoll(line.c_str() + 5);
        }
        if (eol == n) break;
    }
    if (have) { if (!cur.body.empty() && cur.body.back() == '\n') cur.body.pop_back(); out.push_back(cur); }
}

} // namespace

bool AnnouncementsClient::fetch(std::vector<Announcement>& out) {
    out.clear();
    CURL* c = curl_easy_init();
    if (!c) return false;

    std::string body;
    curl_easy_setopt(c, CURLOPT_URL, kUrl);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "PetPal-3DS");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);

    const CURLcode res = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);
    if (res != CURLE_OK) { PP_WARN("announce curl failed (%d)", (int)res); return false; }
    if (http != 200)     { PP_WARN("announce HTTP %ld", http); return false; }
    if (body.rfind("status=ok", 0) != 0) return false;

    parseFeed(body, out);
    return true;
}

} // namespace petpal

#else  // ---- host build: inert stub ----------------------------------------

namespace petpal {
bool AnnouncementsClient::fetch(std::vector<Announcement>&) { return false; }
} // namespace petpal

#endif

// --- background task (both builds) ------------------------------------------
namespace petpal {

#ifdef __3DS__
void AnnouncementsTask::entry(void* arg) {
    AnnouncementsTask* t = static_cast<AnnouncementsTask*>(arg);
    t->ok_ = AnnouncementsClient::fetch(t->result_);
    t->state_.store(State::Done, std::memory_order_release);
}
#endif

void AnnouncementsTask::start() {
    if (state_.load(std::memory_order_acquire) == State::Running) return;
    result_.clear();
    ok_ = false;
    state_.store(State::Running, std::memory_order_relaxed);

#ifdef __3DS__
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    thread_ = threadCreate(entry, this, 32 * 1024, prio + 1, -1, false);
    if (!thread_) {                                   // fallback: run inline
        ok_ = AnnouncementsClient::fetch(result_);
        state_.store(State::Done, std::memory_order_release);
    }
#else
    ok_ = AnnouncementsClient::fetch(result_);
    state_.store(State::Done, std::memory_order_release);
#endif
}

bool AnnouncementsTask::collect(std::vector<Announcement>& out, bool& ok) {
    if (state_.load(std::memory_order_acquire) != State::Done) return false;
#ifdef __3DS__
    if (thread_) {
        threadJoin(thread_, UINT64_MAX);
        threadFree(thread_);
        thread_ = nullptr;
    }
#endif
    out = result_;
    ok = ok_;
    state_.store(State::Idle, std::memory_order_relaxed);
    return true;
}

} // namespace petpal
