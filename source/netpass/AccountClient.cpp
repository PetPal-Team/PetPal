// =============================================================================
//  PetPal - AccountClient.cpp
//  See AccountClient.h. Device build = 3ds-curl requests + JsonLite parsing;
//  host build = inert stubs. curl_global_init / soc are already set up once in
//  Game::init (shared with redeem + pass), so this just uses curl_easy_* here.
// =============================================================================
#include "netpass/AccountClient.h"
#include "netpass/JsonLite.h"
#include "util/Log.h"

#ifdef __3DS__

#include <curl/curl.h>
#include <cctype>
#include <cstdio>

namespace petpal {
namespace {

const char* kBase = "https://teampetpal.com";

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* ud) {
    const size_t bytes = size * nmemb;
    static_cast<std::string*>(ud)->append(ptr, bytes);
    return bytes;
}

// Percent-encode a query value (ids/tokens are already URL-safe, but be safe).
std::string urlEncode(const char* s) {
    std::string out;
    for (; s && *s; ++s) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += static_cast<char>(c);
        else { char b[4]; std::snprintf(b, sizeof(b), "%%%02X", c); out += b; }
    }
    return out;
}

// Shared curl setup: no secret is sent, so cert verification is intentionally
// off (avoids shipping a CA bundle + the 3DS's stale roots rejecting Cloudflare).
void commonOpts(CURL* c, std::string* body) {
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, body);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "PetPal-3DS");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
}

// Run a request, returning true only on CURLE_OK + HTTP 200 (body filled).
bool perform(CURL* c, curl_slist* hdrs, std::string& body) {
    const CURLcode res = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    if (hdrs) curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    if (res != CURLE_OK) { PP_WARN("account curl failed (%d)", (int)res); return false; }
    if (http != 200)     { PP_WARN("account HTTP %ld", http); return false; }
    (void)body;
    return true;
}

} // namespace

RegisterResult AccountClient::registerDevice(const char* device) {
    RegisterResult rr;
    CURL* c = curl_easy_init();
    if (!c) return rr;

    std::string body;
    std::string post = std::string("{\"device\":\"") + (device && device[0] ? device : "3ds") + "\"}";
    curl_slist* hdrs = curl_slist_append(nullptr, "Content-Type: application/json");

    curl_easy_setopt(c, CURLOPT_URL, (std::string(kBase) + "/api/register").c_str());
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, post.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);
    commonOpts(c, &body);

    if (!perform(c, hdrs, body)) return rr;
    if (jsonlite::field(body, "status") != "ok") return rr;
    rr.id    = jsonlite::field(body, "id");
    rr.token = jsonlite::field(body, "token");
    rr.ok    = !rr.id.empty() && !rr.token.empty();
    if (rr.ok) PP_LOG("registered as %s", rr.id.c_str());
    return rr;
}

BanState AccountClient::checkBanned(const char* id, const char* token, const char* device,
                                   const char* petName) {
    if (!id || !id[0]) return BanState::Unknown;
    CURL* c = curl_easy_init();
    if (!c) return BanState::Unknown;

    std::string body;
    std::string url = std::string(kBase) + "/api/petstatus?id=" + urlEncode(id);
    if (token && token[0])    { url += "&token=";  url += urlEncode(token); }
    if (device && device[0])  { url += "&device="; url += urlEncode(device); }
    if (petName && petName[0]){ url += "&name=";   url += urlEncode(petName); }

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 6L);  // keep boot snappy
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);
    commonOpts(c, &body);

    if (!perform(c, nullptr, body)) return BanState::Unknown;
    if (jsonlite::field(body, "status") != "ok") return BanState::Unknown;
    return jsonlite::boolField(body, "banned", false) ? BanState::Yes : BanState::No;
}

bool AccountClient::link(const char* id, const char* token, const char* targetId) {
    if (!targetId || !targetId[0]) return false;
    CURL* c = curl_easy_init();
    if (!c) return false;

    std::string body;
    std::string post = std::string("{\"id\":\"") + (id ? id : "") +
                       "\",\"token\":\"" + (token ? token : "") +
                       "\",\"targetId\":\"" + targetId + "\"}";
    curl_slist* hdrs = curl_slist_append(nullptr, "Content-Type: application/json");

    curl_easy_setopt(c, CURLOPT_URL, (std::string(kBase) + "/api/link").c_str());
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, post.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);
    commonOpts(c, &body);

    if (!perform(c, hdrs, body)) return false;
    return jsonlite::field(body, "status") == "ok";
}

} // namespace petpal

#else  // ---- host build: inert stubs ----------------------------------------

namespace petpal {
RegisterResult AccountClient::registerDevice(const char*) { return {}; }
BanState       AccountClient::checkBanned(const char*, const char*, const char*, const char*) { return BanState::Unknown; }
bool           AccountClient::link(const char*, const char*, const char*) { return false; }
} // namespace petpal

#endif
