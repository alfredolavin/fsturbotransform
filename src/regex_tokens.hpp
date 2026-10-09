#ifndef REGEX_TOKENS_HPP
#define REGEX_TOKENS_HPP

// Syntax highlighting tokens for the GUI's regex editors: a -r spec (/pattern/replacement/flags,
// with \<name=>{js} expression blocks), a --flatten-regex spec (/pattern/flags) and an -e/-i
// filter (a glob, or a /regex/flags). The text is split into fields exactly the way the engine
// splits it (RegexSpec::parse, PatternRule::parse), so a colour never disagrees with what it will
// do. Offsets are UTF-8 bytes.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "matcher.hpp"
#include "named_groups.hpp"
#include "replace_template.hpp"

namespace fsturbo::syntax {

enum class Token : std::uint8_t {
    Delimiter,   // the '/' between fields
    Flag,        // i m s x
    IgnoredFlag, // g u: accepted and ignored
    Escape,      // \d \. \x{41} \Q...\E
    Class,       // [...] bracket expression
    Group,       // ( (?: (?<= ) ...: `depth` is the nesting level
    GroupName,   // the name in (?<name>...)
    Arrow,       // the "=>" of (?<name=>...): the group's text is a number in expressions
    Quantifier,  // * + ? {2,3} and their lazy/possessive forms
    Anchor,      // ^ $
    Alternation, // |
    Dot,         // .
    Comment,     // (?#...), # in x mode, a field the engine ignores
    GroupRef,    // $1 \1 $<name> \<name> \k<name>
    CaseOp,      // \U \L \E \C
    ExprHead,    // \<name=> \<name=v>
    ExprBrace,   // the { } around an expression
    JsNumber,
    JsString,
    JsVariable,  // val, the head's variable, named groups, $0, index, nameIndex, nameLength, depth
    JsFunction,  // Math's members (round, floor, PI, ...) and Math itself
    JsOperator,
    Glob,        // * ** ? in a filter glob
    Error,       // unclosed ( [ {, an unknown group, a stray ')', an unknown flag
    Expression,  // the whole JavaScript between the braces (under its own tokens)
};

struct Span {
    std::size_t start = 0;
    std::size_t length = 0;
    Token token = Token::Error;
    int depth = 0; // Group: nesting level (0 = outermost)
};

enum class Mode : std::uint8_t {
    Rename, // -r: /pattern/replacement/flags
    Select, // --flatten-regex: /pattern/flags
    Filter, // -e / -i: a glob, or /pattern/flags
};

namespace detail {

inline constexpr std::array<std::string_view, 44> kMath = {
    "Math", "abs", "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh", "cbrt", "ceil",
    "clz32", "cos", "cosh", "exp", "expm1", "floor", "fround", "hypot", "imul", "log", "log10",
    "log1p", "log2", "max", "min", "pow", "random", "round", "sign", "sin", "sinh", "sqrt", "tan",
    "tanh", "trunc", "E", "LN10", "LN2", "LOG10E", "LOG2E", "PI", "SQRT1_2", "SQRT2"};

inline constexpr std::array<std::string_view, 5> kVariables = {"val", "index", "nameIndex", "nameLength", "depth"};

constexpr bool contains(auto const& list, std::string_view name) { return std::ranges::find(list, name) != list.end(); }

class Tokenizer {
    std::string_view text_;
    std::vector<Span>& out_;
    std::vector<std::string_view> group_names_; // named groups of the pattern

    void emit(std::size_t start, std::size_t length, Token token, int depth = 0) {
        if (length > 0) out_.push_back({start, length, token, depth});
    }

    // "{2}", "{2,}", "{2,5}", "{,5}": the length of a counted quantifier at `i`, or 0.
    std::size_t counted_quantifier(std::size_t i, std::size_t end) const {
        std::size_t j = i + 1, digits = 0;
        while (j < end && ascii::is_digit(text_[j])) ++j, ++digits;
        if (j < end && text_[j] == ',') {
            ++j;
            while (j < end && ascii::is_digit(text_[j])) ++j, ++digits;
        }
        return digits > 0 && j < end && text_[j] == '}' ? j + 1 - i : 0;
    }

    // An escape at `i` (text_[i] == '\\'): its length, with braced/angled arguments.
    std::size_t escape_length(std::size_t i, std::size_t end) const {
        if (i + 1 >= end) return 1;
        const char next = text_[i + 1];
        if (i + 2 < end && std::string_view("xpPgkoN").contains(next)) {
            const char open = text_[i + 2];
            const char close = open == '{' ? '}' : open == '<' ? '>' : open == '\'' ? '\'' : '\0';
            if (close != '\0') {
                const std::size_t at = text_.find(close, i + 3);
                if (at != std::string_view::npos && at < end) return at + 1 - i;
            }
        }
        return 2;
    }

public:
    Tokenizer(std::string_view text, std::vector<Span>& out) : text_(text), out_(out) {}

    void pattern(std::size_t begin, std::size_t end, bool extended) {
        std::vector<std::size_t> open; // positions of the '(' not closed yet
        for (std::size_t i = begin; i < end;) {
            const char c = text_[i];
            const int depth = static_cast<int>(open.size());
            if (c == '\\') {
                const char next = i + 1 < end ? text_[i + 1] : '\0';
                if (next == 'Q') { // \Q...\E: everything in between is literal
                    const std::size_t stop = text_.substr(0, end).find("\\E", i + 2);
                    emit(i, 2, Token::Escape);
                    if (stop == std::string_view::npos) return;
                    emit(stop, 2, Token::Escape);
                    i = stop + 2;
                    continue;
                }
                const std::size_t n = escape_length(i, end);
                const bool backref = ascii::is_digit(next) || next == 'k' || next == 'g';
                emit(i, n, backref ? Token::GroupRef : Token::Escape);
                i += n;
            } else if (c == '[') {
                std::size_t j = i + 1;
                if (j < end && text_[j] == '^') ++j;
                if (j < end && text_[j] == ']') ++j;
                bool closed = false;
                while (j < end) {
                    if (text_[j] == '\\') {
                        j += 2;
                    } else if (text_.substr(j).starts_with("[:")) {
                        const std::size_t posix = text_.find(":]", j + 2);
                        j = posix == std::string_view::npos || posix >= end ? j + 1 : posix + 2;
                    } else if (text_[j] == ']') {
                        closed = true;
                        ++j;
                        break;
                    } else {
                        ++j;
                    }
                }
                j = std::min(j, end);
                if (!closed) {
                    emit(i, 1, Token::Error);
                    emit(i + 1, j - i - 1, Token::Class);
                } else {
                    emit(i, j - i, Token::Class);
                }
                i = j;
            } else if (c == '(') {
                const std::string_view rest = text_.substr(i, end - i);
                if (rest.starts_with("(?#")) { // comment up to the next ')'
                    const std::size_t close = rest.find(')');
                    const std::size_t n = close == std::string_view::npos ? rest.size() : close + 1;
                    emit(i, n, close == std::string_view::npos ? Token::Error : Token::Comment);
                    i += n;
                    continue;
                }
                std::size_t head = 1;
                const bool named_angle = rest.starts_with("(?<") && !rest.starts_with("(?<=") && !rest.starts_with("(?<!");
                const bool named_p = rest.starts_with("(?P<");
                const bool named_quote = rest.starts_with("(?'");
                if (named_angle || named_p || named_quote) {
                    const std::size_t at = named_p ? 4 : 3;
                    const std::size_t len = identifier_length(rest.substr(at));
                    emit(i, at, Token::Group, depth);
                    emit(i + at, len, Token::GroupName);
                    if (len > 0) group_names_.push_back(rest.substr(at, len));
                    std::size_t after = at + len;
                    if (named_angle && rest.substr(after).starts_with("=>")) {
                        emit(i + after, 2, Token::Arrow);
                        after += 2;
                    } else if (after < rest.size() && rest[after] == (named_quote ? '\'' : '>')) {
                        emit(i + after, 1, Token::Group, depth);
                        ++after;
                    } else {
                        emit(i + after, 0, Token::Error);
                    }
                    open.push_back(i);
                    i += after;
                    continue;
                }
                if (rest.starts_with("(*")) { // (*VERB) / (*UTF)
                    const std::size_t close = rest.find(')');
                    const std::size_t n = close == std::string_view::npos ? rest.size() : close + 1;
                    emit(i, n, Token::Anchor);
                    i += n;
                    continue;
                }
                if (rest.starts_with("(?<=") || rest.starts_with("(?<!")) head = 4;
                else if (rest.starts_with("(?")) {
                    // (?: (?= (?! (?> (?| or inline options (?i) (?i-m:
                    head = 2;
                    while (head < rest.size() && (ascii::is_alnum(rest[head]) || rest[head] == '-' || rest[head] == '^')) ++head;
                    if (head < rest.size() && std::string_view(":=!>|)").contains(rest[head])) ++head;
                    if (rest[head - 1] == ')') { // (?i): options only, not a group
                        emit(i, head, Token::Group, depth);
                        i += head;
                        continue;
                    }
                }
                emit(i, head, Token::Group, depth);
                open.push_back(i);
                i += head;
            } else if (c == ')') {
                if (open.empty()) {
                    emit(i, 1, Token::Error);
                } else {
                    open.pop_back();
                    emit(i, 1, Token::Group, static_cast<int>(open.size()));
                }
                ++i;
            } else if (c == '*' || c == '+' || c == '?') {
                const std::size_t n = i + 1 < end && (text_[i + 1] == '?' || text_[i + 1] == '+') ? 2 : 1;
                emit(i, n, Token::Quantifier);
                i += n;
            } else if (c == '{' && counted_quantifier(i, end) > 0) {
                std::size_t n = counted_quantifier(i, end);
                if (i + n < end && (text_[i + n] == '?' || text_[i + n] == '+')) ++n;
                emit(i, n, Token::Quantifier);
                i += n;
            } else if (c == '^' || c == '$') {
                emit(i++, 1, Token::Anchor);
            } else if (c == '|') {
                emit(i++, 1, Token::Alternation);
            } else if (c == '.') {
                emit(i++, 1, Token::Dot);
            } else if (extended && c == '#') {
                const std::size_t nl = text_.find('\n', i);
                const std::size_t stop = nl == std::string_view::npos || nl > end ? end : nl;
                emit(i, stop - i, Token::Comment);
                i = stop;
            } else {
                ++i;
            }
        }
        for (const std::size_t at : open) // never closed: the opening becomes an error
            for (Span& span : out_)
                if (span.start == at && span.token == Token::Group) span.token = Token::Error;
    }

    void javascript(std::size_t begin, std::size_t end, std::string_view variable) {
        for (std::size_t i = begin; i < end;) {
            const char c = text_[i];
            if (c == '\'' || c == '"' || c == '`') {
                const std::size_t after = skip_string(text_.substr(0, end), i);
                const std::size_t stop = after == std::string_view::npos ? end : after;
                emit(i, stop - i, after == std::string_view::npos ? Token::Error : Token::JsString);
                i = stop;
            } else if (ascii::is_digit(c) || (c == '.' && i + 1 < end && ascii::is_digit(text_[i + 1]))) {
                std::size_t j = i;
                while (j < end && (ascii::is_alnum(text_[j]) || text_[j] == '.' || text_[j] == '_' ||
                                   ((text_[j] == '+' || text_[j] == '-') && (text_[j - 1] == 'e' || text_[j - 1] == 'E'))))
                    ++j;
                emit(i, j - i, Token::JsNumber);
                i = j;
            } else if (is_identifier_start(c)) {
                const std::size_t n = identifier_length(text_.substr(i, end - i));
                const std::string_view name = text_.substr(i, n);
                const bool previous_dot = i > begin && text_[i - 1] == '.';
                const bool raw = name.starts_with('$') && (name.size() == 1 || ascii::is_digit(name[1]) || contains(group_names_, name.substr(1)) || name.substr(1) == variable ||
                                                           name == "$val");
                if (!previous_dot && (contains(kVariables, name) || name == variable || contains(group_names_, name) || raw)) emit(i, n, Token::JsVariable);
                else if (contains(kMath, name)) emit(i, n, Token::JsFunction);
                i += n;
            } else if (std::string_view("+-*/%<>=!&|^~?:").contains(c)) {
                emit(i++, 1, Token::JsOperator);
            } else {
                ++i;
            }
        }
    }

    void replacement(std::size_t begin, std::size_t end) {
        const auto known = [&](std::string_view name) { return contains(group_names_, name); };
        for (std::size_t i = begin; i < end;) {
            const char c = text_[i];
            const char next = i + 1 < end ? text_[i + 1] : '\0';
            if (c == '\\' && next != '\0') {
                if (std::string_view("ULEC").contains(next)) {
                    emit(i, 2, Token::CaseOp);
                    i += 2;
                } else if (ascii::is_digit(next)) {
                    emit(i, 2, Token::GroupRef);
                    i += 2;
                } else if (const auto head = next == '<' ? parse_template_head(text_.substr(0, end), i + 2) : std::nullopt) {
                    const Token kind = known(head->name) ? (head->arrow ? Token::ExprHead : Token::GroupRef) : Token::Error;
                    emit(i, head->end - i, kind);
                    i = head->end;
                    if (!head->arrow) continue;
                    if (i >= end || text_[i] != '{') {
                        emit(i, 0, Token::Error);
                        continue;
                    }
                    const std::size_t close = find_expression_end(text_.substr(0, end), i);
                    if (close == std::string_view::npos) {
                        emit(i, 1, Token::Error);
                        javascript(i + 1, end, head->variable.empty() ? "val" : head->variable);
                        return;
                    }
                    emit(i, 1, Token::ExprBrace);
                    emit(i + 1, close - i - 1, Token::Expression);
                    javascript(i + 1, close, head->variable.empty() ? "val" : head->variable);
                    emit(close, 1, Token::ExprBrace);
                    i = close + 1;
                } else {
                    emit(i, 2, Token::Escape);
                    i += 2;
                }
            } else if (c == '$' && ascii::is_digit(next)) {
                emit(i, 2, Token::GroupRef);
                i += 2;
            } else if (const auto head = c == '$' && next == '<' ? parse_template_head(text_.substr(0, end), i + 2) : std::nullopt; head && !head->arrow) {
                emit(i, head->end - i, known(head->name) ? Token::GroupRef : Token::Error);
                i = head->end;
            } else {
                ++i;
            }
        }
    }

    void flags(std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const char f = text_[i];
            emit(i, 1, std::string_view("imsx").contains(f) ? Token::Flag : std::string_view("gu").contains(f) ? Token::IgnoredFlag : Token::Error);
        }
    }

    // A glob: * ** ? are wildcards, everything else is literal.
    void glob(std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end;) {
            const std::size_t n = text_.substr(i).starts_with("**") ? 2 : 1;
            if (text_[i] == '*' || text_[i] == '?') emit(i, n, Token::Glob);
            i += text_[i] == '*' ? n : 1;
        }
    }
};

// The [start, end) of each '/'-separated field, split as RegexSpec::parse splits a spec.
inline std::vector<std::pair<std::size_t, std::size_t>> split_fields(std::string_view spec, bool with_replacement) {
    std::vector<std::pair<std::size_t, std::size_t>> fields;
    std::size_t start = 0;
    std::string current; // the field so far, to recognise "\<name=>" before a '{'
    bool escaped = false;
    for (std::size_t i = 0; i < spec.size(); ++i) {
        const char c = spec[i];
        if (escaped) {
            escaped = false;
            current += c;
        } else if (c == '\\') {
            escaped = true;
            current += c;
        } else if (c == '/') {
            fields.emplace_back(start, i);
            start = i + 1;
            current.clear();
        } else if (c == '{' && with_replacement && fields.size() == 2 && ends_with_expression_head(current)) {
            const std::size_t close = find_expression_end(spec, i);
            if (close == std::string_view::npos) {
                current += c;
            } else {
                current.append(spec.substr(i, close - i + 1));
                i = close;
            }
        } else {
            current += c;
        }
    }
    fields.emplace_back(start, spec.size());
    return fields;
}

} // namespace detail

// The highlighted spans of `text`, in order of appearance (plain characters get no span).
inline std::vector<Span> tokenize(std::string_view text, Mode mode) {
    std::vector<Span> out;
    detail::Tokenizer t(text, out);
    if (mode == Mode::Filter && !is_regex_spec(text)) {
        t.glob(0, text.size());
        return out;
    }
    if (text.empty()) return out;
    if (mode == Mode::Filter) { // as PatternRule::parse: the pattern runs to the last '/', so it may hold path slashes
        const std::size_t last = text.rfind('/');
        out.push_back({0, 1, Token::Delimiter, 0});
        t.pattern(1, last, text.substr(last + 1).contains('x'));
        out.push_back({last, 1, Token::Delimiter, 0});
        t.flags(last + 1, text.size());
        std::ranges::stable_sort(out, {}, &Span::start);
        return out;
    }
    if (text.front() != '/') { // not a spec at all: the engine will refuse it
        out.push_back({0, text.size(), Token::Error, 0});
        return out;
    }
    const bool rename = mode == Mode::Rename;
    const auto fields = detail::split_fields(text, rename);
    for (std::size_t f = 1; f < fields.size(); ++f) out.push_back({fields[f].first - 1, 1, Token::Delimiter, 0});
    // As in RegexSpec::parse: 3 fields are /pattern/flags, 4 or more /pattern/replacement/flags.
    const std::size_t flags_field = fields.size() >= 4 ? 3 : 2;
    std::string_view flag_text;
    if (fields.size() > flags_field) flag_text = text.substr(fields[flags_field].first, fields[flags_field].second - fields[flags_field].first);
    if (fields.size() >= 2) t.pattern(fields[1].first, fields[1].second, flag_text.contains('x'));
    if (fields.size() >= 4) {
        if (rename) t.replacement(fields[2].first, fields[2].second);
        else out.push_back({fields[2].first, fields[2].second - fields[2].first, Token::Comment, 0});
    }
    if (fields.size() > flags_field) t.flags(fields[flags_field].first, fields[flags_field].second);
    if (fields.size() > flags_field + 1) // everything after the flags is ignored by the engine
        out.push_back({fields[flags_field + 1].first - 1, text.size() - fields[flags_field + 1].first + 1, Token::Comment, 0});
    std::ranges::stable_sort(out, {}, &Span::start);
    return out;
}

// Where in `spec` (a /pattern/... spec) the PCRE2 error offset `offset` falls: PCRE2 compiles
// the pattern without the "=>" arrows (strip_numeric_arrows), so its offsets drift after one.
inline std::size_t spec_offset_of_pattern_error(std::string_view spec, std::size_t offset) {
    const auto fields = detail::split_fields(spec, true);
    if (fields.size() < 2) return 0;
    const std::string_view pattern = spec.substr(fields[1].first, fields[1].second - fields[1].first);
    for (std::size_t k = 0; k <= pattern.size(); ++k)
        if (strip_numeric_arrows(pattern.substr(0, k)).pattern.size() >= offset) return fields[1].first + k;
    return fields[1].second;
}

} // namespace fsturbo::syntax

#endif // REGEX_TOKENS_HPP
