#ifndef PULSE_COMMON_H
#define PULSE_COMMON_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#define PULSE_API __declspec(dllexport)
#else
#define PULSE_API __attribute__((visibility("default")))
#endif

typedef enum {
    PULSE_SUCCESS = 0,
    PULSE_ERROR_INVALID_ARGUMENT = 1,
    PULSE_ERROR_OUT_OF_MEMORY = 2,
    PULSE_ERROR_NOT_IMPLEMENTED = 3,
    PULSE_ERROR_GENERIC = 4,
    PULSE_ERROR_ALLOCATION_FAILED = 5
} pulse_status;

#define PULSE_CATEGORY_PARSER 1
#define PULSE_CODE_INVALID_KEYWORD 1

typedef struct {
    int category;
    int code;
    size_t position;
    long system_code;
    char system_message[256];
} pulse_error_details_t;

extern _Thread_local pulse_error_details_t g_pulse_last_error;

void _pulse_clear_error(void);
void _pulse_set_error(int category, int code, size_t position);
pulse_error_details_t pulse_get_last_error(void);

#endif
