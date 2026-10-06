#ifndef REPORT_VIEW_HPP
#define REPORT_VIEW_HPP

// Execution report rendered as a mini-terminal window in the same style as the live dashboard:
// a Sixel window set in Fira Code (Full) or the matching ANSI box. The items are laid out in as
// many columns as the window is wide, filled top to bottom, so the report takes as few rows as
// possible.
//
// plan_screen() shares the terminal's height between the live console and the report: the
// console takes every row the report does not need, and the report is shown only when it fits in
// 20% of the console's height (a one-column list never would). --no-report gives the console the
// whole height.

#include <algorithm>
#include <format>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "color.hpp"
#include "console_window.hpp"
#include "execution_stats.hpp"
#include "sixel_renderer.hpp"
#include "terminal_style.hpp"

namespace fsturbo::ui {

struct ReportRow {
    sixel::Icon icon;
    std::string_view label;
    std::string value;
    Rgb color;
};

struct ReportStatus {
    std::string text;
    Rgb color;
};

inline constexpr std::string_view kReportTitle = "EXECUTION REPORT · Fira Code Engine";
inline constexpr std::string_view kShortReportTitle = "EXECUTION REPORT";

// The centered title and the right-aligned status must not meet: title/2 + status + margins <= width/2.
// `cells` is the window width in text cells.
constexpr std::string_view report_title(int cells, std::size_t status_cols) {
    return static_cast<int>(ttf::utf8_length(kReportTitle) + 2 * status_cols) + 4 <= cells ? kReportTitle : kShortReportTitle;
}

inline std::vector<ReportRow> make_report_rows(const ExecutionStats& stats) {
    using sixel::Icon;
    std::vector<ReportRow> rows{
        {Icon::Folder, "Scanned Dirs", std::format("{}", stats.scanned_dirs), {230, 230, 240}},
        {Icon::File, "Scanned Files", std::format("{}", stats.scanned_files), {230, 230, 240}},
        {Icon::Rename, "Renamed Files", std::format("{}", stats.renamed_files), {100, 255, 100}},
        {Icon::Rename, "Renamed Dirs", std::format("{}", stats.renamed_dirs), {100, 220, 255}},
        {Icon::Flatten, "Flattened Files", std::format("{}", stats.flattened_files), {255, 200, 50}},
        {Icon::Exclude, "Excluded Items", std::format("{}", stats.excluded_items), {180, 180, 180}},
    };
    if (stats.errors > 0) [[unlikely]] {
        rows.push_back({Icon::Error, "Errors", std::format("{}", stats.errors), {255, 50, 50}});
    }
    rows.push_back({Icon::Success, "Execution Time", std::format("{:.2f} ms", stats.duration_ms), {0, 255, 200}});
    rows.push_back({Icon::Font, "Embedded Font", std::format("Fira Code {}", FIRA_CODE_VERSION), {255, 150, 255}});
    return rows;
}

inline ReportStatus make_report_status(const ExecutionStats& stats, bool dry_run) {
    std::string text = dry_run ? "DRY RUN · NO FILES MODIFIED" : "COMPLETED";
    if (stats.errors > 0) text = std::format("{}{} ERROR{}", dry_run ? "DRY RUN · " : "", stats.errors, stats.errors == 1 ? "" : "S");
    return {std::move(text), stats.errors > 0 ? palette::error : dry_run ? palette::action : palette::dest};
}

// Single-column symbols available in Fira Code: emoji widths vary between terminals
// and would break the box's right edge.
constexpr std::pair<std::string_view, Rgb> ansi_icon(sixel::Icon icon) {
    using sixel::Icon;
    switch (icon) {
        case Icon::Folder: return {"▣", {255, 180, 0}};
        case Icon::File: return {"▤", {150, 200, 255}};
        case Icon::Rename: return {"►", {0, 229, 255}};
        case Icon::Flatten: return {"≡", {255, 214, 0}};
        case Icon::Exclude: return {"∅", {255, 82, 82}};
        case Icon::Error: return {"×", {255, 23, 68}};
        case Icon::Success: return {"✓", {0, 230, 118}};
        case Icon::Rocket: return {"▲", {255, 0, 128}};
        case Icon::Font: return {"¶", {187, 134, 252}};
        case Icon::Count: break;
    }
    return {"·", palette::note};
}

inline std::string dots(std::size_t n) {
    std::string s;
    for (std::size_t i = 0; i < n; ++i) s += "·";
    return s;
}

// --- Column layout -------------------------------------------------------------------------

// All measures are in text cells (the monospace grid of the Fira Code the Sixel window uses).
struct ReportMetrics {
    int inner;     // cells available inside the window's side margins
    int icon_cols; // cells an item's icon (and the gap after it) takes
};

inline ReportMetrics report_metrics() {
    using namespace term;
    if (g_color_mode == ColorMode::Full) {
        const ConsoleFonts f(g_term.cell_height());
        const int width = window_columns() * g_term.cell_width();
        return {std::max(0, (width - 2 * f.adv) / f.adv), 1 + (sixel::kIconSize + f.adv - 1) / f.adv};
    }
    return {std::max(0, window_columns() - 4), 2};
}

struct ReportColumn {
    int x = 0; // first cell of the column
    int label_w = 0;
    int value_w = 0;
};

struct ReportLayout {
    int rows = 0; // item rows (the window adds a title bar and a bottom edge)
    std::vector<ReportColumn> columns;
};

inline constexpr int kColumnGap = 2;
inline constexpr int kLeaderDots = 2; // at least this many dots between a label and its value

// icon, label, " ", leader dots, " ", value
constexpr int column_width(const ReportColumn& c, int icon_cols) { return icon_cols + c.label_w + 2 + kLeaderDots + c.value_w; }

// The fewest rows whose columns (filled top to bottom) fit in `inner` cells. A single column that
// is still too wide gets its values cut.
inline ReportLayout layout_report(std::span<const ReportRow> items, int inner, int icon_cols) {
    const int n = static_cast<int>(items.size());
    ReportLayout layout;
    for (int rows = 1; rows <= std::max(n, 1); ++rows) {
        layout.rows = rows;
        layout.columns.clear();
        int x = 0;
        for (int first = 0; first < n; first += rows) {
            ReportColumn col{x, 0, 0};
            for (int k = first; k < std::min(n, first + rows); ++k) {
                const ReportRow& item = items[static_cast<std::size_t>(k)];
                col.label_w = std::max(col.label_w, static_cast<int>(ttf::utf8_length(item.label)));
                col.value_w = std::max(col.value_w, static_cast<int>(ttf::utf8_length(item.value)));
            }
            layout.columns.push_back(col);
            x += column_width(col, icon_cols) + kColumnGap;
        }
        if (x - kColumnGap <= inner) break;
        if (rows == n) { // one column and still wider than the window
            ReportColumn& col = layout.columns.front();
            col.value_w = std::max(4, inner - (icon_cols + col.label_w + 2 + kLeaderDots));
        }
    }
    return layout;
}

// --- Rendering -----------------------------------------------------------------------------

inline std::string render_report_ansi(std::span<const ReportRow> rows, const ReportStatus& status, const ReportLayout& layout, const ReportMetrics& m) {
    using namespace term;
    const int width = window_columns();
    std::string out = ansi_window_top(width, kReportTitle, status.text, status.color) + "\n";
    for (int r = 0; r < layout.rows; ++r) {
        std::string content;
        std::size_t cols = 0;
        for (std::size_t c = 0; c < layout.columns.size(); ++c) {
            const ReportColumn& col = layout.columns[c];
            const int col_w = column_width(col, m.icon_cols);
            if (c > 0) {
                content.append(static_cast<std::size_t>(kColumnGap), ' ');
                cols += kColumnGap;
            }
            const std::size_t k = c * static_cast<std::size_t>(layout.rows) + static_cast<std::size_t>(r);
            if (k >= rows.size()) {
                content.append(static_cast<std::size_t>(col_w), ' ');
                cols += static_cast<std::size_t>(col_w);
                continue;
            }
            const ReportRow& row = rows[k];
            const auto [symbol, icon_color] = ansi_icon(row.icon);
            const std::size_t label_len = ttf::utf8_length(row.label);
            const std::string value = fit_right(row.value, static_cast<std::size_t>(col.value_w));
            const std::size_t value_len = ttf::utf8_length(value);
            content += std::format("{}{}{} {}{} {}{}{} {}{}{}{}", rgb_fg(icon_color), symbol, reset(), rgb_fg(palette::path_name), row.label,
                                   rgb_fg(palette::console_border), dots(static_cast<std::size_t>(col.label_w) - label_len + kLeaderDots), reset(), bold(),
                                   rgb_fg(row.color), value, reset());
            content.append(static_cast<std::size_t>(col.value_w) - value_len, ' ');
            cols += static_cast<std::size_t>(col_w);
        }
        out += ansi_window_row(width, content, cols) + "\n";
    }
    out += ansi_window_bottom(width) + "\n";
    return out;
}

inline sixel::Canvas draw_report_window(std::span<const ReportRow> rows, const ReportStatus& status, const ReportLayout& layout, const ReportMetrics& m) {
    ConsoleFonts f(term::g_term.cell_height());
    const int ch = f.cell_h;
    const int width = window_columns() * term::g_term.cell_width();
    const int height = (layout.rows + 2) * ch;
    sixel::Canvas c = window_canvas(width, height);

    const std::string_view title = report_title(width / f.adv, ttf::utf8_length(status.text));
    const int title_baseline = draw_console_window(c, f, 0.0f, static_cast<float>(height - 1), title);
    draw_title_status(c, f, title_baseline, status.text, status.color);

    const float content_top = static_cast<float>(ch) * 1.25f + 2.0f;
    for (std::size_t k = 0; k < rows.size(); ++k) {
        const ReportRow& row = rows[k];
        const ReportColumn& col = layout.columns[k / static_cast<std::size_t>(layout.rows)];
        const float top = content_top + static_cast<float>(k % static_cast<std::size_t>(layout.rows)) * static_cast<float>(ch);
        const int baseline = f.baseline_in(top, static_cast<float>(ch));
        const int x0 = f.adv * (1 + col.x);
        c.draw_image(sixel::draw_icon(row.icon), x0, static_cast<int>(top) + (ch - sixel::kIconSize) / 2);
        int x = c.draw_text(f.regular, x0 + m.icon_cols * f.adv, baseline, row.label, palette::path_name, f.adv);
        const std::size_t label_len = ttf::utf8_length(row.label);
        x = c.draw_text(f.regular, x + f.adv, baseline, dots(static_cast<std::size_t>(col.label_w) - label_len + kLeaderDots), palette::console_border, f.adv);
        c.draw_text(f.bold, x + f.adv, baseline, term::fit_right(row.value, static_cast<std::size_t>(col.value_w)), row.color, f.adv);
    }
    return c;
}

inline std::string render_report_sixel(std::span<const ReportRow> rows, const ReportStatus& status, const ReportLayout& layout, const ReportMetrics& m) {
    const sixel::Canvas c = draw_report_window(rows, status, layout, m);
    return term::place_image(sixel::encode(c), c.height());
}

inline void print_report(std::span<const ReportRow> rows, const ReportStatus& status) {
    using namespace term;
    if (g_term.cols < 40) { // too narrow for a window: one icon line per row
        std::print("{}", icon_line(sixel::Icon::Rocket, std::format("{}{}{}", rgb_fg(status.color), status.text, reset())));
        for (const ReportRow& row : rows)
            std::print("{}", icon_line(row.icon, std::format("{}: {}{}{}{}", row.label, bold(), rgb_fg(row.color), row.value, reset())));
        return;
    }
    const ReportMetrics m = report_metrics();
    const ReportLayout layout = layout_report(rows, m.inner, m.icon_cols);
    std::print("{}", g_color_mode == ColorMode::Full ? render_report_sixel(rows, status, layout, m) : render_report_ansi(rows, status, layout, m));
}

// --- Sharing the screen --------------------------------------------------------------------

inline constexpr int kMinConsoleLines = 3; // text lines of the live console, at least

// Whether the live in-place dashboard can run at all in this terminal.
inline bool dashboard_fits() { return term::interactive() && term::g_term.cols >= 40 && term::g_term.rows >= kMinConsoleLines + 7; }

struct ScreenPlan {
    bool live = false;        // the live dashboard is on screen while the work runs
    bool show_report = false; // print the execution report at the end
    int report_rows = 0;      // terminal rows the report window takes (0 if hidden)
    int console_lines = kMinConsoleLines; // text lines of the live console
};

// Rows the report window takes in this terminal, for values wide enough to cover any real run.
inline int planned_report_rows() {
    ExecutionStats widest;
    widest.scanned_dirs = widest.scanned_files = widest.renamed_dirs = widest.renamed_files = widest.flattened_files = widest.excluded_items = 999999;
    widest.errors = 1;
    widest.duration_ms = 99999.99;
    const std::vector<ReportRow> rows = make_report_rows(widest);
    const ReportMetrics m = report_metrics();
    return layout_report(rows, m.inner, m.icon_cols).rows + 2;
}

// Rows of the live frame: a progress row, then the console window (title bar, `lines` text lines,
// bottom edge). Below it the terminal needs a free line for the cursor and, in Sixel mode, one
// spare row that absorbs a terminal's own post-image cursor movement.
inline ScreenPlan plan_screen(bool want_report) {
    using namespace term;
    ScreenPlan plan;
    plan.live = dashboard_fits();
    const bool windowed = g_color_mode != ColorMode::None && g_term.cols >= 40;
    const int report_rows = want_report && windowed ? planned_report_rows() : 0;
    if (!plan.live) {
        plan.show_report = want_report && g_color_mode != ColorMode::None;
        plan.report_rows = report_rows;
        return plan;
    }
    const int spare = g_color_mode == ColorMode::Full ? 1 : 0;
    const int rows = g_term.rows;
    if (report_rows > 0) {
        const int lines = rows - report_rows - 4 - spare;
        if (lines >= kMinConsoleLines && 5 * report_rows <= lines + 2) { // the report is at most 20% of the console window
            plan.show_report = true;
            plan.report_rows = report_rows;
            plan.console_lines = lines;
            return plan;
        }
    }
    plan.console_lines = std::max(kMinConsoleLines, rows - 4 - spare);
    return plan;
}

} // namespace fsturbo::ui

#endif // REPORT_VIEW_HPP
