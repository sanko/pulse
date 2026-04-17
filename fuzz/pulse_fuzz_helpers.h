/**
 * @file pulse_fuzz_helpers.h
 * @brief Common helper functions for Pulse fuzzers.
 */
#ifndef PULSE_FUZZ_HELPERS_H
#define PULSE_FUZZ_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKEN_COUNT 10000

typedef struct {
    const uint8_t * data;
    size_t size;
    size_t pos;
} fuzz_input_t;

static inline bool fuzz_consume_bytes(fuzz_input_t * in, size_t n, const uint8_t ** out) {
    if (in->pos + n > in->size)
        return false;
    *out = in->data + in->pos;
    in->pos += n;
    return true;
}

static inline bool fuzz_consume_uint8(fuzz_input_t * in, uint8_t * out) {
    const uint8_t * bytes;
    if (!fuzz_consume_bytes(in, 1, &bytes))
        return false;
    *out = bytes[0];
    return true;
}

static inline bool fuzz_consume_uint32(fuzz_input_t * in, uint32_t * out) {
    const uint8_t * bytes;
    if (!fuzz_consume_bytes(in, 4, &bytes))
        return false;
    *out = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return true;
}

static inline uint32_t fuzz_consume_uint32_range(fuzz_input_t * in, uint32_t min, uint32_t max) {
    uint32_t val;
    if (!fuzz_consume_uint32(in, &val))
        return min;
    if (max <= min)
        return min;
    return min + (val % (max - min + 1));
}

static inline char * fuzz_null_terminate(fuzz_input_t * in) {
    size_t null_pos = in->pos;
    while (null_pos < in->size && in->data[null_pos] != 0)
        null_pos++;
    size_t len = null_pos - in->pos;
    char * result = malloc(len + 1);
    if (!result)
        return NULL;
    memcpy(result, in->data + in->pos, len);
    result[len] = '\0';
    in->pos = null_pos + 1;
    return result;
}

static inline void * fuzz_alloc_executable(size_t size) {
    (void)size;
    return NULL;
}

static inline void fuzz_free_executable(void * ptr, size_t size) {
    (void)ptr;
    (void)size;
}

#endif /* PULSE_FUZZ_HELPERS_H */
