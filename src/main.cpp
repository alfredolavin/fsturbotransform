#include <iostream>
#include <iomanip>
#include <print>
#include <format>
#include "fira_code_font.hpp"
#include "sixel_renderer.hpp"
#include "terminal_style.hpp"
#include "cli_parser.hpp"
#include "renamer.hpp"

using namespace fsturbo;

inline void render_header() {
    using namespace term;

    RGB c1 = {0, 255, 240};
    RGB c2 = {128, 0, 255};
    RGB c3 = {255, 0, 128};

    std::println();

    // If Full mode, render true full-color Sixel graphical banner!
    if (g_color_mode == ColorMode::Full) {
        std::print("{}", sixel::generate_sixel_banner(380, 36));
        std::println();
    }

    std::println("{}", gradient_text(" ╔════════════════════════════════════════════════════════════════════╗", c1, c2));
    std::println("{}", gradient_text(" ║        FS-TURBO-TRANSFORMER v2.0 (C++26 Sixel Graphics Engine)     ║", c2, c3));
    std::println("{}", gradient_text(" ║        High-Performance Monolithic Filesystem Transformer         ║", c3, c1));
    std::println("{}", gradient_text(" ╚════════════════════════════════════════════════════════════════════╝", c1, c3));
    std::println();
}

inline void render_footer(const ExecutionStats& stats, bool dry_run) {
    using namespace term;

    RGB c_neon = {0, 255, 180};
    RGB c_blue = {50, 150, 255};
    RGB c_pink = {255, 50, 200};

    std::println();
    std::println("{}", gradient_text(" ══════════════════════════ EXECUTION REPORT ══════════════════════════", c_neon, c_blue));

    if (dry_run) [[unlikely]] {
        std::println(" {}{}[DRY RUN MODE - NO FILES MODIFIED]{}", icon_rocket(), bold() + rgb_fg({255, 200, 0}), reset());
        std::println();
    }

    std::println(" {}Scanned Directories : {}{}{}", icon_dir(), bold(), stats.scanned_dirs, reset());
    std::println(" {}Scanned Files       : {}{}{}", icon_file(), bold(), stats.scanned_files, reset());
    std::println(" {}Renamed Files     : {}{}{}{}", icon_rename(), bold(), rgb_fg({100, 255, 100}), stats.renamed_files, reset());
    std::println(" {}Renamed Directories : {}{}{}{}", icon_rename(), bold(), rgb_fg({100, 220, 255}), stats.renamed_dirs, reset());
    std::println(" {}Flattened Files   : {}{}{}{}", icon_flatten(), bold(), rgb_fg({255, 200, 50}), stats.flattened_files, reset());
    std::println(" {}Excluded Items    : {}{}{}{}", icon_exclude(), bold(), rgb_fg({180, 180, 180}), stats.excluded_items, reset());
    
    if (stats.errors > 0) [[unlikely]] {
        std::println(" {}Errors Encountered: {}{}{}{}", icon_error(), bold(), rgb_fg({255, 50, 50}), stats.errors, reset());
    }

    std::println(" {}Execution Time    : {}{:.2f} ms{}", icon_success(), bold() + rgb_fg({0, 255, 200}), stats.duration_ms, reset());

    std::println(" {}Embedded Font     : {}{} v{} ({} bytes){}", icon_font(), bold() + rgb_fg({255, 150, 255}),
                 FIRA_CODE_FONT_NAME, FIRA_CODE_VERSION, fira_code_ttf_len, reset());

    std::println("{}", gradient_text(" ══════════════════════════════════════════════════════════════════════", c_blue, c_pink));
    std::println();
}

int main(int argc, char* argv[]) {
    bool should_exit = false;
    RenameOptions opts = parse_args(argc, argv, should_exit);

    if (should_exit) [[unlikely]] {
        return 0;
    }

    if (term::g_color_mode != term::ColorMode::None) {
        render_header();
    }

    TransformerEngine engine(opts);
    ExecutionStats stats = engine.run();

    if (term::g_color_mode != term::ColorMode::None) {
        render_footer(stats, opts.dry_run);
    }

    return (stats.errors > 0) ? 1 : 0;
}
