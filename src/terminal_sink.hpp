#ifndef TERMINAL_SINK_HPP
#define TERMINAL_SINK_HPP

// The command line's ProgressSink: the live dashboard (gradient bar and mini-console), the
// --verbose log of every change and the error lines.

#include <format>
#include <string>
#include <string_view>
#include <unistd.h>
#include "dashboard.hpp"
#include "progress_sink.hpp"
#include "report_view.hpp"
#include "terminal_style.hpp"

namespace fsturbo {

class TerminalSink final : public ProgressSink {
    term::LiveDashboard dashboard_;
    bool verbose_;

public:
    TerminalSink(const ui::ScreenPlan& plan, bool verbose) : verbose_(verbose) { dashboard_.set_plan(plan); }

    void begin_phase(std::string_view phase, std::size_t total) override { dashboard_.begin_phase(phase, total); }

    void step(std::size_t current, std::string_view action, std::string_view source, std::string_view dest, bool changed) override {
        dashboard_.step(current, action, source, dest, changed);
        if (!verbose_ || !changed) return;
        if (action == "FLATTEN") {
            dashboard_.log(term::icon_line(sixel::Icon::Flatten, std::format("{}[FLATTEN]{} {} → {}", term::rgb_fg({255, 200, 50}),
                                                                             term::reset(), source, dest)));
        } else {
            dashboard_.log(term::icon_line(sixel::Icon::Rename, std::format("{}{}{} {} → {}{}{}", term::rgb_fg({100, 220, 255}), action,
                                                                            term::reset(), source, term::rgb_fg({100, 255, 150}), dest,
                                                                            term::reset())));
        }
    }

    void error(std::string_view message) override {
        static const bool err_tty = isatty(STDERR_FILENO) != 0;
        std::string line = err_tty ? term::icon_line(sixel::Icon::Error, std::format("{}{}{}", term::rgb_fg(palette::error), message, term::reset()))
                                   : std::format("[ERROR] {}\n", message);
        dashboard_.log(std::move(line), term::Stream::Err);
    }

    void finish() override { dashboard_.finish(); }
};

} // namespace fsturbo

#endif // TERMINAL_SINK_HPP
