#ifndef TERMINAL_STYLE_HPP
#define TERMINAL_STYLE_HPP

#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>
#include "color.hpp"
#include "sixel_renderer.hpp"
#include "terminal_probe.hpp"
#include "ttf_font.hpp"

namespace fsturbo::term {

using RGB = Rgb;

enum class ColorMode {
    Full,     // Sixel graphics (banner, icons, live dashboard) + TrueColor text
    Terminal, // TrueColor 24-bit RGB ANSI + in-place live dashboard
    Simple,   // 16-color ANSI + in-place live dashboard
    None      // Plain text, non-interactive
};

inline ColorMode g_color_mode = ColorMode::None;
inline TermInfo g_term;

inline bool is_tty() {
    static const bool tty = isatty(STDOUT_FILENO) != 0;
    return tty;
}

// Returns false for "auto" (or anything unrecognized): the mode is then detected from the terminal.
constexpr bool parse_color_mode(std::string_view s, ColorMode& out) {
    if (s == "full") out = ColorMode::Full;
    else if (s == "terminal") out = ColorMode::Terminal;
    else if (s == "simple") out = ColorMode::Simple;
    else if (s == "none") out = ColorMode::None;
    else return false;
    return true;
}

constexpr bool is_color_mode_name(std::string_view s) {
    ColorMode m{};
    return parse_color_mode(s, m) || s == "auto";
}

// Resolves the effective mode. Terminal queries only run when Sixel output is possible.
inline void init_terminal(std::string_view requested) {
    if (ColorMode forced{}; parse_color_mode(requested, forced)) {
        g_color_mode = forced;
        if (forced != ColorMode::None) g_term = probe_terminal(is_tty() && forced == ColorMode::Full);
        return;
    }
    if (!is_tty()) {
        g_color_mode = ColorMode::None;
        return;
    }
    g_term = probe_terminal(true);
    const char* term_env = std::getenv("TERM");
    const std::string_view term_name = term_env ? term_env : "";
    if (g_term.sixel) g_color_mode = ColorMode::Full;
    else if (term_name == "dumb") g_color_mode = ColorMode::None;
    else if (term_name == "linux") g_color_mode = ColorMode::Simple;
    else g_color_mode = ColorMode::Terminal;
}

inline bool interactive() { return g_color_mode != ColorMode::None && is_tty(); }

// --- SGR helpers ----------------------------------------------------------------

inline constexpr std::array<Rgb, 16> kAnsi16{{{0, 0, 0},       {205, 0, 0},     {0, 205, 0},   {205, 205, 0},
                                              {0, 0, 238},     {205, 0, 205},   {0, 205, 205}, {229, 229, 229},
                                              {127, 127, 127}, {255, 0, 0},     {0, 255, 0},   {255, 255, 0},
                                              {92, 92, 255},   {255, 0, 255},   {0, 255, 255}, {255, 255, 255}}};

// Nearest entry of the xterm 16-color palette (red-mean weighted distance).
constexpr int nearest_ansi16(Rgb c) {
    int best = 0;
    long best_d = -1;
    for (int i = 0; i < 16; ++i) {
        const Rgb p = kAnsi16[static_cast<std::size_t>(i)];
        const long rm = (c.r + p.r) / 2;
        const long dr = c.r - p.r, dg = c.g - p.g, db = c.b - p.b;
        const long d = ((512 + rm) * dr * dr >> 8) + 4 * dg * dg + ((767 - rm) * db * db >> 8);
        if (best_d < 0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

static_assert(nearest_ansi16({250, 10, 10}) == 9);
static_assert(nearest_ansi16({0, 250, 240}) == 14);

inline std::string rgb_fg(Rgb c) {
    switch (g_color_mode) {
        case ColorMode::None: return {};
        case ColorMode::Simple: {
            const int i = nearest_ansi16(c);
            return std::format("\x1b[{}m", i < 8 ? 30 + i : 90 + i - 8);
        }
        default: return std::format("\x1b[38;2;{};{};{}m", c.r, c.g, c.b);
    }
}

inline std::string rgb_bg(Rgb c) {
    switch (g_color_mode) {
        case ColorMode::None: return {};
        case ColorMode::Simple: {
            const int i = nearest_ansi16(c);
            return std::format("\x1b[{}m", i < 8 ? 40 + i : 100 + i - 8);
        }
        default: return std::format("\x1b[48;2;{};{};{}m", c.r, c.g, c.b);
    }
}

inline std::string_view reset() { return g_color_mode == ColorMode::None ? "" : "\x1b[0m"; }
inline std::string_view bold() { return g_color_mode == ColorMode::None ? "" : "\x1b[1m"; }
inline std::string_view dim() { return g_color_mode == ColorMode::None ? "" : "\x1b[2m"; }
inline std::string_view italic() { return g_color_mode == ColorMode::None ? "" : "\x1b[3m"; }

// Per-code-point color gradient (multi-byte UTF-8 sequences are never split).
inline std::string gradient_text(std::string_view text, Rgb start, Rgb end) {
    if (g_color_mode == ColorMode::None) return std::string(text);
    const std::size_t n = ttf::utf8_length(text);
    std::string out;
    out.reserve(text.size() * 20);
    std::size_t i = 0;
    for (std::size_t pos = 0; pos < text.size();) {
        std::size_t len = 1;
        while (pos + len < text.size() && (static_cast<unsigned char>(text[pos + len]) & 0xC0) == 0x80) ++len;
        const float t = n > 1 ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.0f;
        out += rgb_fg(lerp(start, end, t));
        out += text.substr(pos, len);
        pos += len;
        ++i;
    }
    out += reset();
    return out;
}

// --- Inline Sixel image placement ---------------------------------------------------

// Columns covered by a Sixel icon in the current terminal.
inline int icon_columns() {
    const int cw = g_term.cell_width();
    return std::max(1, (sixel::kIconSize + cw - 1) / cw);
}

// Emits a complete Sixel image as its own block of rows. Space for rows+1 lines is
// reserved first, so neither the image nor the terminal's post-image cursor
// placement can scroll the screen; the cursor ends on the line below the image.
inline std::string place_image(std::string_view sixel_data, int pixel_height) {
    const int rows = std::max(1, (pixel_height + g_term.cell_height() - 1) / g_term.cell_height());
    std::string out = "\r";
    out.append(static_cast<std::size_t>(rows + 1), '\n');
    out += std::format("\x1b[{}A\x1b" "7", rows + 1);
    out += sixel_data;
    out += std::format("\x1b" "8\x1b[{}B\r", rows);
    return out;
}

// One output line led by an icon: Sixel icon (Full), emoji (Terminal/Simple), or a text tag (None).
// In Full mode the text is printed first and the icon is painted back onto that line,
// which keeps the cursor math independent of each terminal's Sixel cursor policy.
inline std::string icon_line(sixel::Icon icon, std::string_view text) {
    static constexpr std::array<std::string_view, std::to_underlying(sixel::Icon::Count)> emoji{
        "📁", "📄", "✏️ ", "⚡", "✖️ ", "💥", "✅", "🚀", "🔤"};
    static constexpr std::array<std::string_view, std::to_underlying(sixel::Icon::Count)> tags{
        "[DIR]", "[FILE]", "[RENAME]", "[FLATTEN]", "[EXCLUDE]", "[ERROR]", "[OK]", "[EXEC]", "[FONT]"};
    const auto i = std::to_underlying(icon);
    switch (g_color_mode) {
        case ColorMode::None: return std::format(" {} {}\n", tags[i], text);
        case ColorMode::Terminal:
        case ColorMode::Simple: return std::format(" {} {}\n", emoji[i], text);
        case ColorMode::Full: {
            std::string out(static_cast<std::size_t>(icon_columns() + 2), ' ');
            out += text;
            out += "\n\x1b" "7\x1b[1A\x1b[2G";
            out += sixel::icon(icon);
            out += "\x1b" "8";
            return out;
        }
    }
    return std::string(text);
}

// --- Mini-console syntax highlighting ------------------------------------------------

enum class Role : std::uint8_t { Plain, Prompt, Action, Dir, Name, Ext, Number, Arrow, Dest, Note };

struct Span {
    std::string_view text;
    Role role = Role::Plain;
};

constexpr Rgb role_color(Role r) {
    switch (r) {
        case Role::Prompt: return palette::neon_cyan;
        case Role::Action: return palette::action;
        case Role::Dir: return palette::path_dir;
        case Role::Name: return palette::path_name;
        case Role::Ext: return palette::path_ext;
        case Role::Number: return palette::number;
        case Role::Arrow: return palette::arrow;
        case Role::Dest: return palette::dest;
        case Role::Note: return palette::note;
        case Role::Plain: break;
    }
    return palette::path_name;
}

// Splits a path into directory / name (digit runs as numbers) / extension tokens.
constexpr void highlight_path(std::string_view path, Role name_role, std::vector<Span>& out) {
    const std::size_t slash = path.rfind('/');
    std::string_view name = path;
    if (slash != std::string_view::npos) {
        out.push_back({path.substr(0, slash + 1), Role::Dir});
        name = path.substr(slash + 1);
    }
    std::string_view ext;
    if (const std::size_t dot = name.rfind('.'); dot != std::string_view::npos && dot > 0) {
        ext = name.substr(dot);
        name = name.substr(0, dot);
    }
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    for (std::size_t i = 0; i < name.size();) {
        const bool digits = is_digit(name[i]);
        std::size_t j = i + 1;
        while (j < name.size() && is_digit(name[j]) == digits) ++j;
        out.push_back({name.substr(i, j - i), digits ? Role::Number : name_role});
        i = j;
    }
    if (!ext.empty()) out.push_back({ext, Role::Ext});
}

static_assert([] {
    std::vector<Span> s;
    highlight_path("src/Report_2024.TXT", Role::Name, s);
    return s.size() == 4 && s[0].text == "src/" && s[1].text == "Report_" && s[2].role == Role::Number && s[3].text == ".TXT";
}());

// Code-point aware truncation that keeps the end of a path visible ("…/deep/name.txt").
inline std::string fit_left(std::string_view s, std::size_t max_cols) {
    const std::size_t n = ttf::utf8_length(s);
    if (n <= max_cols) return std::string(s);
    if (max_cols == 0) return {};
    std::size_t skip = n - (max_cols - 1), pos = 0;
    while (skip > 0 && pos < s.size()) {
        ++pos;
        while (pos < s.size() && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) ++pos;
        --skip;
    }
    return "…" + std::string(s.substr(pos));
}

inline std::string fit_right(std::string_view s, std::size_t max_cols) {
    const std::size_t n = ttf::utf8_length(s);
    if (n <= max_cols) return std::string(s);
    if (max_cols == 0) return {};
    std::size_t keep = max_cols - 1, pos = 0;
    while (keep > 0 && pos < s.size()) {
        ++pos;
        while (pos < s.size() && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) ++pos;
        --keep;
    }
    return std::string(s.substr(0, pos)) + "…";
}

} // namespace fsturbo::term

#endif // TERMINAL_STYLE_HPP
