/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_arm64.c
 * @brief ARM64 instruction encoding implementation.
 *
 * This file provides the instruction encoding for the ARM64 (AArch64) architecture.
 * ARM64 uses a uniform 32-bit instruction encoding with various instruction formats.
 *
 * Instruction encoding overview:
 * - Bits [31:28]: Primary condition code
 * - Bits [27:26]: Instruction type identifier
 * - Bits [25:24]: Additional instruction type bits
 * - Bits [23:21]: Opcode/function field
 * - Bits [20:10]: Operands and additional opcode bits
 * - Bits [9:5]: Register or additional operands
 * - Bits [4:0]: Destination/first source register (Rd/Rn)
 */
#define PULSE_BUILDING
#include "emit_arm64.h"

pulse_status emit_add_relocation(
    emit_context_t * ctx, const char * name, uint64_t offset, uint8_t size, uint8_t inst_size);

#define EMIT_CHECK(x)            \
    do {                         \
        pulse_status _s = (x);   \
        if (_s != PULSE_SUCCESS) \
            return _s;           \
    } while (0)

#define ARM64_COND_EQ 0x0
#define ARM64_COND_NE 0x1
#define ARM64_COND_CS 0x2
#define ARM64_COND_HS ARM64_COND_CS
#define ARM64_COND_CC 0x3
#define ARM64_COND_LO ARM64_COND_CC
#define ARM64_COND_MI 0x4
#define ARM64_COND_PL 0x5
#define ARM64_COND_VS 0x6
#define ARM64_COND_VC 0x7
#define ARM64_COND_HI 0x8
#define ARM64_COND_LS 0x9
#define ARM64_COND_GE 0xA
#define ARM64_COND_LT 0xB
#define ARM64_COND_GT 0xC
#define ARM64_COND_LE 0xD
#define ARM64_COND_AL 0xE
#define ARM64_COND_NV 0xF

static uint8_t x64_to_arm64_cc(emit_cc_t cc) {
    switch (cc) {
    case EMIT_CC_E:
        return 0x0;
    case EMIT_CC_NE:
        return 0x1;
    case EMIT_CC_B:
        return 0x2;
    case EMIT_CC_AE:
        return 0x3;
    case EMIT_CC_S:
        return 0x4;
    case EMIT_CC_NS:
        return 0x5;
    case EMIT_CC_P:
        return 0x6;
    case EMIT_CC_NP:
        return 0x7;
    case EMIT_CC_A:
        return 0x8;
    case EMIT_CC_BE:
        return 0x9;
    case EMIT_CC_GE:
        return 0xA;
    case EMIT_CC_L:
        return 0xB;
    case EMIT_CC_G:
        return 0xC;
    case EMIT_CC_LE:
        return 0xD;
    case EMIT_CC_AL:
    case EMIT_CC_NOCARE:
    default:
        return 0xE;
    }
}

pulse_status emit_arm64_movz(emit_context_t * ctx, emit_register_t reg, uint16_t imm, int shift, bool is_64bit) {
    uint32_t instr = 0xD2800000;
    instr |= (is_64bit ? 1U : 0U) << 31;
    instr |= ((shift / 16) & 0x3) << 21;
    instr |= (imm & 0xFFFF) << 5;
    instr |= (reg & 0x1F);
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_movn(emit_context_t * ctx, emit_register_t reg, uint16_t imm, int shift, bool is_64bit) {
    uint32_t instr = 0x92800000;
    instr |= (is_64bit ? 1U : 0U) << 31;
    instr |= ((shift / 16) & 0x3) << 21;
    instr |= (imm & 0xFFFF) << 5;
    instr |= (reg & 0x1F);
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_movk(emit_context_t * ctx, emit_register_t reg, uint16_t imm, int shift, bool is_64bit) {
    uint32_t instr = 0xF2800000;
    instr |= (is_64bit ? 1U : 0U) << 31;
    instr |= ((shift / 16) & 0x3) << 21;
    instr |= (imm & 0xFFFF) << 5;
    instr |= (reg & 0x1F);
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_add(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0x8B000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_adds(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0xAB000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_add_imm(emit_context_t * ctx, emit_register_t dest, emit_register_t src, int16_t imm) {
    uint32_t instr = 0x91000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src & 0x1F) << 5;
    uint16_t imm12 = (imm >= 0) ? (imm & 0xFFF) : ((~(-imm) + 1) & 0xFFF);
    instr |= imm12 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_sub(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0xCB000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_subs(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0xEB000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_mul(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0x9B007C00;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_and(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0x8A000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_orr(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0xAA000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_eor(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0xCA000000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_mvn(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    uint32_t instr = 0xAA2003E0;
    instr |= (dest & 0x1F) << 0;
    instr |= (src & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_neg(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    uint32_t instr = 0xCB2003E0;
    instr |= (dest & 0x1F) << 0;
    instr |= (src & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_cmp(emit_context_t * ctx, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0xEB00001F;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_cmp_imm(emit_context_t * ctx, emit_register_t src, int16_t imm) {
    uint32_t instr = 0xF100001F;
    instr |= (src & 0x1F) << 5;
    uint16_t imm12 = (imm >= 0) ? (imm & 0xFFF) : ((~(-imm) + 1) & 0xFFF);
    instr |= imm12 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_tst(emit_context_t * ctx, emit_register_t src1, emit_register_t src2) {
    uint32_t instr = 0x6A00003F;
    instr |= (src1 & 0x1F) << 5;
    instr |= (src2 & 0x1F) << 16;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_lsl(emit_context_t * ctx, emit_register_t dest, emit_register_t src, uint8_t shift) {
    uint32_t instr = 0xD37CB000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src & 0x1F) << 5;
    uint8_t imm6 = (64 - (shift & 0x3F)) & 0x3F;
    instr |= imm6 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_lsr(emit_context_t * ctx, emit_register_t dest, emit_register_t src, uint8_t shift) {
    uint32_t instr = 0xD3608000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src & 0x1F) << 5;
    uint8_t imm6 = shift & 0x3F;
    instr |= imm6 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_asr(emit_context_t * ctx, emit_register_t dest, emit_register_t src, uint8_t shift) {
    uint32_t instr = 0xD360C000;
    instr |= (dest & 0x1F) << 0;
    instr |= (src & 0x1F) << 5;
    uint8_t imm6 = shift & 0x3F;
    instr |= imm6 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_mov_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    return emit_arm64_orr(ctx, dest, 31, src);
}

pulse_status emit_arm64_b_cond(emit_context_t * ctx, emit_cc_t cc, const char * label) {
    uint64_t branch_offset = ctx->current_section->size;
    uint32_t instr = 0x54000000;
    emit_cc_t arm64_cc = x64_to_arm64_cc(cc);
    instr |= (arm64_cc & 0xF) << 0;
    EMIT_CHECK(emit_emit_u32(ctx, instr));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(emit_add_relocation(ctx, label, branch_offset, 4, 8));
    return PULSE_SUCCESS;
}

pulse_status emit_arm64_b(emit_context_t * ctx, const char * label) {
    uint64_t branch_offset = ctx->current_section->size;
    uint32_t instr = 0x14000000;
    EMIT_CHECK(emit_emit_u32(ctx, instr));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(emit_add_relocation(ctx, label, branch_offset, 4, 8));
    return PULSE_SUCCESS;
}

pulse_status emit_arm64_bl(emit_context_t * ctx, const char * name) {
    uint64_t call_offset = ctx->current_section->size;
    uint32_t instr = 0x94000000;
    EMIT_CHECK(emit_emit_u32(ctx, instr));
    EMIT_CHECK(emit_emit_u32(ctx, 0));
    EMIT_CHECK(emit_add_relocation(ctx, name, call_offset, 4, 8));
    return PULSE_SUCCESS;
}

pulse_status emit_arm64_ret(emit_context_t * ctx, emit_register_t lr) {
    uint32_t instr = 0xD65F03C0;
    instr |= (lr & 0x1F) << 0;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_ldr(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    uint32_t instr = 0xF9400000;
    instr |= (dest & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint16_t offset12 = (offset >= 0) ? ((offset / 8) & 0xFFF) : 0;
    instr |= offset12 << 10;
    if (offset < 0) {
        uint32_t pre_index = 0xF8100000;
        pre_index |= (dest & 0x1F) << 0;
        pre_index |= (base & 0x1F) << 5;
        int16_t simm9 = offset & 0x1FF;
        if (simm9 < 0)
            simm9 = -simm9;
        pre_index |= ((~simm9 + 1) & 0x1FF) << 12;
        pre_index |= 1 << 24;
        return emit_emit_u32(ctx, pre_index);
    }
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_ldrb(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    uint32_t instr = 0x39400000;
    instr |= (dest & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint8_t offset8 = (offset >= 0) ? (offset & 0xFF) : 0;
    instr |= offset8 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_ldrh(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    uint32_t instr = 0x78400000;
    instr |= (dest & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint8_t offset8 = (offset >= 0) ? ((offset / 2) & 0xFF) : 0;
    instr |= offset8 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_ldrsb(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    uint32_t instr = 0x39C00000;
    instr |= (dest & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint8_t offset8 = (offset >= 0) ? (offset & 0xFF) : 0;
    instr |= offset8 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_ldrsh(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    uint32_t instr = 0x79C00000;
    instr |= (dest & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint8_t offset8 = (offset >= 0) ? ((offset / 2) & 0xFF) : 0;
    instr |= offset8 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_ldrsw(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    uint32_t instr = 0xB9800000;
    instr |= (dest & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint8_t offset8 = (offset >= 0) ? ((offset / 4) & 0xFF) : 0;
    instr |= offset8 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_str(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src) {
    uint32_t instr = 0xF9000000;
    instr |= (src & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint16_t offset12 = (offset >= 0) ? ((offset / 8) & 0xFFF) : 0;
    instr |= offset12 << 10;
    if (offset < 0) {
        uint32_t pre_index = 0xF8000000;
        pre_index |= (src & 0x1F) << 0;
        pre_index |= (base & 0x1F) << 5;
        int16_t simm9 = offset & 0x1FF;
        if (simm9 < 0)
            simm9 = -simm9;
        pre_index |= ((~simm9 + 1) & 0x1FF) << 12;
        pre_index |= 1 << 24;
        return emit_emit_u32(ctx, pre_index);
    }
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_strb(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src) {
    uint32_t instr = 0x39000000;
    instr |= (src & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint8_t offset8 = (offset >= 0) ? (offset & 0xFF) : 0;
    instr |= offset8 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_strh(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src) {
    uint32_t instr = 0x78000000;
    instr |= (src & 0x1F) << 0;
    instr |= (base & 0x1F) << 5;
    uint8_t offset8 = (offset >= 0) ? ((offset / 2) & 0xFF) : 0;
    instr |= offset8 << 10;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_nop(emit_context_t * ctx) { return emit_emit_u32(ctx, 0xD503201F); }

pulse_status emit_arm64_brk(emit_context_t * ctx, uint16_t imm) {
    uint32_t instr = 0xD4200000;
    instr |= (imm & 0xFFFF) << 5;
    return emit_emit_u32(ctx, instr);
}

pulse_status emit_arm64_svc(emit_context_t * ctx, uint16_t imm) {
    uint32_t instr = 0xD4000001;
    instr |= (imm & 0xFFFF) << 5;
    return emit_emit_u32(ctx, instr);
}
