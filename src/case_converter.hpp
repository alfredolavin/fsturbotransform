#ifndef CASE_CONVERTER_HPP
#define CASE_CONVERTER_HPP

#include <string>
#include <string_view>

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

// Locale-independent ASCII classification (matches <cctype> in the "C" locale, but constexpr).
namespace ascii {
constexpr bool is_lower(char c) { return c >= 'a' && c <= 'z'; }
constexpr bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool is_alnum(char c) { return is_lower(c) || is_upper(c) || is_digit(c); }
constexpr char to_lower(char c) { return is_upper(c) ? static_cast<char>(c + ('a' - 'A')) : c; }
constexpr char to_upper(char c) { return is_lower(c) ? static_cast<char>(c - ('a' - 'A')) : c; }
} // namespace ascii

// Split a name into word tokens (views into `name`): non-alphanumerics separate words,
// and a lower->UPPER transition starts a new word ("myFile" -> "my", "File").
template <typename F>
constexpr void tokenize(std::string_view name, F&& callback) {
    std::size_t start = std::string_view::npos;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char ch = name[i];
        if (!ascii::is_alnum(ch)) {
            if (start != std::string_view::npos) callback(name.substr(start, i - start));
            start = std::string_view::npos;
        } else if (start == std::string_view::npos) {
            start = i;
        } else if (ascii::is_lower(name[i - 1]) && ascii::is_upper(ch)) {
            callback(name.substr(start, i - start));
            start = i;
        }
    }
    if (start != std::string_view::npos) callback(name.substr(start));
}

struct CaseConverter {
    static constexpr std::string to_lower(std::string_view input) {
        std::string res(input);
        for (char& c : res) c = ascii::to_lower(c);
        return res;
    }

    static constexpr std::string to_upper(std::string_view input) {
        std::string res(input);
        for (char& c : res) c = ascii::to_upper(c);
        return res;
    }

    // Lower-cases every token, optionally capitalizes its first letter, and joins with `sep`.
    static constexpr std::string join_words(std::string_view input, std::string_view sep, bool cap_first, bool cap_rest) {
        std::string res;
        res.reserve(input.size() + 8);
        bool first = true;
        tokenize(input, [&](std::string_view tok) {
            if (!first) res += sep;
            const bool cap = first ? cap_first : cap_rest;
            for (std::size_t i = 0; i < tok.size(); ++i) res.push_back(i == 0 && cap ? ascii::to_upper(tok[i]) : ascii::to_lower(tok[i]));
            first = false;
        });
        return res;
    }

    static constexpr std::string to_snake(std::string_view input) { return join_words(input, "_", false, false); }
    static constexpr std::string to_camel(std::string_view input) { return join_words(input, "", false, true); }
    static constexpr std::string to_pascal(std::string_view input) { return join_words(input, "", true, true); }
    static constexpr std::string to_kebab(std::string_view input) { return join_words(input, "-", false, false); }
    static constexpr std::string to_title(std::string_view input) { return join_words(input, " ", true, true); }

    // Generic transform dispatcher template
    template <CaseStyle Style>
    static constexpr std::string transform(std::string_view input) {
        if constexpr (Style == CaseStyle::Lower) return to_lower(input);
        else if constexpr (Style == CaseStyle::Upper) return to_upper(input);
        else if constexpr (Style == CaseStyle::Snake) return to_snake(input);
        else if constexpr (Style == CaseStyle::Camel) return to_camel(input);
        else if constexpr (Style == CaseStyle::Pascal) return to_pascal(input);
        else if constexpr (Style == CaseStyle::Kebab) return to_kebab(input);
        else if constexpr (Style == CaseStyle::Title) return to_title(input);
        else return std::string(input);
    }

    static constexpr std::string transform_dynamic(std::string_view input, CaseStyle style) {
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

// Compile-time unit tests
static_assert(CaseConverter::to_lower("ReadMe.TXT") == "readme.txt");
static_assert(CaseConverter::to_upper("abc-1") == "ABC-1");
static_assert(CaseConverter::to_snake("HelloWorld-foo bar") == "hello_world_foo_bar");
static_assert(CaseConverter::to_camel("hello_world 2024") == "helloWorld2024");
static_assert(CaseConverter::to_pascal("my file-name") == "MyFileName");
static_assert(CaseConverter::to_kebab("MyFileName") == "my-file-name");
static_assert(CaseConverter::to_title("quarterly_REPORT") == "Quarterly Report");
static_assert(CaseConverter::transform<CaseStyle::Snake>("someName") == "some_name");

} // namespace fsturbo

#endif
