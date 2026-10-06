#ifndef REPLACE_TEMPLATE_HPP
#define REPLACE_TEMPLATE_HPP

// The replacement half of  -r /pattern/replacement/flags  (applied to every match):
//
//   \N  $N              capture group N (0-9)
//   \<name>  $<name>    named capture group (see named_groups.hpp)
//   \U \L \E \C         upper-case / lower-case / end of case change / capitalise next character
//   \<name=>{expr}      expression mode: replaced by the value of the JavaScript expression `expr`
//   \<name=v>{expr}     the same, with the group's value named `v` (default: val)
//
// An expression sees, as JavaScript variables:
//   everything in Math    round, floor, max, PI, E, ...  (as plain names; Math.round works too)
//   val (or v)            the referenced group's value: a number if its group was declared
//                         (?<name=>...) and the text reads as a number, otherwise a string
//   <group>               every named group's value, the same way (undefined if it did not take part)
//   $<group>  $val        the raw text of that group, always a string
//   $0 $1 $2 ...          the raw text of the whole match / of numbered groups
//   index                 1-based position among the processed files (directories count separately)
//   nameIndex             1-based position among the processed entries with the same name
//   nameLength            characters in the entry's name
//   depth                 directories between the working directory and the entry's directory
//
// The template is parsed once and every expression is compiled once; each match only calls the
// compiled functions. The result replaces the span the way a plain group would, or, when it
// sits among other text, is inserted there (and a zero-width group makes a pure insertion).

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include "case_converter.hpp"
#include "js_engine.hpp"
#include "named_groups.hpp"
#include "pcre2_regex.hpp"

namespace fsturbo {

// Per-entry dynamic variables.
struct EntryContext {
    std::size_t index = 0;
    std::size_t name_index = 0;
    std::size_t name_length = 0;
    long depth = 0;
};

// "\<name>", "\<name=>", "\<name=var>" (and the "$<name>" form) starting right after the '<'.
struct TemplateHead {
    std::string_view name;
    std::string_view variable; // empty unless "=var"
    bool arrow = false;        // has '=': expression mode
    std::size_t end = 0;       // index just past the '>'
};

constexpr std::optional<TemplateHead> parse_template_head(std::string_view s, std::size_t at) {
    const std::size_t name_len = identifier_length(s.substr(at));
    if (name_len == 0) return std::nullopt;
    TemplateHead head{s.substr(at, name_len), {}, false, 0};
    std::size_t pos = at + name_len;
    if (pos < s.size() && s[pos] == '=') {
        const std::size_t var_len = identifier_length(s.substr(pos + 1));
        head.variable = s.substr(pos + 1, var_len);
        head.arrow = true;
        pos += 1 + var_len;
    }
    if (pos >= s.size() || s[pos] != '>') return std::nullopt;
    head.end = pos + 1;
    return head;
}

// True when `s` ends with an unescaped "\<name=>" or "\<name=var>": the '{' that follows
// opens an expression block, so a '/' inside it is JavaScript, not a field separator.
constexpr bool ends_with_expression_head(std::string_view s) {
    if (!s.ends_with('>')) return false;
    const std::size_t lt = s.rfind("\\<");
    if (lt == std::string_view::npos) return false;
    std::size_t slashes = 0;
    for (std::size_t k = lt; k > 0 && s[k - 1] == '\\'; --k) ++slashes;
    if (slashes % 2 != 0) return false;
    const auto head = parse_template_head(s, lt + 2);
    return head && head->arrow && head->end == s.size();
}

// A whole-text number in JavaScript's decimal syntax: [+-]digits[.digits][e[+-]digits].
constexpr bool is_number_literal(std::string_view s) {
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    std::size_t digits = 0;
    while (i < s.size() && ascii::is_digit(s[i])) ++i, ++digits;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && ascii::is_digit(s[i])) ++i, ++digits;
    }
    if (digits == 0) return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        const std::size_t exponent = i;
        while (i < s.size() && ascii::is_digit(s[i])) ++i;
        if (i == exponent) return false;
    }
    return i == s.size();
}

inline std::optional<double> parse_number(std::string_view s) {
    if (!is_number_literal(s)) return std::nullopt;
    if (s.front() == '+') s.remove_prefix(1);
    double value = 0.0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || end != s.data() + s.size()) return std::nullopt; // out of range stays text
    return value;
}

static_assert(is_number_literal("12") && is_number_literal("-3.5") && is_number_literal(".5") && is_number_literal("1e3") && is_number_literal("+2E-2"));
static_assert(!is_number_literal("") && !is_number_literal("-") && !is_number_literal("1.2.3") && !is_number_literal("0x1f") && !is_number_literal("1e") &&
              !is_number_literal(" 1") && !is_number_literal("12/05"));
static_assert(ends_with_expression_head(R"(x\<fecha=>)") && ends_with_expression_head(R"(\<n=v>)") && !ends_with_expression_head(R"(\<fecha>)") &&
              !ends_with_expression_head(R"(\\<fecha=>)") && !ends_with_expression_head("<fecha=>"));

// Where an expression argument comes from.
struct ArgSource {
    enum class Kind : std::uint8_t { Index, NameIndex, NameLength, Depth, Typed, Raw };
    Kind kind = Kind::Raw;
    std::size_t group = 0; // Typed / Raw: capture number
    bool numeric = false;  // Typed: convert number-looking text
};

struct Expression {
    std::size_t group = 0;
    std::string variable; // name of the referenced group's value
    std::string body;
    int function = -1;
    std::vector<ArgSource> args; // in the order of the compiled function's parameters
};

class ReplaceTemplate {
    struct Token {
        enum class Kind : std::uint8_t { Char, Group, Upper, Lower, EndCase, CapNext, Expr };
        Kind kind = Kind::Char;
        char ch = 0;
        std::size_t index = 0; // Group: capture number; Expr: index into expressions_
    };
    std::vector<Token> tokens_;
    std::vector<Expression> expressions_;

    static fsjs_value argument(const ArgSource& a, const MatchView& m, const EntryContext& ctx) {
        using Kind = ArgSource::Kind;
        switch (a.kind) {
            case Kind::Index: return js::number(static_cast<double>(ctx.index));
            case Kind::NameIndex: return js::number(static_cast<double>(ctx.name_index));
            case Kind::NameLength: return js::number(static_cast<double>(ctx.name_length));
            case Kind::Depth: return js::number(static_cast<double>(ctx.depth));
            case Kind::Typed:
            case Kind::Raw: {
                if (!m.matched(a.group)) return a.kind == Kind::Typed ? js::undefined() : js::text("");
                const std::string_view text = m[a.group];
                if (a.kind == Kind::Typed && a.numeric)
                    if (const auto n = parse_number(text)) return js::number(*n);
                return js::text(text);
            }
        }
        return js::undefined();
    }

public:
    static constexpr std::expected<ReplaceTemplate, std::string> parse(std::string_view fmt, const ParsedPattern& pattern) {
        ReplaceTemplate out;
        using Kind = Token::Kind;
        auto push = [&](Kind kind, char ch = 0, std::size_t index = 0) { out.tokens_.push_back({kind, ch, index}); };

        for (std::size_t i = 0; i < fmt.size(); ++i) {
            const char c = fmt[i];
            const bool has_next = i + 1 < fmt.size();
            if (c == '\\' && has_next) {
                const char next = fmt[i + 1];
                if (next == 'U' || next == 'L' || next == 'E' || next == 'C') {
                    push(next == 'U' ? Kind::Upper : next == 'L' ? Kind::Lower : next == 'E' ? Kind::EndCase : Kind::CapNext);
                    ++i;
                    continue;
                }
                if (ascii::is_digit(next)) {
                    push(Kind::Group, 0, static_cast<std::size_t>(next - '0'));
                    ++i;
                    continue;
                }
                if (next == '<') {
                    if (const auto head = parse_template_head(fmt, i + 2)) {
                        const NamedGroup* group = pattern.find(head->name);
                        if (group == nullptr) return std::unexpected("the replacement refers to the capture group '" + std::string(head->name) + "', which the pattern does not define");
                        if (!head->arrow) {
                            push(Kind::Group, 0, group->index);
                            i = head->end - 1;
                            continue;
                        }
                        const std::string label = "\\<" + std::string(head->name) + "=" + std::string(head->variable) + ">";
                        if (head->end >= fmt.size() || fmt[head->end] != '{') return std::unexpected("expected '{' after " + label);
                        const std::size_t close = find_expression_end(fmt, head->end);
                        if (close == std::string_view::npos) return std::unexpected("the '{' after " + label + " is never closed");
                        const std::string_view body = fmt.substr(head->end + 1, close - head->end - 1);
                        if (body.find_first_not_of(" \t\r\n") == std::string_view::npos) return std::unexpected("empty expression after " + label);
                        out.expressions_.push_back({group->index, std::string(head->variable.empty() ? "val" : head->variable), std::string(body), -1, {}});
                        push(Kind::Expr, 0, out.expressions_.size() - 1);
                        i = close;
                        continue;
                    }
                }
            } else if (c == '$' && has_next) {
                if (ascii::is_digit(fmt[i + 1])) {
                    push(Kind::Group, 0, static_cast<std::size_t>(fmt[i + 1] - '0'));
                    ++i;
                    continue;
                }
                if (fmt[i + 1] == '<') {
                    if (const auto head = parse_template_head(fmt, i + 2); head && !head->arrow) {
                        const NamedGroup* group = pattern.find(head->name);
                        if (group == nullptr) return std::unexpected("the replacement refers to the capture group '" + std::string(head->name) + "', which the pattern does not define");
                        push(Kind::Group, 0, group->index);
                        i = head->end - 1;
                        continue;
                    }
                }
            }
            push(Kind::Char, c);
        }
        return out;
    }

    constexpr bool has_expressions() const { return !expressions_.empty(); }

    // Compiles every expression as a function of all the variables listed above.
    std::expected<void, std::string> compile(js::Engine& engine, const ParsedPattern& pattern) {
        using Kind = ArgSource::Kind;
        for (Expression& e : expressions_) {
            std::vector<std::string> names;
            std::unordered_set<std::string> seen;
            e.args.clear();
            std::string clash;
            auto add = [&](std::string name, ArgSource source) {
                if (!seen.insert(name).second) {
                    if (clash.empty()) clash = name;
                    return;
                }
                names.push_back(std::move(name));
                e.args.push_back(source);
            };
            add("index", {Kind::Index});
            add("nameIndex", {Kind::NameIndex});
            add("nameLength", {Kind::NameLength});
            add("depth", {Kind::Depth});
            const NamedGroup* self = nullptr;
            for (const NamedGroup& g : pattern.groups)
                if (g.index == e.group) self = &g;
            add(e.variable, {Kind::Typed, e.group, self != nullptr && self->numeric});
            add("$" + e.variable, {Kind::Raw, e.group});
            for (const NamedGroup& g : pattern.groups) {
                add(g.name, {Kind::Typed, g.index, g.numeric});
                add("$" + g.name, {Kind::Raw, g.index});
            }
            for (std::size_t n = 0; n <= pattern.group_count; ++n) {
                std::string name = "$";
                append_number(name, n);
                add(std::move(name), {Kind::Raw, n});
            }
            if (!clash.empty())
                return std::unexpected("the capture group name '" + clash + "' clashes with another variable available to expressions (rename the group, or name the referenced value with \\<group=other>{...})");

            std::string error;
            const auto id = engine.compile(names, e.body, error);
            if (!id) return std::unexpected("in {" + e.body + "}: " + error);
            e.function = *id;
        }
        return {};
    }

    // Appends the replacement for one match to `out`.
    std::expected<void, std::string> expand(std::string& out, const MatchView& match, const EntryContext& ctx, js::Engine* engine) const {
        using Kind = Token::Kind;
        bool in_upper = false, in_lower = false, cap_next = false;
        auto emit = [&](char c) {
            if (cap_next) {
                out += ascii::to_upper(c);
                cap_next = false;
            } else if (in_upper) {
                out += ascii::to_upper(c);
            } else if (in_lower) {
                out += ascii::to_lower(c);
            } else {
                out += c;
            }
        };
        std::vector<fsjs_value> args;
        for (const Token& t : tokens_) {
            switch (t.kind) {
                case Kind::Char: emit(t.ch); break;
                case Kind::Group:
                    for (const char gc : match[t.index]) emit(gc);
                    break;
                case Kind::Upper: in_upper = true, in_lower = false; break;
                case Kind::Lower: in_lower = true, in_upper = false; break;
                case Kind::EndCase: in_upper = in_lower = false; break;
                case Kind::CapNext: cap_next = true; break;
                case Kind::Expr: {
                    const Expression& e = expressions_[t.index];
                    args.clear();
                    for (const ArgSource& a : e.args) args.push_back(argument(a, match, ctx));
                    std::string error;
                    const auto result = engine->call(e.function, args, error);
                    if (!result) return std::unexpected("{" + e.body + "}: " + error);
                    for (const char rc : *result) emit(rc);
                    break;
                }
            }
        }
        return {};
    }
};

static_assert([] {
    ParsedPattern p{{{"fecha", 1, true}, {"dia", 2, true}, {"mes", 3, true}}, 3};
    const auto t = ReplaceTemplate::parse(R"(\U\<fecha=>{round(dia+mes*1.5)} \<dia> $<mes> \1)", p);
    return t && t->has_expressions();
}());
static_assert([] {
    ParsedPattern p{{{"a", 1, false}}, 1};
    return !ReplaceTemplate::parse(R"(\<nope>)", p) && !ReplaceTemplate::parse(R"(\<a=>)", p) && !ReplaceTemplate::parse(R"(\<a=>{1+)", p) &&
           !ReplaceTemplate::parse(R"(\<a=>{ })", p);
}());

} // namespace fsturbo

#endif // REPLACE_TEMPLATE_HPP
