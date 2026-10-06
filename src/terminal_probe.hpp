#ifndef TERMINAL_PROBE_HPP
#define TERMINAL_PROBE_HPP

// Terminal capability detection: window/cell geometry, Sixel support (DA1 attribute 4)
// and background color (OSC 11), queried on the controlling tty with a short timeout.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "color.hpp"

namespace fsturbo::term {

struct TermInfo {
    int cols = 80;
    int rows = 24;
    int cell_w = 0; // pixels; 0 = unknown
    int cell_h = 0;
    bool sixel = false;
    bool kitty = false; // answered the Kitty graphics query
    int color_registers = 256; // Sixel colour registers; 256 when the terminal does not say
    std::optional<Rgb> background;

    // Conservative fallbacks keep reserved rows >= image rows when the size is unknown.
    int cell_width() const { return cell_w > 0 ? cell_w : 8; }
    int cell_height() const { return cell_h > 0 ? cell_h : 16; }
};

namespace detail {

constexpr std::vector<int> parse_params(std::string_view s) {
    std::vector<int> out;
    int v = 0;
    bool any = false;
    for (const char ch : s) {
        if (ch >= '0' && ch <= '9') {
            v = v * 10 + (ch - '0');
            any = true;
        } else if (ch == ';') {
            out.push_back(any ? v : 0);
            v = 0;
            any = false;
        }
    }
    if (any) out.push_back(v);
    return out;
}

// DA1 reply: ESC [ ? Ps ; ... c  — returns its parameter string once complete. Other private
// replies (the colour-register one, ESC [ ? 1 ; 0 ; n S) may come before it: skip them.
constexpr std::optional<std::string_view> find_da1(std::string_view r) {
    for (std::size_t at = r.find("\x1b[?"); at != std::string_view::npos; at = r.find("\x1b[?", at + 3)) {
        const std::size_t end = r.find_first_not_of("0123456789;", at + 3);
        if (end != std::string_view::npos && r[end] == 'c') return r.substr(at + 3, end - at - 3);
    }
    return std::nullopt;
}

// OSC 11 reply: ESC ] 11 ; rgb:RRRR/GGGG/BBBB (BEL | ST), 1-4 hex digits per channel.
constexpr std::optional<Rgb> find_background(std::string_view r) {
    const std::size_t at = r.find("\x1b]11;rgb:");
    if (at == std::string_view::npos) return std::nullopt;
    std::size_t p = at + 9;
    std::uint8_t ch[3]{};
    for (int i = 0; i < 3; ++i) {
        unsigned v = 0, digits = 0;
        while (p < r.size() && digits < 4) {
            const char c = r[p];
            const int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
            if (d < 0) break;
            v = v * 16 + static_cast<unsigned>(d);
            ++digits;
            ++p;
        }
        if (digits == 0) return std::nullopt;
        const unsigned max = (1u << (4 * digits)) - 1;
        ch[i] = static_cast<std::uint8_t>((v * 255 + max / 2) / max);
        if (i < 2) {
            if (p >= r.size() || r[p] != '/') return std::nullopt;
            ++p;
        }
    }
    return Rgb{ch[0], ch[1], ch[2]};
}

// XTWINOPS cell size reply: ESC [ 6 ; height ; width t
constexpr std::optional<std::pair<int, int>> find_cell_size(std::string_view r) {
    const std::size_t at = r.find("\x1b[6;");
    if (at == std::string_view::npos) return std::nullopt;
    const std::size_t end = r.find('t', at);
    if (end == std::string_view::npos) return std::nullopt;
    const auto p = parse_params(r.substr(at + 2, end - at - 2));
    if (p.size() != 3 || p[1] <= 0 || p[2] <= 0) return std::nullopt;
    return std::pair{p[2], p[1]};
}

// Kitty graphics reply to our query (image id 31): an APC string G i=31 ; OK, ended by ST.
constexpr bool find_kitty_graphics(std::string_view r) { return r.find("\x1b_Gi=31;OK") != std::string_view::npos; }

// XTSMGRAPHICS reply for the Sixel colour registers: ESC [ ? 1 ; 0 ; count S
constexpr std::optional<int> find_color_registers(std::string_view r) {
    const std::size_t at = r.find("\x1b[?1;0;");
    if (at == std::string_view::npos) return std::nullopt;
    const std::size_t end = r.find_first_not_of("0123456789", at + 7);
    if (end == at + 7 || end == std::string_view::npos || r[end] != 'S') return std::nullopt;
    const auto p = parse_params(r.substr(at + 7, end - at - 7));
    if (p.size() != 1 || p[0] <= 0) return std::nullopt;
    return p[0];
}

static_assert(find_da1("\x1b[?62;4;22c").value() == "62;4;22");
static_assert(find_kitty_graphics("\x1b_Gi=31;OK\x1b\\\x1b[?62;1;4c") && !find_kitty_graphics("\x1b_Gi=31;ENOTSUPPORTED\x1b\\"));
static_assert(find_color_registers("\x1b[?1;0;1024S") == 1024 && find_color_registers("junk\x1b[?1;0;256S\x1b[?62;4c") == 256);
static_assert(!find_color_registers("\x1b[?1;3;0S") && !find_color_registers("\x1b[?1;0;S") && !find_color_registers("\x1b[?2;0;5S"));
static_assert(!find_da1("\x1b[?62;4"));
static_assert(find_da1("\x1b[?1;0;256S\x1b[?62;1;4c").value() == "62;1;4"); // what Konsole sends
static_assert(!find_da1("\x1b[?1;0;256S") && !find_da1("\x1b[?1;0;256S\x1b[?62;1;4"));
static_assert(find_background("\x1b]11;rgb:1e1e/1e1e/2e2e\x1b\\") == Rgb{30, 30, 46});
static_assert(find_background("\x1b]11;rgb:ff/80/00\a") == Rgb{255, 128, 0});
static_assert(find_cell_size("\x1b[6;20;10t") == std::pair{10, 20});

} // namespace detail

inline TermInfo probe_terminal(bool query) {
    TermInfo info;
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > 0) info.cols = ws.ws_col;
        if (ws.ws_row > 0) info.rows = ws.ws_row;
        if (ws.ws_xpixel > 0 && ws.ws_col > 0) info.cell_w = ws.ws_xpixel / ws.ws_col;
        if (ws.ws_ypixel > 0 && ws.ws_row > 0) info.cell_h = ws.ws_ypixel / ws.ws_row;
    }
    if (!query) return info;

    const int fd = open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) return info;
    // Never touch termios from a background job (it would stop us with SIGTTOU).
    termios saved{};
    if (tcgetpgrp(fd) != getpgrp() || tcgetattr(fd, &saved) != 0) {
        close(fd);
        return info;
    }
    termios raw = saved;
    raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &raw);

    // DA1 goes last: every terminal answers it, so its reply marks the end of the batch.
    // Queries: background colour, cell size, number of Sixel colour registers, Kitty graphics
    // (a 1x1 pixel it must accept), then DA1.
    constexpr std::string_view queries = "\x1b]11;?\x1b\\\x1b[16t\x1b[?1;1;0S\x1b_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\x1b\\\x1b[c";
    std::string reply;
    if (write(fd, queries.data(), queries.size()) == static_cast<ssize_t>(queries.size())) {
        using clock = std::chrono::steady_clock;
        const auto deadline = clock::now() + std::chrono::milliseconds(400);
        char buf[256];
        while (!detail::find_da1(reply)) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
            pollfd pfd{fd, POLLIN, 0};
            if (left <= 0 || poll(&pfd, 1, static_cast<int>(left)) <= 0) break;
            const ssize_t n = read(fd, buf, sizeof buf);
            if (n <= 0) break;
            reply.append(buf, static_cast<std::size_t>(n));
        }
    }
    tcsetattr(fd, TCSANOW, &saved);
    close(fd);

    if (const auto da1 = detail::find_da1(reply)) {
        const auto params = detail::parse_params(*da1);
        info.sixel = std::ranges::contains(params, 4);
    }
    if (const auto cell = detail::find_cell_size(reply)) {
        info.cell_w = cell->first;
        info.cell_h = cell->second;
    }
    if (const auto registers = detail::find_color_registers(reply)) info.color_registers = *registers;
    info.kitty = detail::find_kitty_graphics(reply);
    info.background = detail::find_background(reply);
    return info;
}

} // namespace fsturbo::term

#endif // TERMINAL_PROBE_HPP
