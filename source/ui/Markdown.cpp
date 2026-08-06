// =============================================================================
//  PetPal - Markdown.cpp   (see Markdown.h)
//  Block parser + greedy word-wrap. No citro2d here - width comes from the
//  injected measure() - so this compiles + is tested on the host too.
// =============================================================================
#include "ui/Markdown.h"

#include <cstdio>

namespace petpal {
namespace md {
namespace {

const std::string kBulletFirst = "- ";   // ASCII (system font always has it)
const std::string kListCont    = "   ";  // continuation indent for lists

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

char firstNonSpace(const std::string& s) {
    for (char c : s) if (c != ' ' && c != '\t') return c;
    return '\0';
}

bool isBlank(const std::string& s) {
    for (char c : s) if (c != ' ' && c != '\t' && c != '\r') return false;
    return true;
}

bool isRule(const std::string& s) {
    std::string t;
    for (char c : s) if (c != ' ' && c != '\t' && c != '\r') t += c;
    if (t.size() < 3) return false;
    const char c0 = t[0];
    if (c0 != '-' && c0 != '*' && c0 != '_') return false;
    for (char c : t) if (c != c0) return false;
    return true;
}

int headingLevel(const std::string& s) {
    size_t h = 0;
    while (h < s.size() && s[h] == '#') ++h;
    if (h >= 1 && h <= 6 && h < s.size() && s[h] == ' ') return static_cast<int>(h);
    return 0;
}

bool isBullet(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return i < s.size() && (s[i] == '-' || s[i] == '*' || s[i] == '+') &&
           i + 1 < s.size() && (s[i + 1] == ' ' || s[i + 1] == '\t');
}
std::string bulletContent(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    ++i; // the marker
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

bool isOrdered(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    size_t d = i;
    while (d < s.size() && s[d] >= '0' && s[d] <= '9') ++d;
    return d > i && d < s.size() && s[d] == '.' &&
           d + 1 < s.size() && (s[d + 1] == ' ' || s[d + 1] == '\t');
}
std::string orderedContent(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    ++i; // the '.'
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

std::string stripQuote(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i < s.size() && s[i] == '>') ++i;
    if (i < s.size() && s[i] == ' ') ++i;
    return s.substr(i);
}

bool blockStarts(const std::string& s) {
    return isBlank(s) || s.rfind("```", 0) == 0 || headingLevel(s) > 0 ||
           firstNonSpace(s) == '>' || isBullet(s) || isOrdered(s) || isRule(s);
}

void stripInto(std::string& o, const std::string& s) {
    size_t i = 0, n = s.size();
    while (i < n) {
        const char ch = s[i];
        if (ch == '`') {
            size_t e = s.find('`', i + 1);
            if (e != std::string::npos) { o += s.substr(i + 1, e - i - 1); i = e + 1; continue; }
        }
        if (ch == '[') {
            size_t rb = s.find(']', i + 1);
            if (rb != std::string::npos && rb + 1 < n && s[rb + 1] == '(') {
                size_t rp = s.find(')', rb + 2);
                if (rp != std::string::npos) { stripInto(o, s.substr(i + 1, rb - i - 1)); i = rp + 1; continue; }
            }
        }
        if (ch == '*' && i + 1 < n && s[i + 1] == '*') {
            size_t e = s.find("**", i + 2);
            if (e != std::string::npos) { stripInto(o, s.substr(i + 2, e - i - 2)); i = e + 2; continue; }
        }
        if (ch == '*' || ch == '_') {
            size_t e = s.find(ch, i + 1);
            if (e != std::string::npos && e > i + 1) { stripInto(o, s.substr(i + 1, e - i - 1)); i = e + 1; continue; }
        }
        o += ch; ++i;
    }
}

// Greedy word-wrap `text` into `out` using measure(); firstPrefix on line 0 and
// contPrefix on wrapped continuations (prefixes are part of the measured width).
void wrapInto(std::vector<Line>& out, Style style, const std::string& text,
              float maxWidth, const Measure& measure,
              const std::string& firstPrefix, const std::string& contPrefix) {
    std::vector<std::string> words;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] == ' ') ++i;
        size_t j = i;
        while (j < text.size() && text[j] != ' ') ++j;
        if (j > i) words.push_back(text.substr(i, j - i));
        i = j;
    }
    if (words.empty()) { out.push_back({ style, firstPrefix }); return; }

    std::string cur = firstPrefix;
    bool hasWord = false;
    for (const std::string& w : words) {
        const std::string cand = hasWord ? (cur + " " + w) : (cur + w);
        if (!hasWord || measure(cand, style) <= maxWidth) {
            cur = cand; hasWord = true;
        } else {
            out.push_back({ style, cur });
            cur = contPrefix + w;
            hasWord = true;
        }
    }
    out.push_back({ style, cur });
}

// Char-wrap a verbatim code line (no word breaking).
void wrapCode(std::vector<Line>& out, const std::string& raw,
              float maxWidth, const Measure& measure) {
    if (raw.empty()) { out.push_back({ Style::Code, "" }); return; }
    std::string cur;
    for (char c : raw) {
        const std::string cand = cur + c;
        if (cur.empty() || measure(cand, Style::Code) <= maxWidth) cur = cand;
        else { out.push_back({ Style::Code, cur }); cur = std::string(1, c); }
    }
    out.push_back({ Style::Code, cur });
}

} // namespace

std::string stripInline(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    stripInto(o, s);
    return o;
}

std::vector<Line> layout(const std::string& body, float maxWidth, const Measure& measure) {
    std::vector<Line> out;

    // Split into raw lines.
    std::vector<std::string> lines;
    {
        size_t i = 0, n = body.size();
        while (i <= n) {
            size_t e = body.find('\n', i);
            if (e == std::string::npos) e = n;
            std::string l = body.substr(i, e - i);
            if (!l.empty() && l.back() == '\r') l.pop_back();
            lines.push_back(l);
            if (e == n) break;
            i = e + 1;
        }
    }

    const size_t N = lines.size();
    size_t i = 0;
    bool lastBlank = true; // suppress a leading spacer
    auto pushBlank = [&]() { if (!lastBlank) { out.push_back({ Style::Blank, "" }); lastBlank = true; } };

    while (i < N) {
        const std::string& line = lines[i];

        if (line.rfind("```", 0) == 0) {                    // fenced code
            ++i;
            while (i < N && lines[i].rfind("```", 0) != 0) { wrapCode(out, lines[i], maxWidth, measure); ++i; }
            if (i < N) ++i;                                 // closing fence
            lastBlank = false; continue;
        }
        if (isBlank(line)) { pushBlank(); ++i; continue; }
        if (isRule(line))  { out.push_back({ Style::Rule, "" }); lastBlank = false; ++i; continue; }

        const int hl = headingLevel(line);
        if (hl > 0) {
            const Style st = (hl == 1) ? Style::H1 : (hl == 2) ? Style::H2 : Style::H3;
            wrapInto(out, st, stripInline(trim(line.substr(hl + 1))), maxWidth, measure, "", "");
            lastBlank = false; ++i; continue;
        }
        if (firstNonSpace(line) == '>') {                   // blockquote (run)
            std::string q;
            while (i < N && firstNonSpace(lines[i]) == '>') { q += stripQuote(lines[i]); q += ' '; ++i; }
            wrapInto(out, Style::Quote, stripInline(trim(q)), maxWidth, measure, "", "");
            lastBlank = false; continue;
        }
        if (isBullet(line)) {                               // unordered list
            while (i < N && isBullet(lines[i])) {
                wrapInto(out, Style::Bullet, stripInline(bulletContent(lines[i])), maxWidth, measure, kBulletFirst, kListCont);
                ++i;
            }
            lastBlank = false; continue;
        }
        if (isOrdered(line)) {                              // ordered list
            int num = 1;
            while (i < N && isOrdered(lines[i])) {
                char pre[8];
                std::snprintf(pre, sizeof(pre), "%d. ", num++);
                wrapInto(out, Style::Ordered, stripInline(orderedContent(lines[i])), maxWidth, measure, pre, kListCont);
                ++i;
            }
            lastBlank = false; continue;
        }

        // Paragraph: gather until a blank line or the next block.
        std::string p;
        while (i < N && !isBlank(lines[i]) && !blockStarts(lines[i])) { p += lines[i]; p += ' '; ++i; }
        wrapInto(out, Style::Normal, stripInline(trim(p)), maxWidth, measure, "", "");
        lastBlank = false;
    }

    while (!out.empty() && out.back().style == Style::Blank) out.pop_back();
    return out;
}

} // namespace md
} // namespace petpal
