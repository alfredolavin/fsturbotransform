#ifndef SIXEL_RENDERER_HPP
#define SIXEL_RENDERER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <format>
#include <map>

namespace fsturbo::sixel {

// Run-length compression for Sixel characters (!<count><char>)
inline std::string rle_compress(std::string_view line) {
    if (line.empty()) return "";
    std::string res;
    size_t i = 0;
    size_t n = line.length();
    while (i < n) {
        char c = line[i];
        size_t count = 1;
        while (i + 1 < n && line[i + 1] == c && count < 255) {
            count++;
            i++;
        }
        if (count >= 3) {
            res += "!" + std::to_string(count) + c;
        } else {
            res.append(count, c);
        }
        i++;
    }
    return res;
}

// Encode RGBA buffer into compact Sixel DCS sequence
inline std::string encode_sixel_rgba(const uint8_t* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return "";

    std::string out;
    out += "\033Pq\"1;1;";
    out += std::to_string(width) + ";" + std::to_string(height);

    struct RGBKey {
        uint8_t r, g, b;
        bool operator<(const RGBKey& o) const {
            if (r != o.r) return r < o.r;
            if (g != o.g) return g < o.g;
            return b < o.b;
        }
    };

    std::map<RGBKey, int> palette;
    int next_palette_idx = 0;

    // Build palette (rounded to 8 for compact color clustering)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            uint8_t a = rgba[idx + 3];
            if (a < 64) continue; // Transparent
            RGBKey key{
                static_cast<uint8_t>((rgba[idx] / 8) * 8),
                static_cast<uint8_t>((rgba[idx + 1] / 8) * 8),
                static_cast<uint8_t>((rgba[idx + 2] / 8) * 8)
            };
            if (palette.find(key) == palette.end() && next_palette_idx < 64) {
                palette[key] = next_palette_idx++;
                int r_pct = (key.r * 100) / 255;
                int g_pct = (key.g * 100) / 255;
                int b_pct = (key.b * 100) / 255;
                out += std::format("#{};2;{};{};{}", palette[key], r_pct, g_pct, b_pct);
            }
        }
    }

    // Encode 6-pixel vertical bands
    for (int y_band = 0; y_band < height; y_band += 6) {
        for (const auto& [color_key, color_idx] : palette) {
            bool used = false;
            std::string line_chars;

            for (int x = 0; x < width; ++x) {
                uint8_t bitmask = 0;
                for (int dy = 0; dy < 6; ++dy) {
                    int y = y_band + dy;
                    if (y >= height) break;
                    int idx = (y * width + x) * 4;
                    uint8_t a = rgba[idx + 3];
                    if (a >= 64) {
                        RGBKey key{
                            static_cast<uint8_t>((rgba[idx] / 8) * 8),
                            static_cast<uint8_t>((rgba[idx + 1] / 8) * 8),
                            static_cast<uint8_t>((rgba[idx + 2] / 8) * 8)
                        };
                        if (key.r == color_key.r && key.g == color_key.g && key.b == color_key.b) {
                            bitmask |= (1 << dy);
                        }
                    }
                }
                if (bitmask > 0) used = true;
                line_chars.push_back(static_cast<char>(63 + bitmask));
            }

            if (used) {
                out += std::format("#{}", color_idx);
                out += rle_compress(line_chars);
                out += "$"; // Carriage return for next color in band
            }
        }
        out += "-"; // Next 6-pixel band
    }

    out += "\033\\";
    return out;
}

enum class SixelIconType {
    Folder,
    File,
    Rename,
    Flatten,
    Exclude,
    Error,
    Success,
    Rocket,
    Font
};

// Generate 16x16 pixel icon in Sixel format
inline std::string generate_sixel_icon(SixelIconType type) {
    constexpr int w = 16, h = 16;
    uint8_t pixels[w * h * 4] = {0};

    auto set_px = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            int idx = (y * w + x) * 4;
            pixels[idx] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = 255;
        }
    };

    switch (type) {
        case SixelIconType::Folder:
            for (int y = 5; y < 13; ++y)
                for (int x = 2; x < 14; ++x)
                    set_px(x, y, 255, 180, 0);
            for (int x = 2; x < 7; ++x)
                set_px(x, 4, 255, 210, 60);
            break;

        case SixelIconType::File:
            for (int y = 2; y < 14; ++y)
                for (int x = 3; x < 13; ++x)
                    set_px(x, y, 220, 240, 255);
            for (int y = 2; y < 6; ++y)
                for (int x = 9; x < 13; ++x)
                    set_px(x, y, 0, 200, 255);
            break;

        case SixelIconType::Rename:
            for (int i = 0; i < 10; ++i) {
                set_px(3 + i, 12 - i, 0, 229, 255);
                set_px(4 + i, 12 - i, 0, 180, 255);
            }
            break;

        case SixelIconType::Flatten:
            for (int y = 2; y < 8; ++y) set_px(8 - (y - 2), y, 255, 214, 0);
            for (int x = 4; x < 12; ++x) set_px(x, 8, 255, 235, 59);
            for (int y = 8; y < 14; ++y) set_px(10 - (y - 8), y, 255, 214, 0);
            break;

        case SixelIconType::Exclude:
            for (int i = 2; i < 14; ++i) {
                set_px(i, i, 255, 82, 82);
                set_px(15 - i, i, 255, 82, 82);
            }
            break;

        case SixelIconType::Error:
            for (int y = 2; y < 14; ++y)
                for (int x = 2; x < 14; ++x)
                    if ((x - 8) * (x - 8) + (y - 8) * (y - 8) <= 30)
                        set_px(x, y, 255, 23, 68);
            for (int y = 5; y < 11; ++y) {
                set_px(y, y, 255, 255, 255);
                set_px(15 - y, y, 255, 255, 255);
            }
            break;

        case SixelIconType::Success:
            for (int i = 0; i < 5; ++i) set_px(3 + i, 7 + i, 0, 230, 118);
            for (int i = 0; i < 8; ++i) set_px(7 + i, 11 - i, 0, 230, 118);
            break;

        case SixelIconType::Rocket:
            for (int i = 0; i < 10; ++i) {
                set_px(3 + i, 13 - i, 255, 0, 128);
                set_px(4 + i, 13 - i, 0, 240, 255);
            }
            set_px(2, 14, 255, 170, 0);
            break;

        case SixelIconType::Font:
            for (int y = 3; y < 13; ++y) set_px(4, y, 187, 134, 252);
            for (int x = 4; x < 12; ++x) set_px(x, 3, 187, 134, 252);
            for (int x = 4; x < 10; ++x) set_px(x, 7, 187, 134, 252);
            break;
    }

    return encode_sixel_rgba(pixels, w, h);
}

// Generate Full-Color Cyberpunk Sixel Header Banner
inline std::string generate_sixel_banner(int width = 360, int height = 36) {
    std::vector<uint8_t> pixels(width * height * 4, 0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            float tx = static_cast<float>(x) / width;
            float ty = static_cast<float>(y) / height;

            // Border
            if (x < 2 || x >= width - 2 || y < 2 || y >= height - 2) {
                pixels[idx] = 0;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 230;
                pixels[idx + 3] = 255;
                continue;
            }

            // Neon gradient: Cyan -> Magenta -> Purple
            uint8_t r = static_cast<uint8_t>(tx * 255);
            uint8_t g = static_cast<uint8_t>((1.0f - tx) * 200 * (1.0f - ty * 0.5f));
            uint8_t b = static_cast<uint8_t>(220 + ty * 35);

            pixels[idx]     = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = 255;
        }
    }

    return encode_sixel_rgba(pixels.data(), width, height);
}

} // namespace fsturbo::sixel

#endif
