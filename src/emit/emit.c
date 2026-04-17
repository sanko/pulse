/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit.c
 * @brief Implementation of the emit API for generating machine code.
 */
#define INFIX_BUILDING
#include "common/compat_c23.h"
#include "pulse/emit/emit.h"
#include "emit_internals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

PULSE_API infix_status emit_create(emit_context_t ** out_ctx, emit_architecture_t arch, emit_format_t format) {
    _infix_clear_error();
    if (!out_ctx) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    emit_context_t * ctx = calloc(1, sizeof(emit_context_t));
    if (!ctx)
        return INFIX_ERROR_ALLOCATION_FAILED;

    _emit_context_init(ctx, arch, format);

    *out_ctx = ctx;
    return INFIX_SUCCESS;
}

PULSE_API void emit_destroy(emit_context_t * ctx) {
    _emit_context_free(ctx);
    free(ctx);
}

PULSE_API infix_status emit_add_section(emit_context_t * ctx, const char * name, emit_section_flags_t flags) {
    _infix_clear_error();
    if (!ctx || !name) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    emit_section_t * existing = _emit_lookup_section(ctx, name);
    if (existing) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    emit_section_t * section = _create_section(name, flags);
    if (!section)
        return INFIX_ERROR_ALLOCATION_FAILED;

    section->next = ctx->sections;
    ctx->sections = section;
    ctx->section_count++;

    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_begin_section(emit_context_t * ctx, const char * section_name) {
    _infix_clear_error();
    if (!ctx || !section_name) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    emit_section_t * section = _emit_lookup_section(ctx, section_name);
    if (!section) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    ctx->current_section = section;
    ctx->state = EMIT_STATE_SECTION_ACTIVE;
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_define_symbol(emit_context_t * ctx,
                                          const char * name,
                                          emit_visibility_t visibility,
                                          bool is_function) {
    _infix_clear_error();
    if (!ctx || !name) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    (void)visibility;

    emit_symbol_t * sym = _emit_lookup_symbol(ctx, name);
    if (!sym) {
        sym = calloc(1, sizeof(emit_symbol_t));
        if (!sym)
            return INFIX_ERROR_ALLOCATION_FAILED;

        sym->name = strdup(name);
        sym->next = ctx->symbols;
        ctx->symbols = sym;
    }

    sym->is_defined = true;
    sym->is_function = is_function;
    sym->section = ctx->current_section;
    sym->value = ctx->current_section ? ctx->current_section->size : 0;

    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_emit_label(emit_context_t * ctx, const char * name) {
    _infix_clear_error();
    if (!ctx || !name) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    emit_symbol_t * sym = _emit_lookup_symbol(ctx, name);
    if (!sym) {
        sym = calloc(1, sizeof(emit_symbol_t));
        if (!sym)
            return INFIX_ERROR_ALLOCATION_FAILED;

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

    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_create_label(emit_context_t * ctx, const char * name) {
    return emit_define_symbol(ctx, name, EMIT_VISIBILITY_DEFAULT, false);
}

static infix_status _ensure_section_capacity(emit_context_t * ctx, uint64_t needed) {
    if (!ctx->current_section)
        return INFIX_ERROR_INVALID_ARGUMENT;

    if (needed <= ctx->current_section->capacity)
        return INFIX_SUCCESS;

    uint64_t new_capacity = ctx->current_section->capacity * EMIT_SECTION_GROWTH_FACTOR;
    while (new_capacity < needed)
        new_capacity *= EMIT_SECTION_GROWTH_FACTOR;

    uint8_t * new_data = realloc(ctx->current_section->data, new_capacity);
    if (!new_data)
        return INFIX_ERROR_ALLOCATION_FAILED;

    ctx->current_section->data = new_data;
    ctx->current_section->capacity = new_capacity;
    return INFIX_SUCCESS;
}

static infix_status emit_emit_bytes(emit_context_t * ctx, const void * data, size_t size) {
    _infix_clear_error();
    if (!ctx || !data) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    if (ctx->state != EMIT_STATE_SECTION_ACTIVE || !ctx->current_section) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    infix_status status = _ensure_section_capacity(ctx, ctx->current_section->size + size);
    if (status != INFIX_SUCCESS)
        return status;

    memcpy(ctx->current_section->data + ctx->current_section->size, data, size);
    ctx->current_section->size += size;

    return INFIX_SUCCESS;
}

PULSE_API infix_status __attribute__((warn_unused_result)) emit_emit_u8(emit_context_t * ctx, uint8_t byte) {
    return emit_emit_bytes(ctx, &byte, 1);
}

PULSE_API infix_status emit_emit_u16(emit_context_t * ctx, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
    return emit_emit_bytes(ctx, bytes, 2);
}

PULSE_API infix_status __attribute__((warn_unused_result)) emit_emit_u32(emit_context_t * ctx, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)(value & 0xFF),
                        (uint8_t)((value >> 8) & 0xFF),
                        (uint8_t)((value >> 16) & 0xFF),
                        (uint8_t)((value >> 24) & 0xFF)};
    return emit_emit_bytes(ctx, bytes, 4);
}

PULSE_API infix_status emit_emit_u64(emit_context_t * ctx, uint64_t value) {
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

PULSE_API infix_status emit_align(emit_context_t * ctx, uint64_t alignment) {
    _infix_clear_error();
    if (!ctx || !ctx->current_section)
        return INFIX_ERROR_INVALID_ARGUMENT;

    if (alignment == 0)
        return INFIX_SUCCESS;

    uint64_t current = ctx->current_section->size;
    uint64_t aligned = (current + alignment - 1) & ~(alignment - 1);
    uint64_t padding = aligned - current;

    for (uint64_t i = 0; i < padding; i++) {
        infix_status status = emit_emit_u8(ctx, 0x90);
        if (status != INFIX_SUCCESS)
            return status;
    }

    return INFIX_SUCCESS;
}

PULSE_API infix_status __attribute__((warn_unused_result)) emit_add_relocation(
    emit_context_t * ctx, const char * name, uint64_t offset, uint8_t size, uint8_t inst_size) {
    _infix_clear_error();
    if (!ctx || !name) {
        _infix_set_error(INFIX_CATEGORY_PARSER, INFIX_CODE_INVALID_KEYWORD, 0);
        return INFIX_ERROR_INVALID_ARGUMENT;
    }

    emit_relocation_t * rel = calloc(1, sizeof(emit_relocation_t));
    if (!rel)
        return INFIX_ERROR_ALLOCATION_FAILED;

    rel->symbol_name = strdup(name);
    rel->section_name = ctx->current_section ? strdup(ctx->current_section->name) : NULL;
    rel->offset = offset;
    rel->size = size;
    rel->inst_size = inst_size;
    rel->is_pc_relative = true;

    rel->next = ctx->relocations;
    ctx->relocations = rel;

    return INFIX_SUCCESS;
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

infix_status _emit_resolve_relocations(emit_context_t * ctx) {
    if (!ctx)
        return INFIX_SUCCESS;

    emit_section_t ** secs = malloc(ctx->section_count * sizeof(emit_section_t *));
    if (!secs)
        return INFIX_ERROR_ALLOCATION_FAILED;

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

        int64_t displacement = (int64_t)target_addr - (int64_t)(reloc_addr + rel->size);

        if (rel->size == 4) {
            if (rel->is_pc_relative)
                *(int32_t *)(reloc_sec->data + rel->offset) = (int32_t)displacement;
            else
                *(uint32_t *)(reloc_sec->data + rel->offset) = (uint32_t)target_addr;
        }
        else if (rel->size == 8) {
            if (rel->is_pc_relative)
                *(int64_t *)(reloc_sec->data + rel->offset) = displacement;
            else
                *(uint64_t *)(reloc_sec->data + rel->offset) = target_addr;
        }
    }

    free(secs);
    return INFIX_SUCCESS;
}

void _emit_arch_nop(emit_context_t * ctx, uint8_t size) {
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        for (uint8_t i = 0; i < size; i++) {
            infix_status status = emit_emit_u8(ctx, 0x90);
            (void)status;
        }
        break;
    case EMIT_ARCH_AARCH64: {
        infix_status status = emit_emit_u32(ctx, 0xD503201F);
        (void)status;
        break;
    }
    default:
        break;
    }
}

infix_status _emit_arch_align(emit_context_t * ctx, uint64_t alignment) {
    switch (ctx->arch) {
    case EMIT_ARCH_X86_64:
        for (uint64_t i = 0; i < alignment; i++) {
            infix_status status = emit_emit_u8(ctx, 0x90);
            if (status != INFIX_SUCCESS)
                return status;
        }
        break;
    case EMIT_ARCH_AARCH64:
        for (uint64_t i = 0; i < alignment; i++) {
            infix_status status = emit_emit_u32(ctx, 0xD503201F);
            if (status != INFIX_SUCCESS)
                return status;
        }
        break;
    default:
        break;
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_get_binary(const emit_context_t * ctx, const uint8_t ** out_data, size_t * out_size) {
    _infix_clear_error();
    if (!ctx || !out_data || !out_size)
        return INFIX_ERROR_INVALID_ARGUMENT;

    emit_context_t * mutable_ctx = (emit_context_t *)ctx;
    infix_status status = _emit_resolve_relocations(mutable_ctx);
    if (status != INFIX_SUCCESS)
        return status;

    uint64_t total_size = 0;
    for (emit_section_t * sec = ctx->sections; sec != NULL; sec = sec->next)
        total_size += sec->size;

    uint8_t * buffer = malloc(total_size);
    if (!buffer)
        return INFIX_ERROR_ALLOCATION_FAILED;

    write_raw_binary(mutable_ctx, buffer, total_size);

    *out_data = buffer;
    *out_size = total_size;

    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_get_offset(const emit_context_t * ctx, uint64_t * out_offset) {
    _infix_clear_error();
    if (!ctx || !out_offset)
        return INFIX_ERROR_INVALID_ARGUMENT;

    *out_offset = ctx->current_section ? ctx->current_section->size : 0;
    return INFIX_SUCCESS;
}
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
 * @brief Math operations for JIT code generation (x86-64 and ARM64).
 */
#include "pulse/emit/emit_math.h"
#include "pulse/emit/emit.h"
#include "emit_internals.h"
#include <stdio.h>
#include <string.h>

#define EMIT_CHECK(x) do { infix_status _s = (x); if (_s != INFIX_SUCCESS) return _s; } while(0)

#define EMIT_REG_NEEDS_REX(reg) ((reg) >= 8)

static infix_status emit_x86_rex(emit_context_t * ctx, bool w, bool r, bool x, bool b) {
    if (ctx->arch == EMIT_ARCH_X86_64 && (w || r || x || b)) {
        uint8_t rex = 0x40 | (w << 3) | (r << 2) | (x << 1) | b;
        return emit_emit_u8(ctx, rex);
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_mov_imm(emit_context_t * ctx, emit_register_t dest, uint64_t imm) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0xB8 | (dest & 0x07)));
        EMIT_CHECK(emit_emit_u64(ctx, imm));
    }
    return INFIX_SUCCESS;
}
PULSE_API infix_status emit_math_movq_gpr_xmm(emit_context_t * ctx, emit_register_t gpr_dest, emit_register_t xmm_src) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        /* MOVQ r64, xmm -> 66 REX.W 0F 7E /r */
        EMIT_CHECK(emit_emit_u8(ctx, 0x66));
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(xmm_src), false, EMIT_REG_NEEDS_REX(gpr_dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x7E));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((xmm_src & 0x07) << 3) | (gpr_dest & 0x07)));
    }
    return INFIX_SUCCESS;
}
PULSE_API infix_status emit_math_mov_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x89));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_add(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x01));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_add_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x81));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | (dest & 0x07)));
        EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_sub(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x29));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_sub_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x81));
        EMIT_CHECK(emit_emit_u8(ctx, 0xE8 | (dest & 0x07)));
        EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_mul(emit_context_t * ctx, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(src)));
        EMIT_CHECK(emit_emit_u8(ctx, 0xF7));
        EMIT_CHECK(emit_emit_u8(ctx, 0xE0 | (src & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_imul_imm(emit_context_t * ctx, emit_register_t dest, int32_t imm) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(dest)));
        if (imm >= -128 && imm <= 127) {
            EMIT_CHECK(emit_emit_u8(ctx, 0x6B));
            EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (dest & 0x07)));
            EMIT_CHECK(emit_emit_u8(ctx, (uint8_t)imm));
        } else {
            EMIT_CHECK(emit_emit_u8(ctx, 0x69));
            EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (dest & 0x07)));
            EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
        }
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_and(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x21));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_or(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x09));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_xor(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x31));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((src & 0x07) << 3) | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_not(emit_context_t * ctx, emit_register_t reg) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
        EMIT_CHECK(emit_emit_u8(ctx, 0xF7));
        EMIT_CHECK(emit_emit_u8(ctx, 0xD0 | (reg & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_neg(emit_context_t * ctx, emit_register_t reg) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
        EMIT_CHECK(emit_emit_u8(ctx, 0xF7));
        EMIT_CHECK(emit_emit_u8(ctx, 0xD8 | (reg & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_shl(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        if (src != EMIT_REG_RCX) return INFIX_ERROR_INVALID_ARGUMENT;
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0xD3));
        EMIT_CHECK(emit_emit_u8(ctx, 0xE0 | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_shr(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        if (src != EMIT_REG_RCX) return INFIX_ERROR_INVALID_ARGUMENT;
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(dest)));
        EMIT_CHECK(emit_emit_u8(ctx, 0xD3));
        EMIT_CHECK(emit_emit_u8(ctx, 0xE8 | (dest & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_cmp(emit_context_t * ctx, emit_register_t a, emit_register_t b) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(b), false, EMIT_REG_NEEDS_REX(a)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x39));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((b & 0x07) << 3) | (a & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_cmp_imm(emit_context_t * ctx, emit_register_t reg, int32_t imm) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, false, false, EMIT_REG_NEEDS_REX(reg)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x81));
        EMIT_CHECK(emit_emit_u8(ctx, 0xF8 | (reg & 0x07)));
        EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)imm));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_test(emit_context_t * ctx, emit_register_t a, emit_register_t b) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(b), false, EMIT_REG_NEEDS_REX(a)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x85));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((b & 0x07) << 3) | (a & 0x07)));
    }
    return INFIX_SUCCESS;
}

static const uint8_t x86_jcc_opcodes[16] = {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F
};

PULSE_API infix_status emit_math_jmp(emit_context_t * ctx, const char * label) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        uint64_t jump_offset = ctx->current_section->size;
        EMIT_CHECK(emit_emit_u8(ctx, 0xE9));
        EMIT_CHECK(emit_emit_u32(ctx, 0));
        EMIT_CHECK(emit_add_relocation(ctx, label, jump_offset + 1, 4, 5));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_jmp_cc(emit_context_t * ctx, emit_cc_t cc, const char * label) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        uint64_t jump_offset = ctx->current_section->size;
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, x86_jcc_opcodes[cc]));
        EMIT_CHECK(emit_emit_u32(ctx, 0));
        EMIT_CHECK(emit_add_relocation(ctx, label, jump_offset + 2, 4, 6));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_call(emit_context_t * ctx, const char * name) {
    _infix_clear_error();
    if (!ctx) return INFIX_ERROR_INVALID_ARGUMENT;
    if (ctx->arch == EMIT_ARCH_X86_64) {
        uint64_t call_offset = ctx->current_section->size;
        EMIT_CHECK(emit_emit_u8(ctx, 0xE8));
        EMIT_CHECK(emit_emit_u32(ctx, 0));
        EMIT_CHECK(emit_add_relocation(ctx, name, call_offset + 1, 4, 5));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_prologue(emit_context_t * ctx) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_emit_u8(ctx, 0x55));
        EMIT_CHECK(emit_emit_u8(ctx, 0x48));
        EMIT_CHECK(emit_emit_u8(ctx, 0x8B));
        EMIT_CHECK(emit_emit_u8(ctx, 0xEC));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_epilogue(emit_context_t * ctx) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_emit_u8(ctx, 0xC9));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC3));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_ret(emit_context_t * ctx) {
    if (ctx->arch == EMIT_ARCH_X86_64) EMIT_CHECK(emit_emit_u8(ctx, 0xC3));
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_push(emit_context_t * ctx, emit_register_t reg) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, false, false, false, EMIT_REG_NEEDS_REX(reg)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x50 | (reg & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_pop(emit_context_t * ctx, emit_register_t reg) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_x86_rex(ctx, false, false, false, EMIT_REG_NEEDS_REX(reg)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x58 | (reg & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_load_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t base, int32_t offset) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        uint8_t mod = (offset == 0 && (base & 0x07) != 5) ? 0x00 : ((offset >= -128 && offset <= 127) ? 0x40 : 0x80);
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(base)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x8B));
        EMIT_CHECK(emit_emit_u8(ctx, mod | ((dest & 0x07) << 3) | (base & 0x07)));
        if ((base & 0x07) == 4) EMIT_CHECK(emit_emit_u8(ctx, 0x24));
        if (mod == 0x40) EMIT_CHECK(emit_emit_u8(ctx, (uint8_t)offset));
        else if (mod == 0x80) EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)offset));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_store_reg(emit_context_t * ctx, emit_register_t base, int32_t offset, emit_register_t src) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        uint8_t mod = (offset == 0 && (base & 0x07) != 5) ? 0x00 : ((offset >= -128 && offset <= 127) ? 0x40 : 0x80);
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, EMIT_REG_NEEDS_REX(base)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x89));
        EMIT_CHECK(emit_emit_u8(ctx, mod | ((src & 0x07) << 3) | (base & 0x07)));
        if ((base & 0x07) == 4) EMIT_CHECK(emit_emit_u8(ctx, 0x24));
        if (mod == 0x40) EMIT_CHECK(emit_emit_u8(ctx, (uint8_t)offset));
        else if (mod == 0x80) EMIT_CHECK(emit_emit_u32(ctx, (uint32_t)offset));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_load_sym(emit_context_t * ctx, emit_register_t dest, const char * sym) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        uint64_t load_offset = ctx->current_section->size;
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(dest), false, false));
        EMIT_CHECK(emit_emit_u8(ctx, 0x8B));
        EMIT_CHECK(emit_emit_u8(ctx, 0x05 | ((dest & 0x07) << 3)));
        EMIT_CHECK(emit_emit_u32(ctx, 0));
        EMIT_CHECK(emit_add_relocation(ctx, sym, load_offset + 3, 4, 7));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_store_sym(emit_context_t * ctx, const char * sym, emit_register_t src) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        uint64_t store_offset = ctx->current_section->size;
        EMIT_CHECK(emit_x86_rex(ctx, true, EMIT_REG_NEEDS_REX(src), false, false));
        EMIT_CHECK(emit_emit_u8(ctx, 0x89));
        EMIT_CHECK(emit_emit_u8(ctx, 0x05 | ((src & 0x07) << 3)));
        EMIT_CHECK(emit_emit_u32(ctx, 0));
        EMIT_CHECK(emit_add_relocation(ctx, sym, store_offset + 3, 4, 7));
    }
    return INFIX_SUCCESS;
}

/* Floating Point Operations */
PULSE_API infix_status emit_math_movsd_reg(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_emit_u8(ctx, 0xF2));
        EMIT_CHECK(emit_x86_rex(ctx, false, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(src)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x10));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (src & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_addsd(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_emit_u8(ctx, 0xF2));
        EMIT_CHECK(emit_x86_rex(ctx, false, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(src)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x58));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (src & 0x07)));
    }
    return INFIX_SUCCESS;
}

PULSE_API infix_status emit_math_subsd(emit_context_t * ctx, emit_register_t dest, emit_register_t src) {
    if (ctx->arch == EMIT_ARCH_X86_64) {
        EMIT_CHECK(emit_emit_u8(ctx, 0xF2));
        EMIT_CHECK(emit_x86_rex(ctx, false, EMIT_REG_NEEDS_REX(dest), false, EMIT_REG_NEEDS_REX(src)));
        EMIT_CHECK(emit_emit_u8(ctx, 0x0F));
        EMIT_CHECK(emit_emit_u8(ctx, 0x5C));
        EMIT_CHECK(emit_emit_u8(ctx, 0xC0 | ((dest & 0x07) << 3) | (src & 0x07)));
    }
    return INFIX_SUCCESS;
}
