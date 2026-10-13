// Unit tests of the console format (src/console_style.hpp): colour notations, the colorspec JSON
// with OKLCH adjustments (reference values from plasmoids/shared/colorspec/ColorSpecCore.js under
// node), system colours from a kdeglobals, defaults equal to the built-in palette, file round trip.
// Run: ctest --test-dir build -R console_style
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include "console_style.hpp"

using namespace fsturbo;
namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool ok, std::string_view what) {
    if (!ok) ++failures;
    std::println("{} {}", ok ? "ok  " : "FAIL", what);
}

bool near(const style::Rgbaf& c, int r, int g, int b, double a = 1.0) {
    const auto ch = [](double v) { return static_cast<int>(std::lround(v * 255)); };
    return std::abs(ch(c.r) - r) <= 1 && std::abs(ch(c.g) - g) <= 1 && std::abs(ch(c.b) - b) <= 1 && std::abs(c.a - a) < 0.01;
}

} // namespace

int main() {
    const style::SystemColors breeze;
    const auto colour = [&](std::string_view spec) { return style::resolve(style::parse_spec(spec), breeze); };

    check(near(colour("#3daee9"), 61, 174, 233), "#rrggbb");
    check(near(colour("#803daee9"), 61, 174, 233, 128 / 255.0), "#aarrggbb (Qt's order)");
    check(near(colour("61,174,233"), 61, 174, 233), "r,g,b (KConfig)");
    // ColorSpecCore.js: apply({l:20}, #3daee9) = 89,192,250; ({l:-40,c:50,h:30,a:50}, #ff00c8) = 199,0,9 a .5;
    // ({c:-100}, #00fff0) = 222,222,222
    check(near(colour(R"({"src":"fixed","color":"#ff3daee9","l":20,"c":0,"a":100})"), 89, 192, 250), "lightness +20 as in ColorSpecCore.js");
    check(near(colour(R"({"src":"fixed","color":"#ffff00c8","l":-40,"c":50,"h":30,"a":50})"), 199, 0, 9, 0.5), "l, c, h and opacity together");
    check(near(colour(R"({"src":"fixed","color":"#ff00fff0","l":0,"c":-100,"a":100})"), 222, 222, 222), "chroma -100 is grey");
    check(near(colour(R"({"src":"system","color":"#ffffffff","sys":"highlightColor","l":0,"c":0,"a":100})"), 61, 174, 233), "system role (Breeze defaults)");

    const fs::path dir = fs::temp_directory_path() / "fsturbo_console_style_test";
    fs::create_directories(dir);
    {
        std::ofstream kde(dir / "kdeglobals");
        kde << "[Colors:Window]\nForegroundNormal=252,252,252\nBackgroundNormal=32,35,38\n[Colors:Selection]\nBackgroundNormal=200,10,20\n";
    }
    ::setenv("XDG_CONFIG_HOME", dir.c_str(), 1);
    const style::SystemColors dark = style::SystemColors::load();
    check(near(dark.get("textColor"), 252, 252, 252) && near(dark.get("backgroundColor"), 32, 35, 38), "kdeglobals Window colours");
    check(near(dark.get("highlightColor"), 200, 10, 20), "kdeglobals Selection colour is the highlight");
    check(near(dark.get("linkColor"), 41, 128, 185), "roles the file does not set keep Breeze's");

    const style::ConsolePalette defaults = style::ConsolePalette::from(style::ConsoleStyle(), breeze);
    const style::ConsolePalette builtin;
    check(defaults.background == builtin.background && defaults.gradient == builtin.gradient && defaults.new_name == builtin.new_name &&
              defaults.note == builtin.note && defaults.title == builtin.title,
          "the default format is the built-in palette");

    style::ConsoleStyle s;
    check(s.set("background", R"({"src":"system","sys":"backgroundColor","l":-20,"c":0,"a":100})") && !s.set("nonsense", "1"), "known keys only");
    s.set("font_scale", "500");
    s.set("traffic_lights", "false");
    check(s.save(dir / "console.conf"), "save");
    const style::ConsoleStyle back = style::ConsoleStyle::load(dir / "console.conf");
    check(back.get("background") == s.get("background") && !back.flag("traffic_lights"), "load gives back what was saved");
    check(back.number("font_scale", 60, 140) == 140, "font_scale is clamped");
    const style::ConsolePalette p = style::ConsolePalette::from(back, dark);
    const style::Rgbaf darker = style::resolve(style::parse_spec(R"({"src":"fixed","color":"#ff202326","l":-20})"), dark);
    check(p.background == style::to_rgb(darker, Rgb{}), "a system colour follows the scheme it is resolved with");
    fs::remove_all(dir);

    std::println("{}", failures == 0 ? "console_style: all passed" : "console_style: FAILED");
    return failures == 0 ? 0 : 1;
}
