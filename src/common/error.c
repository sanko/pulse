/**
 * @file error.c
 * @brief Error handling implementation for pulse.
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
