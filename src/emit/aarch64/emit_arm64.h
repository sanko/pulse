/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_arm64.h
 * @brief ARM64 (AArch64) instruction encoding declarations.
 *
 * This header declares the architecture-specific instruction emitters for
 * ARM64, also known as AArch64 or ARMv8-A. These functions generate
 * machine code that conforms to the AAPCS64 (ARM Architecture Procedure
 * Call Standard) used on Linux, macOS (Apple Silicon), and Windows (ARM64).
 */
#ifndef PULSE_EMIT_ARM64_H
#define PULSE_EMIT_ARM64_H

#include "../emit_internals.h"
#include "pulse/emit/emit.h"
#include "pulse/emit/emit_math.h"

/**
 * @brief ARM64 register aliases for clarity.
 */
typedef enum {
    ARM64_REG_X0 = 0, /**< First integer argument/return */
    ARM64_REG_X1 = 1, /**< Second integer argument */
    ARM64_REG_X2 = 2, /**< Third integer argument */
    ARM64_REG_X3 = 3, /**< Fourth integer argument */
    ARM64_REG_X4 = 4,
    ARM64_REG_X5 = 5,
    ARM64_REG_X6 = 6,
    ARM64_REG_X7 = 7,
    ARM64_REG_X8 = 8, /**< Indirect return value */
    ARM64_REG_X9 = 9,
    ARM64_REG_X10 = 10,
    ARM64_REG_X11 = 11,
    ARM64_REG_X12 = 12,
    ARM64_REG_X13 = 13,
    ARM64_REG_X14 = 14,
    ARM64_REG_X15 = 15,
    ARM64_REG_X16 = 16, /**< Intra-procedure-call scratch */
    ARM64_REG_X17 = 17, /**< Intra-procedure-call scratch */
    ARM64_REG_X18 = 18, /**< Platform register */
    ARM64_REG_X19 = 19,
    ARM64_REG_X20 = 20,
    ARM64_REG_X21 = 21,
    ARM64_REG_X22 = 22,
    ARM64_REG_X23 = 23,
    ARM64_REG_X24 = 24,
    ARM64_REG_X25 = 25,
    ARM64_REG_X26 = 26,
    ARM64_REG_X27 = 27,
    ARM64_REG_X28 = 28,
    ARM64_REG_X29 = 29, /**< Frame pointer */
    ARM64_REG_X30 = 30, /**< Link register (LR) */
    ARM64_REG_SP = 31,  /**< Stack pointer */
    ARM64_REG_XZR = 31, /**< Zero register (when used as source) */

    /** Floating point registers */
    ARM64_REG_Q0 = 0,
    ARM64_REG_Q1 = 1,
    ARM64_REG_Q2 = 2,
    ARM64_REG_Q3 = 3,
} arm64_register_t;

/**
 * @brief Emits MOVZ Wd, #imm16 (datasize=32) or MOVZ Xd, #imm16 (datasize=64).
 * @param[in] ctx The emit context.
 * @param[in] reg Destination register.
 * @param[in] imm Immediate value.
 * @param[in] shift Shift amount (0, 16, 32, or 48).
 * @param[in] is_64bit true for X register, false for W register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_movz(emit_context_t * ctx, emit_register_t reg, uint16_t imm, int shift, bool is_64bit);

/**
 * @brief Emits MOVN Wd, #imm16 (datasize=32) or MOVN Xd, #imm16 (datasize=64).
 * @param[in] ctx The emit context.
 * @param[in] reg Destination register.
 * @param[in] imm Immediate value.
 * @param[in] shift Shift amount (0, 16, 32, or 48).
 * @param[in] is_64bit true for X register, false for W register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_movn(emit_context_t * ctx, emit_register_t reg, uint16_t imm, int shift, bool is_64bit);

/**
 * @brief Emits ADD Xd, Xn, Xm ( Rd = Rn + Rm ).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_add(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits ADDS Xd, Xn, Xm with flag update.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_adds(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits SUB Xd, Xn, Xm ( Rd = Rn - Rm ).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_sub(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits SUBS Xd, Xn, Xm with flag update.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_subs(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits MUL Xd, Xn, Xm ( Rd = Rn * Rm ).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_mul(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits AND Xd, Xn, Xm ( Rd = Rn & Rm ).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_and(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits ORR Xd, Xn, Xm ( Rd = Rn | Rm ).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_orr(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits EOR Xd, Xn, Xm ( Rd = Rn ^ Rm ).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src1 First source register.
 * @param[in] src2 Second source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_eor(emit_context_t * ctx, emit_register_t dest, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits CMP Xn, Xm (updates flags).
 * @param[in] ctx The emit context.
 * @param[in] src1 First register.
 * @param[in] src2 Second register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_cmp(emit_context_t * ctx, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits B.cond (conditional branch).
 * @param[in] ctx The emit context.
 * @param[in] cc Condition code.
 * @param[in] label Target label name.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_b_cond(emit_context_t * ctx, emit_cc_t cc, const char * label);

/**
 * @brief Emits B (unconditional branch).
 * @param[in] ctx The emit context.
 * @param[in] label Target label name.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_b(emit_context_t * ctx, const char * label);

/**
 * @brief Emits BL (branch with link).
 * @param[in] ctx The emit context.
 * @param[in] name Target function name.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_bl(emit_context_t * ctx, const char * name);

/**
 * @brief Emits RET (return from function).
 * @param[in] ctx The emit context.
 * @param[in] lr Register containing return address (usually X30).
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_ret(emit_context_t * ctx, emit_register_t lr);

/**
 * @brief Emits LDR Xd, [Xn, #offset] (load register).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Immediate offset.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_ldr(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset);

/**
 * @brief Emits STR Xd, [Xn, #offset] (store register).
 * @param[in] ctx The emit context.
 * @param[in] base Base register.
 * @param[in] offset Immediate offset.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_str(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src);

/**
 * @brief Emits NOP instruction.
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_nop(emit_context_t * ctx);

/**
 * @brief Emits MOVK Xd, #imm16 (move immediate into register, keeping other bits).
 * @param[in] ctx The emit context.
 * @param[in] reg Destination register.
 * @param[in] imm Immediate value.
 * @param[in] shift Shift amount (0, 16, 32, or 48).
 * @param[in] is_64bit true for X register, false for W register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_movk(emit_context_t * ctx, emit_register_t reg, uint16_t imm, int shift, bool is_64bit);

/**
 * @brief Emits MVN Xd, Xm (bitwise NOT).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_mvn(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits NEG Xd, Xm (negate).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_neg(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits CMP Xn, #imm12 (compare with immediate).
 * @param[in] ctx The emit context.
 * @param[in] src Source register.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_cmp_imm(emit_context_t * ctx, emit_register_t src, int16_t imm);

/**
 * @brief Emits TST Xn, Xm (test bits).
 * @param[in] ctx The emit context.
 * @param[in] src1 First register.
 * @param[in] src2 Second register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_tst(emit_context_t * ctx, emit_register_t src1, emit_register_t src2);

/**
 * @brief Emits LSL Xd, Xn, #shift (logical shift left).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @param[in] shift Shift amount.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_lsl(emit_context_t * ctx, emit_register_t dest, emit_register_t src, uint8_t shift);

/**
 * @brief Emits LSR Xd, Xn, #shift (logical shift right).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @param[in] shift Shift amount.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_lsr(emit_context_t * ctx, emit_register_t dest, emit_register_t src, uint8_t shift);

/**
 * @brief Emits ASR Xd, Xn, #shift (arithmetic shift right).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @param[in] shift Shift amount.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_asr(emit_context_t * ctx, emit_register_t dest, emit_register_t src, uint8_t shift);

/**
 * @brief Emits MOV Xd, Xm (register move via ORR with XZR).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_mov_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits ADD Xd, Xn, #imm (add immediate).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_add_imm(emit_context_t * ctx, emit_register_t dest, emit_register_t src, int16_t imm);

/**
 * @brief Emits LDRB Wd, [Xn, #offset] (load byte).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_ldrb(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset);

/**
 * @brief Emits LDRH Wd, [Xn, #offset] (load halfword).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_ldrh(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset);

/**
 * @brief Emits LDSB Wd, [Xn, #offset] (load signed byte).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_ldrsb(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset);

/**
 * @brief Emits LDSH Wd, [Xn, #offset] (load signed halfword).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_ldrsh(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset);

/**
 * @brief Emits LDSW Xd, [Xn, #offset] (load signed word).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_ldrsw(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset);

/**
 * @brief Emits STRB [Xn, #offset], Wd (store byte).
 * @param[in] ctx The emit context.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_strb(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src);

/**
 * @brief Emits STRH [Xn, #offset], Wd (store halfword).
 * @param[in] ctx The emit context.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_strh(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src);

/**
 * @brief Emits BRK #imm (breakpoint trap).
 * @param[in] ctx The emit context.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_brk(emit_context_t * ctx, uint16_t imm);

/**
 * @brief Emits SVC #imm (supervisor call).
 * @param[in] ctx The emit context.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_arm64_svc(emit_context_t * ctx, uint16_t imm);

#endif /* PULSE_EMIT_ARM64_H */
