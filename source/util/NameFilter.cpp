// =============================================================================
//  PetPal - NameFilter.cpp   (see NameFilter.h)
//  Keep the word lists in sync with the server's _namefilter.js.
// =============================================================================
#include "util/NameFilter.h"

#include <string>
#include <cstring>

namespace petpal {
namespace {

// --- word lists (mirror _namefilter.js) --------------------------------------
// SEVERE: matched anywhere in the collapsed name (unambiguous words).
const char* const kSevere[] = {
    "fuck", "fuk", "motherfuck", "shit", "bullshit", "bitch", "biatch",
    "nigger", "nigga", "faggot", "fagget", "retard", "cunt", "whore", "slut",
    "dildo", "blowjob", "handjob", "cumshot", "jizz", "rapist", "pedophile",
    "molest", "wetback", "beaner", "chink", "kike", "gook", "tranny", "shemale",
    "asshole", "dumbass", "jackass", "dickhead", "cockhead", "bastard",
    "wanker", "wank", "bollocks", "arsehole", "twat", "nazi", "hitler", "kkk",
    "porn", "pornhub",
};
// MILD: matched only as a whole word/token (short, ambiguous roots).
const char* const kMild[] = {
    "ass", "arse", "cock", "dick", "cum", "fag", "hoe", "tit", "tits", "boob",
    "boobs", "sex", "anal", "anus", "penis", "vagina", "pussy", "damn", "hell",
    "crap", "coon", "dyke", "homo", "jap", "wop", "dago", "spic", "pedo",
    "prick", "xxx",
};
// SAFE: innocent names that would otherwise trip a match (checked first).
const char* const kSafe[] = {
    "niger", "scunthorpe", "shiitake", "mishit", "cockburn", "penistone",
};

// The name shown instead of a filtered one (matches the server).
const char kReplacement[] = "Pal";

// --- normalization -----------------------------------------------------------
char leetMap(unsigned char c) {
    switch (c) {
        case '@': case '4': return 'a';
        case '8':           return 'b';
        case '(': case '{': case '[': case '<': return 'c';
        case '3':           return 'e';
        case '6': case '9': return 'g';
        case '#':           return 'h';
        case '0':           return 'o';
        case '$': case '5': return 's';
        case '7': case '+': return 't';
        case '2':           return 'z';
        default:            return 0;
    }
}

// Map a UTF-8 Latin-1 accented letter (0xC3 followed by `d`) to its base ASCII
// letter, e.g. á/à/ä -> 'a', ñ -> 'n'. Returns 0 if not a mapped letter.
char latin1Base(unsigned char d) {
    const unsigned cp = 0xC0u + (d - 0x80u); // U+00C0 .. U+00FF
    switch (cp) {
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5:
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return 'a';
        case 0xC7: case 0xE7: return 'c';
        case 0xC8: case 0xC9: case 0xCA: case 0xCB:
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e';
        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i';
        case 0xD1: case 0xF1: return 'n';
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8:
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return 'o';
        case 0xD9: case 0xDA: case 0xDB: case 0xDC:
        case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u';
        case 0xDD: case 0xFD: case 0xFF: return 'y';
        default:   return 0;
    }
}

// Fold a name to lowercase letters; anything else becomes a space (a token
// break). `oneChar` chooses how 1/!/| read ('i' or 'l'), tried both ways.
std::string fold(const char* s, char oneChar) {
    std::string out;
    if (!s) return out;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    while (*p) {
        unsigned char c = *p;
        if (c == 0xC3 && p[1]) {                 // UTF-8 Latin-1 accented letter
            const char b = latin1Base(p[1]);
            out += b ? b : ' ';
            p += 2;
            continue;
        }
        if (c >= 0x80) { out += ' '; ++p; continue; } // other non-ASCII -> break
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        if (c >= 'a' && c <= 'z')                 out += static_cast<char>(c);
        else if (c == '1' || c == '!' || c == '|') out += oneChar;
        else { const char m = leetMap(c); out += m ? m : ' '; }
        ++p;
    }
    // "ph" -> "f" (phuck -> fuck)
    std::string res;
    res.reserve(out.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] == 'p' && i + 1 < out.size() && out[i + 1] == 'h') { res += 'f'; ++i; }
        else res += out[i];
    }
    return res;
}

std::string squeeze(const std::string& s) {       // collapse repeated letters
    std::string r;
    r.reserve(s.size());
    for (char ch : s) if (r.empty() || r.back() != ch) r += ch;
    return r;
}

bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

bool anyMildToken(const std::string& folded) {
    std::size_t i = 0;
    while (i < folded.size()) {
        while (i < folded.size() && folded[i] == ' ') ++i;
        std::size_t j = i;
        while (j < folded.size() && folded[j] != ' ') ++j;
        if (j > i) {
            const std::string tok = folded.substr(i, j - i);
            const std::string sq = squeeze(tok);
            for (const char* w : kMild) {
                if (tok == w) return true;
                const std::string sw = squeeze(w);
                if (sw.size() >= 3 && sq == sw) return true;
            }
        }
        i = j;
    }
    return false;
}

} // namespace

bool isBadName(const char* name) {
    const std::string fi = fold(name, 'i');
    const std::string fl = fold(name, 'l');

    std::string normI, normL;
    for (char c : fi) if (c != ' ') normI += c;
    for (char c : fl) if (c != ' ') normL += c;
    if (normI.empty()) return false;

    for (const char* w : kSafe)
        if (normI == w || normL == w) return false;

    const std::string sqI = squeeze(normI), sqL = squeeze(normL);
    for (const char* w : kSevere) {
        if (has(normI, w) || has(normL, w)) return true;
        const std::string sw = squeeze(w);
        if (sw.size() >= 4 && (has(sqI, sw) || has(sqL, sw))) return true;
    }

    return anyMildToken(fi) || anyMildToken(fl);
}

void cleanName(char* buf, std::size_t bufSize) {
    if (!buf || bufSize == 0) return;
    if (!isBadName(buf)) return;
    std::size_t i = 0;
    for (; i + 1 < bufSize && kReplacement[i]; ++i) buf[i] = kReplacement[i];
    for (; i < bufSize; ++i) buf[i] = '\0';
}

} // namespace petpal
