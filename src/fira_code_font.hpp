#ifndef FIRA_CODE_FONT_HPP
#define FIRA_CODE_FONT_HPP

#include <cstddef>
#include <string_view>

namespace fsturbo {

inline constexpr std::string_view FIRA_CODE_FONT_NAME = "Fira Code Regular";
inline constexpr std::string_view FIRA_CODE_VERSION = "6.2";

inline constexpr unsigned char fira_code_ttf[] = {
#embed "FiraCode-Regular.ttf"
};

inline constexpr size_t fira_code_ttf_len = sizeof(fira_code_ttf);

} // namespace fsturbo

#endif // FIRA_CODE_FONT_HPP
