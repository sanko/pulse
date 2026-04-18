/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_math.c
 * @brief Implementation of the emit API for generating machine code.
 */
#define PULSE_BUILDING
#include "pulse/emit/emit_math.h"
#include "aarch64/emit_arm64.h"
#include "common/compat_c23.h"
#include "elf/emit_elf.h"
#include "emit_internals.h"
#include "pe/emit_pe.h"
#include "pulse/emit/emit.h"
#include "x64/emit_x64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pulse_status emit_write_pe_exec_internal(emit_context_t * ctx,
                                         uint8_t ** out_data,
                                         size_t * out_size,
                                         uint64_t return_value);

#define EMIT_DEFAULT_SECTION_CAPACITY 4096
#define EMIT_SECTION_GROWTH_FACTOR 2

void _emit_context_init(emit_context_t * ctx, emit_architecture_t arch, emit_format_t format) {
    ctx->arch = arch;
    ctx->format = format;
    ctx->state = EMIT_STATE_IDLE;
    ctx->sections = NULL;
    ctx->current_section = NULL;
    ctx->symbols = NULL;
    ctx->relocations = NULL;
    ctx->binary_spec = NULL;
    ctx->current_block_name = NULL;
    ctx->section_count = 0;
}

void _emit_context_free(emit_context_t * ctx) {
    if (!ctx)
        return;

    emit_section_t * sec = ctx->sections;
    while (sec) {
        emit_section_t * next = sec->next;
        free(sec->name);
        free(sec->data);
        free(sec);
        sec = next;
    }

    emit_symbol_t * sym = ctx->symbols;
    while (sym) {
        emit_symbol_t * next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }

    emit_relocation_t * rel = ctx->relocations;
    while (rel) {
        emit_relocation_t * next = rel->next;
        free(rel->symbol_name);
        free(rel->section_name);
        free(rel);
        rel = next;
    }

    free(ctx->current_block_name);
}

static emit_section_t * _create_section(const char * name, emit_section_flags_t flags) {
    emit_section_t * section = calloc(1, sizeof(emit_section_t));
    if (!section)
        return NULL;

    section->name = strdup(name);
    section->flags = flags;
    section->data = malloc(EMIT_DEFAULT_SECTION_CAPACITY);
    if (!section->data) {
        free(section->name);
        free(section);
        return NULL;
    }
    section->capacity = EMIT_DEFAULT_SECTION_CAPACITY;
    section->size = 0;
    section->next = NULL;

    return section;
}

emit_section_t * _emit_lookup_section(emit_context_t * ctx, const char * name) {
    if (!ctx || !name)
        return NULL;

    for (emit_section_t * sec = ctx->sections; sec != NULL; sec = sec->next)
        if (strcmp(sec->name, name) == 0)
            return sec;
    return NULL;
}

emit_symbol_t * _emit_lookup_symbol(emit_context_t * ctx, const char * name) {
    if (!ctx || !name)
        return NULL;

    for (emit_symbol_t * sym = ctx->symbols; sym != NULL; sym = sym->next)
        if (strcmp(sym->name, name) == 0)
            return sym;
    return NULL;
}

PULSE_API pulse_status emit_create(emit_context_t ** out_ctx, emit_architecture_t arch, emit_format_t format) {
    if (!out_ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_context_t * ctx = calloc(1, sizeof(emit_context_t));
    if (!ctx)
        return PULSE_ERROR_ALLOCATION_FAILED;

    _emit_context_init(ctx, arch, format);

    *out_ctx = ctx;
    return PULSE_SUCCESS;
}

PULSE_API void emit_destroy(emit_context_t * ctx) {
    _emit_context_free(ctx);
    free(ctx);
}

PULSE_API pulse_status emit_add_section(emit_context_t * ctx, const char * name, emit_section_flags_t flags) {
    if (!ctx || !name)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_section_t * existing = _emit_lookup_section(ctx, name);
    if (existing)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_section_t * section = _create_section(name, flags);
    if (!section)
        return PULSE_ERROR_ALLOCATION_FAILED;

    section->next = ctx->sections;
    ctx->sections = section;
    ctx->section_count++;

    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_begin_section(emit_context_t * ctx, const char * section_name) {
    if (!ctx || !section_name)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_section_t * section = _emit_lookup_section(ctx, section_name);
    if (!section)
        return PULSE_ERROR_INVALID_ARGUMENT;

    ctx->current_section = section;
    ctx->state = EMIT_STATE_SECTION_ACTIVE;
    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_define_symbol(emit_context_t * ctx,
                                          const char * name,
                                          emit_visibility_t visibility,
                                          bool is_function) {
    if (!ctx || !name)
        return PULSE_ERROR_INVALID_ARGUMENT;

    (void)visibility;

    emit_symbol_t * sym = _emit_lookup_symbol(ctx, name);
    if (!sym) {
        sym = calloc(1, sizeof(emit_symbol_t));
        if (!sym)
            return PULSE_ERROR_ALLOCATION_FAILED;

        sym->name = strdup(name);
        sym->next = ctx->symbols;
        ctx->symbols = sym;
    }

    sym->is_defined = true;
    sym->is_function = is_function;
    sym->section = ctx->current_section;
    sym->value = ctx->current_section ? ctx->current_section->size : 0;

    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_emit_label(emit_context_t * ctx, const char * name) {
    if (!ctx || !name)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_symbol_t * sym = _emit_lookup_symbol(ctx, name);
    if (!sym) {
        sym = calloc(1, sizeof(emit_symbol_t));
        if (!sym)
            return PULSE_ERROR_ALLOCATION_FAILED;

        sym->name = strdup(name);
        sym->is_defined = true;
        sym->is_function = false;
        sym->section = ctx->current_section;
        sym->value = ctx->current_section ? ctx->current_section->size : 0;

        sym->next = ctx->symbols;
        ctx->symbols = sym;
    }
    else {
        sym->is_defined = true;
        sym->value = ctx->current_section ? ctx->current_section->size : 0;
        sym->section = ctx->current_section;
    }

    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_create_label(emit_context_t * ctx, const char * name) {
    return emit_define_symbol(ctx, name, EMIT_VISIBILITY_DEFAULT, false);
}

static pulse_status _ensure_section_capacity(emit_context_t * ctx, uint64_t needed) {
    if (!ctx->current_section)
        return PULSE_ERROR_INVALID_ARGUMENT;

    if (needed <= ctx->current_section->capacity)
        return PULSE_SUCCESS;

    uint64_t new_capacity = ctx->current_section->capacity * EMIT_SECTION_GROWTH_FACTOR;
    while (new_capacity < needed)
        new_capacity *= EMIT_SECTION_GROWTH_FACTOR;

    uint8_t * new_data = realloc(ctx->current_section->data, new_capacity);
    if (!new_data)
        return PULSE_ERROR_ALLOCATION_FAILED;

    ctx->current_section->data = new_data;
    ctx->current_section->capacity = new_capacity;
    return PULSE_SUCCESS;
}

static pulse_status emit_emit_bytes(emit_context_t * ctx, const void * data, size_t size) {
    if (!ctx || !data)
        return PULSE_ERROR_INVALID_ARGUMENT;

    if (ctx->state != EMIT_STATE_SECTION_ACTIVE || !ctx->current_section)
        return PULSE_ERROR_INVALID_ARGUMENT;

    pulse_status status = _ensure_section_capacity(ctx, ctx->current_section->size + size);
    if (status != PULSE_SUCCESS)
        return status;

    memcpy(ctx->current_section->data + ctx->current_section->size, data, size);
    ctx->current_section->size += size;

    return PULSE_SUCCESS;
}

PULSE_API pulse_status __attribute__((warn_unused_result)) emit_emit_u8(emit_context_t * ctx, uint8_t byte) {
    return emit_emit_bytes(ctx, &byte, 1);
}

PULSE_API pulse_status emit_emit_u16(emit_context_t * ctx, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
    return emit_emit_bytes(ctx, bytes, 2);
}

PULSE_API pulse_status __attribute__((warn_unused_result)) emit_emit_u32(emit_context_t * ctx, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)(value & 0xFF),
                        (uint8_t)((value >> 8) & 0xFF),
                        (uint8_t)((value >> 16) & 0xFF),
                        (uint8_t)((value >> 24) & 0xFF)};
    return emit_emit_bytes(ctx, bytes, 4);
}

PULSE_API pulse_status emit_emit_u64(emit_context_t * ctx, uint64_t value) {
    uint8_t bytes[8] = {(uint8_t)(value & 0xFF),
                        (uint8_t)((value >> 8) & 0xFF),
                        (uint8_t)((value >> 16) & 0xFF),
                        (uint8_t)((value >> 24) & 0xFF),
                        (uint8_t)((value >> 32) & 0xFF),
                        (uint8_t)((value >> 40) & 0xFF),
                        (uint8_t)((value >> 48) & 0xFF),
                        (uint8_t)((value >> 56) & 0xFF)};
    return emit_emit_bytes(ctx, bytes, 8);
}

PULSE_API pulse_status emit_alignxxxxx(emit_context_t * ctx, uint64_t alignment) {
    if (!ctx || !ctx->current_section)
        return PULSE_ERROR_INVALID_ARGUMENT;

    if (alignment == 0)
        return PULSE_SUCCESS;

    uint64_t current = ctx->current_section->size;
    uint64_t aligned = (current + alignment - 1) & ~(alignment - 1);
    uint64_t padding = aligned - current;

    for (uint64_t i = 0; i < padding; i++) {
        pulse_status status = emit_emit_u8(ctx, 0x90);
        if (status != PULSE_SUCCESS)
            return status;
    }

    return PULSE_SUCCESS;
}
PULSE_API pulse_status emit_align(emit_context_t * ctx, uint64_t alignment) {
    if (!ctx || !ctx->current_section) return PULSE_ERROR_INVALID_ARGUMENT;
    if (alignment == 0) return PULSE_SUCCESS;

    uint64_t current = ctx->current_section->size;
    uint64_t aligned = (current + alignment - 1) & ~(alignment - 1);
    uint64_t padding = aligned - current;

    if (ctx->arch == EMIT_ARCH_AARCH64) {
        if (padding % 4 != 0) {
            // Logic error: instructions must be 4-byte aligned
            return PULSE_ERROR_GENERIC; 
        }
        for (uint64_t i = 0; i < padding; i += 4) {
            emit_emit_u32(ctx, 0xD503201F); // ARM64 NOP
        }
    } else {
        for (uint64_t i = 0; i < padding; i++) {
            emit_emit_u8(ctx, 0x90); // x64 NOP
        }
    }
    return PULSE_SUCCESS;
}
pulse_status _emit_add_relocation(
    emit_context_t * ctx, const char * name, uint64_t offset, uint8_t size, uint8_t inst_size, bool is_pc_relative) {
    if (!ctx || !name)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_relocation_t * rel = calloc(1, sizeof(emit_relocation_t));
    if (!rel)
        return PULSE_ERROR_ALLOCATION_FAILED;

    rel->symbol_name = strdup(name);
    rel->section_name = ctx->current_section ? strdup(ctx->current_section->name) : NULL;
    rel->offset = offset;
    rel->size = size;
    rel->inst_size = inst_size;
    rel->is_pc_relative = is_pc_relative;

    rel->next = ctx->relocations;
    ctx->relocations = rel;

    return PULSE_SUCCESS;
}

static void write_raw_binary(emit_context_t * ctx, uint8_t * buffer, c23_maybe_unused uint64_t total_size) {
    emit_section_t ** secs = malloc(ctx->section_count * sizeof(emit_section_t *));
    if (!secs)
        return;

    emit_section_t * sec = ctx->sections;
    int count = 0;
    while (sec) {
        secs[count++] = sec;
        sec = sec->next;
    }

    for (int i = count - 1; i >= 0; i--) {
        uint64_t offset = 0;
        for (int j = i + 1; j < count; j++)
            offset += secs[j]->size;

        memcpy(buffer + offset, secs[i]->data, secs[i]->size);
    }

    free(secs);
}

pulse_status _emit_resolve_relocations(emit_context_t * ctx) {
    if (!ctx)
        return PULSE_SUCCESS;

    emit_section_t ** secs = malloc(ctx->section_count * sizeof(emit_section_t *));
    if (!secs)
        return PULSE_ERROR_ALLOCATION_FAILED;

    emit_section_t * sec = ctx->sections;
    int count = 0;
    while (sec) {
        secs[count++] = sec;
        sec = sec->next;
    }

    uint64_t section_offsets[32] = {0};
    for (int i = 0; i < count; i++) {
        section_offsets[i] = 0;
        for (int j = i + 1; j < count; j++)
            section_offsets[i] += secs[j]->size;
    }

    for (emit_relocation_t * rel = ctx->relocations; rel != NULL; rel = rel->next) {
        emit_symbol_t * sym = _emit_lookup_symbol(ctx, rel->symbol_name);
        if (!sym || !sym->is_defined)
            continue;

        emit_section_t * target_sec = sym->section;
        if (!target_sec)
            continue;

        emit_section_t * reloc_sec = NULL;
        if (rel->section_name)
            reloc_sec = _emit_lookup_section(ctx, rel->section_name);
        if (!reloc_sec)
            reloc_sec = ctx->current_section;
        if (!reloc_sec || reloc_sec->size == 0 || !reloc_sec->data)
            continue;

        uint64_t target_sec_offset = 0;
        uint64_t reloc_sec_offset = 0;
        for (int i = 0; i < count; i++) {
            if (secs[i] == target_sec)
                target_sec_offset = section_offsets[i];
            if (secs[i] == reloc_sec)
                reloc_sec_offset = section_offsets[i];
        }

        uint64_t target_addr = target_sec_offset + sym->value;
        uint64_t reloc_addr = reloc_sec_offset + rel->offset;

        int64_t displacement = (int64_t)target_addr - (int64_t)reloc_addr;

        if (rel->size == 4) {
            if (rel->is_pc_relative) {
                if (ctx->arch == EMIT_ARCH_AARCH64) {
                    /* Patch AArch64 branch instructions */
                    uint32_t * instr_ptr = (uint32_t *)(reloc_sec->data + rel->offset);
                    uint32_t instr = *instr_ptr;
                    if ((instr & 0xFF000000) == 0x54000000) {
                        /* B.cond: 19-bit offset at bits 23:5 */
                        int32_t imm19 = (int32_t)(displacement / 4) & 0x7FFFF;
                        *instr_ptr = (instr & 0xFF00001F) | (imm19 << 5);
                    }
                    else if ((instr & 0x7C000000) == 0x14000000) {
                        /* B or BL: 26-bit offset at bits 25:0 */
                        int32_t imm26 = (int32_t)(displacement / 4) & 0x3FFFFFF;
                        *instr_ptr = (instr & 0xFC000000) | imm26;
                    }
                    else {
                        /* Default fallback for 32-bit relative relocations */
                        *(int32_t *)(reloc_sec->data + rel->offset) = (int32_t)displacement;
                    }
                }
                else {
                    /* x64 PC-relative is usually relative to the NEXT instruction */
                    *(int32_t *)(reloc_sec->data + rel->offset) = (int32_t)(displacement - rel->size);
                }
            }
            else {
                *(uint32_t *)(reloc_sec->data + rel->offset) = (uint32_t)target_addr;
            }
        }
        else if (rel->size == 8) {
            if (rel->is_pc_relative)
                *(int64_t *)(reloc_sec->data + rel->offset) = displacement;
            else {
                if (ctx->arch == EMIT_ARCH_AARCH64 && rel->inst_size == 16) {
                    /* Patch 4 MOVZ/MOVK instructions with 64-bit absolute address */
                    uint32_t * instrs = (uint32_t *)(reloc_sec->data + rel->offset);
                    instrs[0] = (instrs[0] & 0xFFE0001F) | (uint32_t)((target_addr & 0xFFFF) << 5);
                    instrs[1] = (instrs[1] & 0xFFE0001F) | (uint32_t)(((target_addr >> 16) & 0xFFFF) << 5);
                    instrs[2] = (instrs[2] & 0xFFE0001F) | (uint32_t)(((target_addr >> 32) & 0xFFFF) << 5);
                    instrs[3] = (instrs[3] & 0xFFE0001F) | (uint32_t)(((target_addr >> 48) & 0xFFFF) << 5);
                }
                else {
                    *(uint64_t *)(reloc_sec->data + rel->offset) = target_addr;
                }
            }
        }
    }

    free(secs);
    return PULSE_SUCCESS;
}

void _emit_arch_nop(emit_context_t * ctx, uint8_t size) {
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        for (uint8_t i = 0; i < size; i++) {
            pulse_status status = emit_emit_u8(ctx, 0x90);
            (void)status;
        }
        break;
    case EMIT_ARCH_AARCH64:
        {
            pulse_status status = emit_emit_u32(ctx, 0xD503201F);
            (void)status;
            break;
        }
    default:
        break;
    }
}

pulse_status _emit_arch_align(emit_context_t * ctx, uint64_t alignment) {
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        for (uint64_t i = 0; i < alignment; i++) {
            pulse_status status = emit_emit_u8(ctx, 0x90);
            if (status != PULSE_SUCCESS)
                return status;
        }
        break;
    case EMIT_ARCH_AARCH64:
        for (uint64_t i = 0; i < alignment; i++) {
            pulse_status status = emit_emit_u32(ctx, 0xD503201F);
            if (status != PULSE_SUCCESS)
                return status;
        }
        break;
    default:
        break;
    }
    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_get_binary(const emit_context_t * ctx, const uint8_t ** out_data, size_t * out_size) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_context_t * mutable_ctx = (emit_context_t *)ctx;
    pulse_status status = _emit_resolve_relocations(mutable_ctx);
    if (status != PULSE_SUCCESS)
        return status;

    if (ctx->format == EMIT_FORMAT_ELF)
        return emit_write_elf(mutable_ctx, (uint8_t **)out_data, out_size);

    if (ctx->format == EMIT_FORMAT_ELF_EXEC)
        return emit_write_elf_exec(mutable_ctx, (uint8_t **)out_data, out_size);

    if (ctx->format == EMIT_FORMAT_PE)
        return emit_write_pe(mutable_ctx, (uint8_t **)out_data, out_size);

    uint64_t total_size = 0;
    for (emit_section_t * sec = ctx->sections; sec != NULL; sec = sec->next)
        total_size += sec->size;

    uint8_t * buffer = malloc(total_size);
    if (!buffer)
        return PULSE_ERROR_ALLOCATION_FAILED;

    write_raw_binary(mutable_ctx, buffer, total_size);

    *out_data = buffer;
    *out_size = total_size;

    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_get_offset(const emit_context_t * ctx, uint64_t * out_offset) {
    if (!ctx || !out_offset)
        return PULSE_ERROR_INVALID_ARGUMENT;

    *out_offset = ctx->current_section ? ctx->current_section->size : 0;
    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_write_file(const emit_context_t * ctx, const char * filename) {
    if (!ctx || !filename)
        return PULSE_ERROR_INVALID_ARGUMENT;

    const uint8_t * data = NULL;
    size_t size = 0;

    pulse_status status = emit_get_binary(ctx, &data, &size);
    if (status != PULSE_SUCCESS)
        return status;

    FILE * f = fopen(filename, "wb");
    if (!f)
        return PULSE_ERROR_GENERIC;

    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    free((void *)data);

    if (written != size)
        return PULSE_ERROR_GENERIC;

    return PULSE_SUCCESS;
}

PULSE_API pulse_status emit_write_pe_exec(const emit_context_t * ctx, const char * filename, uint64_t return_value) {
    if (!ctx || !filename)
        return PULSE_ERROR_INVALID_ARGUMENT;

    emit_context_t * mutable_ctx = (emit_context_t *)ctx;
    uint8_t * data = NULL;
    size_t size = 0;

    pulse_status status = emit_write_pe_exec_internal(mutable_ctx, &data, &size, return_value);
    if (status != PULSE_SUCCESS)
        return status;

    if (data && size > 0) {
        FILE * f = fopen(filename, "wb");
        if (f) {
            fwrite(data, 1, size, f);
            fclose(f);
        }
        free(data);
    }

    return PULSE_SUCCESS;
}

/* ============================================================================
 * Architecture-specific instruction emitters
 * ============================================================================*/

PULSE_API pulse_status emit_math_mov_imm(emit_context_t * ctx, emit_register_t dest, uint64_t imm) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_mov_imm(ctx, dest, imm);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        EMIT_CHECK(emit_arm64_movz(ctx, rd, (uint16_t)(imm & 0xFFFF), 0, true));
        if (imm > 0xFFFF) EMIT_CHECK(emit_arm64_movk(ctx, rd, (uint16_t)((imm >> 16) & 0xFFFF), 16, true));
        if (imm > 0xFFFFFFFF) EMIT_CHECK(emit_arm64_movk(ctx, rd, (uint16_t)((imm >> 32) & 0xFFFF), 32, true));
        if (imm > 0xFFFFFFFFFFFF) EMIT_CHECK(emit_arm64_movk(ctx, rd, (uint16_t)((imm >> 48) & 0xFFFF), 48, true));
        return PULSE_SUCCESS;
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_mov_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_mov_reg(ctx, dest, src);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_mov_reg(ctx, _emit_arm64_reg(dest), _emit_arm64_reg(src));
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_add(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_add(ctx, dest, src);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        return emit_arm64_add(ctx, rd, rd, _emit_arm64_reg(src));
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_add_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_add_imm(ctx, dest, imm);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        return emit_arm64_add_imm(ctx, rd, rd, (int16_t)imm);
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_sub(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_sub(ctx, dest, src);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        return emit_arm64_sub(ctx, rd, rd, _emit_arm64_reg(src));
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_and(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_and(ctx, dest, src);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        return emit_arm64_and(ctx, rd, rd, _emit_arm64_reg(src));
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_or(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_or(ctx, dest, src);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        return emit_arm64_orr(ctx, rd, rd, _emit_arm64_reg(src));
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_xor(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_xor(ctx, dest, src);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        return emit_arm64_eor(ctx, rd, rd, _emit_arm64_reg(src));
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_cmp(emit_context_t * ctx, emit_register_t a, emit_register_t b) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_cmp(ctx, a, b);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_cmp(ctx, _emit_arm64_reg(a), _emit_arm64_reg(b));
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_cmp_imm(emit_context_t * ctx, emit_register_t reg, int32_t imm) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_cmp_imm(ctx, reg, imm);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_cmp_imm(ctx, _emit_arm64_reg(reg), (int16_t)imm);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_jmp(emit_context_t * ctx, const char * label) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_jmp(ctx, label);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_b(ctx, label);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_jmp_cc(emit_context_t * ctx, emit_cc_t cc, const char * label) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_jmp_cc(ctx, cc, label);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_b_cond(ctx, cc, label);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_call(emit_context_t * ctx, const char * name) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_call(ctx, name);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_bl(ctx, name);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_prologue(emit_context_t * ctx) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_prologue(ctx);
    case EMIT_ARCH_AARCH64:
        /* stp x29, x30, [sp, #-16]! ; mov x29, sp */
        EMIT_CHECK(emit_emit_u32(ctx, 0xA9BF7BFD));
        EMIT_CHECK(emit_emit_u32(ctx, 0x910003FD));
        return PULSE_SUCCESS;
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_epilogue(emit_context_t * ctx) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_epilogue(ctx);
    case EMIT_ARCH_AARCH64:
        /* ldp x29, x30, [sp], #16 ; ret */
        EMIT_CHECK(emit_emit_u32(ctx, 0xA8C17BFD));
        EMIT_CHECK(emit_arm64_ret(ctx, 30));
        return PULSE_SUCCESS;
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_ret(emit_context_t * ctx) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_ret(ctx);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_ret(ctx, 30);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_load_reg(emit_context_t * ctx,
                                          emit_register_t dest,
                                          emit_register_t base,
                                          int32_t offset) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_load_reg(ctx, dest, base, offset);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_ldr(ctx, _emit_arm64_reg(dest), _emit_arm64_reg(base), offset);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_store_reg(emit_context_t * ctx,
                                           emit_register_t base,
                                           int32_t offset,
                                           emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_store_reg(ctx, base, offset, src);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_str(ctx, _emit_arm64_reg(base), offset, _emit_arm64_reg(src));
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_mul(emit_context_t * ctx, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_mul(ctx, src);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rs = _emit_arm64_reg(src);
        return emit_arm64_mul(ctx, 0, 0, rs); // X0 = X0 * Xs
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_imul_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_imul_imm(ctx, dest, imm);
    case EMIT_ARCH_AARCH64:
    {
        /* Use X16 as scratch to load immediate then multiply */
        uint8_t rd = _emit_arm64_reg(dest);
        EMIT_CHECK(emit_math_mov_imm(ctx, (emit_register_t)116, (uint64_t)imm)); // EMIT_REG_X16 is 116
        return emit_arm64_mul(ctx, rd, rd, 16);
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_test(emit_context_t * ctx, emit_register_t a, emit_register_t b) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_test(ctx, a, b);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_tst(ctx, _emit_arm64_reg(a), _emit_arm64_reg(b));
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_store_sym(emit_context_t * ctx, const char * sym, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_store_sym(ctx, sym, src);
    case EMIT_ARCH_AARCH64:
    {
        /* Use X16 as scratch for symbol address */
        EMIT_CHECK(emit_math_load_sym(ctx, (emit_register_t)116, sym));
        return emit_arm64_str(ctx, 16, 0, _emit_arm64_reg(src));
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_load_sym(emit_context_t * ctx, emit_register_t dest, const char * sym) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_load_sym(ctx, dest, sym);
    case EMIT_ARCH_AARCH64:
    {
        /* Load address via a sequence that will be patched */
        uint8_t rd = _emit_arm64_reg(dest);
        uint64_t offset = ctx->current_section ? ctx->current_section->size : 0;
        EMIT_CHECK(emit_arm64_movz(ctx, rd, 0, 0, true));
        EMIT_CHECK(emit_arm64_movk(ctx, rd, 0, 16, true));
        EMIT_CHECK(emit_arm64_movk(ctx, rd, 0, 32, true));
        EMIT_CHECK(emit_arm64_movk(ctx, rd, 0, 48, true));
        EMIT_CHECK(_emit_add_relocation(ctx, sym, offset, 8, 16, false));
        return PULSE_SUCCESS;
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_call_reg(emit_context_t * ctx, emit_register_t reg) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_call_reg(ctx, reg);
    case EMIT_ARCH_AARCH64:
        /* BLR Xn -> 0xD63F0000 | (n << 5) */
        return emit_emit_u32(ctx, 0xD63F0000 | (_emit_arm64_reg(reg) << 5));
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_push(emit_context_t * ctx, emit_register_t reg) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_push(ctx, reg);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_push(ctx, reg);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_pop(emit_context_t * ctx, emit_register_t reg) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_pop(ctx, reg);
    case EMIT_ARCH_AARCH64:
        return emit_arm64_pop(ctx, reg);
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_sub_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        return emit_x64_sub_imm(ctx, dest, imm);
    case EMIT_ARCH_AARCH64:
    {
        uint8_t rd = _emit_arm64_reg(dest);
        return emit_arm64_sub_imm(ctx, rd, rd, (int16_t)imm);
    }
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_movq_gpr_xmm(emit_context_t * ctx, emit_register_t gpr_dest, emit_register_t xmm_src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        /* 66 REX.W 0F 7E /r -> MOVQ r64, xmm */
        EMIT_CHECK(emit_emit_u8(ctx, 0x66));
        EMIT_CHECK(emit_x64_rex(ctx, true, _emit_x64_reg_needs_rex(xmm_src), false, _emit_x64_reg_needs_rex(gpr_dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x7E));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | (_emit_x64_reg(xmm_src) << 3) | _emit_x64_reg(gpr_dest)));
        return PULSE_SUCCESS;
    case EMIT_ARCH_AARCH64:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_movsd_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        /* F2 0F 10 /r -> MOVSD xmm1, xmm2 */
        EMIT_CHECK(emit_emit_u8(ctx, 0xF2));
        EMIT_CHECK(emit_x64_rex(ctx, false, _emit_x64_reg_needs_rex(dest), false, _emit_x64_reg_needs_rex(src)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x10));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | (_emit_x64_reg(dest) << 3) | _emit_x64_reg(src)));
        return PULSE_SUCCESS;
    case EMIT_ARCH_AARCH64:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_addsd(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        /* F2 0F 58 /r -> ADDSD xmm1, xmm2 */
        EMIT_CHECK(emit_emit_u8(ctx, 0xF2));
        EMIT_CHECK(emit_x64_rex(ctx, false, _emit_x64_reg_needs_rex(dest), false, _emit_x64_reg_needs_rex(src)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x58));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | (_emit_x64_reg(dest) << 3) | _emit_x64_reg(src)));
        return PULSE_SUCCESS;
    case EMIT_ARCH_AARCH64:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}

PULSE_API pulse_status emit_math_subsd(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (!ctx)
        return PULSE_ERROR_INVALID_ARGUMENT;
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        /* F2 0F 5C /r -> SUBSD xmm1, xmm2 */
        EMIT_CHECK(emit_emit_u8(ctx, 0xF2));
        EMIT_CHECK(emit_x64_rex(ctx, false, _emit_x64_reg_needs_rex(dest), false, _emit_x64_reg_needs_rex(src)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x5C));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | (_emit_x64_reg(dest) << 3) | _emit_x64_reg(src)));
        return PULSE_SUCCESS;
    case EMIT_ARCH_AARCH64:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    default:
        return PULSE_ERROR_NOT_IMPLEMENTED;
    }
}
