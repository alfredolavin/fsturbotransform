// Unit tests for the GUI's syntax-highlighting tokenizer (src/regex_tokens.hpp).
// Build and run: cmake --build build --target regex_tokens_test && ctest --test-dir build
#include <print>
#include <string>
#include <string_view>
#include <vector>
#include "regex_tokens.hpp"

using namespace fsturbo::syntax;

namespace {

int failures = 0;

constexpr std::string_view name(Token t) {
    constexpr std::string_view names[] = {"Delimiter", "Flag", "IgnoredFlag", "Escape", "Class", "Group", "GroupName", "Arrow",
                                          "Quantifier", "Anchor", "Alternation", "Dot", "Comment", "GroupRef", "CaseOp", "ExprHead",
                                          "ExprBrace", "JsNumber", "JsString", "JsVariable", "JsFunction", "JsOperator", "Glob", "Error", "Expression"};
    return names[static_cast<int>(t)];
}

// "text:Token" for every span, space separated.
std::string show(std::string_view text, Mode mode) {
    std::string out;
    for (const Span& s : tokenize(text, mode)) {
        if (!out.empty()) out += ' ';
        out += std::format("{}:{}", text.substr(s.start, s.length), name(s.token));
    }
    return out;
}

void expect(std::string_view text, Mode mode, std::string_view want) {
    const std::string got = show(text, mode);
    if (got == want) return;
    ++failures;
    std::println("FAIL {}\n  want: {}\n  got:  {}", text, want, got);
}

} // namespace

int main() {
    expect("/([a-z]+)_(\\d+)/$1-$2/i", Mode::Rename,
           "/:Delimiter (:Group [a-z]:Class +:Quantifier ):Group (:Group \\d:Escape +:Quantifier ):Group /:Delimiter $1:GroupRef "
           "$2:GroupRef /:Delimiter i:Flag");
    expect("/(?<d=>\\d+)/\\<d=>{round(d/2)}/", Mode::Rename,
           "/:Delimiter (?<:Group d:GroupName =>:Arrow \\d:Escape +:Quantifier ):Group /:Delimiter \\<d=>:ExprHead {:ExprBrace "
           "round(d/2):Expression round:JsFunction d:JsVariable /:JsOperator 2:JsNumber }:ExprBrace /:Delimiter");
    expect("/(?<=x)a{2,3}?$/\\U$0\\E/gq", Mode::Rename,
           "/:Delimiter (?<=:Group ):Group {2,3}?:Quantifier $:Anchor /:Delimiter \\U:CaseOp $0:GroupRef \\E:CaseOp /:Delimiter "
           "g:IgnoredFlag q:Error");
    expect("/(a/\\<b>/", Mode::Rename, "/:Delimiter (:Error /:Delimiter \\<b>:Error /:Delimiter");
    expect("/^tmp_/i", Mode::Select, "/:Delimiter ^:Anchor /:Delimiter i:Flag");
    expect("**/*.log", Mode::Filter, "**:Glob *:Glob");
    expect("/\\.bak$/", Mode::Filter, "/:Delimiter \\.:Escape $:Anchor /:Delimiter");
    expect("/^build/.*\\.o$/i", Mode::Filter, "/:Delimiter ^:Anchor .:Dot *:Quantifier \\.:Escape $:Anchor /:Delimiter i:Flag");
    expect("abc", Mode::Rename, "abc:Error");
    expect("/(?<n>x)/\\<n=v>{v + nameIndex + 'a/b'}/", Mode::Rename,
           "/:Delimiter (?<:Group n:GroupName >:Group ):Group /:Delimiter \\<n=v>:ExprHead {:ExprBrace v + nameIndex + 'a/b':Expression v:JsVariable +:JsOperator "
           "nameIndex:JsVariable +:JsOperator 'a/b':JsString }:ExprBrace /:Delimiter");
    expect("/a # note/x", Mode::Select, "/:Delimiter # note:Comment /:Delimiter x:Flag");

    // PCRE2 offsets are in the pattern without "=>": map them back into the spec.
    const std::string_view spec = "/(?<a=>x)(/$1/";
    if (const auto at = spec_offset_of_pattern_error(spec, 7); at != 9) {
        ++failures;
        std::println("FAIL spec_offset_of_pattern_error: want 9, got {}", at);
    }

    std::println("{}", failures == 0 ? "regex_tokens: all passed" : "regex_tokens: FAILED");
    return failures == 0 ? 0 : 1;
}
