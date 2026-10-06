#ifndef RENAMER_HPP
#define RENAMER_HPP

#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <future>
#include <print>
#include <format>
#include "case_converter.hpp"
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
    size_t scanned_dirs = 0;
    size_t scanned_files = 0;
    size_t renamed_dirs = 0;
    size_t renamed_files = 0;
    size_t flattened_files = 0;
    size_t excluded_items = 0;
    size_t errors = 0;
    double duration_ms = 0.0;
};

class TransformerEngine {
    RenameOptions opts;
    ExecutionStats stats;
    RegexSpec rename_rx_spec;
    RegexSpec flatten_rx_spec;
    term::LiveDashboard dashboard;

public:
    explicit TransformerEngine(RenameOptions options) : opts(std::move(options)) {
        if (!opts.regex_rename.empty()) {
            rename_rx_spec = RegexSpec::parse(opts.regex_rename);
        }
        if (!opts.flatten_regex.empty()) {
            flatten_rx_spec = RegexSpec::parse(opts.flatten_regex);
        }
    }

    ExecutionStats run() {
        auto start_time = std::chrono::high_resolution_clock::now();

        if (!fs::exists(opts.target_dir) || !fs::is_directory(opts.target_dir)) [[unlikely]] {
            std::println(std::cerr, "{}{}Target directory does not exist: {}{}",
                         term::icon_error(), term::rgb_fg({255, 50, 50}), opts.target_dir.string(), term::reset());
            stats.errors++;
            return stats;
        }

        fs::path root = fs::canonical(opts.target_dir);

        if (opts.flatten || !opts.flatten_regex.empty()) {
            execute_flattening(root);
        }

        execute_renaming(root);

        dashboard.finish();

        auto end_time = std::chrono::high_resolution_clock::now();
        stats.duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        return stats;
    }

private:
    void execute_flattening(const fs::path& root) {
        std::vector<fs::path> files_to_flatten;

        try {
            if (opts.recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
                    if (entry.is_regular_file() || entry.is_symlink()) {
                        fs::path rel = fs::relative(entry.path(), root);
                        if (rel.parent_path().empty()) continue;

                        std::string rel_str = rel.string();
                        if (opts.filter.is_excluded(rel_str)) {
                            stats.excluded_items++;
                            continue;
                        }

                        if (!opts.flatten_regex.empty()) {
                            if (flatten_rx_spec.valid && !std::regex_search(rel_str, flatten_rx_spec.rx)) {
                                continue;
                            }
                        }

                        files_to_flatten.push_back(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            std::println(std::cerr, "{}Error scanning directory for flattening: {}", term::icon_error(), e.what());
            stats.errors++;
        }

        size_t total = files_to_flatten.size();
        dashboard.init(total);

        for (size_t i = 0; i < total; ++i) {
            const auto& src_file = files_to_flatten[i];
            fs::path filename = src_file.filename();
            fs::path dest_file = root / filename;

            if (!opts.overwrite && fs::exists(dest_file) && src_file != dest_file) {
                std::string stem = filename.stem().string();
                std::string ext = filename.extension().string();
                int counter = 1;
                while (fs::exists(root / std::format("{}_{}{}", stem, counter, ext))) {
                    counter++;
                }
                dest_file = root / std::format("{}_{}{}", stem, counter, ext);
            }

            std::string src_rel = fs::relative(src_file, root).string();
            std::string dest_name = dest_file.filename().string();

            // Update in-place Mini-Terminal dashboard
            dashboard.update(i + 1, "FLATTEN", src_rel, dest_name, "Exposing to root");

            // Text output ONLY if explicitly requested via --verbose / -v
            if (opts.verbose) {
                std::println("{}{}[FLATTEN]{} {} -> {}",
                             term::icon_flatten(), term::rgb_fg({255, 200, 50}), term::reset(),
                             src_rel, dest_name);
            }

            if (!opts.dry_run) {
                try {
                    fs::rename(src_file, dest_file);
                    stats.flattened_files++;
                } catch (const std::exception& e) {
                    std::println(std::cerr, "{}Failed to flatten {}: {}", term::icon_error(), src_file.string(), e.what());
                    stats.errors++;
                }
            } else {
                stats.flattened_files++;
            }
        }

        if (!opts.dry_run) {
            cleanup_empty_dirs(root);
        }
    }

    void cleanup_empty_dirs(const fs::path& root) {
        bool removed_any = true;
        while (removed_any) {
            removed_any = false;
            try {
                for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
                    if (entry.is_directory() && entry.path() != root) {
                        if (fs::is_empty(entry.path())) {
                            fs::remove(entry.path());
                            removed_any = true;
                        }
                    }
                }
            } catch (...) {}
        }
    }

    void execute_renaming(const fs::path& root) {
        std::vector<fs::directory_entry> all_entries;

        try {
            if (opts.recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
                    all_entries.push_back(entry);
                }
            } else {
                for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied)) {
                    all_entries.push_back(entry);
                }
            }
        } catch (const std::exception& e) {
            std::println(std::cerr, "{}Error scanning directory: {}", term::icon_error(), e.what());
            stats.errors++;
            return;
        }

        std::sort(all_entries.begin(), all_entries.end(), [](const auto& a, const auto& b) {
            return a.path().string().length() > b.path().string().length();
        });

        size_t total = all_entries.size();
        dashboard.init(total);

        for (size_t i = 0; i < total; ++i) {
            const auto& entry = all_entries[i];
            fs::path path = entry.path();
            fs::path rel_path = fs::relative(path, root);
            std::string rel_str = rel_path.string();

            if (entry.is_directory()) stats.scanned_dirs++;
            else stats.scanned_files++;

            if (opts.filter.is_excluded(rel_str)) {
                stats.excluded_items++;
                continue;
            }

            std::string filename = path.filename().string();
            std::string new_filename = transform_name(filename);

            if (filename != new_filename) {
                fs::path target_path = path.parent_path() / new_filename;
                bool is_dir = entry.is_directory();

                std::string action_tag = is_dir ? "DIR RENAME" : "FILE RENAME";

                // Update in-place Mini-Terminal dashboard
                dashboard.update(i + 1, action_tag, filename, new_filename, "Processing item");

                // Text output ONLY if explicitly requested via --verbose / -v
                if (opts.verbose) {
                    std::println("{}{}{} {}{} ➔ {}{}{}",
                                 term::icon_rename(), term::rgb_fg({100, 220, 255}),
                                 action_tag, term::reset(),
                                 filename, term::rgb_fg({100, 255, 150}), new_filename, term::reset());
                }

                if (!opts.dry_run) {
                    try {
                        fs::rename(path, target_path);
                        if (is_dir) stats.renamed_dirs++;
                        else stats.renamed_files++;
                    } catch (const std::exception& e) {
                        std::println(std::cerr, "{}Failed to rename {}: {}", term::icon_error(), path.string(), e.what());
                        stats.errors++;
                    }
                } else {
                    if (is_dir) stats.renamed_dirs++;
                    else stats.renamed_files++;
                }
            } else {
                // If not renamed, update live progress
                dashboard.update(i + 1, "CHECK", filename, "No change", "Inspecting item");
            }
        }
    }

    std::string transform_name(const std::string& name) {
        std::string result = name;

        if (rename_rx_spec.valid) {
            result = apply_regex_replace(result, rename_rx_spec.rx, rename_rx_spec.replacement);
        }

        if (opts.case_style != CaseStyle::None) {
            size_t dot_pos = result.rfind('.');
            if (dot_pos != std::string::npos && dot_pos > 0) {
                std::string stem = result.substr(0, dot_pos);
                std::string ext = result.substr(dot_pos);
                std::string transformed_stem = CaseConverter::transform_dynamic(stem, opts.case_style);
                std::string transformed_ext = CaseConverter::to_lower(ext);
                result = transformed_stem + transformed_ext;
            } else {
                result = CaseConverter::transform_dynamic(result, opts.case_style);
            }
        }

        return result;
    }
};

} // namespace fsturbo

#endif
