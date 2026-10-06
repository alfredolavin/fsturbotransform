#include <algorithm>
#include <format>
#include <print>
#include <string>
#include <vector>
#include "banner.hpp"
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

    // Full mode: a true-color Sixel banner with the app icon and the title set in the embedded Fira Code font.
    if (g_color_mode == ColorMode::Full) {
        const int cell_w = g_term.cell_width(), cell_h = g_term.cell_height();
        const int width = std::min(g_term.cols - 1, 72) * cell_w;
        const int height = sixel::kBannerRows * cell_h;
        const auto banner = sixel::render_banner(width, height, "FS-TURBO-TRANSFORMER v2.0", g_term.background);
        std::print("{}", place_image(sixel::encode(banner), height));
        std::println();
        return;
    }

    std::println("{}", gradient_text(" ╔════════════════════════════════════════════════════════════════════╗", palette::neon_cyan, palette::neon_purple));
    std::println("{}", gradient_text(" ║        FS-TURBO-TRANSFORMER v2.0 (C++26 Sixel Graphics Engine)     ║", palette::neon_purple, palette::neon_pink));
    std::println("{}", gradient_text(" ╚════════════════════════════════════════════════════════════════════╝", palette::neon_cyan, palette::neon_pink));
    std::println();
}

// The execution report. In the live layout the console and the report already share the
// terminal's height exactly, so no blank lines are added around it.
inline void render_footer(const ExecutionStats& stats, bool dry_run, const ui::ScreenPlan& plan) {
    if (!plan.show_report) return;
    if (!plan.live) std::println();
    ui::print_report(ui::make_report_rows(stats), ui::make_report_status(stats, dry_run));
    if (!plan.live) std::println();
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
        render_footer(stats, dry_run, engine.screen_plan());
    }

    return (stats.errors > 0) ? 1 : 0;
}
