#ifndef NAMED_GROUPS_HPP
#define NAMED_GROUPS_HPP

// Named capture groups for the regex rename mode. PCRE2 already understands (?<name>...) and
// \k<name>; the one addition is
//
//   (?<name=>...)   a named group whose text becomes a JavaScript number in replacement
//                   expressions when it reads as one (plain (?<name>...) groups stay strings)
//
// strip_numeric_arrows() rewrites it to (?<name>...) for PCRE2 and remembers the names. The group
// numbers and the full list of names are then read back from the compiled pattern.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "case_converter.hpp"

namespace fsturbo {

struct NamedGroup {
    std::string name;
    std::size_t index = 0; // capture group number, 1-based
    bool numeric = false;  // declared with "=>"
};

struct ParsedPattern {
    std::vector<NamedGroup> groups;
    std::size_t group_count = 0; // every capture group, named or not

    constexpr const NamedGroup* find(std::string_view name) const {
        for (const NamedGroup& g : groups)
            if (g.name == name) return &g;
        return nullptr;
    }
};

constexpr bool is_identifier_start(char c) { return ascii::is_lower(c) || ascii::is_upper(c) || c == '_' || c == '$'; }
constexpr bool is_identifier_char(char c) { return is_identifier_start(c) || ascii::is_digit(c); }

// Length of the JavaScript identifier at the start of `s` (ASCII only), 0 if there is none.
constexpr std::size_t identifier_length(std::string_view s) {
    if (s.empty() || !is_identifier_start(s.front())) return 0;
    std::size_t n = 1;
    while (n < s.size() && is_identifier_char(s[n])) ++n;
    return n;
}

constexpr void append_number(std::string& out, std::size_t n) {
    char digits[20];
    int count = 0;
    do {
        digits[count++] = static_cast<char>('0' + n % 10);
        n /= 10;
    } while (n != 0);
    while (count > 0) out += digits[--count];
}

struct ArrowPattern {
    std::string pattern;              // for PCRE2: every "(?<name=>" is now "(?<name>"
    std::vector<std::string> numeric; // the names that carried the arrow
};

// "(?<" is only a group opener outside character classes, \Q...\E, (?#...) comments and escapes.
constexpr ArrowPattern strip_numeric_arrows(std::string_view p) {
    ArrowPattern out;
    out.pattern.reserve(p.size());
    constexpr std::size_t npos = std::string_view::npos;
    for (std::size_t i = 0; i < p.size(); ++i) {
        const char c = p[i];
        std::size_t stop = i; // end of a span copied verbatim, if this turns out to be one
        if (c == '\\' && i + 1 < p.size()) {
            if (p[i + 1] == 'Q') { // \Q...\E: literal text
                const std::size_t end = p.find("\\E", i + 2);
                stop = end == npos ? p.size() : end + 2;
            } else {
                stop = i + 2;
            }
        } else if (c == '[') { // character class
            std::size_t j = i + 1;
            if (j < p.size() && p[j] == '^') ++j;
            if (j < p.size() && p[j] == ']') ++j; // a leading ']' is literal
            while (j < p.size() && p[j] != ']') {
                if (p[j] == '\\') {
                    ++j;
                } else if (p[j] == '[' && j + 1 < p.size() && p[j + 1] == ':') { // [:alpha:]
                    if (const std::size_t end = p.find(":]", j + 2); end != npos) j = end + 1;
                }
                ++j;
            }
            stop = j < p.size() ? j + 1 : p.size();
        } else if (c == '(' && p.substr(i).starts_with("(?#")) { // comment
            const std::size_t end = p.find(')', i);
            stop = end == npos ? p.size() : end + 1;
        } else if (c == '(' && p.substr(i).starts_with("(?<")) {
            const std::size_t name_len = identifier_length(p.substr(i + 3));
            if (name_len > 0 && p.substr(i + 3 + name_len).starts_with("=>")) {
                const std::string_view name = p.substr(i + 3, name_len);
                out.numeric.emplace_back(name);
                out.pattern += "(?<";
                out.pattern += name;
                out.pattern += '>';
                i += 3 + name_len + 1; // the loop's ++i steps past the '>'
                continue;
            }
        }
        if (stop > i) {
            out.pattern.append(p.substr(i, stop - i));
            i = stop - 1;
        } else {
            out.pattern += c;
        }
    }
    return out;
}

// --- JavaScript expression blocks ----------------------------------------------------------

constexpr std::size_t find_expression_end(std::string_view s, std::size_t open);

// `s[i]` is an opening quote; returns the index just past the closing one, or npos.
constexpr std::size_t skip_string(std::string_view s, std::size_t i) {
    const char quote = s[i];
    for (++i; i < s.size(); ++i) {
        if (s[i] == '\\') {
            ++i;
        } else if (quote == '`' && s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
            const std::size_t end = find_expression_end(s, i + 1);
            if (end == std::string_view::npos) return end;
            i = end;
        } else if (s[i] == quote) {
            return i + 1;
        }
    }
    return std::string_view::npos;
}

// Index of the '}' closing the '{' at `s[open]`, skipping nested braces and JavaScript string
// and template literals; npos if it is never closed. (Regex literals are not understood.)
constexpr std::size_t find_expression_end(std::string_view s, std::size_t open) {
    int depth = 0;
    for (std::size_t i = open; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\'' || c == '"' || c == '`') {
            const std::size_t after = skip_string(s, i);
            if (after == std::string_view::npos) return after;
            i = after - 1;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}' && --depth == 0) {
            return i;
        }
    }
    return std::string_view::npos;
}

static_assert([] {
    const auto r = strip_numeric_arrows(R"((?<fecha=>(?<dia=>\d\d)-(?<mes>\d\d)))");
    return r.pattern == R"((?<fecha>(?<dia>\d\d)-(?<mes>\d\d)))" && r.numeric.size() == 2 && r.numeric[0] == "fecha" && r.numeric[1] == "dia";
}());
static_assert([] {
    // untouched where "(?<x=>" is not a group: in classes, \Q..\E, comments, after a backslash
    constexpr std::string_view in = R"([(?<a=>x)] [^]](?<b=>] [[:alpha:]](?<c=>) \Q(?<d=>\E (?#(?<e=>) \(?<f=> (?<=g)(?<!h)(i)(?:j))";
    const auto r = strip_numeric_arrows(in);
    return r.pattern == "[(?<a=>x)] [^]](?<b>] [[:alpha:]](?<c>) \\Q(?<d=>\\E (?#(?<e=>) \\(?<f=> (?<=g)(?<!h)(i)(?:j)" && r.numeric.size() == 2 &&
           r.numeric[0] == "b" && r.numeric[1] == "c";
}());
static_assert(strip_numeric_arrows("(?<=x)y").pattern == "(?<=x)y" && strip_numeric_arrows("(?<n=>").numeric.size() == 1);
static_assert(find_expression_end("a{b{c}d}e", 1) == 7);
static_assert(find_expression_end(R"({"}" + '{' + `${ {a:1}.a }`})", 0) == 27);
static_assert(find_expression_end("{unclosed", 0) == std::string_view::npos && find_expression_end(R"({"a)", 0) == std::string_view::npos);

} // namespace fsturbo

#endif // NAMED_GROUPS_HPP
