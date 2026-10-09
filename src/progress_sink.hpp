#ifndef PROGRESS_SINK_HPP
#define PROGRESS_SINK_HPP

// What the transformer engine reports while it runs. The terminal front end draws it as the live
// dashboard (terminal_sink.hpp); the QML GUI turns it into a progress bar and a list of changes.
// Every call comes from the thread that runs TransformerEngine::run().

#include <cstddef>
#include <string_view>

namespace fsturbo {

class ProgressSink {
public:
    virtual ~ProgressSink() = default;

    // A phase ("FLATTEN", "RENAME") is about to process `total` entries.
    virtual void begin_phase(std::string_view phase, std::size_t total) = 0;

    // Entry `current` (1-based) of the phase. `action` is "FLATTEN", "FILE RENAME", "DIR RENAME"
    // or "CHECK" (left as it is); `dest` is the new name and is empty unless `changed`.
    virtual void step(std::size_t current, std::string_view action, std::string_view source, std::string_view dest, bool changed) = 0;

    // Something could not be done (the run goes on with the next entry).
    virtual void error(std::string_view message) = 0;

    // The run is over: no more calls follow.
    virtual void finish() {}

    // Polled between entries: true stops the run where it is (entries done so far stay done).
    virtual bool cancelled() const { return false; }
};

} // namespace fsturbo

#endif // PROGRESS_SINK_HPP
