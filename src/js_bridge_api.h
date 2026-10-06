#ifndef FSTURBO_JS_BRIDGE_API_H
#define FSTURBO_JS_BRIDGE_API_H

/* The plain-C boundary between fsturbotransform and libfsturbo_js.so, the V8 bridge.
 *
 * Only C types cross it: the executable links libstdc++ statically while the bridge (and
 * libnode, which provides V8) use the shared one, so no C++ object, exception or allocation
 * may ever travel between the two. The bridge is loaded with dlopen on first use, so runs
 * that never evaluate a JavaScript expression do not pay for mapping V8 (~10 ms, ~25 MB). */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSJS_ABI_VERSION 1

enum { FSJS_UNDEFINED = 0, FSJS_NUMBER = 1, FSJS_STRING = 2 };

typedef struct {
    int kind;
    double number;    /* FSJS_NUMBER */
    const char* text; /* FSJS_STRING: UTF-8, not NUL-terminated */
    size_t length;
} fsjs_value;

typedef struct fsjs_engine fsjs_engine;

/* All functions that can fail write a NUL-terminated message into error[error_size]. */

int fsjs_abi_version(void);

fsjs_engine* fsjs_open(char* error, size_t error_size);
void fsjs_close(fsjs_engine* engine);

/* Compiles `expression` once into a function of the named parameters (every member of
 * JavaScript's Math is in scope, parameters shadow them) and returns its id, or -1. */
int fsjs_compile(fsjs_engine* engine, const char* const* params, size_t param_count, const char* expression, char* error, size_t error_size);

/* Calls a compiled function. Returns 0 on success, with the result converted to UTF-8 text
 * valid until the next call on this engine; -1 on failure (exception, or a result that is not a
 * finite number, string or boolean). */
int fsjs_call(fsjs_engine* engine, int function, const fsjs_value* args, size_t arg_count, const char** result, size_t* result_length,
              char* error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif /* FSTURBO_JS_BRIDGE_API_H */
