#ifndef CONSOLE_WINDOW_HPP
#define CONSOLE_WINDOW_HPP

// The mini-terminal window look shared by the live dashboard and the execution report:
// a rounded window with a title bar and traffic lights, text set in the embedded Fira Code
// (Sixel mode), or the matching box-drawing frame (ANSI modes).

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include "color.hpp"
#include "sixel_renderer.hpp"
#include "terminal_style.hpp"
#include "ttf_font.hpp"

namespace fsturbo::ui {

// Window width in terminal columns.
inline int window_columns() { return std::min(term::g_term.cols - 1, 100); }

// Fira Code metrics for console windows at the terminal's cell height.
struct ConsoleFonts {
    int cell_h;
    ttf::GlyphCache regular;
    ttf::GlyphCache bold;
    int adv; // monospace advance in pixels

    explicit ConsoleFonts(int cell_height)
        : cell_h(cell_height),
          regular(sixel::ui_fonts().regular, static_cast<float>(cell_height) / 1.32f),
          bold(sixel::ui_fonts().bold, static_cast<float>(cell_height) / 1.32f),
          adv(std::max(1, static_cast<int>(std::lround(regular.advance())))) {}

    // Baseline that vertically centers a line of text in [top, top + height).
    int baseline_in(float top, float height) const {
        const float asc = regular.ascent(), desc = regular.descent();
        return static_cast<int>(std::lround(top + (height - (asc + desc)) * 0.5f + asc));
    }
};

// Canvas filled with the terminal background when known (so anti-aliased edges blend),
// otherwise transparent.
inline sixel::Canvas window_canvas(int w, int h) {
    const auto& bg = term::g_term.background;
    return sixel::Canvas(w, h, bg ? Rgba{bg->r, bg->g, bg->b, 255} : Rgba{});
}

// Draws the window chrome spanning the canvas width from `y` with height `h`.
// Returns the baseline of the title bar text.
inline int draw_console_window(sixel::Canvas& c, ConsoleFonts& f, float y, float h, std::string_view title) {
    const float W = static_cast<float>(c.width()), CH = static_cast<float>(f.cell_h), r = CH * 0.4f;
    c.fill_rounded_rect(0.0f, y, W, h, r, palette::console_border);
    c.fill_rounded_rect(1.0f, y + 1.0f, W - 2.0f, h - 2.0f, r - 1.0f, palette::console_bg);
    c.fill_rounded_rect(1.0f, y + 1.0f, W - 2.0f, CH, r - 1.0f, palette::console_title_bg);
    c.fill_rect(1, static_cast<int>(y + CH * 0.5f), c.width() - 2, static_cast<int>(CH * 0.5f) + 1, palette::console_title_bg);
    c.fill_rect(1, static_cast<int>(y) + 1 + f.cell_h, c.width() - 2, 1, palette::console_border);
    constexpr std::array<Rgb, 3> lights{{{255, 95, 86}, {255, 189, 46}, {39, 201, 63}}};
    for (std::size_t i = 0; i < lights.size(); ++i)
        c.fill_circle(CH * 0.7f + static_cast<float>(i) * CH * 0.6f, y + 1.0f + CH * 0.5f, CH * 0.17f, lights[i]);
    const int baseline = f.baseline_in(y, CH);
    const int title_cols = static_cast<int>(ttf::utf8_length(title));
    c.draw_text(f.bold, (c.width() - title_cols * f.adv) / 2, baseline, title, palette::console_title, f.adv);
    return baseline;
}

// Right-aligned text inside the title bar.
inline void draw_title_status(sixel::Canvas& c, ConsoleFonts& f, int baseline, std::string_view text, Rgb color) {
    const int cols = static_cast<int>(ttf::utf8_length(text));
    c.draw_text(f.regular, c.width() - (cols + 1) * f.adv, baseline, text, color, f.adv);
}

// --- ANSI frame -------------------------------------------------------------------

// "┌─ TITLE ───────── status ─┐"
inline std::string ansi_window_top(int width, std::string_view title, std::string_view status, Rgb status_color) {
    const std::string border = term::rgb_fg(palette::console_border);
    const int title_cols = static_cast<int>(ttf::utf8_length(title));
    const int status_cols = static_cast<int>(ttf::utf8_length(status));
    std::string out = border + "┌─ " + std::string(term::bold()) + term::rgb_fg(palette::console_title);
    out += title;
    out += term::reset();
    out += border + " ";
    for (int i = std::max(1, width - 8 - title_cols - status_cols); i > 0; --i) out += "─";
    out += " " + term::rgb_fg(status_color);
    out += status;
    out += border + " ─┐";
    out += term::reset();
    return out;
}

// "│ content          │" — `content` is pre-styled and `content_cols` columns wide.
inline std::string ansi_window_row(int width, std::string_view content, std::size_t content_cols) {
    const std::string border = term::rgb_fg(palette::console_border);
    const auto inner = static_cast<std::size_t>(std::max(0, width - 4));
    std::string out = border + "│ ";
    out += term::reset();
    out += content;
    if (content_cols < inner) out.append(inner - content_cols, ' ');
    out += border + " │";
    out += term::reset();
    return out;
}

// "└──────────────────┘"
inline std::string ansi_window_bottom(int width) {
    std::string out = term::rgb_fg(palette::console_border) + "└";
    for (int i = 0; i < width - 2; ++i) out += "─";
    out += "┘";
    out += term::reset();
    return out;
}

} // namespace fsturbo::ui

#endif // CONSOLE_WINDOW_HPP
