#ifndef MATCHER_HPP
#define MATCHER_HPP

#include <fstream>
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

class FilterMatcher {
    std::vector<PatternRule> includes;
    std::vector<PatternRule> excludes;

public:
    void add_exclude(std::string_view pattern) {
        auto rule = PatternRule::parse(pattern);
        if (!rule.raw_pattern.empty()) excludes.push_back(std::move(rule));
    }

    void add_include(std::string_view pattern) {
        auto rule = PatternRule::parse(pattern);
        if (!rule.raw_pattern.empty()) includes.push_back(std::move(rule));
    }

    void load_gitignore(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line[0] == '!') {
                add_include(std::string_view(line).substr(1));
            } else {
                add_exclude(line);
            }
        }
    }

    bool has_includes() const { return !includes.empty(); }

    // Returns true if the path should be excluded
    bool is_excluded(std::string_view relative_path) const {
        // If explicitly included by include rules, do not exclude
        for (const auto& inc : includes) {
            if (inc.matches(relative_path)) return false;
        }
        for (const auto& exc : excludes) {
            if (exc.matches(relative_path)) return true;
        }
        return false;
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
