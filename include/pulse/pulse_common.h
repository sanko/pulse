/**
 * @file pulse_common.h
 * @brief Common types and definitions shared across the pulse library.
 */
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

/**
 * @brief Status codes returned by pulse library functions.
 */
typedef enum {
    PULSE_SUCCESS = 0,                /**< Operation completed successfully */
    PULSE_ERROR_INVALID_ARGUMENT = 1, /**< Invalid argument provided */
    PULSE_ERROR_OUT_OF_MEMORY = 2,    /**< Memory allocation failed */
    PULSE_ERROR_NOT_IMPLEMENTED = 3,  /**< Feature not implemented */
    PULSE_ERROR_GENERIC = 4,          /**< Generic error */
    PULSE_ERROR_ALLOCATION_FAILED = 5 /**< Allocation failure (alias for OUT_OF_MEMORY) */
} pulse_status;

/**
 * @brief Error category for parser errors.
 */
#define PULSE_CATEGORY_PARSER 1

/**
 * @brief Error code for invalid keyword.
 */
#define PULSE_CODE_INVALID_KEYWORD 1

/**
 * @brief Detailed error information structure.
 */
typedef struct {
    int category;             /**< Error category */
    int code;                 /**< Error code */
    size_t position;          /**< Position in source where error occurred */
    long system_code;         /**< System error code (errno, GetLastError, etc.) */
    char system_message[256]; /**< System error message */
} pulse_error_details_t;

/**
 * @brief Thread-local storage for the last error that occurred.
 */
extern _Thread_local pulse_error_details_t g_pulse_last_error;

/**
 * @brief Clears the last error.
 */
void _pulse_clear_error(void);

/**
 * @brief Sets the last error with category, code, and position.
 * @param[in] category Error category.
 * @param[in] code Error code.
 * @param[in] position Position in source code.
 */
void _pulse_set_error(int category, int code, size_t position);

/**
 * @brief Gets the last error that occurred.
 * @return The last error details.
 */
pulse_error_details_t pulse_get_last_error(void);

#endif
