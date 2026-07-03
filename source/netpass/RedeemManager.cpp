// =============================================================================
//  PetPal - RedeemManager.cpp
//  HTTPS client for the redeem endpoint. Device build uses 3ds-curl (libcurl +
//  mbedTLS) instead of libctru httpc: httpc rides the console's ancient ssl:C
//  module and would complete the TLS handshake to Cloudflare but fail to read
//  the response ("No reply (D8A0A003)" = HTTP module / invalid state). curl +
//  mbedTLS do modern TLS and redirect handling cleanly. The host build returns a
//  friendly "offline" result.
//
//  Sockets and curl are initialized once in Game::init (soc + curl_global_init),
//  so this file just uses curl_easy_* per request (on the RedeemTask worker).
//
//  Wire format (keeps 3DS-side parsing trivial - no JSON needed):
//    request  : GET https://teampetpal.com/api/redeem?code=<code>&pet=<hex>
//    response : text/plain, key=value per line, e.g.
//        status=ok
//        reward=items
//        item=apple
//        qty=1000
//        msg=Enjoy 1000 apples!
// =============================================================================
#include "netpass/RedeemManager.h"
#include "util/Log.h"

#ifdef __3DS__
#include <curl/curl.h>
#endif

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace petpal {

namespace {

// Percent-encode a code for safe use in a URL query (codes are usually alnum).
std::string urlEncode(const char* s) {
    std::string out;
    for (; s && *s; ++s) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char b[4];
            std::snprintf(b, sizeof(b), "%%%02X", c);
            out += b;
        }
    }
    return out;
}

// Pull the value for `key` out of a key=value/line response. Returns "" if
// absent. Trims a trailing CR so CRLF responses parse cleanly.
std::string field(const std::string& body, const char* key) {
    const size_t klen = std::strlen(key);
    size_t pos = 0;
    while (pos < body.size()) {
        size_t eol = body.find('\n', pos);
        if (eol == std::string::npos) eol = body.size();
        // line = [pos, eol)
        if (body.compare(pos, klen, key) == 0 && pos + klen < body.size() &&
            body[pos + klen] == '=') {
            size_t vstart = pos + klen + 1;
            std::string v = body.substr(vstart, eol - vstart);
            if (!v.empty() && v.back() == '\r') v.pop_back();
            return v;
        }
        pos = eol + 1;
    }
    return "";
}

// Map server reward strings -> game enums.
ItemId itemFromName(const std::string& s) {
    if (s == "apple")  return ItemId::Apple;
    if (s == "cookie") return ItemId::Cookie;
    if (s == "cake")   return ItemId::Cake;
    if (s == "fish")   return ItemId::Fish;
    return ItemId::Apple;
}
Accessory accessoryFromId(const std::string& s) {
    if (s == "dog_hat")    return Accessory::DogHat;
    if (s == "crown")      return Accessory::Crown;
    if (s == "top_hat")    return Accessory::TopHat;
    if (s == "headphones") return Accessory::Headphones;
    return Accessory::None;
}
TransformForm formFromName(const std::string& s) {
    if (s == "bonzi") return TransformForm::Bonzi;
    return TransformForm::None;
}

void parseInto(const std::string& body, RedeemResult& r) {
    const std::string status = field(body, "status");
    r.message = field(body, "msg");

    if (status != "ok") {
        r.ok = false;
        if (r.message.empty()) r.message = "Code not accepted.";
        return;
    }

    const std::string reward = field(body, "reward");
    if (reward == "items") {
        r.reward = RedeemResult::Reward::Items;
        r.item   = itemFromName(field(body, "item"));
        r.qty    = static_cast<uint16_t>(std::atoi(field(body, "qty").c_str()));
        r.ok     = r.qty > 0;
    } else if (reward == "accessory") {
        r.reward    = RedeemResult::Reward::Accessory;
        r.accessory = accessoryFromId(field(body, "id"));
        r.ok        = r.accessory != Accessory::None;
    } else if (reward == "transform") {
        r.reward   = RedeemResult::Reward::Transform;
        r.form     = formFromName(field(body, "form"));
        r.seconds  = static_cast<uint32_t>(std::atol(field(body, "seconds").c_str()));
        r.ok       = r.form != TransformForm::None && r.seconds > 0;
    } else {
        r.ok = false;
        if (r.message.empty()) r.message = "Unknown reward.";
    }
    if (!r.ok && r.message.empty()) r.message = "Could not apply reward.";
}

} // namespace

#ifdef __3DS__

namespace {
// libcurl write callback: append the response body into the std::string in ud.
size_t writeCb(char* ptr, size_t size, size_t nmemb, void* ud) {
    const size_t bytes = size * nmemb;
    static_cast<std::string*>(ud)->append(ptr, bytes);
    return bytes;
}
} // namespace

RedeemResult RedeemManager::redeem(const char* code, uint64_t petId) {
    RedeemResult r;
    if (!code || !code[0]) { r.message = "Enter a code first."; return r; }

    char pethex[20];
    std::snprintf(pethex, sizeof(pethex), "%016llX", (unsigned long long)petId);

    // GET with query params. Codes are public, so query params are fine.
    std::string url = "https://teampetpal.com/api/redeem?code=";
    url += urlEncode(code);
    url += "&pet=";
    url += pethex;

    CURL* c = curl_easy_init();
    if (!c) { r.message = "Network unavailable."; return r; }

    std::string body;
    char errbuf[CURL_ERROR_SIZE];
    errbuf[0] = '\0';

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);   // follow CF http->https
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "PetPal-3DS");
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 1L);
    // No secret is transmitted (see RedeemManager.h), so cert verification is
    // intentionally off: it avoids shipping a CA bundle and the 3DS's stale root
    // store rejecting modern Cloudflare certificates.
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(c);
    long httpStatus = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(c);

    char m[96];
    if (res != CURLE_OK) {
        const char* why = errbuf[0] ? errbuf : curl_easy_strerror(res);
        std::snprintf(m, sizeof(m), "Net error: %s", why ? why : "unknown");
        r.message = m;
        PP_WARN("redeem curl failed (%d): %s", (int)res, why ? why : "?");
        return r;
    }
    if (httpStatus != 200) {
        std::snprintf(m, sizeof(m), "HTTP %ld", httpStatus);
        r.message = m;
        return r;
    }

    parseInto(body, r);
    PP_LOG("redeem '%s' -> ok=%d msg=%s", code, (int)r.ok, r.message.c_str());
    return r;
}

#else // host build

RedeemResult RedeemManager::redeem(const char* code, uint64_t) {
    RedeemResult r;
    r.message = "Online codes require a 3DS.";
    (void)code;
    return r;
}

#endif

} // namespace petpal
