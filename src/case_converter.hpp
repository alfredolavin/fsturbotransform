#ifndef CASE_CONVERTER_HPP
#define CASE_CONVERTER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <algorithm>
#include <regex>
#include <functional>

namespace fsturbo {

enum class CaseStyle {
    None,
    Lower,
    Upper,
    Snake,
    Camel,
    Pascal,
    Kebab,
    Title
};

// Split string into word tokens using C++ lambdas
template <typename F>
inline void tokenize(std::string_view name, F&& callback) {
    std::string current;
    for (size_t i = 0; i < name.length(); ++i) {
        char ch = name[i];
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            // Check camelCase transition (e.g. "a" -> "B")
            if (!current.empty() && std::islower(static_cast<unsigned char>(current.back())) &&
                std::isupper(static_cast<unsigned char>(ch))) {
                callback(current);
                current.clear();
            }
            current.push_back(ch);
        } else {
            if (!current.empty()) {
                callback(current);
                current.clear();
            }
        }
    }
    if (!current.empty()) {
        callback(current);
    }
}

// Case transformers implemented via templates & lambdas
struct CaseConverter {
    static std::string to_lower(std::string_view input) {
        std::string res(input);
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::tolower(c); });
        return res;
    }

    static std::string to_upper(std::string_view input) {
        std::string res(input);
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::toupper(c); });
        return res;
    }

    static std::string to_snake(std::string_view input) {
        std::vector<std::string> tokens;
        tokenize(input, [&](std::string_view tok) {
            tokens.push_back(to_lower(tok));
        });
        std::string res;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) res += "_";
            res += tokens[i];
        }
        return res;
    }

    static std::string to_camel(std::string_view input) {
        std::vector<std::string> tokens;
        tokenize(input, [&](std::string_view tok) {
            tokens.push_back(to_lower(tok));
        });
        std::string res;
        for (size_t i = 0; i < tokens.size(); ++i) {
            std::string t = tokens[i];
            if (i == 0) {
                res += t;
            } else if (!t.empty()) {
                t[0] = std::toupper(static_cast<unsigned char>(t[0]));
                res += t;
            }
        }
        return res;
    }

    static std::string to_pascal(std::string_view input) {
        std::vector<std::string> tokens;
        tokenize(input, [&](std::string_view tok) {
            tokens.push_back(to_lower(tok));
        });
        std::string res;
        for (const auto& tok : tokens) {
            if (!tok.empty()) {
                std::string t = tok;
                t[0] = std::toupper(static_cast<unsigned char>(t[0]));
                res += t;
            }
        }
        return res;
    }

    static std::string to_kebab(std::string_view input) {
        std::vector<std::string> tokens;
        tokenize(input, [&](std::string_view tok) {
            tokens.push_back(to_lower(tok));
        });
        std::string res;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) res += "-";
            res += tokens[i];
        }
        return res;
    }

    static std::string to_title(std::string_view input) {
        std::vector<std::string> tokens;
        tokenize(input, [&](std::string_view tok) {
            tokens.push_back(to_lower(tok));
        });
        std::string res;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) res += " ";
            std::string t = tokens[i];
            if (!t.empty()) {
                t[0] = std::toupper(static_cast<unsigned char>(t[0]));
                res += t;
            }
        }
        return res;
    }

    // Generic transform dispatcher template
    template<CaseStyle Style>
    static std::string transform(std::string_view input) {
        if constexpr (Style == CaseStyle::Lower) return to_lower(input);
        else if constexpr (Style == CaseStyle::Upper) return to_upper(input);
        else if constexpr (Style == CaseStyle::Snake) return to_snake(input);
        else if constexpr (Style == CaseStyle::Camel) return to_camel(input);
        else if constexpr (Style == CaseStyle::Pascal) return to_pascal(input);
        else if constexpr (Style == CaseStyle::Kebab) return to_kebab(input);
        else if constexpr (Style == CaseStyle::Title) return to_title(input);
        else return std::string(input);
    }

    static std::string transform_dynamic(std::string_view input, CaseStyle style) {
        switch (style) {
            case CaseStyle::Lower: return to_lower(input);
            case CaseStyle::Upper: return to_upper(input);
            case CaseStyle::Snake: return to_snake(input);
            case CaseStyle::Camel: return to_camel(input);
            case CaseStyle::Pascal: return to_pascal(input);
            case CaseStyle::Kebab: return to_kebab(input);
            case CaseStyle::Title: return to_title(input);
            default: return std::string(input);
        }
    }
};

// Generic Regex Replacer with Case Conversion syntax:
// Supports:
// \U -> uppercase rest of replacement
// \L -> lowercase rest of replacement
// \E -> end case modification
// \C -> capitalize next char
inline std::string apply_regex_replace(
    const std::string& filename,
    const std::regex& pattern,
    const std::string& fmt
) {
    std::string result;
    std::sregex_iterator begin(filename.begin(), filename.end(), pattern);
    std::sregex_iterator end;

    if (begin == end) {
        return filename;
    }

    size_t last_pos = 0;
    for (auto it = begin; it != end; ++it) {
        const std::smatch& match = *it;
        result.append(filename, last_pos, match.position() - last_pos);

        std::string replaced;
        bool in_upper = false, in_lower = false, cap_next = false;

        for (size_t i = 0; i < fmt.length(); ++i) {
            if (fmt[i] == '\\' && i + 1 < fmt.length()) {
                char next = fmt[i+1];
                if (next == 'U') { in_upper = true; in_lower = false; i++; continue; }
                if (next == 'L') { in_lower = true; in_upper = false; i++; continue; }
                if (next == 'E') { in_upper = false; in_lower = false; i++; continue; }
                if (next == 'C') { cap_next = true; i++; continue; }
                if (std::isdigit(static_cast<unsigned char>(next))) {
                    int grp = next - '0';
                    if (grp < static_cast<int>(match.size())) {
                        std::string gstr = match[grp].str();
                        for (char gc : gstr) {
                            if (cap_next) {
                                replaced += std::toupper(static_cast<unsigned char>(gc));
                                cap_next = false;
                            } else if (in_upper) {
                                replaced += std::toupper(static_cast<unsigned char>(gc));
                            } else if (in_lower) {
                                replaced += std::tolower(static_cast<unsigned char>(gc));
                            } else {
                                replaced += gc;
                            }
                        }
                    }
                    i++;
                    continue;
                }
            } else if (fmt[i] == '$' && i + 1 < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[i+1]))) {
                int grp = fmt[i+1] - '0';
                if (grp < static_cast<int>(match.size())) {
                    std::string gstr = match[grp].str();
                    for (char gc : gstr) {
                        if (cap_next) {
                            replaced += std::toupper(static_cast<unsigned char>(gc));
                            cap_next = false;
                        } else if (in_upper) {
                            replaced += std::toupper(static_cast<unsigned char>(gc));
                        } else if (in_lower) {
                            replaced += std::tolower(static_cast<unsigned char>(gc));
                        } else {
                            replaced += gc;
                        }
                    }
                }
                i++;
                continue;
            }

            char c = fmt[i];
            if (cap_next) {
                replaced += std::toupper(static_cast<unsigned char>(c));
                cap_next = false;
            } else if (in_upper) {
                replaced += std::toupper(static_cast<unsigned char>(c));
            } else if (in_lower) {
                replaced += std::tolower(static_cast<unsigned char>(c));
            } else {
                replaced += c;
            }
        }
        result += replaced;
        last_pos = match.position() + match.length();
    }
    result.append(filename, last_pos, std::string::npos);
    return result;
}

} // namespace fsturbo

#endif
