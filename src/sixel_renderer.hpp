#ifndef SIXEL_RENDERER_HPP
#define SIXEL_RENDERER_HPP

// Sixel graphics engine: RGBA canvas + anti-aliased primitives + Fira Code text,
// adaptive median-cut palette quantization, and a run-length-encoded DCS Sixel encoder.
// Icons are rasterized *and* Sixel-encoded at compile time.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "color.hpp"
#include "fira_code_font.hpp"
#include "ttf_font.hpp"

namespace fsturbo::sixel {

class Canvas {
    int w_ = 0;
    int h_ = 0;
    std::vector<Rgba> px_;

    constexpr std::size_t offset(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x);
    }

public:
    constexpr Canvas() = default;
    constexpr Canvas(int w, int h, Rgba fill = {})
        : w_(std::max(w, 0)), h_(std::max(h, 0)), px_(static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_), fill) {}

    constexpr int width() const { return w_; }
    constexpr int height() const { return h_; }
    constexpr std::span<const Rgba> pixels() const { return px_; }
    constexpr bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }

    constexpr void put(int x, int y, Rgb c) {
        if (contains(x, y)) px_[offset(x, y)] = {c.r, c.g, c.b, 255};
    }

    // Source-over blend with 0..255 alpha.
    constexpr void blend(int x, int y, Rgb c, int alpha) {
        if (alpha <= 0 || !contains(x, y)) return;
        Rgba& d = px_[offset(x, y)];
        if (alpha >= 255 || d.a == 0) {
            d = {c.r, c.g, c.b, static_cast<std::uint8_t>(std::min(alpha, 255))};
            return;
        }
        const Rgb mixed = lerp(d.rgb(), c, static_cast<float>(alpha) / 255.0f);
        d = {mixed.r, mixed.g, mixed.b, std::max<std::uint8_t>(d.a, static_cast<std::uint8_t>(alpha))};
    }

    constexpr void fill_rect(int x, int y, int w, int h, Rgb c) {
        const int x0 = std::max(x, 0), y0 = std::max(y, 0);
        const int x1 = std::min(x + w, w_), y1 = std::min(y + h, h_);
        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx) px_[offset(xx, yy)] = {c.r, c.g, c.b, 255};
    }

    // Anti-aliased rounded rectangle (signed-distance coverage per pixel).
    template <typename Shader>
    void fill_rounded_rect(float x, float y, float w, float h, float r, Shader&& shade, float opacity = 1.0f) {
        const float hw = w * 0.5f, hh = h * 0.5f, cx = x + hw, cy = y + hh;
        r = std::clamp(r, 0.0f, std::min(hw, hh));
        const int x0 = std::max(0, static_cast<int>(std::floor(x)));
        const int y0 = std::max(0, static_cast<int>(std::floor(y)));
        const int x1 = std::min(w_, static_cast<int>(std::ceil(x + w)));
        const int y1 = std::min(h_, static_cast<int>(std::ceil(y + h)));
        for (int py = y0; py < y1; ++py) {
            const float qy = std::fabs(static_cast<float>(py) + 0.5f - cy) - (hh - r);
            for (int px = x0; px < x1; ++px) {
                const float qx = std::fabs(static_cast<float>(px) + 0.5f - cx) - (hw - r);
                const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
                const float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
                const float cov = std::clamp(0.5f - d, 0.0f, 1.0f) * opacity;
                if (cov > 0.0f) blend(px, py, shade(px, py), static_cast<int>(cov * 255.0f + 0.5f));
            }
        }
    }

    void fill_rounded_rect(float x, float y, float w, float h, float r, Rgb c, float opacity = 1.0f) {
        fill_rounded_rect(x, y, w, h, r, [c](int, int) { return c; }, opacity);
    }

    void fill_circle(float cx, float cy, float radius, Rgb c) {
        fill_rounded_rect(cx - radius, cy - radius, 2.0f * radius, 2.0f * radius, radius, c);
    }

    void blit(const ttf::GlyphBitmap& g, int pen_x, int baseline, Rgb c) {
        const int ox = pen_x + g.left, oy = baseline - g.top;
        for (int y = 0; y < g.height; ++y)
            for (int x = 0; x < g.width; ++x)
                blend(ox + x, oy + y, c, g.coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(g.width) + static_cast<std::size_t>(x)]);
    }

    // Draws UTF-8 text on a fixed monospace grid; returns the pen position after the text.
    int draw_text(ttf::GlyphCache& gc, int x, int baseline, std::string_view text, Rgb c, int advance) {
        ttf::for_each_codepoint(text, [&](char32_t cp) {
            if (cp != U' ') blit(gc.glyph(cp), x, baseline, c);
            x += advance;
        });
        return x;
    }
};

// --- Palette quantization ------------------------------------------------------

inline constexpr std::uint8_t kTransparent = 255;
inline constexpr std::size_t kMaxColors = 255;

struct IndexedImage {
    int width = 0;
    int height = 0;
    std::vector<Rgb> palette;
    std::vector<std::uint8_t> index; // kTransparent marks pixels left untouched
};

// Exact palette (≤255 distinct colors). Used for compile-time icons.
constexpr std::optional<IndexedImage> quantize_exact(const Canvas& c) {
    IndexedImage img{c.width(), c.height(), {}, std::vector<std::uint8_t>(c.pixels().size(), kTransparent)};
    for (std::size_t i = 0; i < c.pixels().size(); ++i) {
        const Rgba p = c.pixels()[i];
        if (p.a < 128) continue;
        const auto it = std::ranges::find(img.palette, p.rgb());
        if (it == img.palette.end()) {
            if (img.palette.size() == kMaxColors) return std::nullopt;
            img.palette.push_back(p.rgb());
            img.index[i] = static_cast<std::uint8_t>(img.palette.size() - 1);
        } else {
            img.index[i] = static_cast<std::uint8_t>(it - img.palette.begin());
        }
    }
    return img;
}

// 15-bit histogram + population-weighted median cut down to ≤255 colors.
inline IndexedImage quantize_adaptive(const Canvas& c) {
    struct Bin {
        std::uint32_t count = 0, r = 0, g = 0, b = 0;
    };
    struct Entry {
        std::uint16_t key;
        std::uint32_t count;
        Rgb avg;
    };
    struct Box {
        std::size_t begin, end;
        std::uint64_t score;
        int channel;
    };
    thread_local std::vector<Bin> hist(1 << 15);
    thread_local std::vector<std::uint8_t> lut(1 << 15);

    const auto px = c.pixels();
    auto key_of = [](Rgba p) {
        return static_cast<std::uint16_t>((p.r >> 3) << 10 | (p.g >> 3) << 5 | (p.b >> 3));
    };

    std::vector<Entry> entries;
    for (const Rgba p : px) {
        if (p.a < 128) continue;
        Bin& b = hist[key_of(p)];
        if (b.count++ == 0) entries.push_back({key_of(p), 0, {}});
        b.r += p.r;
        b.g += p.g;
        b.b += p.b;
    }
    for (Entry& e : entries) {
        Bin& b = hist[e.key];
        e.count = b.count;
        e.avg = {static_cast<std::uint8_t>(b.r / b.count), static_cast<std::uint8_t>(b.g / b.count), static_cast<std::uint8_t>(b.b / b.count)};
        b = {};
    }

    IndexedImage img{c.width(), c.height(), {}, std::vector<std::uint8_t>(px.size(), kTransparent)};
    auto channel = [](const Entry& e, int ch) { return ch == 0 ? e.avg.r : ch == 1 ? e.avg.g : e.avg.b; };

    if (entries.size() <= kMaxColors) {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            img.palette.push_back(entries[i].avg);
            lut[entries[i].key] = static_cast<std::uint8_t>(i);
        }
    } else {
        auto make_box = [&](std::size_t begin, std::size_t end) {
            Box box{begin, end, 0, 0};
            if (end - begin < 2) return box;
            std::array<int, 3> lo{255, 255, 255}, hi{0, 0, 0};
            std::uint64_t pop = 0;
            for (std::size_t i = begin; i < end; ++i) {
                for (int ch = 0; ch < 3; ++ch) {
                    lo[ch] = std::min<int>(lo[ch], channel(entries[i], ch));
                    hi[ch] = std::max<int>(hi[ch], channel(entries[i], ch));
                }
                pop += entries[i].count;
            }
            for (int ch = 0; ch < 3; ++ch) {
                const auto score = static_cast<std::uint64_t>(hi[ch] - lo[ch]) * pop;
                if (score > box.score) {
                    box.score = score;
                    box.channel = ch;
                }
            }
            return box;
        };

        std::vector<Box> boxes{make_box(0, entries.size())};
        while (boxes.size() < kMaxColors) {
            const auto best = std::ranges::max_element(boxes, {}, &Box::score);
            if (best->score == 0) break;
            const Box box = *best;
            const auto first = entries.begin() + static_cast<std::ptrdiff_t>(box.begin);
            const auto last = entries.begin() + static_cast<std::ptrdiff_t>(box.end);
            std::sort(first, last, [&](const Entry& a, const Entry& b) { return channel(a, box.channel) < channel(b, box.channel); });
            std::uint64_t total = 0, acc = 0;
            for (auto it = first; it != last; ++it) total += it->count;
            std::size_t split = box.begin + 1;
            for (std::size_t i = box.begin; i < box.end - 1; ++i) {
                acc += entries[i].count;
                split = i + 1;
                if (acc * 2 >= total) break;
            }
            *best = make_box(box.begin, split);
            boxes.push_back(make_box(split, box.end));
        }
        for (const Box& box : boxes) {
            std::uint64_t r = 0, g = 0, b = 0, n = 0;
            for (std::size_t i = box.begin; i < box.end; ++i) {
                r += std::uint64_t{entries[i].avg.r} * entries[i].count;
                g += std::uint64_t{entries[i].avg.g} * entries[i].count;
                b += std::uint64_t{entries[i].avg.b} * entries[i].count;
                n += entries[i].count;
                lut[entries[i].key] = static_cast<std::uint8_t>(img.palette.size());
            }
            img.palette.push_back({static_cast<std::uint8_t>(r / n), static_cast<std::uint8_t>(g / n), static_cast<std::uint8_t>(b / n)});
        }
    }

    for (std::size_t i = 0; i < px.size(); ++i)
        if (px[i].a >= 128) img.index[i] = lut[key_of(px[i])];
    return img;
}

constexpr IndexedImage quantize(const Canvas& c) {
    if consteval {
        return quantize_exact(c).value(); // compile-time images must stay within one palette
    } else {
        return quantize_adaptive(c);
    }
}

// --- DCS Sixel encoder --------------------------------------------------------

constexpr void append_uint(std::string& s, std::size_t v) {
    char buf[20]{};
    int n = 0;
    do {
        buf[n++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v != 0);
    while (n > 0) s.push_back(buf[--n]);
}

constexpr void append_run(std::string& s, char ch, std::size_t n) {
    if (n > 3) {
        s.push_back('!');
        append_uint(s, n);
        s.push_back(ch);
    } else {
        s.append(n, ch);
    }
}

// Encodes 6-pixel bands; per band only the colors actually present are emitted,
// each trimmed to its own [first, last] column span and run-length compressed.
// Unset pixels are transparent (P2=1), so an opaque canvas fully repaints its area.
constexpr std::string encode(const IndexedImage& img) {
    std::string out;
    const auto w = static_cast<std::size_t>(std::max(img.width, 0));
    const auto h = static_cast<std::size_t>(std::max(img.height, 0));
    if (w == 0 || h == 0) return out;
    out.reserve(64 + img.palette.size() * 16 + w * ((h + 5) / 6) * 2);

    out += "\x1bP0;1q\"1;1;";
    append_uint(out, w);
    out.push_back(';');
    append_uint(out, h);
    for (std::size_t i = 0; i < img.palette.size(); ++i) {
        const Rgb c = img.palette[i];
        out.push_back('#');
        append_uint(out, i);
        out += ";2;";
        append_uint(out, (c.r * 100u + 127u) / 255u);
        out.push_back(';');
        append_uint(out, (c.g * 100u + 127u) / 255u);
        out.push_back(';');
        append_uint(out, (c.b * 100u + 127u) / 255u);
    }

    const std::size_t nc = img.palette.size();
    std::vector<std::uint8_t> masks(nc * w, 0);
    std::vector<std::size_t> lo(nc, w);
    std::vector<std::ptrdiff_t> hi(nc, -1);
    std::vector<std::uint8_t> used;
    used.reserve(nc);

    for (std::size_t y0 = 0; y0 < h; y0 += 6) {
        used.clear();
        for (std::size_t dy = 0; dy < 6 && y0 + dy < h; ++dy) {
            const std::size_t row = (y0 + dy) * w;
            for (std::size_t x = 0; x < w; ++x) {
                const std::uint8_t k = img.index[row + x];
                if (k == kTransparent) continue;
                if (hi[k] < 0) used.push_back(k);
                masks[k * w + x] |= static_cast<std::uint8_t>(1u << dy);
                lo[k] = std::min(lo[k], x);
                hi[k] = std::max(hi[k], static_cast<std::ptrdiff_t>(x));
            }
        }
        const bool last_band = y0 + 6 >= h;
        for (std::size_t u = 0; u < used.size(); ++u) {
            const std::uint8_t k = used[u];
            out.push_back('#');
            append_uint(out, k);
            append_run(out, '?', lo[k]);
            char prev = 0;
            std::size_t run = 0;
            for (std::size_t x = lo[k]; x <= static_cast<std::size_t>(hi[k]); ++x) {
                std::uint8_t& m = masks[k * w + x];
                const char ch = static_cast<char>(63 + m);
                m = 0;
                if (ch == prev) {
                    ++run;
                } else {
                    append_run(out, prev, run);
                    prev = ch;
                    run = 1;
                }
            }
            append_run(out, prev, run);
            lo[k] = w;
            hi[k] = -1;
            if (u + 1 < used.size()) out.push_back('$');
        }
        if (!last_band) out.push_back('-');
    }
    out += "\x1b\\";
    return out;
}

constexpr std::string encode(const Canvas& c) { return encode(quantize(c)); }

// --- Compile-time icons -------------------------------------------------------

enum class Icon : std::uint8_t { Folder, File, Rename, Flatten, Exclude, Error, Success, Rocket, Font, Count };

inline constexpr int kIconSize = 16;

constexpr Canvas draw_icon(Icon type) {
    Canvas c(kIconSize, kIconSize);
    switch (type) {
        case Icon::Folder:
            c.fill_rect(2, 5, 12, 8, {255, 180, 0});
            c.fill_rect(2, 4, 5, 1, {255, 210, 60});
            c.fill_rect(2, 5, 12, 1, {255, 215, 90});
            break;
        case Icon::File:
            c.fill_rect(3, 2, 10, 12, {220, 240, 255});
            c.fill_rect(9, 2, 4, 4, {0, 200, 255});
            for (int y = 8; y < 13; y += 2) c.fill_rect(5, y, 6, 1, {150, 170, 200});
            break;
        case Icon::Rename:
            for (int i = 0; i < 10; ++i) {
                c.put(3 + i, 12 - i, {0, 229, 255});
                c.put(4 + i, 12 - i, {0, 180, 255});
            }
            c.fill_rect(2, 13, 3, 1, {255, 215, 0});
            break;
        case Icon::Flatten:
            for (int y = 2; y < 8; ++y) c.put(8 - (y - 2), y, {255, 214, 0});
            for (int x = 4; x < 12; ++x) c.put(x, 8, {255, 235, 59});
            for (int y = 8; y < 14; ++y) c.put(10 - (y - 8), y, {255, 214, 0});
            break;
        case Icon::Exclude:
            for (int i = 2; i < 14; ++i) {
                c.put(i, i, {255, 82, 82});
                c.put(15 - i, i, {255, 82, 82});
            }
            break;
        case Icon::Error:
            for (int y = 2; y < 14; ++y)
                for (int x = 2; x < 14; ++x)
                    if ((x - 8) * (x - 8) + (y - 8) * (y - 8) <= 30) c.put(x, y, {255, 23, 68});
            for (int y = 5; y < 11; ++y) {
                c.put(y, y, {255, 255, 255});
                c.put(15 - y, y, {255, 255, 255});
            }
            break;
        case Icon::Success:
            for (int i = 0; i < 5; ++i) c.put(3 + i, 7 + i, {0, 230, 118});
            for (int i = 0; i < 8; ++i) c.put(7 + i, 11 - i, {0, 230, 118});
            break;
        case Icon::Rocket:
            for (int i = 0; i < 10; ++i) {
                c.put(3 + i, 13 - i, {255, 0, 128});
                c.put(4 + i, 13 - i, {0, 240, 255});
            }
            c.put(2, 14, {255, 170, 0});
            break;
        case Icon::Font:
            c.fill_rect(4, 3, 1, 10, {187, 134, 252});
            c.fill_rect(4, 3, 8, 1, {187, 134, 252});
            c.fill_rect(4, 7, 6, 1, {187, 134, 252});
            break;
        case Icon::Count: break;
    }
    return c;
}

constexpr std::string encode_icon(Icon i) { return encode(draw_icon(i)); }

// The constant-evaluated std::string is copied into static storage sized at compile time.
template <Icon I>
inline constexpr auto icon_bytes = [] {
    constexpr std::size_t n = encode_icon(I).size();
    std::array<char, n> out{};
    const std::string s = encode_icon(I);
    std::ranges::copy(s, out.begin());
    return out;
}();

template <std::size_t... I>
consteval auto make_icon_table(std::index_sequence<I...>) {
    return std::array<std::string_view, sizeof...(I)>{
        std::string_view{icon_bytes<static_cast<Icon>(I)>.data(), icon_bytes<static_cast<Icon>(I)>.size()}...};
}

inline constexpr auto icon_table = make_icon_table(std::make_index_sequence<std::to_underlying(Icon::Count)>{});

constexpr std::string_view icon(Icon i) { return icon_table[std::to_underlying(i)]; }

static_assert(icon(Icon::Folder).starts_with("\x1bP0;1q\"1;1;16;16#0;2;"));
static_assert(icon(Icon::Folder).ends_with("\x1b\\"));
static_assert(std::ranges::all_of(icon_table, [](std::string_view s) { return s.size() > 32 && s.size() < 1024; }));

// --- Fonts for the graphical UI -------------------------------------------------

struct UiFonts {
    ttf::Font regular{fira_code_bytes, 400.0f};
    ttf::Font bold{fira_code_bytes, 700.0f};
};

inline const UiFonts& ui_fonts() {
    static const UiFonts fonts;
    return fonts;
}

// Neon gradient header banner with Fira Code title text.
inline Canvas render_banner(int width, int height, std::string_view title, std::string_view subtitle) {
    Canvas c(width, height);
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    c.fill_rounded_rect(0.0f, 0.0f, w, h, h * 0.22f, palette::neon_cyan);
    c.fill_rounded_rect(2.0f, 2.0f, w - 4.0f, h - 4.0f, h * 0.20f, [&](int x, int y) {
        const float tx = static_cast<float>(x) / w, ty = static_cast<float>(y) / h;
        const Rgb base = gradient(tx, Rgb{0, 150, 190}, Rgb{120, 20, 210}, Rgb{200, 0, 120});
        return scale(base, 1.05f - 0.35f * ty);
    });

    const auto& fonts = ui_fonts();
    ttf::GlyphCache title_gc(fonts.bold, h * 0.40f);
    ttf::GlyphCache sub_gc(fonts.regular, h * 0.24f);
    const int title_adv = static_cast<int>(std::lround(title_gc.advance()));
    const int sub_adv = static_cast<int>(std::lround(sub_gc.advance()));
    const auto title_len = static_cast<int>(ttf::utf8_length(title));
    const auto sub_len = static_cast<int>(ttf::utf8_length(subtitle));
    const int title_x = (width - title_adv * title_len) / 2;
    const int sub_x = (width - sub_adv * sub_len) / 2;
    const int title_base = static_cast<int>(h * 0.52f);
    const int sub_base = static_cast<int>(h * 0.82f);

    c.draw_text(title_gc, title_x + 2, title_base + 2, title, {10, 0, 30}, title_adv);
    c.draw_text(title_gc, title_x, title_base, title, {255, 255, 255}, title_adv);
    c.draw_text(sub_gc, sub_x, sub_base, subtitle, {200, 255, 250}, sub_adv);
    return c;
}

} // namespace fsturbo::sixel

#endif // SIXEL_RENDERER_HPP
