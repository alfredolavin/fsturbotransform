#ifndef FIRA_CODE_FONT_HPP
#define FIRA_CODE_FONT_HPP

#include <cstddef>
#include <span>
#include <string_view>

namespace fsturbo {

inline constexpr std::string_view FIRA_CODE_FONT_NAME = "Fira Code Regular";
inline constexpr std::string_view FIRA_CODE_VERSION = "6.2";

// C++26 #embed (P1967): the variable-weight Fira Code TTF is baked into the binary
// and rasterized at runtime by ttf_font.hpp for the Sixel UI.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
inline constexpr unsigned char fira_code_ttf[] = {
#embed "FiraCode-Regular.ttf"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

inline constexpr std::size_t fira_code_ttf_len = sizeof(fira_code_ttf);

inline constexpr std::span<const unsigned char> fira_code_bytes{fira_code_ttf};

static_assert(fira_code_ttf_len > 12 && fira_code_ttf[0] == 0x00 && fira_code_ttf[1] == 0x01,
              "embedded asset is not a TrueType (glyf) font");

} // namespace fsturbo

#endif // FIRA_CODE_FONT_HPP
