/**
 * @file emit_pe.h
 * @brief PE format writer header.
 */
#ifndef EMIT_PE_H
#define EMIT_PE_H

#include "pulse/emit/emit.h"

pulse_status emit_write_pe(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size);

#endif
