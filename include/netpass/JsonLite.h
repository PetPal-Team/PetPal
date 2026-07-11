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

} // namespace jsonlite
} // namespace petpal
