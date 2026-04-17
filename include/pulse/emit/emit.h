/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit.h
 * @brief Public API for the emit JIT code generation system.
 */
#ifndef INFIX_EMIT_H
#define INFIX_EMIT_H

#include "pulse/pulse_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#define PULSE_API __declspec(dllexport)
#else
#define PULSE_API __attribute__((visibility("default")))
#endif

/**
 * @brief Target architecture for code generation.
 */
typedef enum {
    EMIT_ARCH_X86_64,  /**< x86-64 architecture (Windows, Linux, macOS) */
    EMIT_ARCH_AARCH64, /**< ARM64 architecture (Apple Silicon, ARM servers) */
} emit_architecture_t;

/**
 * @brief Output format for generated code.
 */
typedef enum {
    EMIT_FORMAT_BINARY,   /**< Raw machine code (binary) */
    EMIT_FORMAT_ELF,      /**< ELF relocatable object file */
    EMIT_FORMAT_ELF_EXEC, /**< ELF executable (Unix) */
    EMIT_FORMAT_PE,       /**< PE executable (Windows) */
} emit_format_t;

/**
 * @brief Section flags for memory protection and allocation.
 */
typedef enum {
    EMIT_SECTION_FLAG_NONE = 0,
    EMIT_SECTION_FLAG_ALLOC = 1 << 0,   /**< Section should be allocated in memory */
    EMIT_SECTION_FLAG_WRITE = 1 << 1,   /**< Section is writable */
    EMIT_SECTION_FLAG_EXECUTE = 1 << 2, /**< Section is executable */
} emit_section_flags_t;

/**
 * @brief Symbol visibility in the output file.
 */
typedef enum {
    EMIT_VISIBILITY_DEFAULT,   /**< Symbol is visible to linked objects */
    EMIT_VISIBILITY_HIDDEN,    /**< Symbol is not exported */
    EMIT_VISIBILITY_PROTECTED, /**< Symbol is protected from relocation */
} emit_visibility_t;

/**
 * @brief Current state of the emit context.
 */
typedef enum {
    EMIT_STATE_IDLE,             /**< No active section */
    EMIT_STATE_SECTION_ACTIVE,   /**< Currently writing to a section */
    EMIT_STATE_SECTION_INACTIVE, /**< Section writing completed */
} emit_state_t;

/**
 * @brief Opaque context for the emit code generator.
 */
typedef struct emit_context emit_context_t;

/**
 * @brief Creates a new emit context.
 * @param[out] out_ctx Pointer to receive the new context.
 * @param[in] arch Target architecture.
 * @param[in] format Output format.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_create(emit_context_t ** out_ctx, emit_architecture_t arch, emit_format_t format);

/**
 * @brief Destroys an emit context and frees all resources.
 * @param[in] ctx The context to destroy.
 */
PULSE_API void emit_destroy(emit_context_t * ctx);

/**
 * @brief Adds a new section to the output.
 * @param[in] ctx The emit context.
 * @param[in] name Section name (e.g., ".text").
 * @param[in] flags Section flags.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_add_section(emit_context_t * ctx, const char * name, emit_section_flags_t flags);

/**
 * @brief Begins writing to a section.
 * @param[in] ctx The emit context.
 * @param[in] name Section name to begin.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_begin_section(emit_context_t * ctx, const char * name);

/**
 * @brief Defines a symbol at the current position.
 * @param[in] ctx The emit context.
 * @param[in] name Symbol name.
 * @param[in] visibility Symbol visibility.
 * @param[in] is_function true if symbol is a function.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_define_symbol(emit_context_t * ctx,
                                          const char * name,
                                          emit_visibility_t visibility,
                                          bool is_function);

/**
 * @brief Emits a label that can be jumped to.
 * @param[in] ctx The emit context.
 * @param[in] name Label name.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_emit_label(emit_context_t * ctx, const char * name);

/**
 * @brief Creates a local label for internal use.
 * @param[in] ctx The emit context.
 * @param[in] name Label name.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_create_label(emit_context_t * ctx, const char * name);

/**
 * @brief Emits raw bytes to the current section.
 * @param[in] ctx The emit context.
 * @param[in] value Value to emit.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_emit_u8(emit_context_t * ctx, uint8_t value);

/**
 * @brief Emits a 16-bit value in little-endian byte order.
 * @param[in] ctx The emit context.
 * @param[in] value Value to emit.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_emit_u16(emit_context_t * ctx, uint16_t value);

/**
 * @brief Emits a 32-bit value in little-endian byte order.
 * @param[in] ctx The emit context.
 * @param[in] value Value to emit.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_emit_u32(emit_context_t * ctx, uint32_t value);

/**
 * @brief Emits a 64-bit value in little-endian byte order.
 * @param[in] ctx The emit context.
 * @param[in] value Value to emit.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_emit_u64(emit_context_t * ctx, uint64_t value);

/**
 * @brief Retrieves the generated binary data.
 * @param[in] ctx The emit context.
 * @param[out] out_data Pointer to receive data pointer.
 * @param[out] out_size Pointer to receive data size.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_get_binary(const emit_context_t * ctx, const uint8_t ** out_data, size_t * out_size);

/**
 * @brief Gets the current offset in the active section.
 * @param[in] ctx The emit context.
 * @param[out] out_offset Pointer to receive current offset.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_get_offset(const emit_context_t * ctx, uint64_t * out_offset);

/**
 * @brief Writes the generated code to a file.
 * @param[in] ctx The emit context.
 * @param[in] filename Output filename.
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_write_file(const emit_context_t * ctx, const char * filename);

/**
 * @brief Writes a PE executable that calls ExitProcess with the specified return value.
 * @param[in] ctx The emit context.
 * @param[in] filename Output filename.
 * @param[in] return_value Value to return from the process.
 * @return PULSE_SUCCESS on success, error code otherwise.
 *
 * The generated executable contains an import table for KERNEL32.dll and ExitProcess.
 */
PULSE_API pulse_status emit_write_pe_exec(const emit_context_t * ctx, const char * filename, uint64_t return_value);

/**
 * @brief Aligns the current section to the specified boundary.
 * @param[in] ctx The emit context.
 * @param[in] alignment Alignment value (must be power of 2).
 * @return PULSE_SUCCESS on success, error code otherwise.
 */
PULSE_API pulse_status emit_align(emit_context_t * ctx, uint64_t alignment);

#ifdef __cplusplus
}
#endif

#endif /* INFIX_EMIT_H */
