#ifndef MATCHER_HPP
#define MATCHER_HPP

#include <string>
#include <string_view>
#include <regex>
#include <vector>
#include <fstream>
#include <iostream>
#include <memory>
#include <algorithm>

namespace fsturbo {

// Check if string is a regex specification: starts with "/" and ends with "/" (or "/flags")
inline bool is_regex_spec(std::string_view spec) {
    if (spec.size() >= 2 && spec.front() == '/') {
        size_t last_slash = spec.rfind('/');
        return (last_slash != 0 && last_slash != std::string_view::npos);
    }
    return false;
}

struct PatternRule {
    bool is_negated = false; // Starts with !
    bool is_regex = false;
    std::regex rx;
    std::string raw_pattern;

    // Convert gitignore glob pattern to std::regex
    static std::string glob_to_regex(std::string_view glob) {
        std::string regex_str = "^";
        size_t i = 0;
        size_t len = glob.length();

        // Strip leading ./ if present
        if (len >= 2 && glob[0] == '.' && glob[1] == '/') {
            glob.remove_prefix(2);
            len = glob.length();
        }

        while (i < len) {
            char c = glob[i];
            if (c == '*') {
                if (i + 1 < len && glob[i + 1] == '*') {
                    // ** matches anything across slashes
                    regex_str += ".*";
                    i += 2;
                    if (i < len && glob[i] == '/') {
                        i++; // skip trailing slash after **
                    }
                } else {
                    // * matches anything except slash
                    regex_str += "[^/]*";
                    i++;
                }
            } else if (c == '?') {
                regex_str += "[^/]";
                i++;
            } else if (c == '.' || c == '+' || c == '(' || c == ')' || c == '{' || c == '}' ||
                       c == '[' || c == ']' || c == '^' || c == '$' || c == '|' || c == '\\') {
                regex_str += '\\';
                regex_str += c;
                i++;
            } else {
                regex_str += c;
                i++;
            }
        }
        regex_str += "$";
        return regex_str;
    }

    static PatternRule parse(std::string_view input) {
        PatternRule rule;
        std::string str(input);

        // Trim whitespace
        while (!str.empty() && (str.back() == ' ' || str.back() == '\r' || str.back() == '\n')) str.pop_back();
        size_t first = str.find_first_not_of(" \t");
        if (first == std::string::npos) return rule;
        str = str.substr(first);

        if (str.empty() || str[0] == '#') return rule; // Comment

        if (str[0] == '!') {
            rule.is_negated = true;
            str = str.substr(1);
        }

        rule.raw_pattern = str;

        if (is_regex_spec(str)) {
            rule.is_regex = true;
            size_t last_slash = str.rfind('/');
            std::string pattern_body = str.substr(1, last_slash - 1);
            std::string flags = str.substr(last_slash + 1);

            auto syntax_flags = std::regex_constants::ECMAScript;
            for (char f : flags) {
                if (f == 'i') syntax_flags |= std::regex_constants::icase;
            }
            try {
                rule.rx = std::regex(pattern_body, syntax_flags);
            } catch (...) {}
        } else {
            rule.is_regex = false;
            std::string rx_str = glob_to_regex(str);
            try {
                rule.rx = std::regex(rx_str, std::regex_constants::ECMAScript);
            } catch (...) {}
        }

        return rule;
    }

    bool matches(const std::string& path) const {
        if (raw_pattern.empty()) return false;
        try {
            return std::regex_search(path, rx);
        } catch (...) {
            return false;
        }
    }
};

class FilterMatcher {
    std::vector<PatternRule> includes;
    std::vector<PatternRule> excludes;

public:
    void add_exclude(std::string_view pattern) {
        auto rule = PatternRule::parse(pattern);
        if (!rule.raw_pattern.empty()) excludes.push_back(rule);
    }

    void add_include(std::string_view pattern) {
        auto rule = PatternRule::parse(pattern);
        if (!rule.raw_pattern.empty()) includes.push_back(rule);
    }

    void load_gitignore(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line[0] == '!') {
                add_include(line.substr(1));
            } else {
                add_exclude(line);
            }
        }
    }

    // Returns true if the path should be excluded
    bool is_excluded(const std::string& relative_path) const {
        // If explicitly included by include rules, do not exclude
        for (const auto& inc : includes) {
            if (inc.matches(relative_path)) return false;
        }

        // Check exclude rules
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

        std::string str(spec);
        std::vector<std::string> parts;
        std::string current;
        bool escaped = false;

        for (size_t i = 0; i < str.length(); ++i) {
            char c = str[i];
            if (escaped) {
                current += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
                current += c;
            } else if (c == '/') {
                parts.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        parts.push_back(current);

        // parts[0] is empty (before first /)
        if (parts.size() >= 3) {
            std::string pattern = parts[1];
            std::string replace = (parts.size() >= 4) ? parts[2] : "";
            std::string flags = (parts.size() >= 4) ? parts[3] : parts[2];

            auto syntax_flags = std::regex_constants::ECMAScript;
            for (char f : flags) {
                if (f == 'i') syntax_flags |= std::regex_constants::icase;
            }

            try {
                res.rx = std::regex(pattern, syntax_flags);
                res.replacement = replace;
                res.valid = true;
            } catch (const std::exception& e) {
                res.valid = false;
            }
        }
        return res;
    }
};

} // namespace fsturbo

#endif
