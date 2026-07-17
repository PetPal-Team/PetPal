// =============================================================================
//  PetPal - JsonLite.h
//  Dependency-free extraction of a couple of fields from small, well-formed JSON
//  responses (register / petstatus / link). The 3DS doesn't ship a JSON parser
//  and our responses are tiny and ASCII, so a substring scan is enough. Kept in
//  a header so it compiles both on device and in the host tests.
//
//  NOTE: intentionally minimal - no escape/unicode handling. Only use it for
//  fields we control server-side (status, id, token, banned, known).
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace petpal {
namespace jsonlite {

// Extract a string field:  "<key>":"<value>"  -> value  ("" if absent).
inline std::string field(const std::string& body, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    size_t k = body.find(pat);
    if (k == std::string::npos) return "";
    size_t colon = body.find(':', k + pat.size());
    if (colon == std::string::npos) return "";
    size_t q1 = body.find('"', colon + 1);
    if (q1 == std::string::npos) return "";
    size_t q2 = body.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    return body.substr(q1 + 1, q2 - q1 - 1);
}

// Extract a numeric field:  "<key>":<number>  -> value  (dflt if absent or the
// value isn't a number, e.g. null). Parses an optionally-signed integer; wide
// enough for both the small ints (level/stats) and the ms updatedAt timestamp.
inline long long longField(const std::string& body, const char* key, long long dflt) {
    const std::string pat = std::string("\"") + key + "\"";
    size_t k = body.find(pat);
    if (k == std::string::npos) return dflt;
    size_t colon = body.find(':', k + pat.size());
    if (colon == std::string::npos) return dflt;
    size_t v = colon + 1;
    while (v < body.size() &&
           (body[v] == ' ' || body[v] == '\t' || body[v] == '\n' || body[v] == '\r'))
        ++v;
    bool neg = false;
    if (v < body.size() && (body[v] == '-' || body[v] == '+')) { neg = (body[v] == '-'); ++v; }
    long long n = 0;
    bool any = false;
    while (v < body.size() && body[v] >= '0' && body[v] <= '9') {
        n = n * 10 + (body[v] - '0');
        ++v;
        any = true;
    }
    if (!any) return dflt; // not a number (null / string)
    return neg ? -n : n;
}

inline int intField(const std::string& body, const char* key, int dflt) {
    return static_cast<int>(longField(body, key, dflt));
}

// Extract a boolean field:  "<key>":true|false  -> value  (dflt if absent).
inline bool boolField(const std::string& body, const char* key, bool dflt) {
    const std::string pat = std::string("\"") + key + "\"";
    size_t k = body.find(pat);
    if (k == std::string::npos) return dflt;
    size_t colon = body.find(':', k + pat.size());
    if (colon == std::string::npos) return dflt;
    size_t v = colon + 1;
    while (v < body.size() &&
           (body[v] == ' ' || body[v] == '\t' || body[v] == '\n' || body[v] == '\r'))
        ++v;
    if (body.compare(v, 4, "true") == 0) return true;
    if (body.compare(v, 5, "false") == 0) return false;
    return dflt;
}

// Extract an array of string values:  "<key>":["a","b"] -> {"a","b"}  (empty if
// absent). Naive scan of the bracketed span; fine for our controlled responses.
inline std::vector<std::string> stringArray(const std::string& body, const char* key) {
    std::vector<std::string> out;
    const std::string pat = std::string("\"") + key + "\"";
    size_t k = body.find(pat);
    if (k == std::string::npos) return out;
    size_t lb = body.find('[', k + pat.size());
    if (lb == std::string::npos) return out;
    size_t rb = body.find(']', lb + 1);
    if (rb == std::string::npos) return out;
    size_t p = lb + 1;
    while (p < rb) {
        size_t q1 = body.find('"', p);
        if (q1 == std::string::npos || q1 >= rb) break;
        size_t q2 = body.find('"', q1 + 1);
        if (q2 == std::string::npos || q2 > rb) break;
        out.push_back(body.substr(q1 + 1, q2 - q1 - 1));
        p = q2 + 1;
    }
    return out;
}

} // namespace jsonlite
} // namespace petpal
