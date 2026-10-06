#ifndef JS_ENGINE_HPP
#define JS_ENGINE_HPP

// JavaScript expression evaluation for the replacement mode. The engine is V8, used as a
// library through libfsturbo_js.so (src/js_bridge.cpp), which this header loads with dlopen the
// first time a replacement actually contains an expression. Only the plain-C API of
// js_bridge_api.h crosses the boundary, so the executable keeps its static libstdc++.

#include <dlfcn.h>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "js_bridge_api.h"

namespace fsturbo::js {

inline fsjs_value undefined() { return {FSJS_UNDEFINED, 0.0, nullptr, 0}; }
inline fsjs_value number(double v) { return {FSJS_NUMBER, v, nullptr, 0}; }
inline fsjs_value text(std::string_view s) { return {FSJS_STRING, 0.0, s.data(), s.size()}; }

class Engine {
    struct Api {
        int (*abi_version)() = nullptr;
        fsjs_engine* (*open)(char*, std::size_t) = nullptr;
        void (*close)(fsjs_engine*) = nullptr;
        int (*compile)(fsjs_engine*, const char* const*, std::size_t, const char*, char*, std::size_t) = nullptr;
        int (*call)(fsjs_engine*, int, const fsjs_value*, std::size_t, const char**, std::size_t*, char*, std::size_t) = nullptr;
    } api_;
    fsjs_engine* handle_ = nullptr;

    Engine() = default;

    // $FSTURBO_JS_LIB, then next to the executable and in ../lib/fsturbotransform relative to it,
    // then the directory this build installs the bridge to (FSTURBO_LIBDIR: needed when bin/ is a
    // symlink, because /proc/self/exe is the real path), then the dynamic loader's own search.
    static std::vector<std::string> candidates() {
        constexpr const char* kName = "libfsturbo_js.so";
        std::vector<std::string> out;
        if (const char* env = std::getenv("FSTURBO_JS_LIB"); env != nullptr && *env != '\0') out.emplace_back(env);
        std::error_code ec;
        const std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            out.push_back((exe.parent_path() / kName).string());
            out.push_back((exe.parent_path() / ".." / "lib" / "fsturbotransform" / kName).string());
        }
#ifdef FSTURBO_LIBDIR
        out.push_back((std::filesystem::path(FSTURBO_LIBDIR) / kName).string());
#endif
        out.emplace_back(kName);
        return out;
    }

public:
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    // V8 cannot be unloaded cleanly, so the library stays mapped; only the isolate is released.
    ~Engine() {
        if (handle_ != nullptr) api_.close(handle_);
    }

    static std::unique_ptr<Engine> create(std::string& error) {
        void* lib = nullptr;
        std::string last_error, looked_in;
        for (const std::string& path : candidates()) {
            lib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            looked_in += (looked_in.empty() ? "" : ", ") + path;
            if (lib != nullptr) break;
            // A candidate that exists but cannot load (say, libnode missing) says more than "No such file".
            if (const char* why = dlerror(); why != nullptr && (last_error.empty() || std::string_view(why).find("No such file") == std::string_view::npos))
                last_error = why;
        }
        if (lib == nullptr) {
            error = "JavaScript expressions need libfsturbo_js.so (built when libnode-dev is installed; set FSTURBO_JS_LIB to its path). Looked in: " + looked_in + " (" + last_error + ")";
            return nullptr;
        }

        std::unique_ptr<Engine> engine(new Engine);
        Api& api = engine->api_;
        api.abi_version = reinterpret_cast<int (*)()>(dlsym(lib, "fsjs_abi_version"));
        api.open = reinterpret_cast<decltype(api.open)>(dlsym(lib, "fsjs_open"));
        api.close = reinterpret_cast<decltype(api.close)>(dlsym(lib, "fsjs_close"));
        api.compile = reinterpret_cast<decltype(api.compile)>(dlsym(lib, "fsjs_compile"));
        api.call = reinterpret_cast<decltype(api.call)>(dlsym(lib, "fsjs_call"));
        if (!api.abi_version || !api.open || !api.close || !api.compile || !api.call || api.abi_version() != FSJS_ABI_VERSION) {
            error = "libfsturbo_js.so does not match this fsturbotransform (rebuild both)";
            return nullptr;
        }
        char message[512] = {};
        engine->handle_ = api.open(message, sizeof message);
        if (engine->handle_ == nullptr) {
            error = message;
            return nullptr;
        }
        return engine;
    }

    // Compiles `expression` once as a function of `params` and returns its id.
    std::optional<int> compile(std::span<const std::string> params, const std::string& expression, std::string& error) {
        std::vector<const char*> names;
        names.reserve(params.size());
        for (const std::string& p : params) names.push_back(p.c_str());
        char message[512] = {};
        const int id = api_.compile(handle_, names.data(), names.size(), expression.c_str(), message, sizeof message);
        if (id < 0) {
            error = message;
            return std::nullopt;
        }
        return id;
    }

    // The result is valid until the next call.
    std::optional<std::string_view> call(int function, std::span<const fsjs_value> args, std::string& error) {
        const char* result = nullptr;
        std::size_t length = 0;
        char message[512] = {};
        if (api_.call(handle_, function, args.data(), args.size(), &result, &length, message, sizeof message) != 0) {
            error = message;
            return std::nullopt;
        }
        return std::string_view(result, length);
    }
};

} // namespace fsturbo::js

#endif // JS_ENGINE_HPP
