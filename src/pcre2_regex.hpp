#ifndef PCRE2_REGEX_HPP
#define PCRE2_REGEX_HPP

// Regular expressions through PCRE2 (the system libpcre2-8): lookbehind and lookahead, named
// groups, atomic groups, possessive quantifiers, \K, Unicode-aware '.' ... and, when the
// library was built with it, a JIT compiler. Patterns and subjects are UTF-8; bytes that are not
// valid UTF-8 (file names can contain anything) simply never match instead of failing.
//
// Flags (the letters after the last '/' of a /pattern/replacement/flags spec): i caseless,
// m multiline, s dot matches newline, x extended. Others (g, u, ...) are accepted and ignored:
// replacement is always global, and matching is always UTF-8.

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fsturbo {

// One match: byte offsets into the subject, valid until the next match on the same Regex.
class MatchView {
    std::string_view subject_;
    const PCRE2_SIZE* ovector_ = nullptr;
    std::size_t count_ = 0;

public:
    MatchView() = default;
    MatchView(std::string_view subject, const PCRE2_SIZE* ovector, std::size_t count) : subject_(subject), ovector_(ovector), count_(count) {}

    std::size_t size() const { return count_; } // capture groups + 1
    bool matched(std::size_t i) const { return i < count_ && ovector_[2 * i] != PCRE2_UNSET; }
    std::string_view operator[](std::size_t i) const {
        return matched(i) ? subject_.substr(ovector_[2 * i], ovector_[2 * i + 1] - ovector_[2 * i]) : std::string_view{};
    }
    std::size_t start() const { return ovector_[0]; }
    std::size_t end() const { return ovector_[1]; }
};

class Regex {
    pcre2_code* code_ = nullptr;
    pcre2_match_data* data_ = nullptr;
    std::size_t groups_ = 0;

    static std::string error_text(int code) {
        PCRE2_UCHAR buffer[256];
        return pcre2_get_error_message(code, buffer, sizeof buffer) > 0 ? std::string(reinterpret_cast<const char*>(buffer)) : std::format("error {}", code);
    }

    int match_at(std::string_view subject, std::size_t offset, std::uint32_t options) const {
        return pcre2_match(code_, reinterpret_cast<PCRE2_SPTR>(subject.data()), subject.size(), offset, options, data_, nullptr);
    }

    // The next character boundary after `i` (a whole UTF-8 sequence).
    static std::size_t next_char(std::string_view s, std::size_t i) {
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        return i;
    }

    void release() noexcept {
        pcre2_match_data_free(data_);
        pcre2_code_free(code_);
        data_ = nullptr;
        code_ = nullptr;
    }

public:
    Regex() = default;
    Regex(const Regex&) = delete;
    Regex& operator=(const Regex&) = delete;
    Regex(Regex&& other) noexcept
        : code_(std::exchange(other.code_, nullptr)), data_(std::exchange(other.data_, nullptr)), groups_(other.groups_) {}
    Regex& operator=(Regex&& other) noexcept {
        if (this != &other) {
            release();
            code_ = std::exchange(other.code_, nullptr);
            data_ = std::exchange(other.data_, nullptr);
            groups_ = other.groups_;
        }
        return *this;
    }
    ~Regex() { release(); }

    static std::expected<Regex, std::string> compile(std::string_view pattern, std::string_view flags = {}) {
        std::uint32_t options = PCRE2_UTF | PCRE2_MATCH_INVALID_UTF;
        for (const char f : flags) {
            switch (f) {
                case 'i': options |= PCRE2_CASELESS; break;
                case 'm': options |= PCRE2_MULTILINE; break;
                case 's': options |= PCRE2_DOTALL; break;
                case 'x': options |= PCRE2_EXTENDED; break;
                default: break;
            }
        }
        int error = 0;
        PCRE2_SIZE where = 0;
        pcre2_code* code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(), options, &error, &where, nullptr);
        if (code == nullptr) return std::unexpected(std::format("{} (at offset {})", error_text(error), where));

        Regex out;
        out.code_ = code;
        (void)pcre2_jit_compile(code, PCRE2_JIT_COMPLETE); // optional: falls back to the interpreter
        out.data_ = pcre2_match_data_create_from_pattern(code, nullptr);
        std::uint32_t count = 0;
        pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &count);
        out.groups_ = count;
        return out;
    }

    // Number of capture groups, named or not.
    std::size_t group_count() const { return groups_; }

    // (name, group number) of every named group, in pattern order.
    std::vector<std::pair<std::string, std::size_t>> group_names() const {
        std::uint32_t count = 0, entry_size = 0;
        PCRE2_SPTR table = nullptr;
        pcre2_pattern_info(code_, PCRE2_INFO_NAMECOUNT, &count);
        pcre2_pattern_info(code_, PCRE2_INFO_NAMEENTRYSIZE, &entry_size);
        pcre2_pattern_info(code_, PCRE2_INFO_NAMETABLE, &table);
        std::vector<std::pair<std::string, std::size_t>> names;
        for (std::uint32_t i = 0; i < count; ++i) {
            const PCRE2_UCHAR* entry = table + static_cast<std::size_t>(i) * entry_size;
            names.emplace_back(reinterpret_cast<const char*>(entry + 2), (static_cast<std::size_t>(entry[0]) << 8) | entry[1]);
        }
        std::ranges::sort(names, {}, &std::pair<std::string, std::size_t>::second);
        return names;
    }

    // Is there a match anywhere in `subject`?
    bool search(std::string_view subject) const { return match_at(subject, 0, 0) >= 0; }

    // Calls `visit(MatchView)` for every non-overlapping match, left to right. After an empty
    // match the engine retries at the same place for a non-empty one before stepping a character,
    // the way JavaScript's replaceAll does. `visit` returns std::expected<void, std::string>; an
    // error there, or one from the engine (say, a match limit), stops the walk and is returned.
    template <typename Visit>
    std::expected<void, std::string> for_each_match(std::string_view subject, Visit&& visit) const {
        std::size_t offset = 0;
        bool not_empty = false;
        while (offset <= subject.size()) {
            const std::uint32_t options = not_empty ? PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED : 0;
            const int rc = match_at(subject, offset, options);
            if (rc == PCRE2_ERROR_NOMATCH) {
                if (!not_empty) break;
                offset = next_char(subject, offset);
                not_empty = false;
                continue;
            }
            if (rc < 0) return std::unexpected(error_text(rc));
            const PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(data_);
            if (ovector[1] < ovector[0]) return std::unexpected("\\K inside a lookaround moves the match start past its end");
            if (auto result = visit(MatchView(subject, ovector, groups_ + 1)); !result) return result;
            offset = ovector[1];
            not_empty = ovector[1] == ovector[0];
        }
        return {};
    }
};

} // namespace fsturbo

#endif // PCRE2_REGEX_HPP
