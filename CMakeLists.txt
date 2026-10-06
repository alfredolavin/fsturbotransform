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
    src/execution_stats.hpp
    src/graphics.hpp
    src/fira_code_font.hpp
    src/js_bridge_api.h
    src/js_engine.hpp
    src/matcher.hpp
    src/named_groups.hpp
    src/pcre2_regex.hpp
    src/renamer.hpp
    src/replace_template.hpp
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

# PCRE2 (libpcre2-dev) is the regex engine, used as a system library; dlopen is how the
# JavaScript bridge below is loaded on first use.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2 REQUIRED IMPORTED_TARGET libpcre2-8)
find_package(ZLIB REQUIRED) # compresses Kitty images
target_link_libraries(fsturbotransform PRIVATE PkgConfig::PCRE2 ZLIB::ZLIB ${CMAKE_DL_LIBS})
# Where the bridge gets installed: also searched at run time (bin/ may be a symlink).
target_compile_definitions(fsturbotransform PRIVATE FSTURBO_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib/fsturbotransform")

# JavaScript expression bridge: V8 used as a library through libnode (libnode-dev), loaded
# with dlopen only when a replacement contains an expression. It builds next to the executable
# (which looks beside itself first) and installs to <prefix>/lib/fsturbotransform. Without the V8
# headers it is skipped and expression replacements report an error at run time.
option(FSTURBO_JS "Build libfsturbo_js.so, which evaluates -r replacement expressions (needs libnode-dev)" ON)
if(FSTURBO_JS)
    find_path(V8_INCLUDE_DIR v8.h PATH_SUFFIXES node)
    find_library(V8_LIBRARY NAMES node)
    if(V8_INCLUDE_DIR AND V8_LIBRARY)
        add_library(fsturbo_js SHARED src/js_bridge.cpp src/js_bridge_api.h)
        target_include_directories(fsturbo_js SYSTEM PRIVATE ${V8_INCLUDE_DIR})
        target_include_directories(fsturbo_js PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        # No -ffast-math here: the bridge's NaN/Infinity checks must stay meaningful.
        target_compile_options(fsturbo_js PRIVATE -O2 -Wall -Wextra)
        target_link_libraries(fsturbo_js PRIVATE ${V8_LIBRARY})
        set_target_properties(fsturbo_js PROPERTIES CXX_VISIBILITY_PRESET hidden)
        add_dependencies(fsturbotransform fsturbo_js)
        install(TARGETS fsturbo_js LIBRARY DESTINATION lib/fsturbotransform)
    else()
        message(WARNING "V8 headers/libnode not found (libnode-dev): building without libfsturbo_js.so")
    endif()
endif()

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
