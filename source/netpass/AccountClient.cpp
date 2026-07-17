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

// Escape a string for embedding in a JSON string literal (quotes/backslashes;
// drops control chars). Pet names are short + filtered, so this stays tiny.
std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char ch : s) {
        if (ch == '"' || ch == '\\') { o += '\\'; o += ch; }
        else if (static_cast<unsigned char>(ch) >= 0x20) o += ch;
    }
    return o;
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
                                   const char* petName, std::vector<std::string>* outBadges) {
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
    if (outBadges) *outBadges = jsonlite::stringArray(body, "badges");
    return jsonlite::boolField(body, "banned", false) ? BanState::Yes : BanState::No;
}

bool AccountClient::fetchPet(const char* id, const char* token, ServerPet& out) {
    out = ServerPet{};
    if (!id || !id[0]) return false;
    CURL* c = curl_easy_init();
    if (!c) return false;

    std::string body;
    std::string url = std::string(kBase) + "/api/account?id=" + urlEncode(id) +
                      "&token=" + urlEncode(token ? token : "");
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);
    commonOpts(c, &body);

    if (!perform(c, nullptr, body)) return false;
    if (jsonlite::field(body, "status") != "ok") return false;
    out.updatedAt = static_cast<unsigned long long>(jsonlite::longField(body, "updatedAt", 0));
    if (!jsonlite::boolField(body, "hasPet", false)) return true; // ok, but no pet
    out.name       = jsonlite::field(body, "name");
    out.species    = jsonlite::intField(body, "species", -1);
    out.stage      = jsonlite::intField(body, "stage", -1);
    out.level      = jsonlite::intField(body, "level", -1);
    out.happiness  = jsonlite::intField(body, "happiness", -1);
    out.energy     = jsonlite::intField(body, "energy", -1);
    out.hunger     = jsonlite::intField(body, "hunger", -1);
    out.coins      = jsonlite::intField(body, "coins", -1);
    out.encounters = jsonlite::intField(body, "encounters", -1);
    return true;
}

bool AccountClient::savePet(const char* id, const char* token, const ServerPet& in) {
    if (!id || !id[0]) return false;
    CURL* c = curl_easy_init();
    if (!c) return false;

    // Shared ordinal schema (see Android AppController.petJson / save format).
    char pet[320];
    std::snprintf(pet, sizeof(pet),
        "{\"name\":\"%s\",\"species\":%d,\"stage\":%d,\"level\":%d,"
        "\"happiness\":%d,\"energy\":%d,\"hunger\":%d,\"coins\":%d,\"encounters\":%d}",
        jsonEscape(in.name).c_str(), in.species, in.stage, in.level,
        in.happiness, in.energy, in.hunger, in.coins, in.encounters);
    std::string post = std::string("{\"id\":\"") + id +
                       "\",\"token\":\"" + (token ? token : "") + "\",\"pet\":" + pet + "}";

    std::string body;
    curl_slist* hdrs = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, (std::string(kBase) + "/api/account").c_str());
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, post.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);
    commonOpts(c, &body);

    if (!perform(c, hdrs, body)) return false;
    return jsonlite::field(body, "status") == "ok";
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
BanState       AccountClient::checkBanned(const char*, const char*, const char*, const char*, std::vector<std::string>*) { return BanState::Unknown; }
bool           AccountClient::link(const char*, const char*, const char*) { return false; }
bool           AccountClient::fetchPet(const char*, const char*, ServerPet& out) { out = ServerPet{}; return false; }
bool           AccountClient::savePet(const char*, const char*, const ServerPet&) { return false; }
} // namespace petpal

#endif
