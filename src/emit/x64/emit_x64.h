/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_x64.h
 * @brief x86-64 instruction encoding declarations.
 *
 * This header declares the architecture-specific instruction emitters for
 * x86-64 (also known as AMD64 or Intel 64). These functions generate
 * machine code that conforms to the System V AMD64 ABI used on Linux/macOS,
 * or the Microsoft x64 ABI used on Windows.
 */
#ifndef PULSE_EMIT_X64_H
#define PULSE_EMIT_X64_H

#include "../emit_internals.h"
#include "pulse/emit/emit.h"
#include "pulse/emit/emit_math.h"

/**
 * @brief Emits a REX prefix byte for x86-64 instructions.
 * @param[in] ctx The emit context.
 * @param[in] w Word-size operation (0=32-bit, 1=64-bit).
 * @param[in] r Extension of ModRM reg field.
 * @param[in] x Extension of SIB index field.
 * @param[in] b Extension of ModRM r/m field or opcode extension.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_rex(emit_context_t * ctx, bool w, bool r, bool x, bool b);

/**
 * @brief Emits MOV r64, imm64 (B8+rd /o).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_mov_imm(emit_context_t * ctx, emit_register_t dest, uint64_t imm);

/**
 * @brief Emits MOV r64, r64 (89 /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_mov_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits ADD r64, r64 (01 /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_add(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits ADD r64, imm32 (81 /0).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_add_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm);

/**
 * @brief Emits SUB r64, r64 (29 /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_sub(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits SUB r64, imm32 (81 /5).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_sub_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm);

/**
 * @brief Emits IMUL r64, r64 (0F AF /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_mul(emit_context_t * ctx, emit_register_t src);

/**
 * @brief Emits IMUL r64, r64, imm (69 /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_imul_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm);

/**
 * @brief Emits AND r64, r64 (21 /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_and(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits OR r64, r64 (09 /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_or(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits XOR r64, r64 (31 /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_xor(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits NOT r64 (F7 /2).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to invert.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_not(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Emits NEG r64 (F7 /3).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to negate.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_neg(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Emits SHL r64, cl (D3 /4).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Must be RCX (shift amount).
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_shl(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits SHR r64, cl (D3 /5).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] src Must be RCX (shift amount).
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_shr(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits SAL r64, imm8 (C1 /4).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to shift.
 * @param[in] amount Shift amount.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_sal(emit_context_t * ctx, emit_register_t reg, uint8_t amount);

/**
 * @brief Emits SAR r64, imm8 (C1 /7).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to shift.
 * @param[in] amount Shift amount.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_sar(emit_context_t * ctx, emit_register_t reg, uint8_t amount);

/**
 * @brief Emits CMP r64, r64 (39 /r).
 * @param[in] ctx The emit context.
 * @param[in] a First register.
 * @param[in] b Second register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_cmp(emit_context_t * ctx, emit_register_t a, emit_register_t b);

/**
 * @brief Emits CMP r64, imm32 (81 /7).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to compare.
 * @param[in] imm Immediate value.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_cmp_imm(emit_context_t * ctx, emit_register_t reg, int32_t imm);

/**
 * @brief Emits TEST r64, r64 (85 /r).
 * @param[in] ctx The emit context.
 * @param[in] a First register.
 * @param[in] b Second register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_test(emit_context_t * ctx, emit_register_t a, emit_register_t b);

/**
 * @brief Emits JMP rel32 (E9).
 * @param[in] ctx The emit context.
 * @param[in] label Target label name.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_jmp(emit_context_t * ctx, const char * label);

/**
 * @brief Emits Jcc rel32 (0F 80+cc).
 * @param[in] ctx The emit context.
 * @param[in] cc Condition code.
 * @param[in] label Target label name.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_jmp_cc(emit_context_t * ctx, emit_cc_t cc, const char * label);

/**
 * @brief Emits CALL rel32 (E8).
 * @param[in] ctx The emit context.
 * @param[in] name Function name.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_call(emit_context_t * ctx, const char * name);

/**
 * @brief Emits PUSH r64 (50+rd).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to push.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_push(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Emits POP r64 (58+rd).
 * @param[in] ctx The emit context.
 * @param[in] reg Register to pop.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_pop(emit_context_t * ctx, emit_register_t reg);

/**
 * @brief Emits function prologue (push rbp; mov rbp, rsp).
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_prologue(emit_context_t * ctx);

/**
 * @brief Emits function epilogue (leave; ret).
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_epilogue(emit_context_t * ctx);

/**
 * @brief Emits RET (C3).
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_ret(emit_context_t * ctx);

/**
 * @brief Emits MOV r64, [base+offset] (8B /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_load_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset);

/**
 * @brief Emits MOV [base+offset], r64 (89 /r).
 * @param[in] ctx The emit context.
 * @param[in] base Base register.
 * @param[in] offset Memory offset.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_store_reg(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src);

/**
 * @brief Emits MOV r64, [symbol] with relocation.
 * @param[in] ctx The emit context.
 * @param[in] dest Destination register.
 * @param[in] sym Symbol name.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_load_sym(emit_context_t * ctx, emit_register_t dest, const char * sym);

/**
 * @brief Emits MOV [symbol], r64 with relocation.
 * @param[in] ctx The emit context.
 * @param[in] sym Symbol name.
 * @param[in] src Source register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_store_sym(emit_context_t * ctx, const char * sym, emit_register_t src);

/**
 * @brief Emits MOVDQA xmm, xmm (66 0F 6F /r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination XMM register.
 * @param[in] src Source XMM register.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_movdqa(emit_context_t * ctx, emit_register_t dest, emit_register_t src);

/**
 * @brief Emits PADDB/PADDW/PADDD/PADDQ xmm, xmm (66 0F FC+r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination XMM register.
 * @param[in] src Source XMM register.
 * @param[in] size Element size (8, 16, 32, or 64 bits).
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_padd(emit_context_t * ctx, emit_register_t dest, emit_register_t src, int size);

/**
 * @brief Emits PCMPEQB/PCMPEQW/PCMPEQD/PCMPEQQ (66 0F 74+r, 66 0F 76+r, 66 0F 76+r, 66 0F 76+r).
 * @param[in] ctx The emit context.
 * @param[in] dest Destination XMM register.
 * @param[in] src Source XMM register.
 * @param[in] size Element size (8, 16, 32, or 64 bits).
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_pcmpeq(emit_context_t * ctx, emit_register_t dest, emit_register_t src, int size);

/**
 * @brief Emits RET for aarch64 (D65F03C0).
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_x64_nop(emit_context_t * ctx, uint8_t size);

#endif /* PULSE_EMIT_X64_H */
