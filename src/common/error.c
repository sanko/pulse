/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file error.c
 * @brief Error handling implementation for the pulse library.
 *
 * This module provides thread-safe error handling through a thread-local
 * error state. Functions in the library set error details on failure,
 * which can then be retrieved by the caller.
 *
 * Error handling pattern:
 * @code
 * // Clear any previous error
 * _pulse_clear_error();
 *
 * // Call a function that may fail
 * pulse_status status = some_function();
 *
 * // Check for error
 * if (status != PULSE_SUCCESS) {
 *     pulse_error_details_t err = pulse_get_last_error();
 *     // Handle error...
 * }
 * @endcode
 */
#define PULSE_BUILDING
#include "pulse/pulse_common.h"
#include <string.h>

_Thread_local pulse_error_details_t g_pulse_last_error = {0};

pulse_error_details_t pulse_get_last_error(void) { return g_pulse_last_error; }

void _pulse_clear_error(void) { memset(&g_pulse_last_error, 0, sizeof(g_pulse_last_error)); }

void _pulse_set_error(int category, int code, size_t position) {
    _pulse_clear_error();
    g_pulse_last_error.category = category;
    g_pulse_last_error.code = code;
    g_pulse_last_error.position = position;
}
