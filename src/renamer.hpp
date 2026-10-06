#ifndef RENAMER_HPP
#define RENAMER_HPP

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>
#include <unistd.h>
#include "case_converter.hpp"
#include "dashboard.hpp"
#include "matcher.hpp"
#include "terminal_style.hpp"

namespace fs = std::filesystem;

namespace fsturbo {

struct RenameOptions {
    fs::path target_dir = ".";
    CaseStyle case_style = CaseStyle::None;
    bool case_style_set = false;
    std::string regex_rename;
    bool flatten = false;
    std::string flatten_regex;
    bool dry_run = false;
    bool recursive = true;
    bool overwrite = false;
    bool verbose = false; // Only print scrolling text if explicitly requested
    FilterMatcher filter;
};

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

class TransformerEngine {
    RenameOptions opts;
    ExecutionStats stats;
    RegexSpec rename_rx_spec;
    RegexSpec flatten_rx_spec;
    term::LiveDashboard dashboard;

    void report_error(std::string_view message) {
        stats.errors++;
        static const bool err_tty = isatty(STDERR_FILENO) != 0;
        std::string line = err_tty ? term::icon_line(sixel::Icon::Error, std::format("{}{}{}", term::rgb_fg(palette::error), message, term::reset()))
                                   : std::format("[ERROR] {}\n", message);
        dashboard.log(std::move(line), term::Stream::Err);
    }

    // Excluded directories are pruned instead of walked (unless include rules could re-admit children).
    template <typename Visit>
    void walk(const fs::path& root, bool recursive, Visit&& visit) {
        const bool prune = !opts.filter.has_includes();
        std::error_code ec;
        if (!recursive) {
            for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
                visit(*it, [] {});
        } else {
            fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
            for (; !ec && it != end; it.increment(ec))
                visit(*it, [&] { if (prune) it.disable_recursion_pending(); });
        }
        if (ec) report_error(std::format("Error scanning {}: {}", root.string(), ec.message()));
    }

public:
    explicit TransformerEngine(RenameOptions options) : opts(std::move(options)) {
        if (!opts.regex_rename.empty()) rename_rx_spec = RegexSpec::parse(opts.regex_rename);
        if (!opts.flatten_regex.empty()) flatten_rx_spec = RegexSpec::parse(opts.flatten_regex);
    }

    ExecutionStats run() {
        const auto start_time = std::chrono::steady_clock::now();

        std::error_code ec;
        if (!fs::is_directory(opts.target_dir, ec)) [[unlikely]] {
            report_error(std::format("Target directory does not exist: {}", opts.target_dir.string()));
            return stats;
        }
        const fs::path root = fs::canonical(opts.target_dir, ec);
        if (ec) [[unlikely]] {
            report_error(std::format("Cannot resolve {}: {}", opts.target_dir.string(), ec.message()));
            return stats;
        }

        if (opts.flatten || !opts.flatten_regex.empty()) execute_flattening(root);
        execute_renaming(root);
        dashboard.finish();

        stats.duration_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
        return stats;
    }

private:
    void execute_flattening(const fs::path& root) {
        if (!opts.recursive) return;
        std::vector<fs::path> files_to_flatten;
        walk(root, true, [&](const fs::directory_entry& entry, auto&& prune) {
            const fs::path rel = entry.path().lexically_relative(root);
            const std::string rel_str = rel.string();
            std::error_code ec;
            const bool is_dir = entry.is_directory(ec) && !entry.is_symlink(ec);
            if (opts.filter.is_excluded(rel_str)) {
                stats.excluded_items++;
                if (is_dir) prune();
                return;
            }
            if (is_dir || !rel.has_parent_path() || !(entry.is_regular_file(ec) || entry.is_symlink(ec))) return;
            if (!opts.flatten_regex.empty() && flatten_rx_spec.valid && !std::regex_search(rel_str, flatten_rx_spec.rx)) return;
            files_to_flatten.push_back(entry.path());
        });

        // Names already present in (or claimed for) the root, so collisions are resolved
        // identically in dry-run and real mode without re-probing the filesystem.
        std::unordered_set<std::string> claimed;
        std::error_code ec;
        for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) claimed.insert(it->path().filename().string());

        const std::size_t total = files_to_flatten.size();
        dashboard.begin_phase("FLATTEN", total);

        for (std::size_t i = 0; i < total; ++i) {
            const fs::path& src_file = files_to_flatten[i];
            const fs::path filename = src_file.filename();
            std::string dest_name = filename.string();

            if (!opts.overwrite && claimed.contains(dest_name)) {
                const std::string stem = filename.stem().string();
                const std::string ext = filename.extension().string();
                for (int counter = 1;; ++counter) {
                    dest_name = std::format("{}_{}{}", stem, counter, ext);
                    if (!claimed.contains(dest_name)) break;
                }
            }
            claimed.insert(dest_name);

            const std::string src_rel = src_file.lexically_relative(root).string();
            dashboard.step(i + 1, "FLATTEN", src_rel, dest_name, true);

            if (opts.verbose) {
                dashboard.log(term::icon_line(sixel::Icon::Flatten, std::format("{}[FLATTEN]{} {} → {}", term::rgb_fg({255, 200, 50}),
                                                                                term::reset(), src_rel, dest_name)));
            }

            if (opts.dry_run) {
                stats.flattened_files++;
                continue;
            }
            std::error_code rename_ec;
            fs::rename(src_file, root / dest_name, rename_ec);
            if (rename_ec) report_error(std::format("Failed to flatten {}: {}", src_file.string(), rename_ec.message()));
            else stats.flattened_files++;
        }

        if (!opts.dry_run) cleanup_empty_dirs(root);
    }

    // One pass, deepest directories first, so emptied parents are removed in the same sweep.
    void cleanup_empty_dirs(const fs::path& root) {
        std::vector<fs::path> dirs;
        std::error_code ec;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code type_ec;
            if (it->is_directory(type_ec) && !it->is_symlink(type_ec)) dirs.push_back(it->path());
        }
        std::ranges::sort(dirs, std::greater{}, [](const fs::path& p) { return p.native().size(); });
        for (const fs::path& dir : dirs) {
            std::error_code rm_ec;
            if (fs::is_empty(dir, rm_ec) && !rm_ec) fs::remove(dir, rm_ec);
        }
    }

    void execute_renaming(const fs::path& root) {
        struct Item {
            fs::directory_entry entry;
            std::size_t sort_key; // path length: a child always sorts before its parent
            bool is_dir;
        };
        std::vector<Item> items;

        walk(root, opts.recursive, [&](const fs::directory_entry& entry, auto&& prune) {
            std::error_code ec;
            const bool is_dir = entry.is_directory(ec) && !entry.is_symlink(ec);
            if (is_dir) stats.scanned_dirs++;
            else stats.scanned_files++;
            if (opts.filter.is_excluded(entry.path().lexically_relative(root).string())) {
                stats.excluded_items++;
                if (is_dir) prune();
                return;
            }
            items.push_back({entry, entry.path().native().size(), is_dir});
        });

        std::ranges::sort(items, std::greater{}, &Item::sort_key);

        const std::size_t total = items.size();
        dashboard.begin_phase("RENAME", total);

        for (std::size_t i = 0; i < total; ++i) {
            const Item& item = items[i];
            const fs::path& path = item.entry.path();
            const std::string filename = path.filename().string();
            const std::string new_filename = transform_name(filename);
            const std::string rel_str = path.lexically_relative(root).string();

            if (filename == new_filename) {
                dashboard.step(i + 1, "CHECK", rel_str, {}, false);
                continue;
            }

            const std::string_view action_tag = item.is_dir ? "DIR RENAME" : "FILE RENAME";
            dashboard.step(i + 1, action_tag, rel_str, new_filename, true);

            if (opts.verbose) {
                dashboard.log(term::icon_line(sixel::Icon::Rename, std::format("{}{}{} {} → {}{}{}", term::rgb_fg({100, 220, 255}), action_tag,
                                                                               term::reset(), rel_str, term::rgb_fg({100, 255, 150}),
                                                                               new_filename, term::reset())));
            }

            const fs::path target_path = path.parent_path() / new_filename;
            std::error_code ec;
            // Refuse to clobber a different existing entry (case-only renames on
            // case-insensitive filesystems resolve to the same file and are allowed).
            if (!opts.overwrite && fs::exists(target_path, ec) && !fs::equivalent(path, target_path, ec)) {
                report_error(std::format("Skipped {}: {} already exists (use --overwrite)", rel_str, new_filename));
                continue;
            }
            if (!opts.dry_run) {
                fs::rename(path, target_path, ec);
                if (ec) {
                    report_error(std::format("Failed to rename {}: {}", path.string(), ec.message()));
                    continue;
                }
            }
            if (item.is_dir) stats.renamed_dirs++;
            else stats.renamed_files++;
        }
    }

    std::string transform_name(const std::string& name) const {
        std::string result = rename_rx_spec.valid ? apply_regex_replace(name, rename_rx_spec.rx, rename_rx_spec.replacement) : name;
        if (opts.case_style == CaseStyle::None) return result;

        const std::size_t dot_pos = result.rfind('.');
        if (dot_pos == std::string::npos || dot_pos == 0) return CaseConverter::transform_dynamic(result, opts.case_style);
        const std::string_view view = result;
        return CaseConverter::transform_dynamic(view.substr(0, dot_pos), opts.case_style) + CaseConverter::to_lower(view.substr(dot_pos));
    }
};

} // namespace fsturbo

#endif
