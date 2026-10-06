#ifndef BANNER_HPP
#define BANNER_HPP

// Neon gradient header banner: the application icon followed by the title set in Fira Code.

#include <algorithm>
#include <cmath>
#include <string_view>
#include "app_icon.hpp"
#include "color.hpp"
#include "sixel_renderer.hpp"
#include "ttf_font.hpp"

namespace fsturbo::sixel {

// Banner title size: 16 pt, converted to pixels at the conventional 96 DPI (a terminal only
// reports pixel sizes), independent of the terminal's cell size.
inline constexpr float kTitlePoints = 16.0f;
inline constexpr float kTitlePixels = kTitlePoints * 96.0f / 72.0f;

inline Canvas render_banner(int width, int height, std::string_view title) {
    Canvas c(width, height);
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    c.fill_rounded_rect(0.0f, 0.0f, w, h, h * 0.22f, palette::neon_cyan);
    c.fill_rounded_rect(2.0f, 2.0f, w - 4.0f, h - 4.0f, h * 0.20f, [&](int x, int y) {
        const float tx = static_cast<float>(x) / w, ty = static_cast<float>(y) / h;
        const Rgb base = gradient(tx, Rgb{0, 150, 190}, Rgb{120, 20, 210}, Rgb{200, 0, 120});
        return scale(base, 1.05f - 0.35f * ty);
    });

    const auto& fonts = ui_fonts();
    ttf::GlyphCache title_gc(fonts.bold, kTitlePixels);
    const int adv = static_cast<int>(std::lround(title_gc.advance()));
    const int title_w = adv * static_cast<int>(ttf::utf8_length(title));
    const int icon_size = std::max(1, static_cast<int>(std::lround(h * 0.86f)));
    const int gap = adv;

    // Icon + title as one group, centered in the banner.
    const int icon_x = (width - (icon_size + gap + title_w)) / 2;
    const int title_x = icon_x + icon_size + gap;
    const int baseline = static_cast<int>(std::lround((h - (title_gc.ascent() + title_gc.descent())) * 0.5f + title_gc.ascent()));

    draw_app_icon(c, icon_x, (height - icon_size) / 2, icon_size);
    c.draw_text(title_gc, title_x + 2, baseline + 2, title, {10, 0, 30}, adv);
    c.draw_text(title_gc, title_x, baseline, title, {255, 255, 255}, adv);
    return c;
}

} // namespace fsturbo::sixel

#endif // BANNER_HPP
