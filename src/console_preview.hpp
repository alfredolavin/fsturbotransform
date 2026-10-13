#ifndef CONSOLE_PREVIEW_HPP
#define CONSOLE_PREVIEW_HPP

// A still frame of the live mini-console drawn with the active format, for the GUI's "Console
// format" dialog: the same chrome, gradient fill, syntax colours and font as a terminal frame
// (Sixel / Kitty), with sample lines instead of a run's.

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include "console_window.hpp"
#include "terminal_style.hpp"

namespace fsturbo::ui {

// `width` pixels wide, terminal cells `cell_h` pixels high, `text_lines` console lines.
inline sixel::Canvas render_console_preview(int width, int cell_h, int text_lines = 6) {
    using term::Role;
    using term::Span;
    const style::ConsolePalette& p = style::active();
    ConsoleFonts f(cell_h);
    const int rows = text_lines + 3;
    const float W = static_cast<float>(width), CH = static_cast<float>(cell_h);
    sixel::Canvas c(width, rows * cell_h);

    // Row 0: the progress pill, 62 % done (as LiveDashboard::build_sixel_chrome lays it out)
    const float bar_h = std::max(4.0f, CH * 0.42f), bar_x = CH * 0.5f, bar_w = std::max(40.0f, W * 0.42f), bar_y = (CH - bar_h) * 0.5f;
    c.fill_rounded_rect(0.0f, 1.0f, W, CH - 2.0f, (CH - 2.0f) * 0.5f, p.border);
    c.fill_rounded_rect(1.0f, 2.0f, W - 2.0f, CH - 4.0f, (CH - 4.0f) * 0.5f, p.background);
    c.fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, bar_h * 0.5f, p.track);
    draw_gradient_fill(c, bar_x, bar_y, bar_w, bar_h, 0.62f, 0.9f);
    const int row0 = f.baseline_in(0.0f, CH);
    const int x0 = static_cast<int>(bar_x + bar_w) + f.adv;
    const int x1 = c.draw_text(f.bold, x0, row0, "RENAME ", p.action, f.adv);
    c.draw_text(f.regular, x1, row0, "  62.0%  620/1000", p.name, f.adv);

    // The window with its title, the stats in the title bar, and the console lines
    const float win_y = std::round(CH * 1.25f);
    const int title_baseline = draw_console_window(c, f, win_y, static_cast<float>(c.height()) - win_y - 1.0f, p.title);
    draw_title_status(c, f, title_baseline, "1.2k/s · 00:03", p.note);

    struct Line {
        std::string_view action, source, dest;
    };
    static constexpr std::array<Line, 6> kSamples{{
        {"FLATTEN", "docs/old/Report 2023.PDF", "Report 2023.PDF"},
        {"DIR RENAME", "Holiday Photos", "holiday_photos"},
        {"FILE RENAME", "Photos/2024/IMG_0042.JPG", "img_0042.jpg"},
        {"FILE RENAME", "music/Track 07 - Intro.FLAC", "track_07_intro.flac"},
        {"DIR RENAME", "Old Reports", "old_reports"},
        {"FILE RENAME", "src/MainWindow.cpp", "main_window.cpp"},
    }};
    const float lines_top = win_y + CH * 1.2f;
    for (int i = 0; i < text_lines; ++i) {
        const Line& l = kSamples[static_cast<std::size_t>(std::max(0, static_cast<int>(kSamples.size()) - text_lines + i)) % kSamples.size()];
        const bool active = i + 1 == text_lines;
        std::vector<Span> spans;
        spans.push_back({active ? "› " : "· ", active ? Role::Prompt : Role::Note});
        spans.push_back({l.action, Role::Action});
        spans.push_back({std::string_view("            ").substr(0, 12 - l.action.size()), Role::Plain});
        term::highlight_path(l.source, Role::Name, spans);
        spans.push_back({" → ", Role::Arrow});
        term::highlight_path(l.dest, Role::Dest, spans);
        int x = f.adv;
        const int baseline = f.baseline_in(lines_top + static_cast<float>(i) * CH, CH);
        for (const Span& s : spans) {
            ttf::GlyphCache& gc = s.role == Role::Action && p.bold_actions ? f.bold : f.regular;
            x = c.draw_text(gc, x, baseline, s.text, term::role_color(s.role), f.adv);
        }
        if (active) c.draw_text(f.regular, x, baseline, "▌", p.prompt, f.adv);
    }
    return c;
}

} // namespace fsturbo::ui

#endif // CONSOLE_PREVIEW_HPP
