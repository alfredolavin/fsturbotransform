#ifndef TTF_FONT_HPP
#define TTF_FONT_HPP

// Minimal TrueType engine for the embedded Fira Code font:
//   * table directory, cmap (formats 4 and 12), hmtx, loca/glyf (simple + composite)
//   * OpenType variations (fvar/avar/gvar with IUP), so the variable font's
//     default Light master can be instanced at Regular/Bold weights
//   * exact-area anti-aliased scanline rasterizer (signed-area accumulation)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fsturbo::ttf {

// Bounds-checked big-endian reader: malformed offsets read as zero instead of faulting.
class Reader {
    std::span<const unsigned char> d_;

public:
    constexpr Reader() = default;
    constexpr explicit Reader(std::span<const unsigned char> d) : d_(d) {}

    constexpr std::uint8_t u8(std::size_t o) const { return o < d_.size() ? d_[o] : 0; }
    constexpr std::int8_t i8(std::size_t o) const { return static_cast<std::int8_t>(u8(o)); }
    constexpr std::uint16_t u16(std::size_t o) const { return static_cast<std::uint16_t>(u8(o) << 8 | u8(o + 1)); }
    constexpr std::int16_t i16(std::size_t o) const { return static_cast<std::int16_t>(u16(o)); }
    constexpr std::uint32_t u32(std::size_t o) const { return std::uint32_t{u16(o)} << 16 | u16(o + 2); }
    constexpr std::int32_t i32(std::size_t o) const { return static_cast<std::int32_t>(u32(o)); }
    constexpr float f2dot14(std::size_t o) const { return static_cast<float>(i16(o)) / 16384.0f; }
    constexpr float fixed(std::size_t o) const { return static_cast<float>(i32(o)) / 65536.0f; }
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct OutlinePoint {
    float x = 0.0f;
    float y = 0.0f;
    bool on_curve = true;
};

struct Outline {
    std::vector<OutlinePoint> points;
    std::vector<std::uint16_t> contour_ends; // inclusive index of each contour's last point
    float advance = 0.0f;                    // font units, variations applied
};

// Coverage bitmap of one rasterized glyph, positioned relative to the pen origin on the baseline.
struct GlyphBitmap {
    int width = 0;
    int height = 0;
    int left = 0; // pixels from pen x to bitmap column 0
    int top = 0;  // pixels from baseline up to bitmap row 0
    std::vector<std::uint8_t> coverage;
};

// Signed-area accumulation rasterizer: every edge deposits its exact coverage delta,
// a single prefix sum then yields non-zero-winding anti-aliased coverage.
class Rasterizer {
    int w_;
    int h_;
    std::vector<float> acc_;

    void add(std::ptrdiff_t idx, float v) {
        if (idx >= 0 && static_cast<std::size_t>(idx) < acc_.size()) acc_[static_cast<std::size_t>(idx)] += v;
    }

public:
    Rasterizer(int w, int h)
        : w_(w), h_(h), acc_(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) + static_cast<std::size_t>(w) + 2, 0.0f) {}

    void line(Vec2 p0, Vec2 p1) {
        if (p0.y == p1.y) return;
        float dir = 1.0f;
        if (p0.y > p1.y) {
            std::swap(p0, p1);
            dir = -1.0f;
        }
        const float dxdy = (p1.x - p0.x) / (p1.y - p0.y);
        float x = p0.x;
        int y = static_cast<int>(p0.y);
        if (p0.y < 0.0f) {
            x -= p0.y * dxdy;
            y = 0;
        }
        const int y_end = std::min(h_, static_cast<int>(std::ceil(p1.y)));
        for (; y < y_end; ++y) {
            const std::ptrdiff_t row = static_cast<std::ptrdiff_t>(y) * w_;
            const float dy = std::min(static_cast<float>(y + 1), p1.y) - std::max(static_cast<float>(y), p0.y);
            const float x_next = x + dxdy * dy;
            const float d = dy * dir;
            const float x0 = std::min(x, x_next);
            const float x1 = std::max(x, x_next);
            const float x0_floor = std::floor(x0);
            const int x0i = static_cast<int>(x0_floor);
            const float x1_ceil = std::ceil(x1);
            const int x1i = static_cast<int>(x1_ceil);
            if (x1i <= x0i + 1) {
                const float xmf = 0.5f * (x + x_next) - x0_floor;
                add(row + x0i, d - d * xmf);
                add(row + x0i + 1, d * xmf);
            } else {
                const float s = 1.0f / (x1 - x0);
                const float x0f = x0 - x0_floor;
                const float a0 = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
                const float x1f = x1 - x1_ceil + 1.0f;
                const float am = 0.5f * s * x1f * x1f;
                add(row + x0i, d * a0);
                if (x1i == x0i + 2) {
                    add(row + x0i + 1, d * (1.0f - a0 - am));
                } else {
                    const float a1 = s * (1.5f - x0f);
                    add(row + x0i + 1, d * (a1 - a0));
                    for (int xi = x0i + 2; xi < x1i - 1; ++xi) add(row + xi, d * s);
                    const float a2 = a1 + static_cast<float>(x1i - x0i - 3) * s;
                    add(row + x1i - 1, d * (1.0f - a2 - am));
                }
                add(row + x1i, d * am);
            }
            x = x_next;
        }
    }

    void quad(Vec2 p0, Vec2 p1, Vec2 p2) {
        const float dev_x = p0.x - 2.0f * p1.x + p2.x;
        const float dev_y = p0.y - 2.0f * p1.y + p2.y;
        const float dev_sq = dev_x * dev_x + dev_y * dev_y;
        if (dev_sq < 0.333f) {
            line(p0, p2);
            return;
        }
        const int n = 1 + static_cast<int>(std::floor(std::sqrt(std::sqrt(3.0f * dev_sq))));
        const float step = 1.0f / static_cast<float>(n);
        Vec2 p = p0;
        float t = 0.0f;
        for (int i = 0; i < n - 1; ++i) {
            t += step;
            const float mt = 1.0f - t;
            const Vec2 next{mt * mt * p0.x + 2.0f * mt * t * p1.x + t * t * p2.x,
                            mt * mt * p0.y + 2.0f * mt * t * p1.y + t * t * p2.y};
            line(p, next);
            p = next;
        }
        line(p, p2);
    }

    std::vector<std::uint8_t> coverage() const {
        std::vector<std::uint8_t> out(static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_));
        float a = 0.0f;
        for (std::size_t i = 0; i < out.size(); ++i) {
            a += acc_[i];
            out[i] = static_cast<std::uint8_t>(std::min(std::fabs(a), 1.0f) * 255.0f + 0.5f);
        }
        return out;
    }
};

class Font {
    Reader r_;
    bool valid_ = false;
    std::uint32_t glyf_ = 0, loca_ = 0, hmtx_ = 0, cmap_sub_ = 0, gvar_ = 0;
    std::uint16_t cmap_format_ = 0;
    std::uint16_t num_glyphs_ = 0;
    std::uint16_t num_hmetrics_ = 0;
    std::uint16_t units_per_em_ = 1000;
    bool long_loca_ = false;
    float ascender_ = 0.0f;
    float descender_ = 0.0f;
    std::vector<float> coords_; // normalized variation coordinates, one per fvar axis

    static constexpr std::uint32_t tag(std::string_view t) {
        return std::uint32_t(static_cast<unsigned char>(t[0])) << 24 | std::uint32_t(static_cast<unsigned char>(t[1])) << 16 |
               std::uint32_t(static_cast<unsigned char>(t[2])) << 8 | std::uint32_t(static_cast<unsigned char>(t[3]));
    }

    std::uint32_t find_table(std::string_view name) const {
        const std::uint16_t n = r_.u16(4);
        for (std::uint16_t i = 0; i < n; ++i) {
            const std::size_t rec = 12 + 16 * std::size_t{i};
            if (r_.u32(rec) == tag(name)) return r_.u32(rec + 8);
        }
        return 0;
    }

    float advance_units(std::uint16_t gid) const {
        if (num_hmetrics_ == 0) return 0.0f;
        const std::uint16_t i = std::min<std::uint16_t>(gid, num_hmetrics_ - 1);
        return r_.u16(hmtx_ + 4 * std::size_t{i});
    }

    float lsb_units(std::uint16_t gid) const {
        if (gid < num_hmetrics_) return r_.i16(hmtx_ + 4 * std::size_t{gid} + 2);
        return r_.i16(hmtx_ + 4 * std::size_t{num_hmetrics_} + 2 * std::size_t(gid - num_hmetrics_));
    }

    std::pair<std::uint32_t, std::uint32_t> glyph_range(std::uint16_t gid) const {
        if (long_loca_) return {r_.u32(loca_ + 4 * std::size_t{gid}), r_.u32(loca_ + 4 * std::size_t{gid} + 4)};
        return {2u * r_.u16(loca_ + 2 * std::size_t{gid}), 2u * r_.u16(loca_ + 2 * std::size_t{gid} + 2)};
    }

    // --- OpenType variations -------------------------------------------------

    void init_variations(float weight) {
        const std::uint32_t fvar = find_table("fvar");
        if (fvar == 0) return;
        const std::uint16_t axes_off = r_.u16(fvar + 4);
        const std::uint16_t axis_count = r_.u16(fvar + 8);
        const std::uint16_t axis_size = r_.u16(fvar + 10);
        coords_.assign(axis_count, 0.0f);
        for (std::uint16_t i = 0; i < axis_count; ++i) {
            const std::size_t a = fvar + axes_off + std::size_t{i} * axis_size;
            if (r_.u32(a) != tag("wght")) continue;
            const float lo = r_.fixed(a + 4), def = r_.fixed(a + 8), hi = r_.fixed(a + 12);
            const float v = std::clamp(weight, lo, hi);
            float n = 0.0f;
            if (v < def && def > lo) n = (v - def) / (def - lo);
            else if (v > def && hi > def) n = (v - def) / (hi - def);
            coords_[i] = n;
        }
        // avar: piecewise-linear remapping of the normalized coordinates
        if (const std::uint32_t avar = find_table("avar"); avar != 0) {
            std::size_t p = avar + 8;
            const std::uint16_t n_axes = std::min<std::uint16_t>(r_.u16(avar + 6), axis_count);
            for (std::uint16_t i = 0; i < n_axes; ++i) {
                const std::uint16_t count = r_.u16(p);
                const std::size_t maps = p + 2;
                float& v = coords_[i];
                for (std::uint16_t j = 1; j < count; ++j) {
                    const float f0 = r_.f2dot14(maps + 4 * (j - 1)), t0 = r_.f2dot14(maps + 4 * (j - 1) + 2);
                    const float f1 = r_.f2dot14(maps + 4 * j), t1 = r_.f2dot14(maps + 4 * j + 2);
                    if (v < f1) {
                        if (f1 > f0) v = t0 + (v - f0) * (t1 - t0) / (f1 - f0);
                        break;
                    }
                    if (j == count - 1) v = t1;
                }
                p = maps + 4 * std::size_t{count};
            }
        }
        if (std::ranges::any_of(coords_, [](float c) { return c != 0.0f; })) gvar_ = find_table("gvar");
    }

    float tuple_scalar(std::size_t peak, std::size_t start, std::size_t end, bool intermediate) const {
        float scalar = 1.0f;
        for (std::size_t i = 0; i < coords_.size(); ++i) {
            const float pk = r_.f2dot14(peak + 2 * i);
            if (pk == 0.0f) continue;
            const float v = coords_[i];
            if (v == 0.0f) return 0.0f;
            if (intermediate) {
                const float s = r_.f2dot14(start + 2 * i), e = r_.f2dot14(end + 2 * i);
                if (s > pk || pk > e || (s < 0.0f && e > 0.0f)) continue;
                if (v < s || v > e) return 0.0f;
                if (v < pk) scalar *= (v - s) / (pk - s);
                else if (v > pk) scalar *= (e - v) / (e - pk);
            } else {
                if (v < std::min(0.0f, pk) || v > std::max(0.0f, pk)) return 0.0f;
                scalar *= v / pk;
            }
        }
        return scalar;
    }

    // Returns false for the "all points" encoding.
    bool read_points(std::size_t& p, std::vector<std::uint16_t>& out) const {
        out.clear();
        std::uint16_t count = r_.u8(p++);
        if (count == 0) return false;
        if (count & 0x80) count = static_cast<std::uint16_t>((count & 0x7F) << 8 | r_.u8(p++));
        std::uint16_t last = 0;
        while (out.size() < count) {
            const std::uint8_t ctrl = r_.u8(p++);
            const int run = (ctrl & 0x7F) + 1;
            for (int i = 0; i < run && out.size() < count; ++i) {
                if (ctrl & 0x80) {
                    last = static_cast<std::uint16_t>(last + r_.u16(p));
                    p += 2;
                } else {
                    last = static_cast<std::uint16_t>(last + r_.u8(p++));
                }
                out.push_back(last);
            }
        }
        return true;
    }

    void read_deltas(std::size_t& p, std::size_t n, std::vector<float>& out) const {
        out.clear();
        while (out.size() < n) {
            const std::uint8_t ctrl = r_.u8(p++);
            const int run = (ctrl & 0x3F) + 1;
            for (int i = 0; i < run && out.size() < n; ++i) {
                switch (ctrl & 0xC0) {
                    case 0x80: out.push_back(0.0f); break;
                    case 0xC0: out.push_back(static_cast<float>(r_.i32(p))); p += 4; break;
                    case 0x40: out.push_back(r_.i16(p)); p += 2; break;
                    default: out.push_back(r_.i8(p)); p += 1; break;
                }
            }
        }
    }

    // Interpolate Untouched Points (gvar IUP) for one axis of one contour segment.
    static float iup(float v, float c1, float c2, float d1, float d2) {
        if (c1 == c2) return d1 == d2 ? d1 : 0.0f;
        if (c1 > c2) {
            std::swap(c1, c2);
            std::swap(d1, d2);
        }
        if (v <= c1) return d1;
        if (v >= c2) return d2;
        return d1 + (v - c1) * (d2 - d1) / (c2 - c1);
    }

    // Applies gvar deltas to `pts` (outline points + 4 phantom points).
    // `ends` is empty for composite glyphs (their points are component offsets: no IUP).
    void apply_variations(std::uint16_t gid, std::span<Vec2> pts, std::span<const std::uint16_t> ends) const {
        if (gvar_ == 0) return;
        const std::uint16_t axis_count = r_.u16(gvar_ + 4);
        if (axis_count != coords_.size() || gid >= r_.u16(gvar_ + 12)) return;
        const std::size_t shared_tuples = gvar_ + r_.u32(gvar_ + 8);
        const bool long_offsets = r_.u16(gvar_ + 14) & 1;
        const std::size_t array_off = gvar_ + r_.u32(gvar_ + 16);
        auto offset = [&](std::size_t g) -> std::size_t {
            return long_offsets ? r_.u32(gvar_ + 20 + 4 * g) : 2u * r_.u16(gvar_ + 20 + 2 * g);
        };
        const std::size_t off0 = offset(gid), off1 = offset(std::size_t{gid} + 1);
        if (off1 <= off0) return;

        const std::size_t base = array_off + off0;
        const std::uint16_t tuple_word = r_.u16(base);
        const int tuple_count = tuple_word & 0x0FFF;
        std::size_t data = base + r_.u16(base + 2);
        std::size_t header = base + 4;

        std::vector<std::uint16_t> shared_pts, private_pts;
        bool shared_explicit = false;
        if (tuple_word & 0x8000) shared_explicit = read_points(data, shared_pts);

        const std::vector<Vec2> orig(pts.begin(), pts.end());
        std::vector<Vec2> tuple_delta(pts.size());
        std::vector<bool> touched(pts.size());
        std::vector<float> dx, dy;
        std::vector<std::size_t> ring;

        for (int t = 0; t < tuple_count; ++t) {
            const std::uint16_t size = r_.u16(header);
            const std::uint16_t index = r_.u16(header + 2);
            header += 4;
            std::size_t peak = shared_tuples + std::size_t{2} * axis_count * (index & 0x0FFF);
            if (index & 0x8000) {
                peak = header;
                header += std::size_t{2} * axis_count;
            }
            std::size_t start = 0, end = 0;
            const bool intermediate = index & 0x4000;
            if (intermediate) {
                start = header;
                end = header + std::size_t{2} * axis_count;
                header += std::size_t{4} * axis_count;
            }
            std::size_t p = data;
            data += size;

            const float scalar = tuple_scalar(peak, start, end, intermediate);
            if (scalar == 0.0f) continue;

            bool explicit_pts = shared_explicit;
            const std::vector<std::uint16_t>* point_ids = &shared_pts;
            if (index & 0x2000) {
                explicit_pts = read_points(p, private_pts);
                point_ids = &private_pts;
            }
            const std::size_t n = explicit_pts ? point_ids->size() : pts.size();
            read_deltas(p, n, dx);
            read_deltas(p, n, dy);

            if (!explicit_pts) {
                for (std::size_t i = 0; i < pts.size(); ++i) {
                    pts[i].x += scalar * dx[i];
                    pts[i].y += scalar * dy[i];
                }
                continue;
            }

            std::ranges::fill(tuple_delta, Vec2{});
            std::ranges::fill(touched, false);
            for (std::size_t k = 0; k < n; ++k) {
                const std::size_t id = (*point_ids)[k];
                if (id >= pts.size()) continue;
                tuple_delta[id] = {dx[k], dy[k]};
                touched[id] = true;
            }

            std::size_t first = 0;
            for (const std::uint16_t last_u : ends) {
                const std::size_t last = last_u;
                if (last >= pts.size() || last < first) break;
                ring.clear();
                for (std::size_t i = first; i <= last; ++i)
                    if (touched[i]) ring.push_back(i);
                if (ring.size() == 1) {
                    for (std::size_t i = first; i <= last; ++i)
                        if (!touched[i]) tuple_delta[i] = tuple_delta[ring[0]];
                } else if (ring.size() > 1) {
                    for (std::size_t k = 0; k < ring.size(); ++k) {
                        const std::size_t a = ring[k], b = ring[(k + 1) % ring.size()];
                        for (std::size_t i = (a == last ? first : a + 1); i != b; i = (i == last ? first : i + 1)) {
                            tuple_delta[i].x = iup(orig[i].x, orig[a].x, orig[b].x, tuple_delta[a].x, tuple_delta[b].x);
                            tuple_delta[i].y = iup(orig[i].y, orig[a].y, orig[b].y, tuple_delta[a].y, tuple_delta[b].y);
                        }
                    }
                }
                first = last + 1;
            }
            for (std::size_t i = 0; i < pts.size(); ++i) {
                pts[i].x += scalar * tuple_delta[i].x;
                pts[i].y += scalar * tuple_delta[i].y;
            }
        }
    }

    // --- glyf ----------------------------------------------------------------

    void load_glyph(std::uint16_t gid, Outline& out, int depth) const {
        if (depth > 8 || gid >= num_glyphs_) return;
        const auto [start, end] = glyph_range(gid);
        const float adv = advance_units(gid);
        const float lsb = lsb_units(gid);

        if (end <= start) { // blank glyph (space): only the phantom points vary
            std::array<Vec2, 4> phantom{{{0.0f, 0.0f}, {adv, 0.0f}, {}, {}}};
            apply_variations(gid, phantom, {});
            out.advance = phantom[1].x - phantom[0].x;
            return;
        }

        const std::size_t g = glyf_ + start;
        const std::int16_t n_contours = r_.i16(g);
        const float x_min = r_.i16(g + 2);
        const Vec2 pp1{x_min - lsb, 0.0f};
        const Vec2 pp2{pp1.x + adv, 0.0f};

        if (n_contours >= 0) {
            std::vector<std::uint16_t> ends(static_cast<std::size_t>(n_contours));
            std::size_t p = g + 10;
            for (auto& e : ends) {
                e = r_.u16(p);
                p += 2;
            }
            const std::size_t n_pts = ends.empty() ? 0 : std::size_t{ends.back()} + 1;
            p += 2 + r_.u16(p); // skip hinting instructions

            std::vector<std::uint8_t> flags;
            flags.reserve(n_pts);
            while (flags.size() < n_pts) {
                const std::uint8_t f = r_.u8(p++);
                flags.push_back(f);
                if (f & 0x08) {
                    for (int rep = r_.u8(p++); rep > 0 && flags.size() < n_pts; --rep) flags.push_back(f);
                }
            }

            std::vector<Vec2> pts(n_pts + 4);
            auto decode_axis = [&](std::uint8_t short_bit, std::uint8_t same_bit, auto member) {
                int v = 0;
                for (std::size_t i = 0; i < n_pts; ++i) {
                    const std::uint8_t f = flags[i];
                    if (f & short_bit) {
                        const int d = r_.u8(p++);
                        v += (f & same_bit) ? d : -d;
                    } else if (!(f & same_bit)) {
                        v += r_.i16(p);
                        p += 2;
                    }
                    pts[i].*member = static_cast<float>(v);
                }
            };
            decode_axis(0x02, 0x10, &Vec2::x);
            decode_axis(0x04, 0x20, &Vec2::y);
            pts[n_pts] = pp1;
            pts[n_pts + 1] = pp2;

            apply_variations(gid, pts, ends);

            const auto base = static_cast<std::uint16_t>(out.points.size());
            for (std::size_t i = 0; i < n_pts; ++i) out.points.push_back({pts[i].x, pts[i].y, (flags[i] & 1) != 0});
            for (const auto e : ends) out.contour_ends.push_back(static_cast<std::uint16_t>(base + e));
            out.advance = pts[n_pts + 1].x - pts[n_pts].x;
            return;
        }

        // Composite glyph
        struct Component {
            std::uint16_t gid;
            float a, b, c, d;
        };
        std::vector<Component> comps;
        std::vector<Vec2> offsets;
        std::size_t p = g + 10;
        for (;;) {
            const std::uint16_t flags = r_.u16(p);
            const std::uint16_t cg = r_.u16(p + 2);
            p += 4;
            float ox = 0.0f, oy = 0.0f;
            if (flags & 0x0001) {
                ox = r_.i16(p);
                oy = r_.i16(p + 2);
                p += 4;
            } else {
                ox = r_.i8(p);
                oy = r_.i8(p + 1);
                p += 2;
            }
            if (!(flags & 0x0002)) ox = oy = 0.0f; // point-matched anchoring is not supported
            Component comp{cg, 1.0f, 0.0f, 0.0f, 1.0f};
            if (flags & 0x0008) {
                comp.a = comp.d = r_.f2dot14(p);
                p += 2;
            } else if (flags & 0x0040) {
                comp.a = r_.f2dot14(p);
                comp.d = r_.f2dot14(p + 2);
                p += 4;
            } else if (flags & 0x0080) {
                comp.a = r_.f2dot14(p);
                comp.b = r_.f2dot14(p + 2);
                comp.c = r_.f2dot14(p + 4);
                comp.d = r_.f2dot14(p + 6);
                p += 8;
            }
            comps.push_back(comp);
            offsets.push_back({ox, oy});
            if (!(flags & 0x0020)) break;
        }
        offsets.push_back(pp1);
        offsets.push_back(pp2);
        offsets.emplace_back();
        offsets.emplace_back();
        apply_variations(gid, offsets, {});

        for (std::size_t i = 0; i < comps.size(); ++i) {
            const auto& comp = comps[i];
            Outline sub;
            load_glyph(comp.gid, sub, depth + 1);
            const auto base = static_cast<std::uint16_t>(out.points.size());
            for (const auto& pt : sub.points) {
                out.points.push_back({comp.a * pt.x + comp.c * pt.y + offsets[i].x,
                                      comp.b * pt.x + comp.d * pt.y + offsets[i].y, pt.on_curve});
            }
            for (const auto e : sub.contour_ends) out.contour_ends.push_back(static_cast<std::uint16_t>(base + e));
        }
        out.advance = offsets[comps.size() + 1].x - offsets[comps.size()].x;
    }

public:
    explicit Font(std::span<const unsigned char> data, float weight = 400.0f) : r_(data) {
        const std::uint32_t head = find_table("head");
        const std::uint32_t maxp = find_table("maxp");
        const std::uint32_t hhea = find_table("hhea");
        glyf_ = find_table("glyf");
        loca_ = find_table("loca");
        hmtx_ = find_table("hmtx");
        const std::uint32_t cmap = find_table("cmap");
        if (!head || !maxp || !hhea || !glyf_ || !loca_ || !hmtx_ || !cmap) return;

        units_per_em_ = std::max<std::uint16_t>(r_.u16(head + 18), 16);
        long_loca_ = r_.i16(head + 50) != 0;
        num_glyphs_ = r_.u16(maxp + 4);
        ascender_ = r_.i16(hhea + 4);
        descender_ = r_.i16(hhea + 6);
        num_hmetrics_ = r_.u16(hhea + 34);

        // Prefer the full-Unicode format 12 subtable, fall back to BMP format 4.
        int best = -1;
        for (std::uint16_t i = 0, n = r_.u16(cmap + 2); i < n; ++i) {
            const std::size_t rec = cmap + 4 + 8 * std::size_t{i};
            const std::uint16_t platform = r_.u16(rec), encoding = r_.u16(rec + 2);
            const std::uint32_t sub = cmap + r_.u32(rec + 4);
            const std::uint16_t format = r_.u16(sub);
            const bool unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
            const int score = !unicode ? -1 : format == 12 ? 2 : format == 4 ? 1 : -1;
            if (score > best) {
                best = score;
                cmap_sub_ = sub;
                cmap_format_ = format;
            }
        }
        if (best < 0) return;
        init_variations(weight);
        valid_ = true;
    }

    Font(const Font&) = default;
    Font& operator=(const Font&) = default;

    bool valid() const { return valid_; }
    float units_per_em() const { return units_per_em_; }
    float ascender() const { return ascender_; }
    float descender() const { return descender_; }

    std::uint16_t glyph_index(char32_t cp) const {
        if (!valid_) return 0;
        if (cmap_format_ == 12) {
            std::uint32_t lo = 0, hi = r_.u32(cmap_sub_ + 12);
            while (lo < hi) {
                const std::uint32_t mid = (lo + hi) / 2;
                const std::size_t grp = cmap_sub_ + 16 + 12 * std::size_t{mid};
                if (cp < r_.u32(grp)) hi = mid;
                else if (cp > r_.u32(grp + 4)) lo = mid + 1;
                else return static_cast<std::uint16_t>(r_.u32(grp + 8) + (cp - r_.u32(grp)));
            }
            return 0;
        }
        if (cp > 0xFFFF) return 0;
        const std::size_t seg_x2 = r_.u16(cmap_sub_ + 6);
        const std::size_t ends = cmap_sub_ + 14, starts = ends + seg_x2 + 2;
        const std::size_t deltas = starts + seg_x2, ranges = deltas + seg_x2;
        std::size_t lo = 0, hi = seg_x2 / 2;
        while (lo < hi) {
            const std::size_t mid = (lo + hi) / 2;
            if (r_.u16(ends + 2 * mid) < cp) lo = mid + 1;
            else hi = mid;
        }
        if (lo >= seg_x2 / 2) return 0;
        const std::uint16_t first = r_.u16(starts + 2 * lo);
        if (cp < first) return 0;
        const std::uint16_t delta = r_.u16(deltas + 2 * lo);
        const std::uint16_t range = r_.u16(ranges + 2 * lo);
        if (range == 0) return static_cast<std::uint16_t>(cp + delta);
        const std::uint16_t g = r_.u16(ranges + 2 * lo + range + 2 * (cp - first));
        return g == 0 ? 0 : static_cast<std::uint16_t>(g + delta);
    }

    Outline outline(std::uint16_t gid) const {
        Outline out;
        if (valid_) load_glyph(gid, out, 0);
        return out;
    }

    // Rasterizes a glyph outline at `px_per_em` pixels per em.
    GlyphBitmap rasterize(std::uint16_t gid, float px_per_em) const {
        const Outline o = outline(gid);
        GlyphBitmap bmp;
        if (o.points.empty()) return bmp;
        const float scale = px_per_em / units_per_em_;
        float min_x = o.points[0].x, max_x = min_x, min_y = o.points[0].y, max_y = min_y;
        for (const auto& pt : o.points) {
            min_x = std::min(min_x, pt.x);
            max_x = std::max(max_x, pt.x);
            min_y = std::min(min_y, pt.y);
            max_y = std::max(max_y, pt.y);
        }
        bmp.left = static_cast<int>(std::floor(min_x * scale)) - 1;
        bmp.top = static_cast<int>(std::ceil(max_y * scale)) + 1;
        bmp.width = static_cast<int>(std::ceil(max_x * scale)) + 1 - bmp.left;
        bmp.height = bmp.top - (static_cast<int>(std::floor(min_y * scale)) - 1);
        if (bmp.width <= 0 || bmp.height <= 0 || bmp.width > 4096 || bmp.height > 4096) return GlyphBitmap{};

        Rasterizer ras(bmp.width, bmp.height);
        auto to_px = [&](const OutlinePoint& pt) {
            return Vec2{pt.x * scale - static_cast<float>(bmp.left), static_cast<float>(bmp.top) - pt.y * scale};
        };
        auto mid = [](Vec2 a, Vec2 b) { return Vec2{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}; };

        std::size_t first = 0;
        for (const std::uint16_t last_u : o.contour_ends) {
            const std::size_t last = last_u;
            if (last >= o.points.size() || last < first) break;
            const std::size_t n = last - first + 1;
            if (n >= 2) {
                // Start on an on-curve point; synthesize one if the contour is all control points.
                std::size_t begin = first, count = n;
                Vec2 start;
                if (o.points[first].on_curve) {
                    start = to_px(o.points[first]);
                    begin = first + 1;
                    count = n - 1;
                } else if (o.points[last].on_curve) {
                    start = to_px(o.points[last]);
                    count = n - 1;
                } else {
                    start = mid(to_px(o.points[first]), to_px(o.points[last]));
                }
                Vec2 cur = start, ctrl{};
                bool has_ctrl = false;
                for (std::size_t k = 0; k < count; ++k) {
                    const auto& op = o.points[begin + k];
                    const Vec2 q = to_px(op);
                    if (op.on_curve) {
                        if (has_ctrl) ras.quad(cur, ctrl, q);
                        else ras.line(cur, q);
                        cur = q;
                        has_ctrl = false;
                    } else {
                        if (has_ctrl) {
                            const Vec2 m = mid(ctrl, q);
                            ras.quad(cur, ctrl, m);
                            cur = m;
                        }
                        ctrl = q;
                        has_ctrl = true;
                    }
                }
                if (has_ctrl) ras.quad(cur, ctrl, start);
                else ras.line(cur, start);
            }
            first = last + 1;
        }
        bmp.coverage = ras.coverage();
        return bmp;
    }
};

// Decodes UTF-8 into code points (invalid sequences become U+FFFD).
template <typename F>
constexpr void for_each_codepoint(std::string_view s, F&& fn) {
    for (std::size_t i = 0; i < s.size();) {
        const auto b0 = static_cast<unsigned char>(s[i]);
        char32_t cp = 0xFFFD;
        std::size_t len = 1;
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 >> 5) == 0x6 && i + 1 < s.size()) {
            cp = (char32_t(b0 & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
            len = 2;
        } else if ((b0 >> 4) == 0xE && i + 2 < s.size()) {
            cp = (char32_t(b0 & 0x0F) << 12) | (char32_t(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            len = 3;
        } else if ((b0 >> 3) == 0x1E && i + 3 < s.size()) {
            cp = (char32_t(b0 & 0x07) << 18) | (char32_t(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                 (char32_t(static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3F);
            len = 4;
        }
        fn(cp);
        i += len;
    }
}

// Number of code points in a UTF-8 string (continuation bytes are not counted).
constexpr std::size_t utf8_length(std::string_view s) {
    std::size_t n = 0;
    for (const char ch : s) n += (static_cast<unsigned char>(ch) & 0xC0) != 0x80;
    return n;
}

static_assert(utf8_length("a\u00b7\u2192z") == 4);

// Per-size glyph cache for one font instance.
class GlyphCache {
    const Font* font_;
    float px_per_em_;
    std::unordered_map<char32_t, GlyphBitmap> cache_;

public:
    GlyphCache(const Font& font, float px_per_em) : font_(&font), px_per_em_(px_per_em) {}

    float px_per_em() const { return px_per_em_; }
    float ascent() const { return font_->ascender() * px_per_em_ / font_->units_per_em(); }
    float descent() const { return -font_->descender() * px_per_em_ / font_->units_per_em(); }

    // Monospace advance in pixels (Fira Code: every glyph shares the width of '0').
    float advance() const {
        return font_->outline(font_->glyph_index(U'0')).advance * px_per_em_ / font_->units_per_em();
    }

    const GlyphBitmap& glyph(char32_t cp) {
        if (const auto it = cache_.find(cp); it != cache_.end()) return it->second;
        std::uint16_t gid = font_->glyph_index(cp);
        if (gid == 0 && cp > U' ') gid = font_->glyph_index(U'?');
        return cache_.emplace(cp, font_->rasterize(gid, px_per_em_)).first->second;
    }
};

} // namespace fsturbo::ttf

#endif // TTF_FONT_HPP
