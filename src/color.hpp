#ifndef COLOR_HPP
#define COLOR_HPP

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <numeric>

namespace fsturbo {

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    constexpr bool operator==(const Rgb&) const = default;
};

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;

    constexpr bool operator==(const Rgba&) const = default;
    constexpr Rgb rgb() const { return {r, g, b}; }
};

constexpr std::uint8_t mix_channel(std::uint8_t a, std::uint8_t b, float t) {
    // C++26 <numeric> saturate_cast clamps the rounded value into 0..255.
    return std::saturate_cast<std::uint8_t>(static_cast<int>(a + (static_cast<float>(b) - a) * t + 0.5f));
}

constexpr Rgb lerp(Rgb a, Rgb b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {mix_channel(a.r, b.r, t), mix_channel(a.g, b.g, t), mix_channel(a.b, b.b, t)};
}

constexpr Rgb scale(Rgb c, float k) {
    auto ch = [k](std::uint8_t v) { return std::saturate_cast<std::uint8_t>(static_cast<int>(v * k + 0.5f)); };
    return {ch(c.r), ch(c.g), ch(c.b)};
}

// Evenly spaced multi-stop gradient. The end stops are reached through C++26 pack indexing.
template <std::same_as<Rgb>... Stops>
    requires(sizeof...(Stops) >= 2)
constexpr Rgb gradient(float t, Stops... stops) {
    if (t <= 0.0f) return stops...[0];
    if (t >= 1.0f) return stops...[sizeof...(Stops) - 1];
    const Rgb table[] = {stops...};
    const float pos = t * (sizeof...(Stops) - 1);
    const auto seg = static_cast<std::size_t>(pos);
    return lerp(table[seg], table[seg + 1], pos - static_cast<float>(seg));
}

// Rec. 601 luma, 0..255.
constexpr int luma(Rgb c) { return (299 * c.r + 587 * c.g + 114 * c.b) / 1000; }

namespace palette {
inline constexpr Rgb neon_cyan{0, 255, 240};
inline constexpr Rgb neon_magenta{255, 0, 200};
inline constexpr Rgb neon_gold{255, 215, 0};
inline constexpr Rgb neon_purple{128, 0, 255};
inline constexpr Rgb neon_pink{255, 0, 128};
inline constexpr Rgb track{40, 40, 62};
inline constexpr Rgb console_bg{14, 14, 26};
inline constexpr Rgb console_title_bg{28, 28, 50};
inline constexpr Rgb console_border{100, 100, 160};
inline constexpr Rgb console_title{0, 255, 200};
inline constexpr Rgb action{255, 215, 0};
inline constexpr Rgb path_dir{120, 140, 200};
inline constexpr Rgb path_name{228, 230, 242};
inline constexpr Rgb path_ext{0, 215, 255};
inline constexpr Rgb number{255, 121, 198};
inline constexpr Rgb arrow{255, 215, 0};
inline constexpr Rgb dest{100, 255, 150};
inline constexpr Rgb note{140, 140, 175};
inline constexpr Rgb error{255, 70, 90};
} // namespace palette

static_assert(gradient(0.0f, Rgb{0, 0, 0}, Rgb{255, 255, 255}) == Rgb{0, 0, 0});
static_assert(gradient(0.5f, Rgb{0, 0, 0}, Rgb{200, 100, 50}) == Rgb{100, 50, 25});
static_assert(gradient(1.0f, palette::neon_cyan, palette::neon_magenta, palette::neon_gold) == palette::neon_gold);

} // namespace fsturbo

#endif // COLOR_HPP
