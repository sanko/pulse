/**
 * @file fuzz_lexer.c
 * @brief Fuzzer for the Pulse lexer/tokenizer.
 */

#include "pulse_fuzz_helpers.h"
#include "pulse/pulse.h"
#include <stdio.h>
#include <string.h>

#ifndef USE_AFL
int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size);
#endif

static volatile int g_lexer_token_count = 0;
static volatile int g_lexer_errors = 0;

static void reset_lexer_counters(void) {
    g_lexer_token_count = 0;
    g_lexer_errors = 0;
}

static void fuzz_lexer_run(const uint8_t * data, size_t size) {
    if (size == 0)
        return;

    reset_lexer_counters();

    fuzz_input_t in = {.data = data, .size = size, .pos = 0};

    char * source = fuzz_null_terminate(&in);
    if (!source) {
        return;
    }

    lexer_t * lexer = lexer_create(source);
    if (!lexer) {
        free(source);
        return;
    }

    token_t tok;
    do {
        tok = lexer_next_token(lexer);
        g_lexer_token_count++;

        if (tok.type == TOKEN_ERROR) {
            g_lexer_errors++;
        }

        if (tok.type == TOKEN_STRING && tok.string_val)
            free(tok.string_val);
        if (tok.lexeme)
            free(tok.lexeme);

    } while (tok.type != TOKEN_EOF && g_lexer_token_count < MAX_TOKEN_COUNT);

    if (g_lexer_token_count >= MAX_TOKEN_COUNT)
        g_lexer_errors++;

    lexer_destroy(lexer);
    free(source);
}

#ifndef USE_AFL
int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    fuzz_lexer_run(data, size);
    return 0;
}
#else
#include <unistd.h>

int main(void) {
    unsigned char buf[1024 * 16];
    while (__AFL_LOOP(10000)) {
        ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
        if (len < 0)
            return 1;
        fuzz_lexer_run(buf, (size_t)len);
    }
    return 0;
}
#endif
