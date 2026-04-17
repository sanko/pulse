/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file lexer.c
 * @brief Lexer (tokenizer) implementation for the Infix language.
 *
 * The lexer is responsible for converting source code text into a stream of tokens
 * that the parser can then consume. It handles:
 * - Recognition of keywords and identifiers
 * - Numeric literal parsing (integers and floats)
 * - String literal parsing with escape sequences
 * - Operator and punctuation recognition
 * - Whitespace and comment handling
 *
 * @note This is a stub implementation. Replace with actual lexer code.
 */
#include "pulse/pulse.h"

lexer_t * lexer_create(const char * source) {
    (void)source;
    return NULL;
}

void lexer_destroy(lexer_t * lexer) { (void)lexer; }

token_t lexer_next_token(lexer_t * lexer) {
    (void)lexer;
    token_t tok = {TOKEN_EOF, NULL, 0, 0, {0}};
    return tok;
}
