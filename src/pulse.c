#define PULSE_BUILDING

/* Common components */
#include "common/compat_c23.h"
#include "common/error.c"

/* Compiler components */
#include "compiler/lexer.c"
#include "compiler/parser.c"
#include "compiler/codegen.c"

/* Emitter components - Platform independent */
#include "emit/emit_math.c"

/* Emitter components - File formats */
#include "emit/elf/emit_elf.c"
#include "emit/pe/emit_pe.c"

/* Emitter components - Architectures */
#include "emit/x64/emit_x64.c"
#include "emit/aarch64/emit_arm64.c"
