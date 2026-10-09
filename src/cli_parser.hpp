#ifndef CLI_PARSER_HPP
#define CLI_PARSER_HPP

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "fira_code_font.hpp"
#include "renamer.hpp"
#include "terminal_style.hpp"

namespace fsturbo {

inline void print_help(const char* prog_name) {
    using namespace term;

    const auto heading = [](std::string_view text) { return std::format("{}{}{}", bold(), text, reset()); };

    std::println();
    std::println("{}", gradient_text("⚡ FS-TURBO-TRANSFORM v2.0 - High Performance FS Utility ⚡", palette::neon_cyan, palette::neon_pink));
    std::println();
    std::println("{}", heading("USAGE:"));
    std::println("  {} [OPTIONS] [DIRECTORY_PATH]", prog_name);
    std::println();
    std::println("{}", heading("COLOR & TERMINAL MODES:"));
    std::println("  -c, --color <mode>       Set terminal output mode [auto, full, terminal, simple, none]");
    std::println("      --color=<mode>       • auto (default): full when the terminal reports Sixel support");
    std::println("                           • full: Sixel banner/icons, gradient progress bar and mini-terminal");
    std::println("                             rendered as graphics with the embedded Fira Code font");
    std::println("                           • terminal: TrueColor 24-bit RGB ANSI + in-place mini-terminal");
    std::println("                           • simple: 16-color basic ANSI");
    std::println("                           • none: Plain non-interactive uncolored text");
    std::println("      --graphics=<proto>   Image protocol for full mode [auto, kitty, sixel]; auto asks the");
    std::println("                           terminal (Kitty first: 24-bit RGBA with real transparency). Force one");
    std::println("                           for terminals that answer no queries; also $FSTURBO_GRAPHICS");
    std::println();
    std::println("{}", heading("OUTPUT & VERBOSITY:"));
    std::println("  -v, --verbose, --log     Output scrolling text log of all file modifications");
    std::println("                           (By default, all updates occur in-place inside the mini-terminal)");
    std::println();
    std::println("{}", heading("DEFAULT BEHAVIOR:"));
    std::println("  • Recursively traverses directory tree.");
    std::println("  • Case conversion defaults to {}lowercase{} if case transform is active.", rgb_fg(palette::neon_gold), reset());
    std::println("  • Directory flattening is {}DISABLED{} by default.", rgb_fg({255, 100, 100}), reset());
    std::println();
    std::println("{}", heading("TRANSFORMATION OPTIONS:"));
    std::println("  -l, --lower              Convert names to lowercase (default case option)");
    std::println("  -u, --upper              Convert names to UPPERCASE");
    std::println("  -s, --snake              Convert names to snake_case");
    std::println("  -C, --camel              Convert names to camelCase (-c also works when not followed by a mode)");
    std::println("  -p, --pascal             Convert names to PascalCase");
    std::println("  -k, --kebab              Convert names to kebab-case");
    std::println("  -t, --title              Convert names to Title Case");
    std::println("  -r, --regex /pat/rep/flg PCRE2 regex rename (e.g. /([a-z]+)_(\\d+)/$1-$2/i)");
    std::println("                           Lookahead/lookbehind, named groups, \\K, UTF-8; replacement: $N, \\N,");
    std::println("                           \\U \\L \\E \\C. Flags: i caseless, m multiline, s dotall, x extended");
    std::println();
    std::println("{}", heading("EXPRESSION REPLACEMENT (with -r):"));
    std::println("  (?<name>...)             Named group: \\<name> or $<name> inserts its text");
    std::println("  (?<name=>...)            Named group whose text is a number inside expressions");
    std::println("  \\<name=>{{expr}}           Replaced by the result of the JavaScript expression expr;");
    std::println("  \\<name=v>{{expr}}          the same, with the group's value named v (default: val)");
    std::println("                           Everything in Math is in scope: round, floor, max, PI, E, ...");
    std::println("                           Strings take the usual JS methods: padStart, toUpperCase, ...");
    std::println("                           Variables: val, each named group (a number for \"=>\" groups),");
    std::println("                           $name / $1 / $0 (raw text), index, nameIndex, nameLength, depth");
    std::println("                           index = file number (directories count apart), nameIndex = number");
    std::println("                           among same-named entries, nameLength = characters in the name,");
    std::println("                           depth = directories below where the program was started");
    std::println("                           e.g. -r '/(?<d=>\\d+)-(?<m=>\\d+)/\\<d=>{{round(d*10+m/2)}}_\\<m>/'");
    std::println("                           Needs libfsturbo_js.so (V8 through libnode); FSTURBO_JS_LIB sets its path");
    std::println();
    std::println("{}", heading("FLATTENING OPTIONS:"));
    std::println("  -f, --flatten            Flatten directory hierarchy (exposes all files in root)");
    std::println("      --flatten-regex /rx/ Selective flattening for paths matching regex /rx/");
    std::println();
    std::println("{}", heading("FILTERING & EXCLUSIONS (.gitignore & globs vs regex):"));
    std::println("  -e, --exclude <pattern>  Exclude pattern matched against the whole relative path");
    std::println("                           (Glob: *.log, **/*.o or PCRE2 regex: /^test_.*/i)");
    std::println("  -i, --include <pattern>  Include pattern (Glob or Regex); overrides every exclusion");
    std::println("                           Everything inside an excluded directory is excluded too");
    std::println("      --gitignore [file]   Also apply a .gitignore-style file (default: ./.gitignore)");
    std::println("      --no-gitignore       Don't apply the target directory's own .gitignore");
    std::println("                           .gitignore rules follow git: no-slash patterns match at any");
    std::println("                           depth, '/x' is rooted, 'x/' matches directories, '!' re-includes");
    std::println("                           (last match wins); patterns are relative to the target directory");
    std::println("                           .git directories/files are always skipped (unless matched by -i)");
    std::println();
    std::println("{}", heading("EXECUTION CONTROLS:"));
    std::println("  -d, --dry-run            Simulate operations without modifying filesystem");
    std::println("      --no-recursive       Disable recursive scanning");
    std::println("      --no-report          Skip the final report: the mini-console then takes the whole height.");
    std::println("                           Otherwise the report is laid out in columns and shown only when it");
    std::println("                           is at most 20% of the console's height (about 33 rows at 100 columns)");
    std::println("      --overwrite          Allow replacing existing entries on name collisions");
    std::println("      --extract-font       Save embedded Fira Code TTF font to disk (FiraCode-Regular.ttf)");
    std::println("  -h, --help               Display this styled help menu");
    std::println();
}

inline void extract_font() {
    std::ofstream out("FiraCode-Regular.ttf", std::ios::binary);
    out.write(reinterpret_cast<const char*>(fira_code_ttf), static_cast<std::streamsize>(fira_code_ttf_len));
    out.close();
    std::print("{}", term::icon_line(sixel::Icon::Font, std::format("{}Extracted embedded Fira Code font to FiraCode-Regular.ttf ({} bytes){}",
                                                                    term::rgb_fg({0, 255, 150}), fira_code_ttf_len, term::reset())));
}

inline RenameOptions parse_args(int argc, char* argv[], bool& should_exit) {
    RenameOptions opts;
    should_exit = false;
    std::string_view color_mode = "auto";
    std::string_view graphics = std::getenv("FSTURBO_GRAPHICS") ? std::getenv("FSTURBO_GRAPHICS") : "auto";
    bool want_help = false, want_font = false;
    bool auto_gitignore = true;
    std::vector<fs::path> gitignore_files;

    const auto set_case = [&](CaseStyle style) {
        opts.case_style = style;
        opts.case_style_set = true;
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool has_value = i + 1 < argc;

        if (arg.starts_with("--color=")) {
            color_mode = arg.substr(8);
        } else if ((arg == "-c" || arg == "--color") && has_value && term::is_color_mode_name(argv[i + 1])) {
            color_mode = argv[++i];
        } else if (arg == "--color") {
            color_mode = "auto";
        } else if (arg.starts_with("--graphics=")) {
            graphics = arg.substr(11);
        } else if (arg == "--graphics" && has_value && term::is_graphics_name(argv[i + 1])) {
            graphics = argv[++i];
        } else if (arg == "-v" || arg == "--verbose" || arg == "--log") {
            opts.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            want_help = true;
        } else if (arg == "--extract-font") {
            want_font = true;
        } else if (arg == "-l" || arg == "--lower") {
            set_case(CaseStyle::Lower);
        } else if (arg == "-u" || arg == "--upper") {
            set_case(CaseStyle::Upper);
        } else if (arg == "-s" || arg == "--snake") {
            set_case(CaseStyle::Snake);
        } else if (arg == "-c" || arg == "-C" || arg == "--camel") {
            set_case(CaseStyle::Camel);
        } else if (arg == "-p" || arg == "--pascal") {
            set_case(CaseStyle::Pascal);
        } else if (arg == "-k" || arg == "--kebab") {
            set_case(CaseStyle::Kebab);
        } else if (arg == "-t" || arg == "--title") {
            set_case(CaseStyle::Title);
        } else if (arg == "-f" || arg == "--flatten") {
            opts.flatten = true;
        } else if (arg == "--flatten-regex" && has_value) {
            opts.flatten_regex = argv[++i];
        } else if ((arg == "-r" || arg == "--regex" || arg == "--regex-rename") && has_value) {
            opts.regex_rename = argv[++i];
        } else if ((arg == "-e" || arg == "--exclude") && has_value) {
            opts.filter.add_exclude(argv[++i]);
        } else if ((arg == "-i" || arg == "--include") && has_value) {
            opts.filter.add_include(argv[++i]);
        } else if (arg == "--gitignore") {
            // A directory is never taken as the value, so "--gitignore <dir>" keeps <dir> as the target.
            std::error_code ec;
            if (has_value && argv[i + 1][0] != '-' && !fs::is_directory(argv[i + 1], ec)) gitignore_files.emplace_back(argv[++i]);
            else gitignore_files.emplace_back(".gitignore");
        } else if (arg == "--no-gitignore") {
            auto_gitignore = false;
        } else if (arg == "-d" || arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "--no-report") {
            opts.report = false;
        } else if (arg == "--no-recursive") {
            opts.recursive = false;
        } else if (arg == "--overwrite") {
            opts.overwrite = true;
        } else if (!arg.empty() && arg[0] != '-') {
            opts.target_dir = arg;
        }
    }

    term::init_terminal(color_mode, graphics);

    for (const fs::path& file : load_gitignores(opts, gitignore_files, auto_gitignore))
        std::println(stderr, "warning: cannot read gitignore file {}", file.string());

    if (want_help || want_font) {
        if (want_help) print_help(argv[0]);
        if (want_font) extract_font();
        should_exit = true;
        return opts;
    }

    if (!opts.case_style_set && opts.regex_rename.empty() && !opts.flatten && opts.flatten_regex.empty()) {
        set_case(CaseStyle::Lower);
    }

    return opts;
}

} // namespace fsturbo

#endif
