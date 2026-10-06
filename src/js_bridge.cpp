// libfsturbo_js.so: evaluates the JavaScript expressions of the replacement mode with V8
// (used as a library through libnode, not embedded). Expressions are compiled once into
// functions and then called for every match. Built without -ffast-math so the NaN/Infinity
// checks on results stay meaningful. See js_bridge_api.h for the C boundary.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <v8.h>
#include <libplatform/libplatform.h>
#include "js_bridge_api.h"

#define FSJS_EXPORT extern "C" __attribute__((visibility("default")))

namespace {

void set_error(char* out, std::size_t size, std::string_view message) {
    if (out == nullptr || size == 0) return;
    const std::size_t n = std::min(message.size(), size - 1);
    std::memcpy(out, message.data(), n);
    out[n] = '\0';
}

// V8 is initialised once per process and, like its platform, never torn down.
void initialize_v8() {
    static const bool done = [] {
        v8::V8::SetFlagsFromString("--single-threaded");
        static std::unique_ptr<v8::Platform> platform = v8::platform::NewSingleThreadedDefaultPlatform();
        v8::V8::InitializePlatform(platform.get());
        v8::V8::Initialize();
        return true;
    }();
    (void)done;
}

// Every property of Math whose name is an identifier (abs, round, PI, ...) becomes a local
// of the generated function, so expressions can write `round(x)` as well as `Math.round(x)`.
// Taken from Math itself, so the list is always whatever this V8 provides.
constexpr const char* kFactorySource = R"JS(
(function () {
  'use strict';
  const names = Object.getOwnPropertyNames(Math).filter(n => /^[A-Za-z_$][\w$]*$/.test(n));
  const values = names.map(n => Math[n]);
  return function (params, expression) {
    const outer = new Function(...names, '"use strict"; return function (' + params.join(',') + ') { return (' + expression + '\n); };');
    return outer(...values);
  };
})()
)JS";

std::string describe(v8::Isolate* isolate, v8::TryCatch& trap) {
    if (!trap.HasCaught()) return "unknown JavaScript error";
    v8::HandleScope scope(isolate);
    v8::String::Utf8Value text(isolate, trap.Exception());
    return *text ? std::string(*text, static_cast<std::size_t>(text.length())) : "unknown JavaScript error";
}

} // namespace

struct fsjs_engine {
    std::unique_ptr<v8::ArrayBuffer::Allocator> allocator;
    v8::Isolate* isolate = nullptr;
    v8::Global<v8::Context> context;
    v8::Global<v8::Function> factory;
    std::vector<v8::Global<v8::Function>> functions;
    std::string result;
};

FSJS_EXPORT int fsjs_abi_version(void) { return FSJS_ABI_VERSION; }

FSJS_EXPORT fsjs_engine* fsjs_open(char* error, std::size_t error_size) {
    try {
        initialize_v8();
        auto engine = std::make_unique<fsjs_engine>();
        engine->allocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
        v8::Isolate::CreateParams params;
        params.array_buffer_allocator = engine->allocator.get();
        engine->isolate = v8::Isolate::New(params);

        v8::Isolate::Scope isolate_scope(engine->isolate);
        v8::HandleScope scope(engine->isolate);
        v8::Local<v8::Context> context = v8::Context::New(engine->isolate);
        v8::Context::Scope context_scope(context);
        engine->context.Reset(engine->isolate, context);

        v8::TryCatch trap(engine->isolate);
        v8::Local<v8::String> source = v8::String::NewFromUtf8(engine->isolate, kFactorySource).ToLocalChecked();
        v8::Local<v8::Value> factory;
        if (!v8::Script::Compile(context, source).ToLocalChecked()->Run(context).ToLocal(&factory) || !factory->IsFunction()) {
            set_error(error, error_size, "cannot initialise the JavaScript engine: " + describe(engine->isolate, trap));
            engine->context.Reset();
            engine->isolate->Dispose();
            return nullptr;
        }
        engine->factory.Reset(engine->isolate, factory.As<v8::Function>());
        return engine.release();
    } catch (...) {
        set_error(error, error_size, "cannot initialise the JavaScript engine");
        return nullptr;
    }
}

FSJS_EXPORT void fsjs_close(fsjs_engine* engine) {
    if (engine == nullptr) return;
    engine->functions.clear();
    engine->factory.Reset();
    engine->context.Reset();
    engine->isolate->Dispose();
    delete engine;
}

FSJS_EXPORT int fsjs_compile(fsjs_engine* engine, const char* const* params, std::size_t param_count, const char* expression, char* error,
                             std::size_t error_size) {
    try {
        v8::Isolate* isolate = engine->isolate;
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = engine->context.Get(isolate);
        v8::Context::Scope context_scope(context);
        v8::TryCatch trap(isolate);

        v8::Local<v8::Array> names = v8::Array::New(isolate, static_cast<int>(param_count));
        for (std::size_t i = 0; i < param_count; ++i)
            names->Set(context, static_cast<std::uint32_t>(i), v8::String::NewFromUtf8(isolate, params[i]).ToLocalChecked()).Check();
        v8::Local<v8::Value> argv[] = {names, v8::String::NewFromUtf8(isolate, expression).ToLocalChecked()};

        v8::Local<v8::Value> compiled;
        if (!engine->factory.Get(isolate)->Call(context, v8::Undefined(isolate), 2, argv).ToLocal(&compiled) || !compiled->IsFunction()) {
            set_error(error, error_size, describe(isolate, trap));
            return -1;
        }
        engine->functions.emplace_back(isolate, compiled.As<v8::Function>());
        return static_cast<int>(engine->functions.size() - 1);
    } catch (...) {
        set_error(error, error_size, "out of memory while compiling the expression");
        return -1;
    }
}

FSJS_EXPORT int fsjs_call(fsjs_engine* engine, int function, const fsjs_value* args, std::size_t arg_count, const char** result,
                          std::size_t* result_length, char* error, std::size_t error_size) {
    try {
        v8::Isolate* isolate = engine->isolate;
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = engine->context.Get(isolate);
        v8::Context::Scope context_scope(context);
        v8::TryCatch trap(isolate);

        std::vector<v8::Local<v8::Value>> argv;
        argv.reserve(arg_count);
        for (std::size_t i = 0; i < arg_count; ++i) {
            switch (args[i].kind) {
                case FSJS_NUMBER: argv.push_back(v8::Number::New(isolate, args[i].number)); break;
                case FSJS_STRING:
                    argv.push_back(v8::String::NewFromUtf8(isolate, args[i].text, v8::NewStringType::kNormal, static_cast<int>(args[i].length))
                                       .ToLocalChecked());
                    break;
                default: argv.push_back(v8::Undefined(isolate)); break;
            }
        }

        v8::Local<v8::Value> value;
        if (!engine->functions[static_cast<std::size_t>(function)]
                 .Get(isolate)
                 ->Call(context, v8::Undefined(isolate), static_cast<int>(argv.size()), argv.data())
                 .ToLocal(&value)) {
            set_error(error, error_size, describe(isolate, trap));
            return -1;
        }

        if (value->IsNumber()) {
            const double number = value.As<v8::Number>()->Value();
            if (!std::isfinite(number)) {
                set_error(error, error_size, std::isnan(number) ? "the result is NaN" : number > 0 ? "the result is Infinity" : "the result is -Infinity");
                return -1;
            }
        } else if (!value->IsString() && !value->IsBoolean() && !value->IsBigInt()) {
            set_error(error, error_size, "the result must be a number, string or boolean");
            return -1;
        }
        v8::String::Utf8Value text(isolate, value);
        engine->result.assign(*text, static_cast<std::size_t>(text.length()));
        *result = engine->result.data();
        *result_length = engine->result.size();
        return 0;
    } catch (...) {
        set_error(error, error_size, "out of memory while evaluating the expression");
        return -1;
    }
}
