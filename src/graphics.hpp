#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

// Image output for the terminal: Sixel (a palette of colour registers, no alpha) or the Kitty
// graphics protocol (24-bit RGBA with real transparency, zlib-compressed). A Canvas is encoded
// for whichever protocol is active; callers place the result with term::place_image().
//
// Kitty images carry an id: sending an id again replaces the picture in place, which is how the
// live frame is redrawn. They are anchored where the cursor is, scaled to `cols` x `rows` cells
// (so a wrong guess of the cell size cannot misplace them), and never move the cursor (C=1),
// which the callers manage themselves. Every command is quiet (q=2): the terminal's replies would
// otherwise land in the user's input.

#include <zlib.h>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "sixel_renderer.hpp"

namespace fsturbo::gfx {

enum class Protocol : std::uint8_t { Sixel, Kitty };
inline Protocol g_protocol = Protocol::Sixel;

// The images this program keeps on screen. Small icons get no id: each stays where it was drawn.
inline constexpr std::uint32_t kBannerId = 1, kFrameId = 2, kReportId = 3;

inline constexpr std::string_view kBase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr std::string base64(std::span<const unsigned char> in) {
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const std::uint32_t v = std::uint32_t{in[i]} << 16 | std::uint32_t{in[i + 1]} << 8 | in[i + 2];
        out += kBase64[v >> 18];
        out += kBase64[(v >> 12) & 63];
        out += kBase64[(v >> 6) & 63];
        out += kBase64[v & 63];
    }
    if (const std::size_t rest = in.size() - i; rest > 0) {
        const std::uint32_t v = std::uint32_t{in[i]} << 16 | (rest == 2 ? std::uint32_t{in[i + 1]} << 8 : 0u);
        out += kBase64[v >> 18];
        out += kBase64[(v >> 12) & 63];
        out += rest == 2 ? kBase64[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

static_assert([] {
    constexpr unsigned char a[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    return base64({a, 6}) == "Zm9vYmFy" && base64({a, 5}) == "Zm9vYmE=" && base64({a, 4}) == "Zm9vYg==" && base64({a, 3}) == "Zm9v" && base64({a, 0}).empty();
}());

// Transmits and displays `c` at the cursor, scaled to `cols` x `rows` cells (0: keep the other
// dimension's aspect ratio). With id 0 the image is anonymous. Chunks are 4096 base64 characters.
inline std::string kitty_image(const sixel::Canvas& c, std::uint32_t id, int cols, int rows) {
    const auto pixels = std::as_bytes(c.pixels());
    uLongf packed_size = compressBound(static_cast<uLong>(pixels.size()));
    std::vector<unsigned char> packed(packed_size);
    const bool compressed = compress2(packed.data(), &packed_size, reinterpret_cast<const Bytef*>(pixels.data()), static_cast<uLong>(pixels.size()), Z_BEST_SPEED) == Z_OK;
    const std::span<const unsigned char> payload = compressed ? std::span<const unsigned char>(packed.data(), packed_size)
                                                              : std::span<const unsigned char>(reinterpret_cast<const unsigned char*>(pixels.data()), pixels.size());
    const std::string data = base64(payload);

    std::string keys = "a=T,f=32,s=" + std::to_string(c.width()) + ",v=" + std::to_string(c.height()) + (compressed ? ",o=z" : "");
    if (id != 0) keys += ",i=" + std::to_string(id) + ",p=1";
    if (cols > 0) keys += ",c=" + std::to_string(cols);
    if (rows > 0) keys += ",r=" + std::to_string(rows);
    keys += ",C=1,q=2";

    constexpr std::size_t kChunk = 4096;
    std::string out;
    out.reserve(data.size() + data.size() / kChunk * 16 + keys.size() + 32);
    for (std::size_t at = 0; at < data.size() || at == 0; at += kChunk) {
        const bool more = at + kChunk < data.size();
        out += "\x1b_G";
        out += at == 0 ? keys + (more ? ",m=1" : "") : std::string(more ? "m=1" : "m=0") + ",q=2";
        out += ';';
        out.append(data, at, kChunk);
        out += "\x1b\\";
    }
    return out;
}

// Removes an image and all its placements (and frees its data).
inline std::string kitty_delete(std::uint32_t id) { return "\x1b_Ga=d,d=I,i=" + std::to_string(id) + ",q=2\x1b\\"; }

// `c` for the active protocol; `cols` x `rows` is the span of terminal cells it covers.
inline std::string image(const sixel::Canvas& c, std::uint32_t id, int cols, int rows) {
    return g_protocol == Protocol::Kitty ? kitty_image(c, id, cols, rows) : sixel::encode(c);
}

} // namespace fsturbo::gfx

#endif // GRAPHICS_HPP
