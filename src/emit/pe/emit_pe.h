/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_pe.h
 * @brief PE (Portable Executable) format writer for Windows.
 *
 * This module handles generation of Windows PE32/PE32+ executable files.
 * It produces valid Windows executables with proper DOS headers, PE signatures,
 * section tables, and optional import tables for external DLL dependencies.
 */
#ifndef EMIT_PE_H
#define EMIT_PE_H

#include "pulse/emit/emit.h"

/**
 * @brief Writes user-generated sections to a PE file.
 * @param[in] ctx The emit context.
 * @param[out] out_data Pointer to receive allocated output buffer.
 * @param[out] out_size Pointer to receive output size.
 * @return PULSE_SUCCESS on success, error code otherwise.
 *
 * Unlike emit_write_pe_exec(), this function writes the sections created by the user
 * rather than generating a wrapper with ExitProcess.
 */
pulse_status emit_write_pe(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size);

#endif
