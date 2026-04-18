/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_x64.c
 * @brief x86-64 instruction encoding implementation.
 */
#define PULSE_BUILDING
#include "emit_x64.h"

#define EMIT_REG_NEEDS_REX(reg) ((reg) >= 8)

pulse_status emit_x64_rex(emit_context_t * ctx, bool w, bool r, bool x, bool b) {
    if (ctx->arch == EMIT_ARCH_X86_64 && (w || r || x || b)) {
        uint8_t rex = 0x40 | (w << 3) | (r << 2) | (x << 1) | b;
        return emit_emit_u8(ctx, rex);
    }
    return PULSE_SUCCESS;
}

pulse_status emit_x64_mov_imm(emit_context_t * ctx, emit_register_t dest, uint64_t imm) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xB8 | (dest & 0x07)));
    EMIT_CHECK(emit_emit_u64(ctx, imm));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_mov_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x89));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_add(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x01));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_add_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x81));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | (dest & 0x07)));
    EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_sub(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x29));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_sub_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x81));
    EMIT_CHECK(emit_emit_u8(ctx, 0xE8 | (dest & 0x07)));
    EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_mul(emit_context_t * ctx, emit_register_t src) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(src)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xF7));
    EMIT_CHECK(emit_emit_u8(ctx, 0xE0 | (src & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_imul_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    if (imm >= -128 && imm <= 127) {
        EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x6B));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (dest & 0x07)));
        EMIT_CHECK(emit_emit_u8(ctx, (uint8_t)imm));
    }
    else {
        EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x69));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (dest & 0x07)));
        EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
    }
    return PULSE_SUCCESS;
}

pulse_status emit_x64_and(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x21));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_or(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x09));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_xor(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x31));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_not(emit_context_t * ctx, emit_register_t reg) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xF7));
    EMIT_CHECK(emit_emit_u8(ctx, 0xD0 | (reg & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_neg(emit_context_t * ctx, emit_register_t reg) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xF7));
    EMIT_CHECK(emit_emit_u8(ctx, 0xD8 | (reg & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_shl(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (src != EMIT_REG_RCX)
        return PULSE_ERROR_INVALID_ARGUMENT;
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xD3));
    EMIT_CHECK(emit_emit_u8(ctx, 0xE0 | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_shr(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (src != EMIT_REG_RCX)
        return PULSE_ERROR_INVALID_ARGUMENT;
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xD3));
    EMIT_CHECK(emit_emit_u8(ctx, 0xE8 | (dest & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_sal(emit_context_t * ctx, emit_register_t reg, uint8_t amount) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC1));
    EMIT_CHECK(emit_emit_u8(ctx, 0xE0 | (reg & 0x07)));
    EMIT_CHECK(emit_emit_u8(ctx, amount));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_sar(emit_context_t * ctx, emit_register_t reg, uint8_t amount) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC1));
    EMIT_CHECK(emit_emit_u8(ctx, 0xF8 | (reg & 0x07)));
    EMIT_CHECK(emit_emit_u8(ctx, amount));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_cmp(emit_context_t * ctx, emit_register_t a, emit_register_t b) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(b), false, EMIT_REG_NEEDS_REX(a)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x39));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((b & 0x07) << 3) | (a & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_cmp_imm(emit_context_t * ctx, emit_register_t reg, int32_t imm) {
    EMIT_CHECK(emit_x64_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x81));
    EMIT_CHECK(emit_emit_u8(ctx, 0xF8 | (reg & 0x07)));
    EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_test(emit_context_t * ctx, emit_register_t a, emit_register_t b) {
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(b), false, EMIT_REG_NEEDS_REX(a)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x85));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((b & 0x07) << 3) | (a & 0x07)));
    return PULSE_SUCCESS;
}

static const uint8_t x86_jcc_opcodes[16] = {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F};

pulse_status emit_x64_jmp(emit_context_t * ctx, const char * label) {
    uint64_t jump_offset = ctx->current_section->size;
    EMIT_CHECK(emit_emit_u8(ctx, 0xE9));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(_emit_add_relocation(ctx, label, jump_offset + 1, 4, 5, true));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_jmp_cc(emit_context_t * ctx, emit_cc_t cc, const char * label) {
    uint64_t jump_offset = ctx->current_section->size;
    EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
    EMIT_CHECK(emit_emit_u8(ctx, x86_jcc_opcodes[cc]));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(_emit_add_relocation(ctx, label, jump_offset + 2, 4, 6, true));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_call(emit_context_t * ctx, const char * name) {
    uint64_t call_offset = ctx->current_section->size;
    EMIT_CHECK(emit_emit_u8(ctx, 0xE8));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(_emit_add_relocation(ctx, name, call_offset + 1, 4, 5, true));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_call_reg(emit_context_t * ctx, emit_register_t reg) {
    EMIT_CHECK(emit_x64_rex(ctx, false, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0xFF));
    EMIT_CHECK(emit_emit_u8(ctx, 0xD0 | (reg & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_push(emit_context_t * ctx, emit_register_t reg) {
    EMIT_CHECK(emit_x64_rex(ctx, false, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x50 | (reg & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_pop(emit_context_t * ctx, emit_register_t reg) {
    EMIT_CHECK(emit_x64_rex(ctx, false, false, false, EMIT_REG_NEEDS_REX(reg)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x58 | (reg & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_prologue(emit_context_t * ctx) {
    EMIT_CHECK(emit_emit_u8(ctx, 0x55));
    EMIT_CHECK(emit_emit_u8(ctx, 0x48));
    EMIT_CHECK(emit_emit_u8(ctx, 0x8B));
    EMIT_CHECK(emit_emit_u8(ctx, 0xEC));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_epilogue(emit_context_t * ctx) {
    EMIT_CHECK(emit_emit_u8(ctx, 0xC9));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC3));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_ret(emit_context_t * ctx) { return emit_emit_u8(ctx, 0xC3); }

pulse_status emit_x64_load_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    uint8_t mod = (offset == 0 && (base & 0x07) != 5) ? 0x00 : ((offset >= -128 && offset <= 127) ? 0x40 : 0x80);
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(base)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x8B));
    EMIT_CHECK(emit_emit_u8(ctx, mod | ((dest & 0x07) << 3) | (base & 0x07)));
    if ((base & 0x07) == 4)
        EMIT_CHECK(emit_emit_u8(ctx, 0x24));
    if (mod == 0x40)
        EMIT_CHECK(emit_emit_u8(ctx, (uint8_t)offset));
    else if (mod == 0x80)
        EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)offset));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_store_reg(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src) {
    uint8_t mod = (offset == 0 && (base & 0x07) != 5) ? 0x00 : ((offset >= -128 && offset <= 127) ? 0x40 : 0x80);
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(base)));
    EMIT_CHECK(emit_emit_u8(ctx, 0x89));
    EMIT_CHECK(emit_emit_u8(ctx, mod | ((src & 0x07) << 3) | (base & 0x07)));
    if ((base & 0x07) == 4)
        EMIT_CHECK(emit_emit_u8(ctx, 0x24));
    if (mod == 0x40)
        EMIT_CHECK(emit_emit_u8(ctx, (uint8_t)offset));
    else if (mod == 0x80)
        EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)offset));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_load_sym(emit_context_t * ctx, emit_register_t dest, const char * sym) {
    uint64_t load_offset = ctx->current_section->size;
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(dest), false, false));
    EMIT_CHECK(emit_emit_u8(ctx, 0x8B));
    EMIT_CHECK(emit_emit_u8(ctx, 0x05 | ((dest & 0x07) << 3)));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(_emit_add_relocation(ctx, sym, load_offset + 3, 4, 7, true));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_store_sym(emit_context_t * ctx, const char * sym, emit_register_t src) {
    uint64_t store_offset = ctx->current_section->size;
    EMIT_CHECK(emit_x64_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, false));
    EMIT_CHECK(emit_emit_u8(ctx, 0x89));
    EMIT_CHECK(emit_emit_u8(ctx, 0x05 | ((src & 0x07) << 3)));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(_emit_add_relocation(ctx, sym, store_offset + 3, 4, 7, true));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_movdqa(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    EMIT_CHECK(emit_emit_u8(ctx, 0x66));
    EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
    EMIT_CHECK(emit_emit_u8(ctx, 0x6F));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (src & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_padd(emit_context_t * ctx, emit_register_t dest, emit_register_t src, int size) {
    uint8_t opcode;
    switch (size) {
    case 8:
        opcode = 0xFC;
        break;
    case 16:
        opcode = 0xFD;
        break;
    case 32:
        opcode = 0xFE;
        break;
    case 64:
        opcode = 0xD4;
        break;
    default:
        return PULSE_ERROR_INVALID_ARGUMENT;
    }
    EMIT_CHECK(emit_emit_u8(ctx, 0x66));
    EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
    EMIT_CHECK(emit_emit_u8(ctx, opcode));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (src & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_pcmpeq(emit_context_t * ctx, emit_register_t dest, emit_register_t src, int size) {
    uint8_t opcode;
    switch (size) {
    case 8:
        opcode = 0x74;
        break;
    case 16:
        opcode = 0x75;
        break;
    case 32:
        opcode = 0x76;
        break;
    case 64:
        opcode = 0x76;
        break;
    default:
        return PULSE_ERROR_INVALID_ARGUMENT;
    }
    EMIT_CHECK(emit_emit_u8(ctx, 0x66));
    EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
    EMIT_CHECK(emit_emit_u8(ctx, opcode));
    EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (src & 0x07)));
    return PULSE_SUCCESS;
}

pulse_status emit_x64_nop(emit_context_t * ctx, uint8_t size) {
    for (uint8_t i = 0; i < size; i++)
        EMIT_CHECK(emit_emit_u8(ctx, 0x90));
    return PULSE_SUCCESS;
}
