cmake_minimum_required(VERSION 3.25)
project(fsturbotransform VERSION 2.0 LANGUAGES CXX)

# C++26 with #embed: GCC >= 15 or Clang >= 19
if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15) OR
   (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19))
    message(FATAL_ERROR "fsturbotransform needs C++26 #embed: GCC >= 15 or Clang >= 19 "
                        "(found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}). "
                        "Select one with -DCMAKE_CXX_COMPILER=...")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default install prefix to ~/.local if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Default install path (~/.local)" FORCE)
endif()

include(FetchContent)

# Fetch FiraCode font asset if not present in source tree
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/FiraCode-Regular.ttf")
    message(STATUS "Fetching Fira Code TrueType font asset...")
    FetchContent_Declare(
        firacode_font
        URL "https://github.com/google/fonts/raw/main/ofl/firacode/FiraCode%5Bwght%5D.ttf"
        DOWNLOAD_NO_EXTRACT TRUE
    )
    FetchContent_MakeAvailable(firacode_font)
    if(EXISTS "${firacode_font_SOURCE_DIR}/FiraCode[wght].ttf")
        file(COPY_FILE "${firacode_font_SOURCE_DIR}/FiraCode[wght].ttf" "${CMAKE_CURRENT_SOURCE_DIR}/src/FiraCode-Regular.ttf")
    endif()
endif()

# Source files
set(SOURCES
    src/main.cpp
)

set(HEADERS
    src/app_icon.hpp
    src/banner.hpp
    src/case_converter.hpp
    src/cli_parser.hpp
    src/color.hpp
    src/console_window.hpp
    src/dashboard.hpp
    src/fira_code_font.hpp
    src/matcher.hpp
    src/renamer.hpp
    src/report_view.hpp
    src/sixel_renderer.hpp
    src/terminal_probe.hpp
    src/terminal_style.hpp
    src/ttf_font.hpp
)

# Executable target
add_executable(fsturbotransform ${SOURCES} ${HEADERS})

# #embed is not tracked by every dependency scanner: rebuild when an embedded asset changes
set_source_files_properties(src/main.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/FiraCode-Regular.ttf;${CMAKE_CURRENT_SOURCE_DIR}/src/app_icon.rgba")

# Include directories (needed for #embed to locate embedded assets)
target_include_directories(fsturbotransform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler optimization and monolithic static flags
target_compile_options(fsturbotransform PRIVATE
    -O3
    -march=native
    -flto
    -ffast-math
    -pthread
    -Wall
    -Wextra
)

target_link_options(fsturbotransform PRIVATE
    -static-libstdc++
    -static-libgcc
    -pthread
    -flto
)

# Standard installation into <prefix>/bin (default: ~/.local/bin)
install(TARGETS fsturbotransform
    RUNTIME DESTINATION bin
)

# Custom helper target for explicit local installation
add_custom_target(install_local
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix $ENV{HOME}/.local
    COMMENT "Installing fsturbotransform directly to $ENV{HOME}/.local/bin"
    DEPENDS fsturbotransform
)
