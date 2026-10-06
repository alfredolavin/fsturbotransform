#ifndef APP_ICON_HPP
#define APP_ICON_HPP

// The application icon: a 128x128 straight-alpha RGBA bitmap baked in with #embed
// (made by tools/make_app_icon.py from assets/icon.png's master) and composited onto a
// Canvas at any size with a premultiplied area-average filter, so its translucent glass
// and glow blend smoothly into whatever is already drawn underneath.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include "color.hpp"
#include "sixel_renderer.hpp"

namespace fsturbo::sixel {

inline constexpr int kAppIconSize = 128; // keep in sync with EMBED_SIZE in tools/make_app_icon.py

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
inline constexpr unsigned char app_icon_rgba[] = {
#embed "app_icon.rgba"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static_assert(sizeof(app_icon_rgba) == static_cast<std::size_t>(kAppIconSize) * kAppIconSize * 4,
              "app_icon.rgba must be kAppIconSize x kAppIconSize RGBA");

// Composites the icon scaled to size x size pixels with its top-left corner at (x, y).
inline void draw_app_icon(Canvas& c, int x, int y, int size) {
    if (size <= 0) return;
    constexpr int N = kAppIconSize;
    const float step = static_cast<float>(N) / static_cast<float>(size);
    for (int dy = 0; dy < size; ++dy) {
        const float y0 = static_cast<float>(dy) * step, y1 = y0 + step;
        for (int dx = 0; dx < size; ++dx) {
            const float x0 = static_cast<float>(dx) * step, x1 = x0 + step;
            float sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0, area = 0;
            const int sy_end = std::min(N, static_cast<int>(std::ceil(y1)));
            const int sx_end = std::min(N, static_cast<int>(std::ceil(x1)));
            for (int sy = static_cast<int>(y0); sy < sy_end; ++sy) {
                const float wy = std::min(static_cast<float>(sy + 1), y1) - std::max(static_cast<float>(sy), y0);
                for (int sx = static_cast<int>(x0); sx < sx_end; ++sx) {
                    const float w = wy * (std::min(static_cast<float>(sx + 1), x1) - std::max(static_cast<float>(sx), x0));
                    const unsigned char* p = &app_icon_rgba[(static_cast<std::size_t>(sy) * N + static_cast<std::size_t>(sx)) * 4];
                    const float wa = w * static_cast<float>(p[3]) / 255.0f; // premultiplied weight
                    sum_r += static_cast<float>(p[0]) * wa;
                    sum_g += static_cast<float>(p[1]) * wa;
                    sum_b += static_cast<float>(p[2]) * wa;
                    sum_a += wa;
                    area += w;
                }
            }
            const int alpha = static_cast<int>(sum_a / area * 255.0f + 0.5f);
            if (alpha <= 0) continue;
            const auto channel = [&](float s) { return static_cast<std::uint8_t>(std::clamp(s / sum_a + 0.5f, 0.0f, 255.0f)); };
            c.blend(x + dx, y + dy, {channel(sum_r), channel(sum_g), channel(sum_b)}, alpha);
        }
    }
}

} // namespace fsturbo::sixel

#endif // APP_ICON_HPP
