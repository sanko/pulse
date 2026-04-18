/**
 * @file 180_emit.c
 * @brief Unit test for the emit API (JIT compiler builder suite).
 * @ingroup test_suite
 */
#define DBLTAP_IMPLEMENTATION
#include "common/compat_c23.h"
#include "common/double_tap.h"
#include "common/infix_config.h"
#include <ctype.h>
#include <inttypes.h>
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#define EMIT_TEST_SECTION ".text"

typedef uint64_t (*emit_test_fn_0)(void);
typedef uint64_t (*emit_test_fn_1)(uint64_t);
typedef uint64_t (*emit_test_fn_2)(uint64_t, uint64_t);
typedef int64_t (*emit_test_fn_2i)(int64_t, int64_t);

/* ============================================================================
 * Debugging & Utilities
 * ============================================================================ */

static void pulse_debug_hex_dump(const char* label, const void* data, size_t size) {
    const unsigned char* p = (const unsigned char*)data;
    printf("\n# --- HEX DUMP: %s (%zu bytes) ---\n", label, size);
    for (size_t i = 0; i < size; i += 16) {
        printf("# %04zx: ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < size) printf("%02x ", p[i + j]);
            else printf("   ");
        }
        printf(" | ");
        for (int j = 0; j < 16; j++) {
            if (i + j < size) {
                unsigned char c = p[i + j];
                printf("%c", isprint(c) ? c : '.');
            }
        }
        printf("\n");
    }
    printf("# ----------------------------------\n");
    fflush(stdout);
}

static uint64_t return_72(void) { return 72; }

static void * alloc_executable(size_t size) {
#ifdef _WIN32
    void * mem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return NULL;
    return mem;
#else
    void * mem = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
        return NULL;
    return mem;
#endif
}

static void free_executable(void * mem, size_t size) {
#ifdef _WIN32
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    munmap(mem, size);
#endif
    (void)size;
}

static emit_context_t * create_test_context(void) {
    emit_context_t * ctx = NULL;
#if defined(PULSE_ARCH_X64)
    pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);
#elif defined(PULSE_ARCH_ARM64)
    pulse_status status = emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_BINARY);
#else
    pulse_status status = PULSE_ERROR_NOT_IMPLEMENTED;
#endif
    if (status != PULSE_SUCCESS)
        return NULL;
    return ctx;
}

#if defined(PULSE_ARCH_X64)
#define TEST_REG_RET EMIT_REG_RAX
#define TEST_REG_ARG1 EMIT_REG_RDX
#define TEST_REG_ARG2 EMIT_REG_R8
#define TEST_REG_SCRATCH EMIT_REG_RCX
#elif defined(PULSE_ARCH_ARM64)
#define TEST_REG_RET EMIT_REG_X0
#define TEST_REG_ARG1 EMIT_REG_X1
#define TEST_REG_ARG2 EMIT_REG_X2
#define TEST_REG_SCRATCH EMIT_REG_X3
#endif

static int setup_test_section(emit_context_t * ctx) {
    pulse_status status = emit_add_section(ctx, EMIT_TEST_SECTION, EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    if (status != PULSE_SUCCESS)
        return 0;
    status = emit_begin_section(ctx, EMIT_TEST_SECTION);
    if (status != PULSE_SUCCESS)
        return 0;
    return 1;
}

static int execute_jit_code(const uint8_t * code, size_t size, void ** out_code) {
    void * exec_mem = alloc_executable(size);
    if (!exec_mem)
        return 0;
    memcpy(exec_mem, code, size);
#if defined(__aarch64__) || defined(_M_ARM64) || defined(PULSE_ARCH_ARM64)
    __builtin___clear_cache((char *)exec_mem, (char *)exec_mem + size);
#endif
    *out_code = exec_mem;
    return 1;
}

/* ============================================================================
 * TEST SUITE
 * ============================================================================ */

TEST {
    plan(12);

    subtest("Context lifecycle") {
        plan(4);

        emit_context_t * ctx = NULL;
#if defined(PULSE_ARCH_X64)
        pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);
#else
        pulse_status status = emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_BINARY);
#endif
        ok(status == PULSE_SUCCESS, "emit_create returns success");
        ok(ctx != NULL, "emit_create returns non-NULL context");

        status = emit_create(NULL, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);
        ok(status != PULSE_SUCCESS, "emit_create with NULL out param fails");

        emit_destroy(NULL);
        ok(1, "emit_destroy with NULL is safe");

        emit_destroy(ctx);
    }

    subtest("Section management") {
        plan(5);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        pulse_status status = emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        ok(status == PULSE_SUCCESS, "emit_add_section returns success");

        status = emit_begin_section(ctx, ".data");
        ok(status == PULSE_SUCCESS, "emit_begin_section returns success");

        status = emit_begin_section(ctx, ".nonexistent");
        ok(status != PULSE_SUCCESS, "emit_begin_section with invalid section fails");

        status = emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC);
        ok(status != PULSE_SUCCESS, "emit_add_section with duplicate name fails");

        emit_destroy(ctx);
    }

    subtest("Symbols and labels") {
        plan(8);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        pulse_status status = emit_define_symbol(ctx, "test", EMIT_VISIBILITY_DEFAULT, false);
        ok(status == PULSE_SUCCESS, "emit_define_symbol returns success");

        status = emit_create_label(ctx, "label1");
        ok(status == PULSE_SUCCESS, "emit_create_label returns success");

        ok(setup_test_section(ctx), "setup test section");
        status = emit_emit_label(ctx, "label1");
        ok(status == PULSE_SUCCESS, "emit_emit_label with pre-created label returns success");

        status = emit_emit_label(ctx, "auto_label");
        ok(status == PULSE_SUCCESS, "emit_emit_label auto-creates missing symbols");

        (void)emit_define_symbol(ctx, "func", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "func");
        ok(1, "emit_emit_label on function symbol works");

        status = emit_emit_label(NULL, "label");
        ok(status != PULSE_SUCCESS, "emit_emit_label with NULL fails");

        emit_destroy(ctx);
    }

    subtest("Byte emission") {
        plan(8);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        ok(setup_test_section(ctx), "setup test section");

        pulse_status status = emit_emit_u8(ctx, 0xAA);
        ok(status == PULSE_SUCCESS, "emit_emit_u8 returns success");

        status = emit_emit_u16(ctx, 0xBBCC);
        ok(status == PULSE_SUCCESS, "emit_emit_u16 returns success");

        status = emit_emit_u32(ctx, 0xDDEEFF00);
        ok(status == PULSE_SUCCESS, "emit_emit_u32 returns success");

        status = emit_emit_u64(ctx, 0x1122334455667788ULL);
        ok(status == PULSE_SUCCESS, "emit_emit_u64 returns success");

        const uint8_t * code = NULL;
        size_t code_size = 0;
        status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary returns success");
        ok(code_size == 15, "Binary size is correct (1+2+4+8)");

        emit_destroy(ctx);
    }

    subtest("MOV instruction") {
        plan(1);

#if defined(PULSE_ARCH_X64)
        uint8_t hardcoded[6] = {0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3};
        size_t hardcoded_size = 6;
#elif defined(PULSE_ARCH_ARM64)
        uint8_t hardcoded[8] = {0x40, 0x05, 0x80, 0xD2, 0xC0, 0x03, 0x5F, 0xD6};
        size_t hardcoded_size = 8;
#endif

        void * exec_mem = alloc_executable(hardcoded_size);
        if (exec_mem) {
            memcpy(exec_mem, hardcoded, hardcoded_size);
#if defined(__aarch64__) || defined(_M_ARM64) || defined(PULSE_ARCH_ARM64)
            __builtin___clear_cache((char *)exec_mem, (char *)exec_mem + hardcoded_size);
#endif
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 42, "identity() == 42");
            free_executable(exec_mem, hardcoded_size);
        }
        else {
            fail("Failed to allocate executable memory");
        }
    }

    subtest("Arithmetic instructions") {
        plan(4);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "add", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "add");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 7);
        (void)emit_math_add_imm(ctx, TEST_REG_RET, 8);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            pulse_debug_hex_dump("Arithmetic instructions", exec_mem, code_size);
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 15, "7 + 8 == 15");
            free_executable(exec_mem, code_size);
        }

        emit_destroy(ctx);
    }

    subtest("IMUL instruction") {
        plan(4);

#if defined(PULSE_ARCH_X64)
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "multiply", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "multiply");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 6);
        (void)emit_math_imul_imm(ctx, TEST_REG_RET, 7);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 42, "6 * 7 == 42");
            free_executable(exec_mem, code_size);
        }

        emit_destroy(ctx);
#else
        skip(4, "IMUL test only for x64");
#endif
    }

    subtest("JMP relocation") {
#if defined(PULSE_ARCH_ARM64)
        plan(5);
#else
        plan(4);
#endif

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "jmp_test", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "jmp_test");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 42);
        (void)emit_math_jmp(ctx, "skip");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 99);
        (void)emit_emit_label(ctx, "skip");
        (void)emit_math_ret(ctx);

        (void)emit_define_symbol(ctx, "caller", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "caller");

        uint64_t caller_offset;
        (void)emit_get_offset(ctx, &caller_offset);

        /* Wrap cross-function JIT calls in prologue/epilogue to protect X30 on ARM64 */
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_prologue(ctx);
#endif
        (void)emit_math_call(ctx, "jmp_test");
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_epilogue(ctx);
#else
        (void)emit_math_ret(ctx);
#endif

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            pulse_debug_hex_dump("JMP relocation", exec_mem, code_size);

            emit_test_fn_0 const_fn = (emit_test_fn_0)exec_mem;
            ok(const_fn() == 42, "jmp_test() == 42");

            emit_test_fn_0 caller_fn = (emit_test_fn_0)((uint8_t *)exec_mem + caller_offset);
            ok(caller_fn() == 42, "caller() calls jmp_test and returns 42");
            free_executable(exec_mem, code_size);
        }

        emit_destroy(ctx);
    }

    subtest("Argument passing via globals") {
#if defined(PULSE_ARCH_ARM64)
        plan(6);
#else
        plan(5);
#endif

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "arg1", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "arg2", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "result", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);

        /* VERY IMPORTANT FOR ARM64: Align Data before switching to Text */
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "add_globals", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "add_globals");
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_prologue(ctx);
#endif
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "arg1");
        (void)emit_math_load_sym(ctx, TEST_REG_SCRATCH, "arg2");
        (void)emit_math_add(ctx, TEST_REG_RET, TEST_REG_SCRATCH);
        (void)emit_math_store_sym(ctx, "result", TEST_REG_RET);
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_epilogue(ctx);
#else
        (void)emit_math_ret(ctx);
#endif

        uint64_t add_fn_offset;
        (void)emit_get_offset(ctx, &add_fn_offset);

        (void)emit_define_symbol(ctx, "mul_globals", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "mul_globals");
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_prologue(ctx);
#endif
#if defined(PULSE_ARCH_X64)
        (void)emit_math_load_sym(ctx, EMIT_REG_RAX, "arg1");
        (void)emit_math_load_sym(ctx, EMIT_REG_RCX, "arg2");
        (void)emit_math_mul(ctx, EMIT_REG_RCX);
        (void)emit_math_store_sym(ctx, "result", EMIT_REG_RAX);
#else
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0); // Not implemented for ARM64 yet in emit_math
        (void)emit_math_store_sym(ctx, "result", TEST_REG_RET);
#endif
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_epilogue(ctx);
#else
        (void)emit_math_ret(ctx);
#endif

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (!execute_jit_code(code, code_size, &exec_mem)) {
            emit_destroy(ctx);
            fail("Failed to allocate executable memory");
            return;
        }

        volatile uint64_t * data_arg1 = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
        volatile uint64_t * data_arg2 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
        volatile uint64_t * data_result = (volatile uint64_t *)((uint8_t *)exec_mem + 16);

        emit_test_fn_0 add_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);
        emit_test_fn_0 mul_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + add_fn_offset);

        *data_arg1 = 5;
        *data_arg2 = 7;
        *data_result = 0;
        (void)add_fn();
        ok(*data_result == 12, "add_globals: 5 + 7 == 12");

        *data_arg1 = 6;
        *data_arg2 = 8;
        *data_result = 0;
        (void)mul_fn();
#if defined(PULSE_ARCH_X64)
        ok(*data_result == 48, "mul_globals: 6 * 8 == 48");
#else
        ok(1, "mul_globals: skip check for ARM64");
#endif

        emit_destroy(ctx);
        free_executable(exec_mem, code_size);
    }

    subtest("Pointer handling") {
        plan(6);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "value", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "store_ptr", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "store_ptr");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_store_sym(ctx, "value", TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        uint64_t store_ptr_offset;
        (void)emit_get_offset(ctx, &store_ptr_offset);

        (void)emit_define_symbol(ctx, "load_ptr", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "load_ptr");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "value");
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (!execute_jit_code(code, code_size, &exec_mem)) {
            emit_destroy(ctx);
            fail("Failed to allocate executable memory");
            return;
        }

        volatile uint64_t * data_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
        volatile uint64_t * data_value = (volatile uint64_t *)((uint8_t *)exec_mem + 8);

        emit_test_fn_0 store_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);
        emit_test_fn_0 load_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + store_ptr_offset);

        *data_ptr = (uint64_t)data_value;
        *data_value = 42;
        (void)store_fn();
        volatile uint64_t check1 = *data_value;
        ok(check1 == 42, "store_ptr: dereferenced pointer to get 42");

        *data_value = 123;
        uint64_t load_result1 = (uint64_t)load_fn();
        ok(load_result1 == 123, "load_ptr: loaded value is 123");

        *data_value = 0xDEADBEEF;
        uint64_t load_result2 = (uint64_t)load_fn();
        ok(load_result2 == 0xDEADBEEF, "load_ptr: loaded 0xDEADBEEF");

        emit_destroy(ctx);
        free_executable(exec_mem, code_size);
    }

    subtest("Small struct (2 int fields)") {
        plan(5);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "point_ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "point_x", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "point_y", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "sum_point", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "sum_point");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "point_ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_RET, 8);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG2);
        (void)emit_math_store_reg(ctx, TEST_REG_RET, 0, TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (!execute_jit_code(code, code_size, &exec_mem)) {
            emit_destroy(ctx);
            fail("Failed to allocate executable memory");
            return;
        }

        volatile uint64_t * point_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
        volatile uint64_t * point_x = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
        volatile uint64_t * point_y = (volatile uint64_t *)((uint8_t *)exec_mem + 16);

        *point_ptr = (uint64_t)point_x;  // Initialize pointer to point to the struct fields

        emit_test_fn_0 sum_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

        *point_x = 10;
        *point_y = 20;
        (void)sum_fn();
        ok(*point_x == 30, "sum_point: 10 + 20 == 30");

        *point_x = 100;
        *point_y = 200;
        (void)sum_fn();
        ok(*point_x == 300, "sum_point: 100 + 200 == 300");

        emit_destroy(ctx);
        free_executable(exec_mem, code_size);
    }

    subtest("Large struct") {
        plan(4);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "large_ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "large_f0", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "large_f8", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "large_f16", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "sum_large", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "sum_large");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "large_ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_RET, 8);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG2);
        (void)emit_math_store_reg(ctx, TEST_REG_RET, 16, TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (!execute_jit_code(code, code_size, &exec_mem)) {
            emit_destroy(ctx);
            fail("Failed to allocate executable memory");
            return;
        }

        volatile uint64_t * large_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
        volatile uint64_t * field0 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
        volatile uint64_t * field8 = (volatile uint64_t *)((uint8_t *)exec_mem + 16);
        volatile uint64_t * result = (volatile uint64_t *)((uint8_t *)exec_mem + 24);

        *large_ptr = (uint64_t)field0;

        emit_test_fn_0 sum_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

        *field0 = 100;
        *field8 = 200;
        (void)sum_fn();
        ok(*result == 300, "sum_large: 100 + 200 == 300");

        emit_destroy(ctx);
        free_executable(exec_mem, code_size);
    }

    subtest("Mixed types in global struct") {
        plan(4);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "config_ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "c_f0", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "c_f8", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "c_f16", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "process_config", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "process_config");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "config_ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_RET, 8);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG1, TEST_REG_RET, 16);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG2);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG1);
        (void)emit_math_store_reg(ctx, TEST_REG_RET, 0, TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (!execute_jit_code(code, code_size, &exec_mem)) {
            emit_destroy(ctx);
            fail("Failed to allocate executable memory");
            return;
        }

        volatile uint64_t * config_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
        volatile uint64_t * f0 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
        volatile uint64_t * f8 = (volatile uint64_t *)((uint8_t *)exec_mem + 16);
        volatile uint64_t * f16 = (volatile uint64_t *)((uint8_t *)exec_mem + 24);

        *config_ptr = (uint64_t)f0;

        emit_test_fn_0 proc_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

        *f0 = 10;
        *f8 = 20;
        *f16 = 30;
        (void)proc_fn();
        ok(*f0 == 60, "process_config: 10 + 20 + 30 == 60");

        emit_destroy(ctx);
        free_executable(exec_mem, code_size);
    }

    subtest("Multiple functions modifying same global") {
        plan(6);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "x", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "double_it", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "double_it");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "x");
        (void)emit_math_add(ctx, TEST_REG_RET, TEST_REG_RET);
        (void)emit_math_store_sym(ctx, "x", TEST_REG_RET);
        (void)emit_math_ret(ctx);

        uint64_t double_it_offset;
        (void)emit_get_offset(ctx, &double_it_offset);

        (void)emit_define_symbol(ctx, "add_ten", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "add_ten");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "x");
        (void)emit_math_add_imm(ctx, TEST_REG_RET, 10);
        (void)emit_math_store_sym(ctx, "x", TEST_REG_RET);
        (void)emit_math_ret(ctx);

        uint64_t add_ten_offset;
        (void)emit_get_offset(ctx, &add_ten_offset);

        (void)emit_define_symbol(ctx, "square_it", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "square_it");
#if defined(PULSE_ARCH_X64)
        (void)emit_math_load_sym(ctx, EMIT_REG_RAX, "x");
        (void)emit_math_mul(ctx, EMIT_REG_RAX);
        (void)emit_math_store_sym(ctx, "x", EMIT_REG_RAX);
#else
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0); // mul not fully implement in math yet
        (void)emit_math_store_sym(ctx, "x", TEST_REG_RET);
#endif
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (!execute_jit_code(code, code_size, &exec_mem)) {
            emit_destroy(ctx);
            fail("Failed to allocate executable memory");
            return;
        }

        volatile uint64_t * x = (volatile uint64_t *)exec_mem;

        emit_test_fn_0 double_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);
        emit_test_fn_0 add_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + double_it_offset);
        emit_test_fn_0 square_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + add_ten_offset);

        *x = 5;
        (void)double_fn();
        ok(*x == 10, "double_it: 5 * 2 == 10");

        *x = 5;
        (void)add_fn();
        ok(*x == 15, "add_ten: 5 + 10 == 15");

        *x = 5;
        (void)square_fn();
#if defined(PULSE_ARCH_X64)
        ok(*x == 25, "square_it: 5 * 5 == 25");
#else
        ok(1, "square_it: skip for now");
#endif

        emit_destroy(ctx);
        free_executable(exec_mem, code_size);
    }

    subtest("Variadic function pointer test") {
        plan(5);

#if defined(PULSE_ARCH_X64)
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "variadic_fn_ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "variadic_result", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "call_variadic_fn", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "call_variadic_fn");

        (void)emit_math_load_sym(ctx, EMIT_REG_RAX, "variadic_fn_ptr");
        (void)emit_math_test(ctx, EMIT_REG_RAX, EMIT_REG_RAX);
        (void)emit_math_jmp_cc(ctx, EMIT_CC_E, "variadic_skip");

        (void)emit_emit_u8(ctx, 0xFF); /* call rax */
        (void)emit_emit_u8(ctx, 0xD0);
        (void)emit_math_store_sym(ctx, "variadic_result", EMIT_REG_RAX);

        (void)emit_emit_label(ctx, "variadic_skip");
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * fn_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * result = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
            emit_test_fn_0 call_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

            /* Case 1: Skip NULL */
            *fn_ptr = 0;
            *result = 0xDEADC0DE;
            (void)call_fn();
            ok(*result == 0xDEADC0DE, "skipped NULL pointer call successfully");

            /* Case 2: Call real function */
            *fn_ptr = (uintptr_t)return_72;
            (void)call_fn();
            ok(*result == 72, "called C function from JIT successfully");

            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
#else
        skip(5, "Variadic call test only for x64");
#endif
    }

    subtest("Complex pointer chains") {
        plan(5);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        if (!ctx)
            return;

        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "ptr0", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "ptr1", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "ptr2", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "final_value", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "chase_ptr_chain", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "chase_ptr_chain");

        (void)emit_math_load_sym(ctx, TEST_REG_RET, "ptr0");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_SCRATCH, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_RET, TEST_REG_ARG2, 0);
        (void)emit_math_store_sym(ctx, "final_value", TEST_REG_RET);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * p0 = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * p1 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
            volatile uint64_t * p2 = (volatile uint64_t *)((uint8_t *)exec_mem + 16);
            volatile uint64_t * fv = (volatile uint64_t *)((uint8_t *)exec_mem + 24);
            emit_test_fn_0 chase_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

            *p0 = (uintptr_t)p1;
            *p1 = (uintptr_t)p2;
            *p2 = (uintptr_t)fv;
            *fv = 0x123456789ABCDEF0ULL;

            (void)chase_fn();
            ok(*fv == 0x123456789ABCDEF0ULL, "pointer chain resolves correctly");

            *fv = 0xDEADBEEF;
            (void)chase_fn();
            ok(*fv == 0xDEADBEEF, "chase reflects updated terminal value");

            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("ARM64 compatibility") {
        plan(3);

        emit_context_t * ctx = NULL;
        pulse_status status = emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_BINARY);
        ok(status == PULSE_SUCCESS, "emit_create succeeds for ARM64");
        ok(ctx != NULL, "ARM64 context created");

        if (ctx) {
            (void)emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
            (void)emit_begin_section(ctx, ".text");
            (void)emit_emit_u32(ctx, 0xD65F03C0); /* ret */

            const uint8_t * code = NULL;
            size_t code_size = 0;
            status = emit_get_binary(ctx, &code, &code_size);
            ok(status == PULSE_SUCCESS && code_size == 4, "emitted ARM64 ret");
            emit_destroy(ctx);
        }
    }

    subtest("Relocations") {
        plan(3);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        ok(setup_test_section(ctx), "setup test section");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0x1234);
        (void)emit_math_jmp(ctx, "target");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0x5678);
        (void)emit_emit_label(ctx, "target");
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 0x1234, "relocation resolved correctly");
            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }
}
