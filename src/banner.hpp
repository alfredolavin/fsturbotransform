#ifndef BANNER_HPP
#define BANNER_HPP

// Neon gradient header: a slim bar (one and a half terminal rows) with the title set in Fira Code at 1 em (the terminal
// font's size, the one the console windows use), and the application icon, far taller than the
// bar, standing out of it above and below.

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include "app_icon.hpp"
#include "color.hpp"
#include "sixel_renderer.hpp"
#include "ttf_font.hpp"

namespace fsturbo::sixel {

inline constexpr int kBannerRows = 4; // terminal rows the banner image covers; the icon is this tall, the bar 1.5

// `height` is kBannerRows terminal rows in pixels. With the terminal's background colour known
// the canvas starts as that colour, so the icon's translucent glow outside the bar blends into it
// (Sixel has no alpha channel); otherwise only its more opaque pixels survive.
inline Canvas render_banner(int width, int height, std::string_view title, std::optional<Rgb> background = std::nullopt) {
    Canvas c(width, height, background ? Rgba{background->r, background->g, background->b, 255} : Rgba{});
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float row = h / static_cast<float>(kBannerRows);
    const float bar_h = 1.5f * row, bar_y = (h - bar_h) * 0.5f;
    c.fill_rounded_rect(0.0f, bar_y, w, bar_h, bar_h * 0.22f, palette::neon_cyan);
    c.fill_rounded_rect(2.0f, bar_y + 2.0f, w - 4.0f, bar_h - 4.0f, bar_h * 0.20f, [&](int x, int y) {
        const float tx = static_cast<float>(x) / w, ty = std::clamp((static_cast<float>(y) - bar_y) / bar_h, 0.0f, 1.0f);
        const Rgb base = gradient(tx, Rgb{0, 150, 190}, Rgb{120, 20, 210}, Rgb{200, 0, 120});
        return scale(base, 1.05f - 0.35f * ty);
    });

    const auto& fonts = ui_fonts();
    ttf::GlyphCache title_gc(fonts.bold, row / 1.32f); // 1 em
    const int adv = static_cast<int>(std::lround(title_gc.advance()));
    const int title_w = adv * static_cast<int>(ttf::utf8_length(title));
    const int icon_size = height;
    const int gap = adv;

    // Icon + title as one group, centered in the bar; the icon spans the whole image height.
    const int icon_x = (width - (icon_size + gap + title_w)) / 2;
    const int title_x = icon_x + icon_size + gap;
    const int baseline = static_cast<int>(std::lround(bar_y + (bar_h - (title_gc.ascent() + title_gc.descent())) * 0.5f + title_gc.ascent()));
    const int shadow = std::max(1, static_cast<int>(std::lround(title_gc.advance() / 8.0f)));

    draw_app_icon(c, icon_x, 0, icon_size);
    c.draw_text(title_gc, title_x + shadow, baseline + shadow, title, {10, 0, 30}, adv);
    c.draw_text(title_gc, title_x, baseline, title, {255, 255, 255}, adv);
    return c;
}

} // namespace fsturbo::sixel

#endif // BANNER_HPP
