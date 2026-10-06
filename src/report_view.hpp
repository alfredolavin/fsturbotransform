#ifndef REPORT_VIEW_HPP
#define REPORT_VIEW_HPP

// Execution report rendered as a second mini-terminal window in the same style as the
// live dashboard: a Sixel window set in Fira Code (Full) or the matching ANSI box.

#include <algorithm>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "console_window.hpp"
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
inline constexpr std::size_t kLeaderEnd = 22; // label + dot leader end at this column

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

inline std::size_t leader_dots(std::string_view label) {
    const std::size_t n = ttf::utf8_length(label);
    return n + 3 < kLeaderEnd ? kLeaderEnd - n - 1 : 2;
}

inline std::string dots(std::size_t n) {
    std::string s;
    for (std::size_t i = 0; i < n; ++i) s += "·";
    return s;
}

inline std::string render_report_ansi(std::span<const ReportRow> rows, const ReportStatus& status) {
    using namespace term;
    const int width = window_columns();
    const auto inner = static_cast<std::size_t>(std::max(0, width - 4));
    std::string out = ui::ansi_window_top(width, kReportTitle, status.text, status.color) + "\n";
    for (const ReportRow& row : rows) {
        const auto [symbol, icon_color] = ansi_icon(row.icon);
        const std::size_t n_dots = leader_dots(row.label);
        const std::string value = fit_right(row.value, inner - std::min(inner, 2 + ttf::utf8_length(row.label) + 1 + n_dots + 1));
        const std::string content = std::format("{}{}{} {}{} {}{}{} {}{}{}{}", rgb_fg(icon_color), symbol, reset(), rgb_fg(palette::path_name), row.label,
                                                rgb_fg(palette::console_border), dots(n_dots), reset(), bold(), rgb_fg(row.color), value, reset());
        const std::size_t cols = 2 + ttf::utf8_length(row.label) + 1 + n_dots + 1 + ttf::utf8_length(value);
        out += ui::ansi_window_row(width, content, cols) + "\n";
    }
    out += ui::ansi_window_bottom(width) + "\n";
    return out;
}

inline std::string render_report_sixel(std::span<const ReportRow> rows, const ReportStatus& status) {
    ConsoleFonts f(term::g_term.cell_height());
    const int ch = f.cell_h;
    const int width = window_columns() * term::g_term.cell_width();
    const int height = (static_cast<int>(rows.size()) + 2) * ch;
    sixel::Canvas c = window_canvas(width, height);

    const int title_baseline = draw_console_window(c, f, 0.0f, static_cast<float>(height - 1), kReportTitle);
    draw_title_status(c, f, title_baseline, status.text, status.color);

    const float content_top = static_cast<float>(ch) * 1.25f + 2.0f;
    const int icon_x = f.adv;
    const int text_x = icon_x + sixel::kIconSize + f.adv;
    const int value_cols = std::max(4, (width - text_x) / f.adv - static_cast<int>(kLeaderEnd) - 2);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const ReportRow& row = rows[i];
        const float top = content_top + static_cast<float>(i) * static_cast<float>(ch);
        const int baseline = f.baseline_in(top, static_cast<float>(ch));
        c.draw_image(sixel::draw_icon(row.icon), icon_x, static_cast<int>(top) + (ch - sixel::kIconSize) / 2);
        int x = c.draw_text(f.regular, text_x, baseline, row.label, palette::path_name, f.adv);
        x = c.draw_text(f.regular, x + f.adv, baseline, dots(leader_dots(row.label)), palette::console_border, f.adv);
        c.draw_text(f.bold, x + f.adv, baseline, term::fit_right(row.value, static_cast<std::size_t>(value_cols)), row.color, f.adv);
    }
    return term::place_image(sixel::encode(c), height);
}

inline void print_report(std::span<const ReportRow> rows, const ReportStatus& status) {
    using namespace term;
    if (g_term.cols < 40) { // too narrow for a window: one icon line per row
        std::print("{}", icon_line(sixel::Icon::Rocket, std::format("{}{}{}", rgb_fg(status.color), status.text, reset())));
        for (const ReportRow& row : rows)
            std::print("{}", icon_line(row.icon, std::format("{}: {}{}{}{}", row.label, bold(), rgb_fg(row.color), row.value, reset())));
        return;
    }
    std::print("{}", g_color_mode == ColorMode::Full ? render_report_sixel(rows, status) : render_report_ansi(rows, status));
}

} // namespace fsturbo::ui

#endif // REPORT_VIEW_HPP
