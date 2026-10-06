#include <algorithm>
#include <format>
#include <print>
#include "cli_parser.hpp"
#include "fira_code_font.hpp"
#include "renamer.hpp"
#include "sixel_renderer.hpp"
#include "terminal_style.hpp"

using namespace fsturbo;

inline void render_header() {
    using namespace term;

    std::println();

    // Full mode: a true-color Sixel banner with the title set in the embedded Fira Code font.
    if (g_color_mode == ColorMode::Full) {
        const int cell_w = g_term.cell_width(), cell_h = g_term.cell_height();
        const int width = std::min(g_term.cols - 1, 72) * cell_w;
        const int height = 3 * cell_h;
        const auto banner = sixel::render_banner(width, height, "FS-TURBO-TRANSFORMER v2.0", "C++26 · Sixel Graphics Engine · Fira Code");
        std::print("{}", place_image(sixel::encode(banner), height));
        std::println();
        return;
    }

    std::println("{}", gradient_text(" ╔════════════════════════════════════════════════════════════════════╗", palette::neon_cyan, palette::neon_purple));
    std::println("{}", gradient_text(" ║        FS-TURBO-TRANSFORMER v2.0 (C++26 Sixel Graphics Engine)     ║", palette::neon_purple, palette::neon_pink));
    std::println("{}", gradient_text(" ║        High-Performance Monolithic Filesystem Transformer          ║", palette::neon_pink, palette::neon_cyan));
    std::println("{}", gradient_text(" ╚════════════════════════════════════════════════════════════════════╝", palette::neon_cyan, palette::neon_pink));
    std::println();
}

inline void render_footer(const ExecutionStats& stats, bool dry_run) {
    using namespace term;
    using sixel::Icon;

    const auto row = [](Icon icon, std::string_view label, auto value, Rgb color) {
        std::print("{}", icon_line(icon, std::format("{:<20}: {}{}{}{}", label, bold(), rgb_fg(color), value, reset())));
    };

    std::println();
    std::println("{}", gradient_text(" ══════════════════════════ EXECUTION REPORT ══════════════════════════", Rgb{0, 255, 180}, Rgb{50, 150, 255}));

    if (dry_run) [[unlikely]] {
        std::print("{}", icon_line(Icon::Rocket, std::format("{}{}[DRY RUN MODE - NO FILES MODIFIED]{}", bold(), rgb_fg({255, 200, 0}), reset())));
        std::println();
    }

    row(Icon::Folder, "Scanned Directories", stats.scanned_dirs, {230, 230, 240});
    row(Icon::File, "Scanned Files", stats.scanned_files, {230, 230, 240});
    row(Icon::Rename, "Renamed Files", stats.renamed_files, {100, 255, 100});
    row(Icon::Rename, "Renamed Directories", stats.renamed_dirs, {100, 220, 255});
    row(Icon::Flatten, "Flattened Files", stats.flattened_files, {255, 200, 50});
    row(Icon::Exclude, "Excluded Items", stats.excluded_items, {180, 180, 180});
    if (stats.errors > 0) [[unlikely]] {
        row(Icon::Error, "Errors Encountered", stats.errors, {255, 50, 50});
    }
    row(Icon::Success, "Execution Time", std::format("{:.2f} ms", stats.duration_ms), {0, 255, 200});
    row(Icon::Font, "Embedded Font", std::format("{} v{} ({} bytes)", FIRA_CODE_FONT_NAME, FIRA_CODE_VERSION, fira_code_ttf_len), {255, 150, 255});

    std::println("{}", gradient_text(" ══════════════════════════════════════════════════════════════════════", Rgb{50, 150, 255}, palette::neon_pink));
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

    const bool dry_run = opts.dry_run;
    TransformerEngine engine(std::move(opts));
    const ExecutionStats stats = engine.run();

    if (term::g_color_mode != term::ColorMode::None) {
        render_footer(stats, dry_run);
    }

    return (stats.errors > 0) ? 1 : 0;
}
