#ifndef TERMINAL_STYLE_HPP
#define TERMINAL_STYLE_HPP

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cmath>
#include <cstdint>
#include <unistd.h>
#include <sstream>
#include <print>
#include <format>
#include <regex>
#include "sixel_renderer.hpp"

namespace fsturbo::term {

enum class ColorMode {
    Full,     // Sixel graphics + TrueColor RGB + In-place Live Dashboard
    Terminal, // TrueColor RGB ANSI + In-place Live Dashboard
    Simple,   // 16-color ANSI + In-place Live Dashboard
    None      // Plain text, non-interactive
};

inline ColorMode g_color_mode = ColorMode::Full;

inline bool is_tty() {
    static const bool tty = (isatty(STDOUT_FILENO) != 0);
    return tty;
}

inline void set_color_mode_from_string(std::string_view mode_str) {
    if (mode_str == "full") g_color_mode = ColorMode::Full;
    else if (mode_str == "terminal") g_color_mode = ColorMode::Terminal;
    else if (mode_str == "simple") g_color_mode = ColorMode::Simple;
    else if (mode_str == "none") g_color_mode = ColorMode::None;
    else g_color_mode = is_tty() ? ColorMode::Full : ColorMode::None;
}

struct RGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

inline std::string rgb_fg(RGB c) {
    if (g_color_mode == ColorMode::None) return "";
    if (g_color_mode == ColorMode::Simple) return "\033[36m";
    return std::format("\033[38;2;{};{};{}m", c.r, c.g, c.b);
}

inline std::string rgb_bg(RGB c) {
    if (g_color_mode == ColorMode::None) return "";
    if (g_color_mode == ColorMode::Simple) return "\033[44m";
    return std::format("\033[48;2;{};{};{}m", c.r, c.g, c.b);
}

inline std::string reset() {
    if (g_color_mode == ColorMode::None) return "";
    return "\033[0m";
}

inline std::string bold() {
    if (g_color_mode == ColorMode::None) return "";
    return "\033[1m";
}

inline std::string italic() {
    if (g_color_mode == ColorMode::None) return "";
    return "\033[3m";
}

inline RGB lerp(RGB a, RGB b, float t) {
    return RGB{
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t)
    };
}

inline std::string gradient_text(std::string_view text, RGB start, RGB end) {
    if (g_color_mode == ColorMode::None) return std::string(text);
    if (g_color_mode == ColorMode::Simple) return std::format("\033[33m{}\033[0m", text);
    std::string out;
    size_t len = text.length();
    if (len == 0) [[unlikely]] return out;
    for (size_t i = 0; i < len; ++i) {
        float t = (len > 1) ? static_cast<float>(i) / (len - 1) : 0.0f;
        RGB color = lerp(start, end, t);
        out += rgb_fg(color) + text[i];
    }
    out += reset();
    return out;
}

// Icons (Sixel full-color graphics in Full mode, ANSI in Terminal/Simple)
inline std::string icon_dir() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Folder) + " ";
    return (g_color_mode != ColorMode::None) ? "📁 " : "[DIR] ";
}

inline std::string icon_file() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::File) + " ";
    return (g_color_mode != ColorMode::None) ? "📄 " : "[FILE] ";
}

inline std::string icon_flatten() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Flatten) + " ";
    return (g_color_mode != ColorMode::None) ? "⚡ " : "[FLATTEN] ";
}

inline std::string icon_rename() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Rename) + " ";
    return (g_color_mode != ColorMode::None) ? "✏️  " : "[RENAME] ";
}

inline std::string icon_exclude() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Exclude) + " ";
    return (g_color_mode != ColorMode::None) ? "✖️  " : "[EXCLUDE] ";
}

inline std::string icon_error() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Error) + " ";
    return (g_color_mode != ColorMode::None) ? "💥 " : "[ERROR] ";
}

inline std::string icon_success() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Success) + " ";
    return (g_color_mode != ColorMode::None) ? "✅ " : "[OK] ";
}

inline std::string icon_font() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Font) + " ";
    return (g_color_mode != ColorMode::None) ? "🔤 " : "[FONT] ";
}

inline std::string icon_rocket() {
    if (g_color_mode == ColorMode::Full) return sixel::generate_sixel_icon(sixel::SixelIconType::Rocket) + " ";
    return (g_color_mode != ColorMode::None) ? "🚀 " : "[EXEC] ";
}

// Syntax Highlighting helper for Mini-Terminal text
inline std::string highlight_syntax(std::string_view text) {
    if (g_color_mode == ColorMode::None) return std::string(text);

    std::string str(text);
    static const std::regex ext_rx(R"(\.([a-zA-Z0-9]+))");
    str = std::regex_replace(str, ext_rx, "\033[36m.$1\033[0m");

    static const std::regex num_rx(R"(\b(\d+)\b)");
    str = std::regex_replace(str, num_rx, "\033[35m$1\033[0m");

    static const std::regex arrow_rx(R"(➔|->)");
    str = std::regex_replace(str, arrow_rx, "\033[1;33m➔\033[0m");

    return str;
}

// Reusable Live Dashboard: Progress bar + Mini-Terminal Console
class LiveDashboard {
    bool is_interactive = false;
    size_t last_current = 0;
    size_t total_items = 0;
    const int frame_height = 4; // 1 progress bar + 3 lines of mini-terminal box

public:
    LiveDashboard() = default;

    void init(size_t total) {
        total_items = total;
        is_interactive = (g_color_mode != ColorMode::None) && is_tty();
        last_current = 0;
    }

    void update(size_t current, std::string_view action, std::string_view source, std::string_view dest, std::string_view extra = "") {
        if (!is_interactive) return;

        // If previously drawn, move cursor up to overwrite in-place!
        if (last_current > 0) {
            std::print("\033[{}A\r", frame_height);
        }

        // 1. Line: Real-time Multi-Color Gradient Progress Bar
        constexpr int bar_width = 30;
        float ratio = (total_items > 0) ? static_cast<float>(current) / total_items : 1.0f;
        int filled = static_cast<int>(ratio * bar_width);

        RGB c_start = {0, 255, 240};
        RGB c_mid   = {255, 0, 200};
        RGB c_end   = {255, 215, 0};

        std::string bar_str = "[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) {
                float t = static_cast<float>(i) / bar_width;
                RGB c = (t < 0.5f) ? lerp(c_start, c_mid, t * 2.0f) : lerp(c_mid, c_end, (t - 0.5f) * 2.0f);
                bar_str += rgb_fg(c) + "█";
            } else {
                bar_str += rgb_fg({60, 60, 80}) + "░";
            }
        }
        bar_str += reset() + "] ";

        std::string pct_str = std::format("{:5.1f}% ({}/{})", ratio * 100.0f, current, total_items);
        std::println("\033[2K{}{}{}{} {}", bold(), bar_str, rgb_fg({200, 220, 255}), pct_str, rgb_fg({150, 150, 180}) + std::string(extra));

        // 2-4. Lines: Reused framed Mini-Terminal with Fira Code monospace aesthetic
        RGB border_col = {100, 100, 160};
        RGB title_col  = {0, 255, 200};
        RGB action_col = {255, 215, 0};

        std::string border = rgb_fg(border_col);
        std::string title  = bold() + rgb_fg(title_col) + "MINI-TERMINAL [Fira Code Engine]" + reset() + border;

        std::string content = std::format("{} ➔ {}", source, dest);
        if (dest.empty()) content = std::string(source);

        std::println("\033[2K{}┌─ {} ──────────────────────────────────────────────────┐{}", border, title, reset());
        std::println("\033[2K{}│{} {}{} {} {:<48} {}│{}",
                     border, reset(),
                     bold() + rgb_fg(action_col), action, reset(),
                     highlight_syntax(content),
                     border, reset());
        std::println("\033[2K{}└─────────────────────────────────────────────────────────────────────────────┘{}", border, reset());

        std::fflush(stdout);
        last_current = current;
    }

    void finish() {
        if (is_interactive && last_current > 0) {
            std::println(); // Leave clean spacing below the completed dashboard
        }
    }
};

} // namespace fsturbo::term

#endif
