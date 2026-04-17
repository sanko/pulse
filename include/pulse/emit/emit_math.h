/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_math.h
 * @brief Math operations for JIT code generation.
 */
#ifndef INFIX_EMIT_MATH_H
#define INFIX_EMIT_MATH_H

#include "emit.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Condition codes for conditional jumps and moves.
 *
 * Based on x86-64 flags register.
 */
typedef enum {
    EMIT_CC_O,  /**< Overflow */
    EMIT_CC_NO, /**< No overflow */
    EMIT_CC_B,  /**< Below (unsigned) */
    EMIT_CC_AE, /**< Above or equal (unsigned) */
    EMIT_CC_E,  /**< Equal */
    EMIT_CC_NE, /**< Not equal */
    EMIT_CC_BE, /**< Below or equal (unsigned) */
    EMIT_CC_A,  /**< Above (unsigned) */
    EMIT_CC_S,  /**< Sign (negative) */
    EMIT_CC_NS, /**< No sign (non-negative) */
    EMIT_CC_P,  /**< Parity */
    EMIT_CC_NP, /**< No parity */
    EMIT_CC_L,  /**< Less (signed) */
    EMIT_CC_GE, /**< Greater or equal (signed) */
    EMIT_CC_LE, /**< Less or equal (signed) */
    EMIT_CC_G,  /**< Greater (signed) */
} emit_cc_t;

/**
 * @brief x86-64 general-purpose registers.
 *
 * Register numbers correspond to ModRM encoding.
 */
typedef enum {
    EMIT_REG_RAX = 0,  /**< Accumulator */
    EMIT_REG_RCX = 1,  /**< Counter */
    EMIT_REG_RDX = 2,  /**< Data */
    EMIT_REG_RBX = 3,  /**< Base */
    EMIT_REG_RSP = 4,  /**< Stack pointer */
    EMIT_REG_RBP = 5,  /**< Base pointer */
    EMIT_REG_RSI = 6,  /**< Source index */
    EMIT_REG_RDI = 7,  /**< Destination index */
    EMIT_REG_R8 = 8,   /**< Extended register 8 */
    EMIT_REG_R9 = 9,   /**< Extended register 9 */
    EMIT_REG_R10 = 10, /**< Extended register 10 */
    EMIT_REG_R11 = 11, /**< Extended register 11 */
    EMIT_REG_R12 = 12, /**< Extended register 12 */
    EMIT_REG_R13 = 13, /**< Extended register 13 */
    EMIT_REG_R14 = 14, /**< Extended register 14 */
    EMIT_REG_R15 = 15, /**< Extended register 15 */
} emit_register_t;

/**
 * @brief Moves an immediate value into a register.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate value to move.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_mov_imm(emit_context_t * ctx, emit_register_t dest, uint64_t imm);

/**
 * @brief Moves data between registers.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_mov_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Moves from XMM register to general-purpose register.
 * @param[in] ctx The emit context.
 * @param[in] gpr_dest Destination GPR.
 * @param[in] xmm_src Source XMM register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_movq_gpr_xmm(emit_context_t * ctx, emit_register_t gpr_dest, emit_register_t xmm_src);

/**
 * @brief Moves scalar double-precision value between XMM registers.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination XMM register.
 * @param[in] src Source XMM register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_movsd_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Adds scalar double-precision floats (XMM).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination XMM register.
 * @param[in] src Source XMM register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_addsd(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Loads a 64-bit float from memory.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination XMM register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_load_f64(emit_context_t * ctx,
                                          emit_register_t dest,
                                          emit_register_t base,
                                          int32_t offset);

/**
 * @brief Adds src to dest (dest = dest + src).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_add(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Adds immediate value to register.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate value to add.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_add_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm);

/**
 * @brief Subtracts src from dest (dest = dest - src).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_sub(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Subtracts immediate value from register.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate value to subtract.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_sub_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm);

/**
 * @brief Multiplies RAX by src (RDX:RAX = RAX * src).
 * @param[in] ctx The emit context.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_mul(emit_context_t * ctx, emit_register_t src);

/**
 * @brief Immediate multiply (dest = dest * imm).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate multiplier.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_imul_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm);

/**
 * @brief Bitwise AND (dest = dest & src).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_and(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Bitwise OR (dest = dest | src).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_or(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Bitwise XOR (dest = dest ^ src).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_xor(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Bitwise NOT (reg = ~reg).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to invert.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_not(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Negate (reg = -reg).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to negate.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_neg(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Shift left logical (dest = dest << src).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Shift amount register (CL for x86).
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_shl(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Shift right logical (dest = dest >> src).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Shift amount register (CL for x86).
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_shr(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Shift arithmetic left (reg = reg << amount).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to shift.
 * @param[in] amount Shift amount.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_sal(emit_context_t * ctx, emit_register_t reg, uint8_t amount);

/**
 * @brief Shift arithmetic right (reg = reg >> amount, sign-extended).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to shift.
 * @param[in] amount Shift amount.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_sar(emit_context_t * ctx, emit_register_t reg, uint8_t amount);

/**
 * @brief Compare registers (sets flags).
 * @param[in] ctx The emit context.
 * @param[in] a First register.
 * @param[in] b Second register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_cmp(emit_context_t * ctx, emit_register_t a, emit_register_t b);

/**
 * @brief Compare register with immediate (sets flags).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to compare.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_cmp_imm(emit_context_t * ctx, emit_register_t reg, int32_t imm);

/**
 * @brief TEST instruction (sets flags based on AND result).
 * @param[in] ctx The emit context.
 * @param[in] a First register.
 * @param[in] b Second register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_test(emit_context_t * ctx, emit_register_t a, emit_register_t b);

/**
 * @brief Unconditional jump to label.
 * @param[in] ctx The emit context.
 * @param[in] label Target label name.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_jmp(emit_context_t * ctx, const char * label);

/**
 * @brief Conditional jump based on condition code.
 * @param[in] ctx The emit context.
 * @param[in] cc Condition code.
 * @param[in] label Target label name.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_jmp_cc(emit_context_t * ctx, emit_cc_t cc, const char * label);

/**
 * @brief Call external function.
 * @param[in] ctx The emit context.
 * @param[in] name Function name.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_call(emit_context_t * ctx, const char * name);

/**
 * @brief Emits function prologue (push rbp; mov rbp, rsp).
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_prologue(emit_context_t * ctx);

/**
 * @brief Emits function epilogue (leave; ret).
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_epilogue(emit_context_t * ctx);

/**
 * @brief Returns from function.
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_ret(emit_context_t * ctx);

/**
 * @brief Pushes register onto stack.
 * @param[in] ctx The emit context.
 * @param[in] reg Register to push.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_push(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Pops from stack into register.
 * @param[in] ctx The emit context.
 * @param[in] reg Register to pop into.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_pop(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Loads from memory [base + offset] into dest.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_load_reg(emit_context_t * ctx,
                                          emit_register_t dest,
                                          emit_register_t base,
                                          int32_t offset);

/**
 * @brief Stores src into memory [base + offset].
 * @param[in] ctx The emit context.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_store_reg(emit_context_t * ctx,
                                           emit_register_t base,
                                           int32_t offset,
                                           emit_register_t src);

/**
 * @brief Loads address of a symbol into register.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] sym Symbol name.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_load_sym(emit_context_t * ctx, emit_register_t dest, const char * sym);

/**
 * @brief Stores register value into symbol address.
 * @param[in] ctx The emit context.
 * @param[in] sym Symbol name.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
PULSE_API pulse_status emit_math_store_sym(emit_context_t * ctx, const char * sym, emit_register_t src);

#ifdef __cplusplus
}
#endif

#endif /* INFIX_EMIT_MATH_H */
