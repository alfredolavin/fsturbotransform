#include <algorithm>
#include <format>
#include <print>
#include <string>
#include <vector>
#include "cli_parser.hpp"
#include "fira_code_font.hpp"
#include "renamer.hpp"
#include "report_view.hpp"
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
    using sixel::Icon;

    std::vector<ui::ReportRow> rows{
        {Icon::Folder, "Scanned Directories", std::format("{}", stats.scanned_dirs), {230, 230, 240}},
        {Icon::File, "Scanned Files", std::format("{}", stats.scanned_files), {230, 230, 240}},
        {Icon::Rename, "Renamed Files", std::format("{}", stats.renamed_files), {100, 255, 100}},
        {Icon::Rename, "Renamed Directories", std::format("{}", stats.renamed_dirs), {100, 220, 255}},
        {Icon::Flatten, "Flattened Files", std::format("{}", stats.flattened_files), {255, 200, 50}},
        {Icon::Exclude, "Excluded Items", std::format("{}", stats.excluded_items), {180, 180, 180}},
    };
    if (stats.errors > 0) [[unlikely]] {
        rows.push_back({Icon::Error, "Errors Encountered", std::format("{}", stats.errors), {255, 50, 50}});
    }
    rows.push_back({Icon::Success, "Execution Time", std::format("{:.2f} ms", stats.duration_ms), {0, 255, 200}});
    rows.push_back({Icon::Font, "Embedded Font", std::format("{} v{} ({} bytes)", FIRA_CODE_FONT_NAME, FIRA_CODE_VERSION, fira_code_ttf_len), {255, 150, 255}});

    std::string status = dry_run ? "DRY RUN · NO FILES MODIFIED" : "COMPLETED";
    if (stats.errors > 0) status = std::format("{}{} ERROR{}", dry_run ? "DRY RUN · " : "", stats.errors, stats.errors == 1 ? "" : "S");
    const Rgb status_color = stats.errors > 0 ? palette::error : dry_run ? palette::action : palette::dest;

    std::println();
    ui::print_report(rows, {std::move(status), status_color});
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
