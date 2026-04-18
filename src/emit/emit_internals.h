/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_internals.h
 * @brief Internal structures for the emit JIT code generation system.
 *
 * This header defines the internal data structures used by the emit library.
 * These structures are not part of the public API and should not be accessed
 * directly by users of the library.
 */
#ifndef INFIX_EMIT_INTERNALS_H
#define INFIX_EMIT_INTERNALS_H

#include "pulse/emit/emit.h"
#include "pulse/emit/emit_math.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline uint8_t _emit_x64_reg(emit_register_t reg) {
    if (reg >= 0 && reg <= 15) return (uint8_t)reg;
    if (reg >= 200 && reg <= 215) return (uint8_t)(reg - 200);
    return 0; // Fallback
}

static inline bool _emit_x64_reg_needs_rex(emit_register_t reg) {
    if (reg >= 8 && reg <= 15) return true;
    if (reg >= 208 && reg <= 215) return true;
    return false;
}

static inline uint8_t _emit_arm64_reg(emit_register_t reg) {
    if (reg >= 100 && reg <= 131) return (uint8_t)(reg - 100);
    if (reg == 132) return 31; // XZR
    return (uint8_t)(reg & 0x1F);
}

#define EMIT_CHECK(x)            \
    do {                         \
        pulse_status _s = (x);   \
        if (_s != PULSE_SUCCESS) \
            return _s;           \
    } while (0)

/**
 * @brief Represents a section in the output binary.
 *
 * Sections are used to organize code and data in the generated binary.
 * Common sections include .text (code), .data (initialized data), and .rdata
 * (read-only data such as import tables).
 */
typedef struct emit_section {
    char * name;                /**< Section name (e.g., ".text") */
    emit_section_flags_t flags; /**< Section flags (ALLOC, WRITE, EXECUTE) */
    uint8_t * data;             /**< Raw section data */
    uint64_t size;              /**< Current size of data in bytes */
    uint64_t capacity;          /**< Allocated capacity */
    struct emit_section * next; /**< Next section in list */
} emit_section_t;

/**
 * @brief Represents a symbol (function or variable) in the output.
 *
 * Symbols are used for function addresses, global variables, and relocation
 * resolution during code generation.
 */
typedef struct emit_symbol {
    char * name;               /**< Symbol name */
    bool is_defined;           /**< true if symbol has been defined */
    bool is_function;          /**< true if symbol is a function */
    emit_section_t * section;  /**< Section containing the symbol */
    uint64_t value;            /**< Symbol value (address or offset) */
    struct emit_symbol * next; /**< Next symbol in list */
} emit_symbol_t;

/**
 * @brief Represents a relocation to be resolved.
 *
 * Relocations record places in the code that need to be fixed up when
 * linking. For example, a call instruction to an external function needs
 * a relocation entry to record where the call target address should go.
 */
typedef struct emit_relocation {
    char * symbol_name;            /**< Name of the symbol to resolve */
    char * section_name;           /**< Section containing the relocation */
    uint64_t offset;               /**< Offset in section where relocation applies */
    uint8_t size;                  /**< Size of the relocation in bytes */
    uint8_t inst_size;             /**< Size of the instruction containing relocation */
    bool is_pc_relative;           /**< true if relocation is PC-relative */
    struct emit_relocation * next; /**< Next relocation in list */
} emit_relocation_t;

/**
 * @brief The main context for the emit code generator.
 *
 * This structure holds all state needed for code generation, including
 * the list of sections, symbols, and relocations. It is created by
 * emit_create() and destroyed by emit_destroy().
 */
typedef struct emit_context {
    emit_architecture_t arch;         /**< Target architecture */
    emit_format_t format;             /**< Output format (PE, ELF, etc.) */
    emit_state_t state;               /**< Current state of the context */
    emit_section_t * sections;        /**< List of all sections */
    emit_section_t * current_section; /**< Section currently being written */
    emit_symbol_t * symbols;          /**< List of all symbols */
    emit_relocation_t * relocations;  /**< List of all relocations */
    void * binary_spec;               /**< Format-specific binary state */
    char * current_block_name;        /**< Name of current basic block */
    int section_count;                /**< Number of sections created */
} emit_context_t;

/**
 * @brief Initializes an emit context.
 * @param[in] ctx Context to initialize.
 * @param[in] arch Target architecture.
 * @param[in] format Output format.
 */
void _emit_context_init(emit_context_t * ctx, emit_architecture_t arch, emit_format_t format);

/**
 * @brief Frees all resources in an emit context.
 * @param[in] ctx Context to free.
 */
void _emit_context_free(emit_context_t * ctx);

/**
 * @brief Looks up a section by name.
 * @param[in] ctx The emit context.
 * @param[in] name Section name to find.
 * @return Section pointer, or NULL if not found.
 */
emit_section_t * _emit_lookup_section(emit_context_t * ctx, const char * name);

/**
 * @brief Looks up a symbol by name.
 * @param[in] ctx The emit context.
 * @param[in] name Symbol name to find.
 * @return Symbol pointer, or NULL if not found.
 */
emit_symbol_t * _emit_lookup_symbol(emit_context_t * ctx, const char * name);

/**
 * @brief Resolves all pending relocations.
 * @param[in] ctx The emit context.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
pulse_status _emit_resolve_relocations(emit_context_t * ctx);

/**
 * @brief Adds a relocation to the current section.
 */
pulse_status _emit_add_relocation(
    emit_context_t * ctx, const char * name, uint64_t offset, uint8_t size, uint8_t inst_size, bool is_pc_relative);

#endif /* INFIX_EMIT_INTERNALS_H */
