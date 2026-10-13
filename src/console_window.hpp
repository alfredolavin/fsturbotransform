#ifndef CONSOLE_WINDOW_HPP
#define CONSOLE_WINDOW_HPP

// The mini-terminal window look shared by the live dashboard and the execution report:
// a rounded window with a title bar, traffic lights and the application icon, text set in the
// embedded Fira Code (Sixel mode), or the matching box-drawing frame (ANSI modes).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include "app_icon.hpp"
#include "color.hpp"
#include "console_style.hpp"
#include "graphics.hpp"
#include "sixel_renderer.hpp"
#include "terminal_style.hpp"
#include "ttf_font.hpp"

namespace fsturbo::ui {

// Window width in terminal columns.
inline int window_columns() { return std::min(term::g_term.cols - 1, 100); }

// The file fontconfig picks for a monospace `family` (bold or regular), or "" without fc-match.
inline std::string fc_match_file(std::string_view family, bool bold) {
    std::string quoted = "'";
    for (const char ch : family) quoted += ch == '\'' ? std::string("'\\''") : std::string(1, ch);
    quoted += bold ? ":spacing=mono:weight=bold'" : ":spacing=mono:weight=regular'";
    std::string file;
    if (std::FILE* p = ::popen(("fc-match -f '%{file}' " + quoted + " 2>/dev/null").c_str(), "r")) {
        char buf[512];
        while (std::fgets(buf, sizeof buf, p)) file += buf;
        ::pclose(p);
    }
    return file;
}

// The console's font: the format's font_family when fontconfig finds a TrueType (glyf) file for it,
// else the embedded Fira Code. Chosen once, on first use (the format is set before the first frame).
struct ConsoleFontSet {
    std::vector<unsigned char> regular_data, bold_data;
    ttf::Font regular{fira_code_bytes, 400.0f};
    ttf::Font bold{fira_code_bytes, 700.0f};
    std::string family = "Fira Code";

    ConsoleFontSet() {
        const std::string& wanted = style::active().font_family;
        if (wanted.empty()) return;
        const auto read = [](const std::string& path) {
            std::ifstream in(path, std::ios::binary);
            return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), {});
        };
        const std::string reg_file = fc_match_file(wanted, false), bold_file = fc_match_file(wanted, true);
        if (reg_file.empty()) return;
        regular_data = read(reg_file);
        bold_data = bold_file.empty() || bold_file == reg_file ? std::vector<unsigned char>() : read(bold_file);
        const ttf::Font reg(regular_data, 400.0f);
        if (!reg.valid()) return; // a CFF (.otf) or collection file: keep Fira Code
        regular = reg;
        // One variable file serves both weights; a static family has its own bold file.
        const ttf::Font b = bold_data.empty() ? ttf::Font(regular_data, 700.0f) : ttf::Font(bold_data, 700.0f);
        bold = b.valid() ? b : reg;
        family = wanted;
    }
};

inline const ConsoleFontSet& console_fonts() {
    static const ConsoleFontSet fonts;
    return fonts;
}

// Console font metrics at the terminal's cell height (scaled by the format's font_scale).
struct ConsoleFonts {
    int cell_h;
    ttf::GlyphCache regular;
    ttf::GlyphCache bold;
    int adv; // monospace advance in pixels

    explicit ConsoleFonts(int cell_height)
        : cell_h(cell_height),
          regular(console_fonts().regular, static_cast<float>(cell_height) / 1.32f * style::active().font_scale),
          bold(console_fonts().bold, static_cast<float>(cell_height) / 1.32f * style::active().font_scale),
          adv(std::max(1, static_cast<int>(std::lround(regular.advance())))) {}

    // Baseline that vertically centers a line of text in [top, top + height).
    int baseline_in(float top, float height) const {
        const float asc = regular.ascent(), desc = regular.descent();
        return static_cast<int>(std::lround(top + (height - (asc + desc)) * 0.5f + asc));
    }
};

// Canvas filled with the terminal background when known (so anti-aliased edges blend), otherwise
// transparent. Kitty has real alpha, so there it always stays transparent and the terminal's own
// background shows through.
inline sixel::Canvas window_canvas(int w, int h) {
    const auto& bg = term::g_term.background;
    return sixel::Canvas(w, h, bg && gfx::g_protocol == gfx::Protocol::Sixel ? Rgba{bg->r, bg->g, bg->b, 255} : Rgba{});
}

// Draws the window chrome spanning the canvas width from `y` with height `h`.
// Returns the baseline of the title bar text.
inline int draw_console_window(sixel::Canvas& c, ConsoleFonts& f, float y, float h, std::string_view title) {
    const style::ConsolePalette& p = style::active();
    const float W = static_cast<float>(c.width()), CH = static_cast<float>(f.cell_h), r = CH * 0.4f;
    c.fill_rounded_rect(0.0f, y, W, h, r, p.border);
    c.fill_rounded_rect(1.0f, y + 1.0f, W - 2.0f, h - 2.0f, r - 1.0f, p.background);
    c.fill_rounded_rect(1.0f, y + 1.0f, W - 2.0f, CH, r - 1.0f, p.title_background);
    c.fill_rect(1, static_cast<int>(y + CH * 0.5f), c.width() - 2, static_cast<int>(CH * 0.5f) + 1, p.title_background);
    c.fill_rect(1, static_cast<int>(y) + 1 + f.cell_h, c.width() - 2, 1, p.border);
    constexpr std::array<Rgb, 3> lights{{{255, 95, 86}, {255, 189, 46}, {39, 201, 63}}};
    if (p.traffic_lights)
        for (std::size_t i = 0; i < lights.size(); ++i)
            c.fill_circle(CH * 0.7f + static_cast<float>(i) * CH * 0.6f, y + 1.0f + CH * 0.5f, CH * 0.17f, lights[i]);
    const int baseline = f.baseline_in(y, CH);
    const int title_cols = static_cast<int>(ttf::utf8_length(title));
    const int title_x = (c.width() - title_cols * f.adv) / 2;
    c.draw_text(f.bold, title_x, baseline, title, p.title_color, f.adv);
    // The application icon sits as the window icon just left of the centered title.
    if (p.app_icon) sixel::draw_app_icon(c, title_x - f.cell_h - f.adv / 2, static_cast<int>(y) + 1, f.cell_h);
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
    const std::string border = term::rgb_fg(style::active().border);
    const int title_cols = static_cast<int>(ttf::utf8_length(title));
    const int status_cols = static_cast<int>(ttf::utf8_length(status));
    std::string out = border + "┌─ " + std::string(term::bold()) + term::rgb_fg(style::active().title_color);
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
    const std::string border = term::rgb_fg(style::active().border);
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
    std::string out = term::rgb_fg(style::active().border) + "└";
    for (int i = 0; i < width - 2; ++i) out += "─";
    out += "┘";
    out += term::reset();
    return out;
}

} // namespace fsturbo::ui

#endif // CONSOLE_WINDOW_HPP
