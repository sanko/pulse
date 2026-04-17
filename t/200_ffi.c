/**
 * @file 200_ffi.c
 * @brief Test Suite: Infix FFI Integration (Optional)
 *
 * This test requires the infix FFI library to be present. If infix is not
 * available, the test is skipped gracefully.
 */

#define DBLTAP_IMPLEMENTATION
#include "common/compat_c23.h"
#include "common/double_tap.h"
#include "common/infix_config.h"
#include "pulse/pulse_common.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INFIX_FFI_AVAILABLE
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>

TEST {
    plan(0);
    diag("infix FFI not available - test skipped");
}
#else
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
typedef HANDLE pulse_thread_h;
#else
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
typedef pthread_t pulse_thread_h;
#endif

#define TAG_PRIMITIVE 0
#define TAG_ARRAY 1
#define TAG_OBJECT 4
#define TAG_EXCEPTION 5
#define TAG_FIBER 6
#define TAG_THREAD 7
#define TAG_TUPLE 8
#define TAG_STRING 9
#define TAG_HASH 10

#define PULSE_STACK_SIZE (64 * 1024)
#define MAX_VREGS 64
#define PHYS_POOL_SIZE 3


static uint64_t pulse_hash_get(pulse_hash_t * h, const char * key) {
    for (size_t i = 0; i < h->capacity; i++)
        if (h->entries[i].key && strcmp(h->entries[i].key, key) == 0)
            return h->entries[i].value;
    return 0;
}

static void mock_ffi_handler(infix_reverse_t * ctx, void * ret, void ** args) {
    (void)ctx;
    (void)args;
    *(uint64_t *)ret = 777;
}

TEST {
    plan(3);
    infix_reverse_t * reverse_cb = NULL;
    pulse_status s = infix_reverse_create_closure(&reverse_cb, "()->uint64", mock_ffi_handler, NULL, NULL);
    ok(s == PULSE_SUCCESS, "Infix created reverse FFI handle");
    void * native_func = infix_reverse_get_code(reverse_cb);

    emit_context_t * ctx = create_test_context();
    emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
    emit_begin_section(ctx, ".data");
    emit_define_symbol(ctx, "ffi_target", EMIT_VISIBILITY_DEFAULT, false);
    emit_emit_u64(ctx, 0);
    uint64_t data_sz;
    emit_get_offset(ctx, &data_sz);
    setup_test_section(ctx);

    emit_math_load_sym(ctx, EMIT_REG_RAX, "ffi_target");
    emit_emit_u8(ctx, 0xFF);
    emit_emit_u8(ctx, 0xD0); /* CALL RAX */
    emit_math_ret(ctx);

    const uint8_t * code;
    size_t sz;
    emit_get_binary(ctx, &code, &sz);
    void * mem = NULL;
    if (execute_jit_code(code, sz, &mem)) {
        ok(1, "FFI bridge logic emitted");
        volatile uint64_t * ffi_ptr = (uint64_t *)mem;
        emit_test_fn_0 fn = (emit_test_fn_0)((uint8_t *)mem + data_sz);
        *ffi_ptr = (uintptr_t)native_func;
        ok(fn() == 777, "Pulse JIT successfully executed Infix FFI closure");
        free_executable(mem, sz);
    }
    emit_destroy(ctx);
    infix_reverse_destroy(reverse_cb);
}
#endif
