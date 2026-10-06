#ifndef EXECUTION_STATS_HPP
#define EXECUTION_STATS_HPP

#include <cstddef>

namespace fsturbo {

struct ExecutionStats {
    std::size_t scanned_dirs = 0;
    std::size_t scanned_files = 0;
    std::size_t renamed_dirs = 0;
    std::size_t renamed_files = 0;
    std::size_t flattened_files = 0;
    std::size_t excluded_items = 0;
    std::size_t errors = 0;
    double duration_ms = 0.0;
};

} // namespace fsturbo

#endif // EXECUTION_STATS_HPP
