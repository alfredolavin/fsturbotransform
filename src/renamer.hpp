#ifndef RENAMER_HPP
#define RENAMER_HPP

#include <algorithm>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <numeric>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "case_converter.hpp"
#include "execution_stats.hpp"
#include "matcher.hpp"
#include "progress_sink.hpp"
#include "ttf_font.hpp"

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
    bool report = true;   // the final execution report (--no-report gives the console the whole height)
    FilterMatcher filter;
    fs::path depth_origin; // where `depth` counts from; empty: the working directory (the GUI uses the target)
};

// Adds the .gitignore-style `files` to the filter, then (unless `target_own` is false) the target
// directory's own .gitignore, which, as in git, takes precedence. All patterns are matched relative
// to the target directory. Returns the files that could not be read.
inline std::vector<fs::path> load_gitignores(RenameOptions& opts, const std::vector<fs::path>& files, bool target_own) {
    std::vector<fs::path> unreadable;
    for (const fs::path& file : files) {
        if (!opts.filter.load_gitignore(file.string())) unreadable.push_back(file);
    }
    if (target_own) {
        const fs::path own = opts.target_dir / ".gitignore";
        std::error_code ec;
        const bool listed = std::ranges::any_of(files, [&](const fs::path& f) { return fs::equivalent(f, own, ec); });
        if (!listed && fs::is_regular_file(own, ec)) opts.filter.load_gitignore(own.string());
    }
    return unreadable;
}

class TransformerEngine {
    RenameOptions opts;
    ExecutionStats stats;
    RegexSpec rename_rx_spec;
    RegexSpec flatten_rx_spec;
    std::string config_error_; // an invalid --regex / --flatten-regex: reported by run() before anything is touched
    fs::path cwd_;             // where the program was started: the origin of the `depth` variable
    ProgressSink* sink_ = nullptr;

    void report_error(std::string_view message) {
        stats.errors++;
        sink_->error(message);
    }

    // Visits every entry with its root-relative path and exclusion verdict. Excluded
    // directories are pruned; when -i rules could re-admit something below them they are
    // walked instead, with the exclusion inherited by their contents.
    template <typename Visit>
    void walk(const fs::path& root, bool recursive, Visit&& visit) {
        const bool prune = !opts.filter.has_includes();
        std::vector<char> excluded_at_depth; // verdict of the directory currently open at each depth
        auto consider = [&](const fs::directory_entry& entry, int depth) {
            std::error_code ec;
            const bool is_dir = entry.is_directory(ec) && !entry.is_symlink(ec);
            const std::string rel = entry.path().lexically_relative(root).string();
            const auto parent = static_cast<std::size_t>(depth) - 1;
            const bool parent_excluded = depth > 0 && parent < excluded_at_depth.size() && excluded_at_depth[parent];
            const bool excluded = opts.filter.is_excluded(rel, is_dir, parent_excluded);
            if (is_dir) {
                excluded_at_depth.resize(static_cast<std::size_t>(depth) + 1);
                excluded_at_depth.back() = excluded;
            }
            if (excluded) stats.excluded_items++;
            visit(entry, std::string_view(rel), is_dir, excluded);
            return is_dir && excluded;
        };
        std::error_code ec;
        if (!recursive) {
            for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
                consider(*it, 0);
        } else {
            fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
            for (; !ec && it != end; it.increment(ec))
                if (consider(*it, it.depth()) && prune) it.disable_recursion_pending();
        }
        if (ec) report_error(std::format("Error scanning {}: {}", root.string(), ec.message()));
    }

public:
    explicit TransformerEngine(RenameOptions options) : opts(std::move(options)) {
        std::error_code ec;
        cwd_ = opts.depth_origin.empty() ? fs::current_path(ec) : fs::weakly_canonical(opts.depth_origin, ec);
        if (!opts.regex_rename.empty()) {
            rename_rx_spec = RegexSpec::parse(opts.regex_rename);
            if (!rename_rx_spec.valid) config_error_ = std::format("Invalid --regex '{}': {}", opts.regex_rename, rename_rx_spec.error);
        }
        if (!opts.flatten_regex.empty()) {
            flatten_rx_spec = RegexSpec::parse(opts.flatten_regex, false);
            if (!flatten_rx_spec.valid) config_error_ = std::format("Invalid --flatten-regex '{}': {}", opts.flatten_regex, flatten_rx_spec.error);
        }
    }

    // The first configuration error (an invalid --regex / --flatten-regex), or empty.
    const std::string& config_error() const { return config_error_; }

    // Runs once; everything that happens is reported to `sink`.
    ExecutionStats run(ProgressSink& sink) {
        const auto start_time = std::chrono::steady_clock::now();
        sink_ = &sink;
        stats = {};

        if (!config_error_.empty()) [[unlikely]] {
            report_error(config_error_);
            sink.finish();
            return stats;
        }

        std::error_code ec;
        if (!fs::is_directory(opts.target_dir, ec)) [[unlikely]] {
            report_error(std::format("Target directory does not exist: {}", opts.target_dir.string()));
            sink.finish();
            return stats;
        }
        const fs::path root = fs::canonical(opts.target_dir, ec);
        if (ec) [[unlikely]] {
            report_error(std::format("Cannot resolve {}: {}", opts.target_dir.string(), ec.message()));
            sink.finish();
            return stats;
        }

        if (opts.flatten || !opts.flatten_regex.empty()) execute_flattening(root);
        if (!sink.cancelled()) execute_renaming(root);
        sink.finish();

        stats.duration_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
        return stats;
    }

private:
    void execute_flattening(const fs::path& root) {
        if (!opts.recursive) return;
        std::vector<fs::path> files_to_flatten;
        walk(root, true, [&](const fs::directory_entry& entry, std::string_view rel, bool is_dir, bool excluded) {
            if (excluded || is_dir || rel.find('/') == std::string_view::npos) return;
            std::error_code ec;
            if (!(entry.is_regular_file(ec) || entry.is_symlink(ec))) return;
            if (!opts.flatten_regex.empty() && flatten_rx_spec.valid && !flatten_rx_spec.rx.search(rel)) return;
            files_to_flatten.push_back(entry.path());
        });

        // Names already present in (or claimed for) the root, so collisions are resolved
        // identically in dry-run and real mode without re-probing the filesystem.
        std::unordered_set<std::string> claimed;
        std::error_code ec;
        for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) claimed.insert(it->path().filename().string());

        const std::size_t total = files_to_flatten.size();
        sink_->begin_phase("FLATTEN", total);

        for (std::size_t i = 0; i < total && !sink_->cancelled(); ++i) {
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
            sink_->step(i + 1, "FLATTEN", src_rel, dest_name, true);

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
            std::size_t index = 0;      // `index` / `nameIndex` expression variables
            std::size_t name_index = 0;
        };
        std::vector<Item> items;

        walk(root, opts.recursive, [&](const fs::directory_entry& entry, std::string_view, bool is_dir, bool excluded) {
            if (is_dir) stats.scanned_dirs++;
            else stats.scanned_files++;
            if (!excluded) items.push_back({entry, entry.path().native().size(), is_dir});
        });

        if (rename_rx_spec.valid && rename_rx_spec.tmpl.has_expressions()) number_items(items);
        std::ranges::sort(items, std::greater{}, &Item::sort_key);

        const std::size_t total = items.size();
        sink_->begin_phase("RENAME", total);

        for (std::size_t i = 0; i < total && !sink_->cancelled(); ++i) {
            const Item& item = items[i];
            const fs::path& path = item.entry.path();
            const std::string filename = path.filename().string();
            const std::string rel_str = path.lexically_relative(root).string();
            const EntryContext ctx{item.index, item.name_index, ttf::utf8_length(filename), depth_of(path)};
            const auto renamed = transform_name(filename, ctx);
            if (!renamed) {
                sink_->step(i + 1, "CHECK", rel_str, {}, false);
                report_error(std::format("Skipped {}: {}", rel_str, renamed.error()));
                continue;
            }
            const std::string& new_filename = *renamed;

            if (filename == new_filename) {
                sink_->step(i + 1, "CHECK", rel_str, {}, false);
                continue;
            }
            if (!is_valid_name(new_filename)) {
                sink_->step(i + 1, "CHECK", rel_str, {}, false);
                report_error(std::format("Skipped {}: '{}' is not a valid file name", rel_str, new_filename));
                continue;
            }

            const std::string_view action_tag = item.is_dir ? "DIR RENAME" : "FILE RENAME";
            sink_->step(i + 1, action_tag, rel_str, new_filename, true);

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

    // Numbers the entries for the `index` (per kind: files, directories) and `nameIndex` (among
    // entries of the same name) variables. The order is by path, so it does not depend on the
    // filesystem's directory order or on the order the renames happen in.
    template <typename Items>
    static void number_items(Items& items) {
        std::vector<std::size_t> order(items.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::ranges::sort(order, {}, [&](std::size_t i) -> const std::string& { return items[i].entry.path().native(); });
        std::size_t counts[2] = {0, 0};
        std::unordered_map<std::string, std::size_t> same_name[2];
        for (const std::size_t i : order) {
            auto& item = items[i];
            const std::size_t kind = item.is_dir ? 1 : 0;
            item.index = ++counts[kind];
            item.name_index = ++same_name[kind][item.entry.path().filename().string()];
        }
    }

    // Directory levels between the working directory and the entry's directory: 0 for an entry
    // directly inside it, negative when the entry lies above it.
    long depth_of(const fs::path& path) const {
        long depth = 0;
        for (const fs::path& part : path.parent_path().lexically_relative(cwd_)) {
            if (part == "..") --depth;
            else if (part != "." && !part.empty()) ++depth;
        }
        return depth;
    }

public:
    // A computed name could be anything; refuse the ones that cannot name a directory entry.
    static bool is_valid_name(std::string_view name) {
        return !name.empty() && name != "." && name != ".." && name.find_first_of(std::string_view("/\0", 2)) == std::string_view::npos;
    }

    // The new name of one entry (unchanged if nothing applies); the GUI also uses it to try the
    // options on a sample name.
    std::expected<std::string, std::string> transform_name(const std::string& name, const EntryContext& ctx) const {
        std::string result = name;
        if (rename_rx_spec.valid) {
            auto replaced = rename_rx_spec.apply(name, ctx);
            if (!replaced) return replaced;
            result = std::move(*replaced);
        }
        if (opts.case_style == CaseStyle::None) return result;

        const std::size_t dot_pos = result.rfind('.');
        if (dot_pos == std::string::npos || dot_pos == 0) return CaseConverter::transform_dynamic(result, opts.case_style);
        const std::string_view view = result;
        return CaseConverter::transform_dynamic(view.substr(0, dot_pos), opts.case_style) + CaseConverter::to_lower(view.substr(dot_pos));
    }
};

} // namespace fsturbo

#endif
