/**
 * @file fuzz_emit.c
 * @brief Fuzzer for the Pulse emit library (JIT code generation).
 */

#include "pulse/emit/emit.h"
#include "pulse/emit/emit_math.h"
#include "pulse_fuzz_helpers.h"

#ifndef USE_AFL
int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size);
#endif

static void fuzz_emit_basic(fuzz_input_t * in) {
    emit_context_t * ctx = NULL;

    uint32_t arch_val;
    fuzz_consume_uint32(in, &arch_val);
    emit_architecture_t arch = (arch_val % 2 == 0) ? EMIT_ARCH_X86_64 : EMIT_ARCH_AARCH64;

    if (emit_create(&ctx, arch, EMIT_FORMAT_BINARY) != PULSE_SUCCESS)
        return;

    uint32_t num_sections = fuzz_consume_uint32_range(in, 0, 10);
    for (uint32_t i = 0; i < num_sections && ctx; i++) {
        char name[32];
        snprintf(name, sizeof(name), "section%u", i);
        emit_add_section(ctx, name, EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
        emit_begin_section(ctx, name);

        uint32_t num_bytes = fuzz_consume_uint32_range(in, 0, 256);
        for (uint32_t j = 0; j < num_bytes; j++) {
            uint8_t byte;
            if (fuzz_consume_uint8(in, &byte))
                emit_emit_u8(ctx, byte);
        }
    }

    const uint8_t * binary = NULL;
    size_t binary_size = 0;
    if (ctx)
        emit_get_binary(ctx, &binary, &binary_size);

    if (ctx)
        emit_destroy(ctx);
}

static void fuzz_emit_math(fuzz_input_t * in) {
    emit_context_t * ctx = NULL;

    if (emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY) != PULSE_SUCCESS)
        return;

    emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    emit_begin_section(ctx, ".text");

    uint32_t num_ops = fuzz_consume_uint32_range(in, 0, 100);

    for (uint32_t i = 0; i < num_ops && ctx; i++) {
        uint8_t op;
        if (!fuzz_consume_uint8(in, &op))
            break;

        switch (op % 16) {
        case 0:
            emit_math_mov_imm(ctx, EMIT_REG_RAX, fuzz_consume_uint32_range(in, 0, 0xFFFFFFFF));
            break;
        case 1:
            emit_math_add(ctx, EMIT_REG_RAX, EMIT_REG_RCX);
            break;
        case 2:
            emit_math_sub(ctx, EMIT_REG_RAX, EMIT_REG_RCX);
            break;
        case 3:
            emit_math_mul(ctx, EMIT_REG_RCX);
            break;
        case 4:
            emit_math_and(ctx, EMIT_REG_RAX, EMIT_REG_RCX);
            break;
        case 5:
            emit_math_or(ctx, EMIT_REG_RAX, EMIT_REG_RCX);
            break;
        case 6:
            emit_math_xor(ctx, EMIT_REG_RAX, EMIT_REG_RCX);
            break;
        case 7:
            emit_math_not(ctx, EMIT_REG_RAX);
            break;
        case 8:
            emit_math_neg(ctx, EMIT_REG_RAX);
            break;
        case 9:
            emit_math_push(ctx, EMIT_REG_RAX);
            break;
        case 10:
            emit_math_pop(ctx, EMIT_REG_RAX);
            break;
        case 11:
            emit_math_cmp(ctx, EMIT_REG_RAX, EMIT_REG_RCX);
            break;
        case 12:
            emit_math_ret(ctx);
            break;
        case 13:
            emit_align(ctx, 16);
            break;
        case 14:
            emit_math_prologue(ctx);
            break;
        case 15:
            emit_math_epilogue(ctx);
            break;
        }
    }

    const uint8_t * binary = NULL;
    size_t binary_size = 0;
    if (ctx)
        emit_get_binary(ctx, &binary, &binary_size);

    if (ctx)
        emit_destroy(ctx);
}

static void fuzz_emit_run(const uint8_t * data, size_t size) {
    if (size < 4)
        return;

    fuzz_input_t in = {.data = data, .size = size, .pos = 0};

    uint8_t mode;
    if (!fuzz_consume_uint8(&in, &mode))
        return;

    switch (mode % 2) {
    case 0:
        fuzz_emit_basic(&in);
        break;
    case 1:
        fuzz_emit_math(&in);
        break;
    }
}

#ifndef USE_AFL
int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    fuzz_emit_run(data, size);
    return 0;
}
#else
#include <unistd.h>

int main(void) {
    unsigned char buf[1024 * 16];
    while (__AFL_LOOP(10000)) {
        ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
        if (len < 0)
            return 1;
        fuzz_emit_run(buf, (size_t)len);
    }
    return 0;
}
#endif
