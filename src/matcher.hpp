#ifndef MATCHER_HPP
#define MATCHER_HPP

#include <fstream>
#include <initializer_list>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace fsturbo {

// Check if string is a regex specification: starts with "/" and ends with "/" (or "/flags")
constexpr bool is_regex_spec(std::string_view spec) {
    if (spec.size() >= 2 && spec.front() == '/') {
        const std::size_t last_slash = spec.rfind('/');
        return last_slash != 0 && last_slash != std::string_view::npos;
    }
    return false;
}

// Anchored glob match without std::regex:
//   **  (optionally followed by '/') matches any characters, including '/'
//   *   matches any characters except '/'
//   ?   matches one character except '/'
//   everything else is literal
constexpr bool glob_match(std::string_view pattern, std::string_view text) {
    while (!pattern.empty()) {
        if (pattern.starts_with("**")) {
            pattern.remove_prefix(2);
            if (pattern.starts_with('/')) pattern.remove_prefix(1);
            for (std::size_t i = 0; i <= text.size(); ++i)
                if (glob_match(pattern, text.substr(i))) return true;
            return false;
        }
        if (pattern.front() == '*') {
            pattern.remove_prefix(1);
            for (std::size_t i = 0;; ++i) {
                if (glob_match(pattern, text.substr(i))) return true;
                if (i == text.size() || text[i] == '/') return false;
            }
        }
        if (text.empty()) return false;
        if (pattern.front() == '?' ? text.front() == '/' : pattern.front() != text.front()) return false;
        pattern.remove_prefix(1);
        text.remove_prefix(1);
    }
    return text.empty();
}

static_assert(glob_match("*.log", "debug.log"));
static_assert(!glob_match("*.log", "logs/debug.log"));
static_assert(glob_match("**/*.o", "main.o") && glob_match("**/*.o", "build/x/main.o"));
static_assert(glob_match("src/?.c", "src/a.c") && !glob_match("src/?.c", "src/ab.c"));
static_assert(glob_match("a[1].txt", "a[1].txt"));
static_assert(!glob_match("build", "build/out"));

// --- .gitignore semantics -------------------------------------------------------

namespace detail {

constexpr bool ascii_class(std::string_view name, char c) {
    const bool lower = c >= 'a' && c <= 'z', upper = c >= 'A' && c <= 'Z', digit = c >= '0' && c <= '9';
    const bool space = c == ' ' || (c >= '\t' && c <= '\r');
    const bool cntrl = (c >= 0 && c < 32) || c == 127;
    const bool graph = c > 32 && c < 127;
    if (name == "alpha") return lower || upper;
    if (name == "digit") return digit;
    if (name == "alnum") return lower || upper || digit;
    if (name == "upper") return upper;
    if (name == "lower") return lower;
    if (name == "space") return space;
    if (name == "blank") return c == ' ' || c == '\t';
    if (name == "punct") return graph && !(lower || upper || digit);
    if (name == "xdigit") return digit || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (name == "cntrl") return cntrl;
    if (name == "graph") return graph;
    if (name == "print") return graph || c == ' ';
    return false;
}

// Matches the bracket expression starting at p[pi] == '[' against `c`.
// Returns the index just past the closing ']', or npos when the expression is unterminated.
constexpr std::size_t match_bracket(std::string_view p, std::size_t pi, char c, bool& matched) {
    std::size_t i = pi + 1;
    bool negate = false;
    if (i < p.size() && (p[i] == '!' || p[i] == '^')) {
        negate = true;
        ++i;
    }
    bool hit = false;
    for (bool first = true; i < p.size() && (first || p[i] != ']'); first = false) {
        if (p[i] == '[' && i + 1 < p.size() && p[i + 1] == ':') {
            const std::size_t end = p.find(":]", i + 2);
            if (end == std::string_view::npos) return std::string_view::npos;
            hit |= ascii_class(p.substr(i + 2, end - i - 2), c);
            i = end + 2;
            continue;
        }
        char lo = p[i];
        if (lo == '\\' && i + 1 < p.size()) lo = p[++i];
        if (i + 2 < p.size() && p[i + 1] == '-' && p[i + 2] != ']') {
            std::size_t j = i + 2;
            char hi = p[j];
            if (hi == '\\' && j + 1 < p.size()) hi = p[++j];
            hit |= lo <= c && c <= hi;
            i = j + 1;
        } else {
            hit |= c == lo;
            ++i;
        }
    }
    if (i >= p.size()) return std::string_view::npos;
    matched = hit != negate;
    return i + 1;
}

} // namespace detail

// git's wildmatch with WM_PATHNAME: '*', '?' and [...] never match '/',
// and "**" is special only as a whole path component:
//   "**/x" any leading directories, "x/**" everything inside, "a/**/b" zero or more directories.
// A backslash escapes the next character.
constexpr bool wildmatch(std::string_view p, std::string_view t) {
    std::size_t pi = 0, ti = 0;
    while (pi < p.size()) {
        const char c = p[pi];
        if (c == '*') {
            const bool component_start = pi == 0 || p[pi - 1] == '/';
            std::size_t end = pi;
            while (end < p.size() && p[end] == '*') ++end;
            const bool globstar = end - pi >= 2 && component_start && (end == p.size() || p[end] == '/');
            pi = end;
            if (globstar) {
                if (pi == p.size()) return true;
                const std::string_view rest = p.substr(pi + 1);
                if (wildmatch(rest, t.substr(ti))) return true;
                for (std::size_t k = ti; k < t.size(); ++k)
                    if (t[k] == '/' && wildmatch(rest, t.substr(k + 1))) return true;
                return false;
            }
            const std::string_view rest = p.substr(pi);
            for (std::size_t k = ti;; ++k) {
                if (wildmatch(rest, t.substr(k))) return true;
                if (k == t.size() || t[k] == '/') return false;
            }
        }
        if (ti == t.size()) return false;
        if (c == '?') {
            if (t[ti] == '/') return false;
        } else if (c == '[') {
            bool matched = false;
            const std::size_t next = detail::match_bracket(p, pi, t[ti], matched);
            if (next != std::string_view::npos) {
                if (!matched || t[ti] == '/') return false;
                pi = next;
                ++ti;
                continue;
            }
            if (t[ti] != '[') return false; // unterminated: a literal '['
        } else if (c == '\\') {
            if (pi + 1 == p.size() || p[pi + 1] != t[ti]) return false; // a trailing '\' never matches
            ++pi;
        } else if (c != t[ti]) {
            return false;
        }
        ++pi;
        ++ti;
    }
    return ti == t.size();
}

static_assert(wildmatch("*.log", "debug.log") && !wildmatch("*.log", "logs/debug.log"));
static_assert(wildmatch("**/foo", "foo") && wildmatch("**/foo", "a/b/foo") && !wildmatch("**/foo", "afoo"));
static_assert(wildmatch("abc/**", "abc/x/y") && !wildmatch("abc/**", "abc"));
static_assert(wildmatch("a/**/b", "a/b") && wildmatch("a/**/b", "a/x/y/b") && !wildmatch("a/**/b", "a/xb"));
static_assert(wildmatch("x**y", "xaay") && !wildmatch("x**y", "xa/y")); // not a whole component: plain '*'
static_assert(wildmatch("*.[oa]", "lib.a") && !wildmatch("*.[oa]", "lib.c"));
static_assert(wildmatch("[!a-c]x", "dx") && !wildmatch("[!a-c]x", "bx") && wildmatch("[]]", "]"));
static_assert(wildmatch("[[:digit:]][[:upper:]]", "7Q") && !wildmatch("[[:digit:]]", "q"));
static_assert(wildmatch("\\#notes", "#notes") && wildmatch("a\\*b", "a*b") && !wildmatch("a\\*b", "axb"));
static_assert(wildmatch("[ab", "[ab") && !wildmatch("trail\\", "trail\\"));

// One line of a .gitignore file.
struct GitignoreRule {
    std::string pattern;   // leading '/', trailing '/' and '!' removed
    bool negated = false;  // "!pattern" re-includes
    bool dir_only = false; // "pattern/" matches directories only
    bool anchored = false; // a '/' at the start or in the middle: match the whole relative path

    // Returns false for blank lines and comments.
    constexpr bool parse(std::string_view line) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.remove_suffix(1);
        // Trailing spaces are ignored unless escaped with a backslash.
        while (!line.empty() && line.back() == ' ' && !(line.size() >= 2 && line[line.size() - 2] == '\\')) line.remove_suffix(1);
        if (line.empty() || line.front() == '#') return false;
        if (line.front() == '!') {
            negated = true;
            line.remove_prefix(1);
        }
        if (line.ends_with('/') && !line.ends_with("\\/")) {
            dir_only = true;
            line.remove_suffix(1);
        }
        anchored = line.find('/') != std::string_view::npos;
        if (line.starts_with('/')) line.remove_prefix(1);
        pattern = line;
        return !pattern.empty();
    }

    constexpr bool matches(std::string_view rel_path, bool is_dir) const {
        if (dir_only && !is_dir) return false;
        if (anchored) return wildmatch(pattern, rel_path);
        const std::size_t slash = rel_path.rfind('/');
        return wildmatch(pattern, slash == std::string_view::npos ? rel_path : rel_path.substr(slash + 1));
    }
};

// Ordered .gitignore rules: the last matching rule decides, as in git.
class GitignoreRules {
    std::vector<GitignoreRule> rules_;

public:
    constexpr void add_line(std::string_view line) {
        GitignoreRule rule;
        if (rule.parse(line)) rules_.push_back(std::move(rule));
    }

    bool load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) add_line(line);
        return true;
    }

    constexpr bool empty() const { return rules_.empty(); }

    constexpr bool ignored(std::string_view rel_path, bool is_dir) const {
        for (auto it = rules_.rbegin(); it != rules_.rend(); ++it)
            if (it->matches(rel_path, is_dir)) return !it->negated;
        return false;
    }
};

namespace detail {
constexpr bool gitignored(std::initializer_list<std::string_view> lines, std::string_view path, bool is_dir = false) {
    GitignoreRules rules;
    for (const auto line : lines) rules.add_line(line);
    return rules.ignored(path, is_dir);
}
} // namespace detail

// Compile-time checks against git's documented behavior
static_assert(detail::gitignored({"*.log"}, "a/b/debug.log"));                     // no slash: any depth
static_assert(detail::gitignored({"build/"}, "src/build", true));                  // trailing slash: directories...
static_assert(!detail::gitignored({"build/"}, "src/build"));                       // ...only
static_assert(detail::gitignored({"/dist"}, "dist") && !detail::gitignored({"/dist"}, "app/dist")); // rooted
static_assert(detail::gitignored({"doc/frotz"}, "doc/frotz") && !detail::gitignored({"doc/frotz"}, "a/doc/frotz"));
static_assert(detail::gitignored({"**/cache"}, "x/y/cache", true));
static_assert(!detail::gitignored({"*.log", "!keep.log"}, "logs/keep.log"));       // last match wins
static_assert(detail::gitignored({"!keep.log", "*.log"}, "keep.log"));
static_assert(detail::gitignored({"name\\ "}, "name ") && detail::gitignored({"name   "}, "name"));
static_assert(detail::gitignored({"\\!bang"}, "!bang") && !detail::gitignored({"# comment"}, "# comment"));
static_assert(detail::gitignored({"[Bb]in/"}, "Bin", true) && !detail::gitignored({"[Bb]in/"}, "bin.txt"));

struct PatternRule {
    bool is_negated = false; // Starts with !
    bool is_regex = false;
    std::regex rx;           // regex rules only
    std::string glob;        // glob rules only
    std::string raw_pattern;

    static PatternRule parse(std::string_view input) {
        PatternRule rule;

        // Trim whitespace
        while (!input.empty() && (input.back() == ' ' || input.back() == '\r' || input.back() == '\n')) input.remove_suffix(1);
        const std::size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos) return rule;
        input.remove_prefix(first);

        if (input.front() == '#') return rule; // Comment

        if (input.front() == '!') {
            rule.is_negated = true;
            input.remove_prefix(1);
        }

        rule.raw_pattern = input;

        if (is_regex_spec(input)) {
            rule.is_regex = true;
            const std::size_t last_slash = input.rfind('/');
            const std::string pattern_body(input.substr(1, last_slash - 1));
            auto syntax_flags = std::regex_constants::ECMAScript;
            for (const char f : input.substr(last_slash + 1))
                if (f == 'i') syntax_flags |= std::regex_constants::icase;
            try {
                rule.rx = std::regex(pattern_body, syntax_flags);
            } catch (const std::regex_error&) {
                rule.raw_pattern.clear(); // invalid regex never matches
            }
        } else {
            if (input.starts_with("./")) input.remove_prefix(2);
            rule.glob = input;
        }
        return rule;
    }

    bool matches(std::string_view path) const {
        if (raw_pattern.empty()) return false;
        if (!is_regex) return glob_match(glob, path);
        return std::regex_search(path.begin(), path.end(), rx);
    }
};

// Combines command-line rules (-e/-i: anchored globs or /regex/) with .gitignore rules.
class FilterMatcher {
    std::vector<PatternRule> includes;
    std::vector<PatternRule> excludes;
    GitignoreRules gitignore;

public:
    void add_exclude(std::string_view pattern) {
        auto rule = PatternRule::parse(pattern);
        if (!rule.raw_pattern.empty()) excludes.push_back(std::move(rule));
    }

    void add_include(std::string_view pattern) {
        auto rule = PatternRule::parse(pattern);
        if (!rule.raw_pattern.empty()) includes.push_back(std::move(rule));
    }

    // Appends the rules of a .gitignore-style file; later files take precedence.
    bool load_gitignore(const std::string& filepath) { return gitignore.load(filepath); }

    bool has_includes() const { return !includes.empty(); }

    // `parent_excluded`: an ancestor directory is excluded. As in git, nothing below an
    // excluded directory can be re-included by a .gitignore "!" rule; only -i can.
    bool is_excluded(std::string_view relative_path, bool is_dir, bool parent_excluded = false) const {
        for (const auto& inc : includes) {
            if (inc.matches(relative_path)) return false;
        }
        if (parent_excluded) return true;
        // Like git, never treat a repository's own .git directory (or a submodule's .git file) as content.
        const std::size_t slash = relative_path.rfind('/');
        if (relative_path.substr(slash == std::string_view::npos ? 0 : slash + 1) == ".git") return true;
        for (const auto& exc : excludes) {
            if (exc.matches(relative_path)) return true;
        }
        return gitignore.ignored(relative_path, is_dir);
    }
};

// Helper for parsing raw regex pattern e.g. /pattern/replace/flags or /pattern/
struct RegexSpec {
    std::regex rx;
    std::string replacement;
    bool valid = false;

    static RegexSpec parse(std::string_view spec) {
        RegexSpec res;
        if (!is_regex_spec(spec)) return res;

        std::vector<std::string> parts;
        std::string current;
        bool escaped = false;

        for (const char c : spec) {
            if (escaped) {
                current += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
                current += c;
            } else if (c == '/') {
                parts.push_back(std::move(current));
                current.clear();
            } else {
                current += c;
            }
        }
        parts.push_back(std::move(current));

        // parts[0] is empty (before first /)
        if (parts.size() >= 3) {
            const std::string& pattern = parts[1];
            const std::string replace = (parts.size() >= 4) ? parts[2] : "";
            const std::string& flags = (parts.size() >= 4) ? parts[3] : parts[2];

            auto syntax_flags = std::regex_constants::ECMAScript;
            for (const char f : flags) {
                if (f == 'i') syntax_flags |= std::regex_constants::icase;
            }

            try {
                res.rx = std::regex(pattern, syntax_flags);
                res.replacement = replace;
                res.valid = true;
            } catch (const std::regex_error&) {
                res.valid = false;
            }
        }
        return res;
    }
};

} // namespace fsturbo

#endif
