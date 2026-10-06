#ifndef CLI_PARSER_HPP
#define CLI_PARSER_HPP

#include <string>
#include <vector>
#include <iostream>
#include <print>
#include "renamer.hpp"
#include "terminal_style.hpp"
#include "fira_code_font.hpp"

namespace fsturbo {

inline void print_help(const char* prog_name) {
    using namespace term;

    RGB color_neon = {0, 255, 200};
    RGB color_pink = {255, 0, 128};
    RGB color_gold = {255, 215, 0};

    std::println();
    std::println("{}", gradient_text("⚡ FS-TURBO-TRANSFORM v2.0 - High Performance FS Utility ⚡", color_neon, color_pink));
    std::println();
    std::println("{}USAGE:{}", bold(), reset());
    std::println("  {} [OPTIONS] [DIRECTORY_PATH]", prog_name);
    std::println();
    std::println("{}COLOR & TERMINAL MODES:{}", bold(), reset());
    std::println("  -c, --color=<mode>       Set terminal color output mode [full, terminal, simple, none]");
    std::println("                           • full: Sixel full graphics/icons, gradient progress, in-place mini-terminal");
    std::println("                           • terminal: TrueColor 24-bit RGB ANSI + in-place mini-terminal");
    std::println("                           • simple: 16-color basic ANSI");
    std::println("                           • none: Plain non-interactive uncolored text");
    std::println();
    std::println("{}OUTPUT & VERBOSITY:{}", bold(), reset());
    std::println("  -v, --verbose, --log     Output scrolling text log of all file modifications");
    std::println("                           (By default, all updates occur in-place inside the mini-terminal)");
    std::println();
    std::println("{}DEFAULT BEHAVIOR:{}", bold(), reset());
    std::println("  • Recursively traverses directory tree.");
    std::println("  • Case conversion defaults to {}lowercase{} if case transform is active.", rgb_fg(color_gold), reset());
    std::println("  • Directory flattening is {}DISABLED{} by default.", rgb_fg({255, 100, 100}), reset());
    std::println();
    std::println("{}TRANSFORMATION OPTIONS:{}", bold(), reset());
    std::println("  -l, --lower              Convert names to lowercase (default case option)");
    std::println("  -u, --upper              Convert names to UPPERCASE");
    std::println("  -s, --snake              Convert names to snake_case");
    std::println("  -c, --camel              Convert names to camelCase");
    std::println("  -p, --pascal             Convert names to PascalCase");
    std::println("  -k, --kebab              Convert names to kebab-case");
    std::println("  -t, --title              Convert names to Title Case");
    std::println("  -r, --regex /pat/rep/flg Extended Regex rename (e.g. /([a-z]+)_(\\d+)/$1-$2/i)");
    std::println();
    std::println("{}FLATTENING OPTIONS:{}", bold(), reset());
    std::println("  -f, --flatten            Flatten directory hierarchy (exposes all files in root)");
    std::println("      --flatten-regex /rx/ Selective flattening for paths matching regex /rx/");
    std::println();
    std::println("{}FILTERING & EXCLUSIONS (.gitignore & globs vs regex):{}", bold(), reset());
    std::println("  -e, --exclude <pattern>  Exclude pattern (Glob: *.log, **/*.o or Regex: /^test_.*/i)");
    std::println("  -i, --include <pattern>  Include pattern (Glob or Regex)");
    std::println("      --gitignore [file]   Load exclude rules from .gitignore file");
    std::println();
    std::println("{}EXECUTION CONTROLS:{}", bold(), reset());
    std::println("  -d, --dry-run            Simulate operations without modifying filesystem");
    std::println("      --no-recursive       Disable recursive scanning");
    std::println("      --overwrite          Overwrite target files on name collision during flattening");
    std::println("      --extract-font       Save embedded Fira Code TTF font to disk (FiraCode-Regular.ttf)");
    std::println("  -h, --help               Display this styled help menu");
    std::println();
}

inline RenameOptions parse_args(int argc, char* argv[], bool& should_exit) {
    RenameOptions opts;
    should_exit = false;

    term::g_color_mode = term::is_tty() ? term::ColorMode::Full : term::ColorMode::None;

    if (fs::exists(".gitignore")) {
        opts.filter.load_gitignore(".gitignore");
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.rfind("--color=", 0) == 0) {
            std::string mode = arg.substr(8);
            term::set_color_mode_from_string(mode);
        } else if ((arg == "-c" || arg == "--color") && i + 1 < argc && argv[i+1][0] != '-') {
            std::string mode = argv[++i];
            term::set_color_mode_from_string(mode);
        } else if (arg == "-v" || arg == "--verbose" || arg == "--log") {
            opts.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            should_exit = true;
            return opts;
        } else if (arg == "--extract-font") {
            std::ofstream out("FiraCode-Regular.ttf", std::ios::binary);
            out.write(reinterpret_cast<const char*>(fira_code_ttf), fira_code_ttf_len);
            out.close();
            std::println("{}{}Extracted embedded Fira Code font to FiraCode-Regular.ttf ({} bytes){}",
                         term::icon_font(), term::rgb_fg({0, 255, 150}), fira_code_ttf_len, term::reset());
            should_exit = true;
            return opts;
        } else if (arg == "-l" || arg == "--lower") {
            opts.case_style = CaseStyle::Lower;
            opts.case_style_set = true;
        } else if (arg == "-u" || arg == "--upper") {
            opts.case_style = CaseStyle::Upper;
            opts.case_style_set = true;
        } else if (arg == "-s" || arg == "--snake") {
            opts.case_style = CaseStyle::Snake;
            opts.case_style_set = true;
        } else if (arg == "-c" || arg == "--camel") {
            opts.case_style = CaseStyle::Camel;
            opts.case_style_set = true;
        } else if (arg == "-p" || arg == "--pascal") {
            opts.case_style = CaseStyle::Pascal;
            opts.case_style_set = true;
        } else if (arg == "-k" || arg == "--kebab") {
            opts.case_style = CaseStyle::Kebab;
            opts.case_style_set = true;
        } else if (arg == "-t" || arg == "--title") {
            opts.case_style = CaseStyle::Title;
            opts.case_style_set = true;
        } else if (arg == "-f" || arg == "--flatten") {
            opts.flatten = true;
        } else if (arg == "--flatten-regex" && i + 1 < argc) {
            opts.flatten_regex = argv[++i];
        } else if ((arg == "-r" || arg == "--regex" || arg == "--regex-rename") && i + 1 < argc) {
            opts.regex_rename = argv[++i];
        } else if ((arg == "-e" || arg == "--exclude") && i + 1 < argc) {
            opts.filter.add_exclude(argv[++i]);
        } else if ((arg == "-i" || arg == "--include") && i + 1 < argc) {
            opts.filter.add_include(argv[++i]);
        } else if (arg == "--gitignore") {
            std::string gi_path = ".gitignore";
            if (i + 1 < argc && argv[i+1][0] != '-') {
                gi_path = argv[++i];
            }
            opts.filter.load_gitignore(gi_path);
        } else if (arg == "-d" || arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "--no-recursive") {
            opts.recursive = false;
        } else if (arg == "--overwrite") {
            opts.overwrite = true;
        } else if (arg[0] != '-') {
            opts.target_dir = arg;
        }
    }

    if (!opts.case_style_set && opts.regex_rename.empty() && !opts.flatten && opts.flatten_regex.empty()) {
        opts.case_style = CaseStyle::Lower;
        opts.case_style_set = true;
    }

    return opts;
}

} // namespace fsturbo

#endif
