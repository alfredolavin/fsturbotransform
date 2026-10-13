#ifndef CONSOLE_STYLE_HPP
#define CONSOLE_STYLE_HPP

// The format of the mini-console: its font, colours and window details. One file,
// ~/.config/fsturbotransform/console.conf (or $FSTURBO_CONSOLE_STYLE, or --console-style FILE), is read
// by the terminal dashboard and the GUI's console view and written by the GUI's "Console format"
// dialog. Lines are `key = value`; '#' starts a comment; a missing key keeps its default.
//
// Colours are stored the way the shared colorspec controls (plasmoids/shared/colorspec) store them,
// so the GUI edits them with those controls: "#rrggbb", "#aarrggbb", "r,g,b[,a]", or the JSON
// {"src":"fixed"|"system", "color":"#aarrggbb", "sys":"<role>", "l":…, "c":…, "h":…, "a":…}, where
// "system" names a role of the desktop colour scheme (textColor, highlightColor, …, the names of
// Kirigami.Theme) and l / c / h are OKLCH adjustments and a the opacity (ColorSpecCore.js, ported
// here). A system colour is stored as the role, so it follows scheme changes: the terminal reads
// the scheme from kdeglobals, the GUI from Kirigami. Colour groups (grp) are kept but not applied.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "color.hpp"

namespace fsturbo::style {

// ---- colours (a port of plasmoids/shared/colorspec/ColorSpecCore.js) ------------------------

struct Rgbaf {
    double r = 1, g = 1, b = 1, a = 1;
};

struct ColorSpec {
    std::string src = "fixed";
    Rgbaf color;
    std::string sys; // the system role when src == "system"
    double l = 0, c = 0, h = 0, a = 100;
};

namespace detail {

constexpr std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
    return s;
}

constexpr int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

// "a,b,c[,d]" of integers
inline std::optional<std::vector<double>> int_list(std::string_view s) {
    std::vector<double> out;
    while (true) {
        s = trim(s);
        std::size_t n = 0;
        double v = 0;
        while (n < s.size() && s[n] >= '0' && s[n] <= '9') v = v * 10 + (s[n++] - '0');
        if (n == 0) return std::nullopt;
        out.push_back(v);
        s = trim(s.substr(n));
        if (s.empty()) break;
        if (s.front() != ',') return std::nullopt;
        s.remove_prefix(1);
    }
    return out;
}

inline double clamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
inline double to_linear(double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); }
inline double to_gamma(double v) { return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055; }

struct Lab {
    double L, A, B;
};

inline Lab rgb_to_oklab(const Rgbaf& c) {
    const double r = to_linear(c.r), g = to_linear(c.g), b = to_linear(c.b);
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s, 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

inline Rgbaf oklab_to_rgb(double L, double A, double B, double alpha) {
    const double l = std::pow(L + 0.3963377774 * A + 0.2158037573 * B, 3);
    const double m = std::pow(L - 0.1055613458 * A - 0.0638541728 * B, 3);
    const double s = std::pow(L - 0.0894841775 * A - 1.2914855480 * B, 3);
    const auto out = [](double v) { return to_gamma(clamp(v, 0, 1)); };
    return {out(4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s), out(-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s),
            out(-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s), alpha};
}

// ColorSpecCore.shade(): luminosity moves lightness toward 0 / 1, chroma scales from grey to
// double (fading toward the lightness ends), hue rotates.
inline Rgbaf shade(const Rgbaf& c, double luminosity, double chroma, double hue) {
    const double t = clamp(luminosity, -100, 100) / 100;
    const double k = (1 + clamp(chroma, -100, 100) / 100) * (1 - t * t);
    const Lab lab = rgb_to_oklab(c);
    const double L = t < 0 ? lab.L * (1 + t) : lab.L + (1 - lab.L) * t;
    const double hr = clamp(hue, -180, 180) * std::numbers::pi / 180;
    const double cs = std::cos(hr), sn = std::sin(hr);
    return oklab_to_rgb(L, k * (lab.A * cs - lab.B * sn), k * (lab.A * sn + lab.B * cs), c.a);
}

// The members of a flat JSON object: strings and numbers (anything else is skipped).
inline std::map<std::string, std::string> json_object(std::string_view s) {
    std::map<std::string, std::string> out;
    std::size_t i = s.find('{');
    if (i == std::string_view::npos) return out;
    ++i;
    const auto skip_ws = [&] { while (i < s.size() && std::string_view(" \t\r\n").contains(s[i])) ++i; };
    const auto read_string = [&]() -> std::string {
        std::string v;
        for (++i; i < s.size() && s[i] != '"'; ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) ++i;
            v += s[i];
        }
        ++i;
        return v;
    };
    while (true) {
        skip_ws();
        if (i >= s.size() || s[i] != '"') break;
        const std::string key = read_string();
        skip_ws();
        if (i >= s.size() || s[i] != ':') break;
        ++i;
        skip_ws();
        if (i < s.size() && s[i] == '"') {
            out[key] = read_string();
        } else { // a number, literal, array or object: up to the next ',' or '}' at this level
            int depth = 0;
            const std::size_t start = i;
            for (; i < s.size(); ++i) {
                if (s[i] == '[' || s[i] == '{') ++depth;
                else if (s[i] == ']' || s[i] == '}') { if (depth-- == 0) break; }
                else if (s[i] == ',' && depth == 0) break;
            }
            out[key] = std::string(trim(s.substr(start, i - start)));
        }
        skip_ws();
        if (i < s.size() && s[i] == ',') ++i;
        else break;
    }
    return out;
}

} // namespace detail

// "#rrggbb", "#aarrggbb" (Qt's order) or "r,g,b[,a]" (KConfig, 0..255)
inline std::optional<Rgbaf> parse_color(std::string_view text) {
    const std::string_view s = detail::trim(text);
    if (const auto list = detail::int_list(s); list && (list->size() == 3 || list->size() == 4)) {
        const auto& v = *list;
        return Rgbaf{v[0] / 255, v[1] / 255, v[2] / 255, v.size() == 4 ? v[3] / 255 : 1.0};
    }
    std::string_view h = s;
    if (h.starts_with('#')) h.remove_prefix(1);
    if (h.size() != 6 && h.size() != 8) return std::nullopt;
    std::array<int, 4> bytes{255, 0, 0, 0};
    const std::size_t first = h.size() == 8 ? 0 : 1;
    for (std::size_t k = 0; k < h.size() / 2; ++k) {
        const int hi = detail::hex_digit(h[2 * k]), lo = detail::hex_digit(h[2 * k + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        bytes[first + k] = hi * 16 + lo;
    }
    return Rgbaf{bytes[1] / 255.0, bytes[2] / 255.0, bytes[3] / 255.0, bytes[0] / 255.0};
}

inline ColorSpec parse_spec(std::string_view text) {
    ColorSpec spec;
    const std::string_view s = detail::trim(text);
    if (s.starts_with('{')) {
        const auto o = detail::json_object(s);
        const auto get = [&](const char* key) -> std::string { const auto it = o.find(key); return it == o.end() ? "" : it->second; };
        const auto num = [&](const char* key, double fallback, double lo, double hi) {
            const std::string v = get(key);
            char* end = nullptr;
            const double d = v.empty() ? fallback : std::strtod(v.c_str(), &end);
            return detail::clamp(end == v.c_str() ? fallback : d, lo, hi);
        };
        spec.src = get("src") == "system" ? "system" : "fixed";
        if (const auto c = parse_color(get("color"))) spec.color = *c;
        spec.sys = get("sys");
        spec.l = num("l", 0, -100, 100);
        spec.c = num("c", 0, -100, 100);
        spec.h = num("h", 0, -180, 180);
        spec.a = num("a", 100, 0, 100);
        return spec;
    }
    if (const auto c = parse_color(s)) spec.color = *c;
    return spec;
}

// ---- the desktop colour scheme ------------------------------------------------------------

// Kirigami.Theme's role names -> colour, read from kdeglobals (Breeze Light where it says nothing).
class SystemColors {
    std::map<std::string, Rgbaf, std::less<>> roles_;

public:
    SystemColors() {
        const auto set = [&](const char* role, double r, double g, double b) { roles_[role] = {r / 255, g / 255, b / 255, 1}; };
        set("textColor", 35, 38, 41);
        set("backgroundColor", 239, 240, 241);
        set("alternateBackgroundColor", 227, 229, 231);
        set("highlightColor", 61, 174, 233);
        set("highlightedTextColor", 255, 255, 255);
        set("focusColor", 61, 174, 233);
        set("hoverColor", 61, 174, 233);
        set("linkColor", 41, 128, 185);
        set("visitedLinkColor", 155, 89, 182);
        set("activeTextColor", 61, 174, 233);
        set("disabledTextColor", 112, 125, 138);
        set("positiveTextColor", 39, 174, 96);
        set("neutralTextColor", 246, 116, 0);
        set("negativeTextColor", 218, 68, 83);
    }

    // The scheme in $XDG_CONFIG_HOME/kdeglobals (~/.config/kdeglobals): the Window colour set, as
    // Kirigami gives a window's controls, and the Selection set for the highlight.
    static SystemColors load() {
        SystemColors sc;
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        const std::filesystem::path dir = xdg && *xdg ? std::filesystem::path(xdg) : home ? std::filesystem::path(home) / ".config" : std::filesystem::path();
        std::ifstream in(dir / "kdeglobals");
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 12> kWindow{{
            {"ForegroundNormal", "textColor"}, {"BackgroundNormal", "backgroundColor"}, {"BackgroundAlternate", "alternateBackgroundColor"},
            {"DecorationFocus", "focusColor"}, {"DecorationHover", "hoverColor"}, {"ForegroundLink", "linkColor"},
            {"ForegroundVisited", "visitedLinkColor"}, {"ForegroundActive", "activeTextColor"}, {"ForegroundInactive", "disabledTextColor"},
            {"ForegroundPositive", "positiveTextColor"}, {"ForegroundNeutral", "neutralTextColor"}, {"ForegroundNegative", "negativeTextColor"},
        }};
        std::string line, section;
        while (std::getline(in, line)) {
            const std::string_view l = detail::trim(line);
            if (l.starts_with('[')) {
                section = std::string(l);
                continue;
            }
            const std::size_t eq = l.find('=');
            if (eq == std::string_view::npos) continue;
            const std::string_view key = detail::trim(l.substr(0, eq));
            const auto colour = parse_color(l.substr(eq + 1));
            if (!colour) continue;
            if (section == "[Colors:Window]") {
                for (const auto& [kde, role] : kWindow)
                    if (key == kde) sc.roles_[std::string(role)] = *colour;
            } else if (section == "[Colors:Selection]") {
                if (key == "BackgroundNormal") sc.roles_["highlightColor"] = *colour;
                if (key == "ForegroundNormal") sc.roles_["highlightedTextColor"] = *colour;
            }
        }
        return sc;
    }

    Rgbaf get(std::string_view role) const {
        const auto it = roles_.find(role);
        return it != roles_.end() ? it->second : roles_.find("textColor")->second;
    }
};

// ColorSpecCore.apply(): the base colour (fixed, or the scheme's role) adjusted
inline Rgbaf resolve(const ColorSpec& spec, const SystemColors& system) {
    const Rgbaf base = spec.src == "system" ? system.get(spec.sys) : spec.color;
    const Rgbaf out = spec.l != 0 || spec.c != 0 || spec.h != 0 ? detail::shade(base, spec.l, spec.c, spec.h) : base;
    return {out.r, out.g, out.b, detail::clamp(base.a * spec.a / 100, 0, 1)};
}

inline Rgb to_rgb(const Rgbaf& c, Rgb under) {
    const auto ch = [&](double v, std::uint8_t u) { return static_cast<std::uint8_t>(std::lround(detail::clamp(v * c.a * 255 + u * (1 - c.a), 0, 255))); };
    return {ch(c.r, under.r), ch(c.g, under.g), ch(c.b, under.b)};
}

inline std::string hex(Rgb c) {
    static constexpr char d[] = "0123456789abcdef";
    std::string s = "#";
    for (const std::uint8_t v : {c.r, c.g, c.b}) {
        s += d[v >> 4];
        s += d[v & 15];
    }
    return s;
}

// ---- the console format -------------------------------------------------------------------

enum class Kind : std::uint8_t { Color, Bool, Int, Text };

struct Option {
    std::string_view key;
    Kind kind;
    std::string_view fallback; // the default, as stored
    std::string_view help;     // one line for the file's comments
};

// Every option, in the order of the file and of the GUI's dialog.
inline constexpr std::array<Option, 26> kOptions{{
    {"font_family", Kind::Text, "", "monospace family; empty: the embedded Fira Code"},
    {"font_scale", Kind::Int, "100", "text size, % of the default (60-140)"},
    {"title", Kind::Text, "MINI-TERMINAL · Fira Code Engine", "the window title"},
    {"background", Kind::Color, "#0e0e1a", "console background"},
    {"title_background", Kind::Color, "#1c1c32", "title bar"},
    {"border", Kind::Color, "#6464a0", "window border and frame"},
    {"title_color", Kind::Color, "#00ffc8", "title text"},
    {"traffic_lights", Kind::Bool, "true", "the three window buttons in the title bar"},
    {"app_icon", Kind::Bool, "true", "the application icon beside the title"},
    {"track", Kind::Color, "#28283e", "empty part of the progress bar"},
    {"gradient_start", Kind::Color, "#00fff0", "progress gradient, left"},
    {"gradient_middle", Kind::Color, "#ff00c8", "progress gradient, middle"},
    {"gradient_end", Kind::Color, "#ffd700", "progress gradient, right"},
    {"shimmer", Kind::Bool, "true", "the light that travels along the progress bar"},
    {"prompt", Kind::Color, "#00fff0", "prompt mark and cursor"},
    {"blink_cursor", Kind::Bool, "true", "the cursor blinks"},
    {"action", Kind::Color, "#ffd700", "FLATTEN / FILE RENAME / DIR RENAME"},
    {"bold_actions", Kind::Bool, "true", "actions in bold"},
    {"directory", Kind::Color, "#788cc8", "directories of a path"},
    {"name", Kind::Color, "#e4e6f2", "file names, progress text"},
    {"extension", Kind::Color, "#00d7ff", "file extensions"},
    {"number", Kind::Color, "#ff79c6", "digits in names"},
    {"arrow", Kind::Color, "#ffd700", "the arrow before a new name"},
    {"new_name", Kind::Color, "#64ff96", "new names"},
    {"note", Kind::Color, "#8c8caf", "statistics and older lines' marks"},
    {"error", Kind::Color, "#ff465a", "errors"},
}};

inline const Option* find_option(std::string_view key) {
    const auto it = std::ranges::find(kOptions, key, &Option::key);
    return it == kOptions.end() ? nullptr : &*it;
}

// The stored values (strings, as in the file), every key present.
class ConsoleStyle {
    std::map<std::string, std::string, std::less<>> values_;

public:
    ConsoleStyle() {
        for (const Option& o : kOptions) values_[std::string(o.key)] = o.fallback;
    }

    // $FSTURBO_CONSOLE_STYLE, else $XDG_CONFIG_HOME/fsturbotransform/console.conf (~/.config/…)
    static std::filesystem::path default_path() {
        if (const char* env = std::getenv("FSTURBO_CONSOLE_STYLE"); env && *env) return env;
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        const std::filesystem::path dir = xdg && *xdg ? std::filesystem::path(xdg) : home ? std::filesystem::path(home) / ".config" : std::filesystem::path(".");
        return dir / "fsturbotransform" / "console.conf";
    }

    // Missing file or keys keep the defaults; unknown keys are ignored.
    static ConsoleStyle load(const std::filesystem::path& path) {
        ConsoleStyle s;
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            const std::string_view l = detail::trim(line);
            if (l.empty() || l.starts_with('#')) continue;
            const std::size_t eq = l.find('=');
            if (eq == std::string_view::npos) continue;
            s.set(detail::trim(l.substr(0, eq)), detail::trim(l.substr(eq + 1)));
        }
        return s;
    }

    bool save(const std::filesystem::path& path) const {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        const std::filesystem::path tmp = path.string() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out) return false;
            out << "# fsturbotransform: the mini-console's format (terminal dashboard and GUI).\n"
                   "# Edited by fsturbotransform-gui (Console format). Colours: #rrggbb, #aarrggbb, r,g,b or the\n"
                   "# colorspec JSON {\"src\":\"system\",\"sys\":\"highlightColor\",\"l\":0,\"c\":0,\"a\":100}.\n";
            for (const Option& o : kOptions) out << "\n# " << o.help << "\n" << o.key << " = " << get(o.key) << "\n";
            if (!out) return false;
        }
        std::filesystem::rename(tmp, path, ec);
        return !ec;
    }

    std::string get(std::string_view key) const {
        const auto it = values_.find(key);
        return it == values_.end() ? std::string() : it->second;
    }

    // Unknown keys are refused; values are kept as given (bad colours read as the default's colour).
    bool set(std::string_view key, std::string_view value) {
        if (!find_option(key)) return false;
        values_[std::string(key)] = std::string(value);
        return true;
    }

    bool flag(std::string_view key) const {
        const std::string v = get(key);
        return v == "true" || v == "1" || v == "yes" || v == "on";
    }

    int number(std::string_view key, int lo, int hi) const {
        const std::string v = get(key);
        const Option* o = find_option(key);
        char* end = nullptr;
        const long n = std::strtol(v.c_str(), &end, 10);
        if (end == v.c_str()) return std::atoi(std::string(o->fallback).c_str());
        return static_cast<int>(std::clamp<long>(n, lo, hi));
    }

    Rgbaf color(std::string_view key, const SystemColors& system) const {
        const std::string v = get(key);
        if (!v.empty() && (v.front() == '{' || parse_color(v))) return resolve(parse_spec(v), system);
        return resolve(parse_spec(find_option(key)->fallback), system);
    }
};

// ---- what the renderers use ---------------------------------------------------------------

struct ConsolePalette {
    Rgb background = palette::console_bg, title_background = palette::console_title_bg, border = palette::console_border,
        title_color = palette::console_title, track = palette::track;
    std::array<Rgb, 3> gradient{palette::neon_cyan, palette::neon_magenta, palette::neon_gold};
    Rgb prompt = palette::neon_cyan, action = palette::action, directory = palette::path_dir, name = palette::path_name,
        extension = palette::path_ext, number = palette::number, arrow = palette::arrow, new_name = palette::dest, note = palette::note,
        error = palette::error;
    bool traffic_lights = true, app_icon = true, shimmer = true, blink_cursor = true, bold_actions = true;
    float font_scale = 1.0f;
    std::string font_family;
    std::string title = "MINI-TERMINAL · Fira Code Engine";

    // Translucent colours are laid over the console background (the title bar's colours over the
    // title bar), since a terminal cannot blend text with what is under it.
    static ConsolePalette from(const ConsoleStyle& s, const SystemColors& system) {
        ConsolePalette p;
        const Rgb bg = to_rgb(s.color("background", system), Rgb{0, 0, 0});
        const auto c = [&](std::string_view key, Rgb under) { return to_rgb(s.color(key, system), under); };
        p.background = bg;
        p.title_background = c("title_background", bg);
        p.border = c("border", bg);
        p.title_color = c("title_color", p.title_background);
        p.track = c("track", bg);
        p.gradient = {c("gradient_start", p.track), c("gradient_middle", p.track), c("gradient_end", p.track)};
        p.prompt = c("prompt", bg);
        p.action = c("action", bg);
        p.directory = c("directory", bg);
        p.name = c("name", bg);
        p.extension = c("extension", bg);
        p.number = c("number", bg);
        p.arrow = c("arrow", bg);
        p.new_name = c("new_name", bg);
        p.note = c("note", bg);
        p.error = c("error", bg);
        p.traffic_lights = s.flag("traffic_lights");
        p.app_icon = s.flag("app_icon");
        p.shimmer = s.flag("shimmer");
        p.blink_cursor = s.flag("blink_cursor");
        p.bold_actions = s.flag("bold_actions");
        p.font_scale = static_cast<float>(s.number("font_scale", 60, 140)) / 100.0f;
        p.font_family = s.get("font_family");
        p.title = s.get("title");
        return p;
    }
};

// The palette every renderer reads; set once, before the first frame.
inline ConsolePalette& active() {
    static ConsolePalette p;
    return p;
}

} // namespace fsturbo::style

#endif // CONSOLE_STYLE_HPP
