#ifndef DASHBOARD_HPP
#define DASHBOARD_HPP

// In-place live dashboard: animated gradient progress bar + reusable mini-console.
//   Full mode           -> one Sixel image per frame, text set in the embedded Fira Code
//   Terminal/Simple mode -> ANSI text frame (TrueColor or 16-color)
// The frame occupies a fixed block of reserved rows that is redrawn from a saved cursor
// position (DECSC/DECRC), so the terminal never scrolls while work is in progress. The console
// takes every row the final report (see ui::plan_screen) leaves free: the whole terminal height
// when there is no report.
// Optional log lines (--verbose, errors) are flushed above the frame in batches.

#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <deque>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <unistd.h>
#include "console_window.hpp"
#include "report_view.hpp"
#include "sixel_renderer.hpp"
#include "terminal_style.hpp"

namespace fsturbo::term {

enum class Stream : std::uint8_t { Out, Err };

namespace detail {

// Reserved rows of the live frame, read by the (async-signal-safe) interrupt handler.
inline volatile std::sig_atomic_t g_live_rows = 0;

inline void restore_cursor_and_reraise(int sig) {
    char seq[32];
    std::size_t n = 0;
    auto put = [&](std::string_view s) {
        for (const char c : s) seq[n++] = c;
    };
    put("\x1b\\"); // terminate a Sixel DCS that may have been cut off mid-frame
    if (const int below = g_live_rows - 1; below > 0 && below < 100) {
        put("\x1b" "8\x1b[");
        if (below >= 10) seq[n++] = static_cast<char>('0' + below / 10);
        seq[n++] = static_cast<char>('0' + below % 10);
        put("B");
    }
    put("\r\n\x1b[?25h");
    auto _ = write(STDOUT_FILENO, seq, n);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

inline constexpr std::string_view kSpaces = "                                                                ";

inline std::string format_rate(double per_sec) {
    if (per_sec >= 1000.0) return std::format("{:.1f}k/s", per_sec / 1000.0);
    return std::format("{:.0f}/s", per_sec);
}

inline std::string format_elapsed(std::chrono::steady_clock::duration d) {
    const auto s = std::chrono::duration_cast<std::chrono::seconds>(d).count();
    return std::format("{:02}:{:02}", s / 60, s % 60);
}

} // namespace detail

class LiveDashboard {
    using clock = std::chrono::steady_clock;

    struct ConsoleLine {
        std::string action, source, dest;
        bool changed = false;
    };

    // A console line laid out for a given column budget; spans point into the owned strings.
    struct LaidOutLine {
        std::string src, dest;
        std::vector<Span> spans;
        std::size_t cols = 0;
    };

    static constexpr std::string_view kTitle = "MINI-TERMINAL · Fira Code Engine";

    bool configured_ = false;
    bool interactive_ = false;
    bool sixel_ = false;
    bool attached_ = false;
    ui::ScreenPlan plan_;
    int text_lines_ = ui::kMinConsoleLines;
    int frame_rows_ = ui::kMinConsoleLines + 3; // progress + console (title, text lines, bottom edge)
    int reserved_rows_ = frame_rows_;
    int width_cols_ = 80;
    std::chrono::milliseconds interval_{33};

    std::string phase_;
    std::size_t total_ = 0;
    std::size_t current_ = 0;
    clock::time_point run_start_{}, phase_start_{}, next_draw_{};

    std::deque<ConsoleLine> history_; // changes only, the newest at the back; text_lines_ - 1 of them are shown
    ConsoleLine active_;
    bool has_active_ = false;
    std::vector<std::pair<std::string, Stream>> pending_logs_;
    std::vector<LaidOutLine> laid_; // one per text line: history above, the active line last

    using SignalHandler = void (*)(int);
    SignalHandler prev_int_ = SIG_DFL, prev_term_ = SIG_DFL;

    // Sixel backend state
    struct Geometry {
        int w = 0, h = 0, cell_h = 0, adv = 0;
        float bar_x = 0, bar_y = 0, bar_w = 0, bar_h = 0;
        int row0_baseline = 0, progress_text_x = 0, progress_cols = 0;
        int win_y = 0, title_baseline = 0, text_x = 0, text_cols = 0;
        std::vector<int> line_baseline;
    } geo_;
    std::optional<ui::ConsoleFonts> fonts_;
    sixel::Canvas base_, frame_;

    void configure() {
        configured_ = true;
        interactive_ = ui::dashboard_fits();
        if (!interactive_) return;
        sixel_ = g_color_mode == ColorMode::Full;
        text_lines_ = std::max(ui::kMinConsoleLines, plan_.console_lines);
        frame_rows_ = text_lines_ + 3;
        reserved_rows_ = frame_rows_;
        laid_.resize(static_cast<std::size_t>(text_lines_));
        width_cols_ = ui::window_columns();
        interval_ = std::chrono::milliseconds(sixel_ ? 50 : 33);
        if (sixel_) build_sixel_chrome();
    }

    // ---- shared layout -----------------------------------------------------------

    void layout_line(const ConsoleLine& l, bool active, std::size_t cols, LaidOutLine& out) const {
        out.spans.clear();
        out.spans.push_back({active ? "› " : "· ", active ? Role::Prompt : Role::Note});
        const std::size_t action_cols = std::min<std::size_t>(ttf::utf8_length(l.action), 11);
        out.spans.push_back({std::string_view(l.action).substr(0, action_cols), Role::Action});
        out.spans.push_back({detail::kSpaces.substr(0, 12 - action_cols), Role::Plain});

        const std::size_t rem = cols > 14 ? cols - 14 : 0;
        std::size_t dest_budget = 0;
        if (!l.dest.empty()) {
            dest_budget = std::min(ttf::utf8_length(l.dest), std::max<std::size_t>(8, rem > 3 ? (rem - 3) / 2 : 0));
            dest_budget = std::min(dest_budget, rem > 4 ? rem - 4 : 0);
        }
        const std::size_t src_budget = rem - (l.dest.empty() ? 0 : std::min(rem, 3 + dest_budget));
        out.src = fit_left(l.source, src_budget);
        highlight_path(out.src, Role::Name, out.spans);
        if (!l.dest.empty() && dest_budget > 0) {
            out.dest = fit_right(l.dest, dest_budget);
            out.spans.push_back({" → ", Role::Arrow});
            highlight_path(out.dest, Role::Dest, out.spans);
        }
        out.cols = 0;
        for (const Span& s : out.spans) out.cols += ttf::utf8_length(s.text);
    }

    void layout_console(std::size_t cols) {
        auto lay = [&](LaidOutLine& slot, const ConsoleLine* line, bool active) {
            if (line) {
                layout_line(*line, active, cols, slot);
            } else {
                slot.spans.clear();
                if (active) slot.spans.push_back({"› ", Role::Prompt});
                slot.cols = active ? 2 : 0;
            }
        };
        const std::size_t last = laid_.size() - 1;
        for (std::size_t slot = 0; slot < last; ++slot) {
            const std::size_t age = last - 1 - slot; // 0 = the entry right above the active line
            lay(laid_[slot], age < history_.size() ? &history_[history_.size() - 1 - age] : nullptr, false);
        }
        lay(laid_[last], has_active_ ? &active_ : nullptr, true);
    }

    float ratio() const { return total_ > 0 ? std::min(1.0f, static_cast<float>(current_) / static_cast<float>(total_)) : 1.0f; }

    std::string stats_text(clock::time_point now) const {
        const double secs = std::chrono::duration<double>(now - phase_start_).count();
        const double rate = secs > 0.0 ? static_cast<double>(current_) / secs : 0.0;
        return std::format("{} · {}", detail::format_rate(rate), detail::format_elapsed(now - run_start_));
    }

    std::string progress_text() const {
        return std::format("{:<7} {:5.1f}%  {}/{}", phase_, ratio() * 100.0f, current_, total_);
    }

    // ---- ANSI backend ------------------------------------------------------------------

    std::string render_ansi(clock::time_point now) {
        const int W = width_cols_;
        std::string out;
        out.reserve(4096);
        const std::string border = rgb_fg(palette::console_border);
        const std::string reset_s(reset());

        // Row 0: animated gradient bar with eighth-block sub-cell resolution and a moving shimmer.
        const int bar_w = std::clamp(W / 3, 10, 40);
        const float exact = ratio() * static_cast<float>(bar_w);
        const int full = static_cast<int>(exact);
        const int eighths = static_cast<int>((exact - static_cast<float>(full)) * 8.0f);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - run_start_).count();
        const float shimmer = static_cast<float>(ms % 1600) / 1600.0f * static_cast<float>(bar_w + 8) - 4.0f;
        static constexpr std::array<std::string_view, 8> partial{" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};

        out += "\x1b[2K ";
        out += border;
        out += "▕";
        for (int i = 0; i < bar_w; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(bar_w - 1);
            Rgb c = gradient(t, palette::neon_cyan, palette::neon_magenta, palette::neon_gold);
            const float glow = std::max(0.0f, 1.0f - std::fabs(static_cast<float>(i) - shimmer) / 3.0f);
            c = lerp(c, Rgb{255, 255, 255}, glow * 0.55f);
            if (i < full) {
                out += rgb_fg(c);
                out += "█";
            } else if (i == full && eighths > 0) {
                out += rgb_fg(c);
                out += rgb_bg(palette::track);
                out += partial[static_cast<std::size_t>(eighths)];
                out += reset_s;
            } else {
                out += rgb_fg(palette::track);
                out += "█";
            }
        }
        out += border;
        out += "▏";
        out += reset_s;
        out += std::format(" {}{}{:<7}{} {}{:5.1f}%{}  {}{}/{}{}", bold(), rgb_fg(palette::action), phase_, reset_s,
                           rgb_fg(palette::path_name), ratio() * 100.0f, reset_s, rgb_fg(palette::note), current_, total_, reset_s);
        out += "\n";

        // Row 1: window top edge with title and live stats
        out += "\x1b[2K";
        out += ui::ansi_window_top(W, kTitle, stats_text(now), palette::note);
        out += "\n";

        // Rows 2..: console history + active line
        const std::size_t inner = static_cast<std::size_t>(W - 4);
        layout_console(inner - 1);
        const bool cursor_on = (ms / 530) % 2 == 0;
        for (std::size_t li = 0; li < laid_.size(); ++li) {
            const LaidOutLine& l = laid_[li];
            std::string content;
            for (const Span& s : l.spans) {
                if (s.role == Role::Action) content += bold();
                if (s.role != Role::Plain) content += rgb_fg(role_color(s.role));
                content += s.text;
                content += reset_s;
            }
            std::size_t used = l.cols;
            if (li + 1 == laid_.size() && used < inner) {
                content += rgb_fg(palette::neon_cyan);
                content += cursor_on ? "▌" : " ";
                content += reset_s;
                ++used;
            }
            out += "\x1b[2K";
            out += ui::ansi_window_row(W, content, used);
            out += "\n";
        }

        // Last row: bottom edge (no trailing newline: the frame must not scroll)
        out += "\x1b[2K";
        out += ui::ansi_window_bottom(W);
        return out;
    }

    // ---- Sixel backend --------------------------------------------------------------------

    void build_sixel_chrome() {
        const int ch = g_term.cell_height();
        const int cw = g_term.cell_width();
        ui::ConsoleFonts& f = fonts_.emplace(ch);

        Geometry& g = geo_;
        g.cell_h = ch;
        g.adv = f.adv;
        g.w = width_cols_ * cw;
        g.h = frame_rows_ * ch;
        reserved_rows_ = frame_rows_ + 1; // spare row absorbs any post-image cursor advance

        auto baseline_in = [&](float top, float height) { return f.baseline_in(top, height); };
        const float W = static_cast<float>(g.w), CH = static_cast<float>(ch);

        g.bar_h = std::max(4.0f, CH * 0.42f);
        g.bar_x = CH * 0.5f;
        g.bar_w = std::max(40.0f, W * 0.42f);
        g.bar_y = (CH - g.bar_h) * 0.5f;
        g.row0_baseline = baseline_in(0.0f, CH);
        g.progress_text_x = static_cast<int>(g.bar_x + g.bar_w) + g.adv;
        g.progress_cols = std::max(0, (g.w - g.progress_text_x - static_cast<int>(CH * 0.5f)) / g.adv);

        g.win_y = static_cast<int>(std::lround(CH * 1.25f));
        g.text_x = g.adv;
        g.text_cols = std::max(10, (g.w - 2 * g.adv) / g.adv);
        const float lines_top = static_cast<float>(g.win_y) + CH * 1.2f;
        g.line_baseline.resize(static_cast<std::size_t>(text_lines_));
        for (int i = 0; i < text_lines_; ++i) g.line_baseline[static_cast<std::size_t>(i)] = baseline_in(lines_top + static_cast<float>(i) * CH, CH);

        base_ = ui::window_canvas(g.w, g.h);
        sixel::Canvas& c = base_;

        // Progress pill
        c.fill_rounded_rect(0.0f, 1.0f, W, CH - 2.0f, (CH - 2.0f) * 0.5f, palette::console_border);
        c.fill_rounded_rect(1.0f, 2.0f, W - 2.0f, CH - 4.0f, (CH - 4.0f) * 0.5f, palette::console_bg);
        c.fill_rounded_rect(g.bar_x, g.bar_y, g.bar_w, g.bar_h, g.bar_h * 0.5f, palette::track);

        // Console window with title bar and traffic lights
        const float wy = static_cast<float>(g.win_y);
        g.title_baseline = ui::draw_console_window(c, f, wy, static_cast<float>(g.h) - wy - 1.0f, kTitle);
        frame_ = base_;
    }

    void draw_spans(const std::vector<Span>& spans, int x, int baseline) {
        for (const Span& s : spans) {
            ttf::GlyphCache& gc = s.role == Role::Action ? fonts_->bold : fonts_->regular;
            x = frame_.draw_text(gc, x, baseline, s.text, role_color(s.role), geo_.adv);
        }
    }

    std::string render_sixel(clock::time_point now) {
        const Geometry& g = geo_;
        frame_ = base_;
        const float elapsed = std::chrono::duration<float>(now - run_start_).count();

        // Gradient fill with glossy top highlight and a travelling shimmer.
        const float fill_w = std::max(ratio() * g.bar_w, ratio() > 0.0f ? g.bar_h : 0.0f);
        if (fill_w > 0.0f) {
            const float span = fill_w + 2.0f * g.bar_h;
            const float shimmer_x = g.bar_x - g.bar_h + std::fmod(elapsed * g.bar_w * 0.55f, span);
            frame_.fill_rounded_rect(g.bar_x, g.bar_y, fill_w, g.bar_h, g.bar_h * 0.5f, [&](int x, int y) {
                const float t = (static_cast<float>(x) - g.bar_x) / g.bar_w;
                Rgb c = gradient(t, palette::neon_cyan, palette::neon_magenta, palette::neon_gold);
                const float d = (static_cast<float>(x) - shimmer_x) / g.bar_h;
                c = lerp(c, Rgb{255, 255, 255}, 0.55f * std::exp(-d * d));
                if (static_cast<float>(y) < g.bar_y + g.bar_h * 0.4f) c = lerp(c, Rgb{255, 255, 255}, 0.18f);
                return c;
            });
        }

        // Progress text: phase (bold) + percentage + counts
        const std::string text = fit_right(progress_text(), static_cast<std::size_t>(g.progress_cols));
        const std::size_t phase_len = std::min(text.size(), phase_.size());
        int x = frame_.draw_text(fonts_->bold, g.progress_text_x, g.row0_baseline, std::string_view(text).substr(0, phase_len), palette::action, g.adv);
        frame_.draw_text(fonts_->regular, x, g.row0_baseline, std::string_view(text).substr(phase_len), palette::path_name, g.adv);

        // Live stats right-aligned in the title bar
        ui::draw_title_status(frame_, *fonts_, g.title_baseline, stats_text(now), palette::note);

        // Console lines + blinking block cursor on the active line
        layout_console(static_cast<std::size_t>(g.text_cols - 1));
        for (std::size_t i = 0; i < laid_.size(); ++i) draw_spans(laid_[i].spans, g.text_x, g.line_baseline[i]);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - run_start_).count();
        if ((ms / 530) % 2 == 0) {
            const int cx = g.text_x + static_cast<int>(laid_.back().cols) * g.adv;
            frame_.draw_text(fonts_->regular, cx, g.line_baseline.back(), "▌", palette::neon_cyan, g.adv);
        }
        return sixel::encode(frame_);
    }

    // ---- frame output -----------------------------------------------------------------------

    void reserve(std::string& out) const {
        out += '\r';
        out.append(static_cast<std::size_t>(reserved_rows_), '\n');
        out += std::format("\x1b[{}A\x1b" "7", reserved_rows_);
    }

    void draw(clock::time_point now) {
        std::string out;
        if (!attached_) {
            attached_ = true;
            prev_int_ = std::signal(SIGINT, detail::restore_cursor_and_reraise);
            prev_term_ = std::signal(SIGTERM, detail::restore_cursor_and_reraise);
            detail::g_live_rows = reserved_rows_;
            out += "\x1b[?25l";
            reserve(out);
        }
        if (!pending_logs_.empty()) {
            out += "\x1b" "8\x1b[J";
            for (auto& [line, stream] : pending_logs_) {
                if (stream == Stream::Out) {
                    out += line;
                    continue;
                }
                std::fwrite(out.data(), 1, out.size(), stdout);
                std::fflush(stdout);
                out.clear();
                std::fputs(line.c_str(), stderr);
                std::fflush(stderr);
            }
            pending_logs_.clear();
            reserve(out);
        }
        out += "\x1b" "8";
        out += sixel_ ? render_sixel(now) : render_ansi(now);
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
        // Adaptive frame pacing: rendering plus a (possibly slow, e.g. remote) terminal
        // draining the frame may use at most ~20% of wall time.
        const auto end = clock::now();
        next_draw_ = end + std::max<clock::duration>(interval_, 4 * (end - now));
    }

public:
    LiveDashboard() = default;
    LiveDashboard(const LiveDashboard&) = delete("the dashboard owns the terminal cursor and its reserved screen rows");
    LiveDashboard& operator=(const LiveDashboard&) = delete("the dashboard owns the terminal cursor and its reserved screen rows");
    ~LiveDashboard() { finish(); }

    bool interactive() const { return interactive_; }

    // How the screen is shared with the final report; call before the first phase starts.
    void set_plan(const ui::ScreenPlan& plan) { plan_ = plan; }

    // Starts (or restarts) a phase; the console area and its history are reused in place.
    void begin_phase(std::string_view phase, std::size_t total) {
        if (!configured_) {
            configure();
            run_start_ = clock::now();
        }
        phase_.assign(phase);
        total_ = total;
        current_ = 0;
        phase_start_ = clock::now();
        if (interactive_ && total > 0) draw(phase_start_);
    }

    void step(std::size_t current, std::string_view action, std::string_view source, std::string_view dest, bool changed) {
        if (!interactive_) return;
        if (has_active_ && active_.changed) {
            history_.push_back(std::move(active_));
            if (history_.size() + 1 > static_cast<std::size_t>(text_lines_)) history_.pop_front();
            active_ = {};
        }
        active_.action.assign(action);
        active_.source.assign(source);
        active_.dest.assign(dest);
        active_.changed = changed;
        has_active_ = true;
        current_ = current;
        if (const auto now = clock::now(); now >= next_draw_) draw(now);
    }

    // Writes one complete line (including '\n'). While the dashboard is live, lines are
    // batched and flushed above the frame on the next redraw.
    void log(std::string line, Stream stream = Stream::Out) {
        if (!attached_) {
            std::FILE* f = stream == Stream::Out ? stdout : stderr;
            if (stream == Stream::Err) std::fflush(stdout);
            std::fputs(line.c_str(), f);
            return;
        }
        pending_logs_.emplace_back(std::move(line), stream);
        if (const auto now = clock::now(); now >= next_draw_) draw(now);
    }

    void finish() {
        if (!attached_) return;
        draw(clock::now()); // final frame (and any pending log lines)
        std::string out = std::format("\x1b" "8\x1b[{}B\r", reserved_rows_ - 1);
        if (reserved_rows_ == frame_rows_) out += '\n';
        out += "\x1b[?25h";
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
        detail::g_live_rows = 0;
        std::signal(SIGINT, prev_int_);
        std::signal(SIGTERM, prev_term_);
        attached_ = false;
    }
};

} // namespace fsturbo::term

#endif // DASHBOARD_HPP
