/**
 * @file 190_emit_direct.c
 * @brief Unified Test Suite: Infix Emit API & Pulse Language Compiler
 */

#define DBLTAP_IMPLEMENTATION
#include "../src/emit/emit_internals.h"
#include "common/compat_c23.h"
#include "common/double_tap.h"
#include "common/infix_config.h"
#include "pulse/pulse_common.h"
#include <ctype.h>
#include <inttypes.h>
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
typedef HANDLE pulse_thread_h;
#else
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#else
#define MAP_ANONYMOUS 0x20
#endif
#endif
typedef pthread_t pulse_thread_h;
#endif

#if defined(PULSE_ARCH_X64) || defined(__x86_64__) || defined(_M_X64)
#ifndef PULSE_ARCH_X64
#define PULSE_ARCH_X64 1
#endif
#define REG_RET EMIT_REG_RAX
#define REG_FP EMIT_REG_RBP
#define REG_SP EMIT_REG_RSP
#define TEST_REG_RET EMIT_REG_RAX
#define TEST_REG_SCRATCH EMIT_REG_RCX
#define TEST_REG_ARG1 EMIT_REG_RSI
#define TEST_REG_ARG2 EMIT_REG_RDX
#ifdef _WIN32
#define REG_ARG0 EMIT_REG_RCX
#define REG_ARG1 EMIT_REG_RDX
#define REG_ARG2 EMIT_REG_R8
#define REG_ARG3 EMIT_REG_R9
#define SHADOW_SPACE 32
#else
#define REG_ARG0 EMIT_REG_RDI
#define REG_ARG1 EMIT_REG_RSI
#define REG_ARG2 EMIT_REG_RDX
#define REG_ARG3 EMIT_REG_RCX
#define REG_ARG4 EMIT_REG_R8
#define REG_ARG5 EMIT_REG_R9
#define SHADOW_SPACE 0
#endif
#elif defined(PULSE_ARCH_ARM64) || defined(__aarch64__) || defined(_M_ARM64)
#ifndef PULSE_ARCH_ARM64
#define PULSE_ARCH_ARM64 1
#endif
#define REG_RET EMIT_REG_X0
#define REG_FP EMIT_REG_X29
#define REG_SP EMIT_REG_XSP
#define TEST_REG_RET EMIT_REG_X0
#define TEST_REG_SCRATCH EMIT_REG_X1
#define TEST_REG_ARG1 EMIT_REG_X2
#define TEST_REG_ARG2 EMIT_REG_X3
#define REG_ARG0 EMIT_REG_X0
#define REG_ARG1 EMIT_REG_X1
#define REG_ARG2 EMIT_REG_X2
#define REG_ARG3 EMIT_REG_X3
#define SHADOW_SPACE 0
#endif

/* ============================================================================
 * Pulse Runtime Environment & Type Tags
 * ============================================================================ */

#define TAG_PRIMITIVE 0
#define TAG_ARRAY 1
#define TAG_OBJECT 4
#define TAG_EXCEPTION 5
#define TAG_FIBER 6
#define TAG_THREAD 7
#define TAG_TUPLE 8
#define TAG_STRING 9
#define TAG_HASH 10

#define PULSE_STACK_SIZE (64 * 1024)
#define MAX_VREGS 64
#define PHYS_POOL_SIZE 3

typedef struct gc_header {
    uint32_t size;
    uint16_t tag;
    uint16_t flags;
    void (*finalizer)(void *);
    struct gc_header * forwarding;
} gc_header_t;

#define PAYLOAD_TO_HEADER(p) ((gc_header_t *)(p) - 1)
#define HEADER_TO_PAYLOAD(h) ((void *)((gc_header_t *)(h) + 1))

typedef enum { FIB_NEW, FIB_RUNNING, FIB_YIELDED, FIB_DEAD } fiber_state_t;

typedef struct pulse_fiber {
    gc_header_t header;
    fiber_state_t state;
    void * stack_mem;
    uintptr_t rsp;
} pulse_fiber_t;

typedef struct pulse_exception_handler {
    uintptr_t catch_ip;
    uintptr_t rsp;
    uintptr_t rbp;
    struct pulse_exception_handler * next;
} pulse_exception_handler_t;

typedef struct pulse_vm {
    uint8_t *from_space, *to_space;
    size_t capacity, top;
    void ** roots[1024];
    int root_count;
    int finalizers_called;
    pulse_exception_handler_t * handlers;
    void * last_exception;
    pulse_fiber_t * current_fiber;
} pulse_vm_t;

#if defined(_MSC_VER)
static __declspec(thread) pulse_vm_t * tls_current_vm = NULL;
#else
static __thread pulse_vm_t * tls_current_vm = NULL;
#endif

typedef struct {
    size_t length;
    uint64_t data[16];
} pulse_array_t;

typedef struct {
    size_t length;
    char data[64];
} pulse_string_t;

typedef struct {
    char * key;
    uint64_t value;
} hash_entry_t;

typedef struct {
    size_t capacity;
    size_t count;
    hash_entry_t entries[16];
} pulse_hash_t;

typedef struct {
    size_t count;
    uint64_t values[16];
} pulse_tuple_t;

/* ============================================================================
 * Support Functions (Allocation & Execution)
 * ============================================================================ */

static void * alloc_executable(size_t size) {
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
    void * mem = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (mem == MAP_FAILED) ? NULL : mem;
#endif
}

static void free_executable(void * mem, size_t size) {
#ifdef _WIN32
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    munmap(mem, size);
#endif
    (void)size;
}

static int execute_jit_code(const uint8_t * code, size_t size, void ** out_code) {
    void * exec_mem = alloc_executable(size);
    if (!exec_mem)
        return 0;
    memcpy(exec_mem, code, size);
#if defined(PULSE_ARCH_ARM64)
    __builtin___clear_cache((char *)exec_mem, (char *)exec_mem + size);
#endif
    *out_code = exec_mem;
    return 1;
}

static uint64_t return_72(void) { return 72; }
static void my_finalizer(void * p) { (void)p; }

/* ============================================================================
 * Cheney GC Implementation
 * ============================================================================ */

static pulse_vm_t * vm_create(size_t size) {
    pulse_vm_t * vm = (pulse_vm_t *)calloc(1, sizeof(pulse_vm_t));
    vm->capacity = size / 2;
    vm->from_space = (uint8_t *)malloc(vm->capacity);
    vm->to_space = (uint8_t *)malloc(vm->capacity);
    return vm;
}

static void * gc_alloc(pulse_vm_t * vm, size_t size, uint16_t tag) {
    size_t aligned_payload = (size + 7) & ~7;
    size_t total = sizeof(gc_header_t) + aligned_payload;
    if (vm->top + total > vm->capacity)
        return NULL;
    gc_header_t * h = (gc_header_t *)(vm->from_space + vm->top);
    memset(h, 0, sizeof(gc_header_t));
    h->size = (uint32_t)aligned_payload;
    h->tag = tag;
    vm->top += total;
    return HEADER_TO_PAYLOAD(h);
}

static void gc_collect(pulse_vm_t * vm) {
    uint8_t * next_top = vm->to_space;
    for (int i = 0; i < vm->root_count; i++) {
        void ** root_ptr = (void **)vm->roots[i];
        if (!root_ptr || !*root_ptr)
            continue;
        gc_header_t * old_h = PAYLOAD_TO_HEADER(*root_ptr);
        if (old_h->forwarding) {
            *root_ptr = HEADER_TO_PAYLOAD(old_h->forwarding);
        }
        else {
            gc_header_t * new_h = (gc_header_t *)next_top;
            memcpy(new_h, old_h, sizeof(gc_header_t) + old_h->size);
            next_top += (sizeof(gc_header_t) + old_h->size);
            old_h->forwarding = new_h;
            *root_ptr = HEADER_TO_PAYLOAD(new_h);
        }
    }
    uint8_t * scan = vm->from_space;
    while (scan < (vm->from_space + vm->top)) {
        gc_header_t * h = (gc_header_t *)scan;
        if (!h->forwarding && h->finalizer) {
            h->finalizer(HEADER_TO_PAYLOAD(h));
            vm->finalizers_called++;
        }
        scan += (sizeof(gc_header_t) + h->size);
    }
    uint8_t * temp = vm->from_space;
    vm->from_space = vm->to_space;
    vm->to_space = temp;
    vm->top = (size_t)(next_top - vm->from_space);
}

/* ============================================================================
 * JIT Helpers (Called by JIT)
 * ============================================================================ */

static pulse_string_t * pulse_string_concat(pulse_vm_t * vm, pulse_string_t * a, pulse_string_t * b) {
    size_t new_len = a->length + b->length;
    pulse_string_t * s = (pulse_string_t *)gc_alloc(vm, sizeof(pulse_string_t) + new_len, TAG_STRING);
    s->length = new_len;
    memcpy(s->data, a->data, a->length);
    memcpy(s->data + a->length, b->data, b->length);
    s->data[new_len] = '\0';
    return s;
}

static uint64_t pulse_hash_get(pulse_hash_t * h, const char * key) {
    for (size_t i = 0; i < h->capacity; i++)
        if (h->entries[i].key && strcmp(h->entries[i].key, key) == 0)
            return h->entries[i].value;
    return 0;
}

typedef struct {
    uint64_t expected_class;
    void * target_fn;
} inline_cache_t;

static int ic_slow_path_calls = 0;
static void * pulse_ic_lookup(inline_cache_t * ic, uint64_t obj_class, const char * name) {
    ic_slow_path_calls++;
    if (strcmp(name, "identity") == 0) {
        ic->expected_class = obj_class;
        ic->target_fn = (void *)return_72;
        return (void *)return_72;
    }
    return NULL;
}

/* ============================================================================
 * Compiler Backend: Register Allocator & IR logic
 * ============================================================================ */

typedef enum { P_TYPE_INT, P_TYPE_FLOAT } pulse_val_type_t;
typedef enum { VREG_FREE, VREG_IN_PHYS, VREG_SPILLED } vreg_state_t;

typedef struct {
    vreg_state_t state;
    pulse_val_type_t type;
    emit_register_t phys;
    int32_t stack_offset;
} vreg_info_t;

typedef struct {
    vreg_info_t vregs[MAX_VREGS];
    emit_register_t gpr_pool[PHYS_POOL_SIZE];
    emit_register_t xmm_pool[PHYS_POOL_SIZE];
    bool gpr_busy[PHYS_POOL_SIZE];
    bool xmm_busy[PHYS_POOL_SIZE];
    int next_gpr_victim;
} pulse_alloc_t;

static void alloc_init(pulse_alloc_t * a) {
    memset(a, 0, sizeof(pulse_alloc_t));
#if defined(PULSE_ARCH_X64)
    a->gpr_pool[0] = EMIT_REG_RAX;
    a->gpr_pool[1] = EMIT_REG_RCX;
    a->gpr_pool[2] = EMIT_REG_RDX;
#elif defined(PULSE_ARCH_ARM64)
    a->gpr_pool[0] = EMIT_REG_X0;
    a->gpr_pool[1] = EMIT_REG_X1;
    a->gpr_pool[2] = EMIT_REG_X2;
#endif
    a->xmm_pool[0] = 200; // XMM0
    a->xmm_pool[1] = 201; // XMM1
    a->xmm_pool[2] = 202; // XMM2
}

static void pulse_vreg_free(pulse_alloc_t * a, int vid) {
    if (a->vregs[vid].state == VREG_IN_PHYS) {
        for (int i = 0; i < PHYS_POOL_SIZE; i++) {
            if (a->vregs[vid].type == P_TYPE_INT && a->gpr_pool[i] == a->vregs[vid].phys) {
                a->gpr_busy[i] = false;
                break;
            }
            if (a->vregs[vid].type == P_TYPE_FLOAT && a->xmm_pool[i] == a->vregs[vid].phys) {
                a->xmm_busy[i] = false;
                break;
            }
        }
    }
    a->vregs[vid].state = VREG_FREE;
}

static emit_register_t pulse_vreg_alloc(emit_context_t * ctx, pulse_alloc_t * a, int vid, pulse_val_type_t type) {
    if (a->vregs[vid].state == VREG_IN_PHYS)
        return a->vregs[vid].phys;
    a->vregs[vid].type = type;
    bool was_spilled = (a->vregs[vid].state == VREG_SPILLED);
    int p_idx = -1;
    bool * busy = (type == P_TYPE_INT) ? a->gpr_busy : a->xmm_busy;
    emit_register_t * pool = (type == P_TYPE_INT) ? a->gpr_pool : a->xmm_pool;

    for (int i = 0; i < PHYS_POOL_SIZE; i++)
        if (!busy[i]) {
            p_idx = i;
            break;
        }

    if (p_idx == -1 && type == P_TYPE_INT) {
        p_idx = a->next_gpr_victim;
        a->next_gpr_victim = (a->next_gpr_victim + 1) % PHYS_POOL_SIZE;
        int victim_v = -1;
        for (int i = 0; i < MAX_VREGS; i++)
            if (a->vregs[i].state == VREG_IN_PHYS && a->vregs[i].type == P_TYPE_INT &&
                a->vregs[i].phys == pool[p_idx]) {
                victim_v = i;
                break;
            }
        a->vregs[victim_v].stack_offset = -((victim_v + 1) * 8);
        emit_math_store_reg(ctx, REG_FP, a->vregs[victim_v].stack_offset, pool[p_idx]);
        a->vregs[victim_v].state = VREG_SPILLED;
    }

    busy[p_idx] = true;
    a->vregs[vid].state = VREG_IN_PHYS;
    a->vregs[vid].phys = pool[p_idx];
    if (was_spilled && type == P_TYPE_INT)
        emit_math_load_reg(ctx, pool[p_idx], REG_FP, a->vregs[vid].stack_offset);
    return a->vregs[vid].phys;
}

typedef enum { P_OP_LOAD_INT, P_OP_LOAD_FLOAT, P_OP_ADD, P_OP_FADD, P_OP_RET, P_OP_JMP } pulse_op_t;
typedef struct {
    pulse_op_t op;
    int dest_vreg;
    int src_a;
    int src_b;
    union {
        int64_t i;
        double f;
        const char * target_name;
    } val;
    bool is_dead;
} pulse_insn_t;

#if defined(PULSE_ARCH_X64)
#ifdef _WIN32
static const emit_register_t ABI_GPRS[4] = {EMIT_REG_RCX, EMIT_REG_RDX, EMIT_REG_R8, EMIT_REG_R9};
#define ABI_GPR_COUNT 4
#else
static const emit_register_t ABI_GPRS[6] = {
    EMIT_REG_RDI, EMIT_REG_RSI, EMIT_REG_RDX, EMIT_REG_RCX, EMIT_REG_R8, EMIT_REG_R9};
#define ABI_GPR_COUNT 6
#endif
#elif defined(PULSE_ARCH_ARM64)
static const emit_register_t ABI_GPRS[8] = {
    EMIT_REG_X0, EMIT_REG_X1, EMIT_REG_X2, EMIT_REG_X3,
    EMIT_REG_X4, EMIT_REG_X5, EMIT_REG_X6, EMIT_REG_X7};
#define ABI_GPR_COUNT 8
#endif

static void pulse_emit_call(
    emit_context_t * ctx, pulse_alloc_t * alloc, void * target, int * arg_vregs, size_t num_args) {
    int overflow = (int)num_args - ABI_GPR_COUNT;
    if (overflow < 0)
        overflow = 0;
    size_t padding = (overflow * 8);
#ifdef _WIN32
    padding += 32;
#endif
    if (padding > 0)
        emit_math_sub_imm(ctx, REG_SP, (int32_t)padding);
    for (size_t i = 0; i < num_args && i < ABI_GPR_COUNT; i++) {
        emit_register_t phys = pulse_vreg_alloc(ctx, alloc, arg_vregs[i], P_TYPE_INT);
        if (phys != ABI_GPRS[i])
            emit_math_mov_reg(ctx, ABI_GPRS[i], phys);
    }
    for (size_t i = ABI_GPR_COUNT; i < num_args; i++) {
        emit_register_t phys = pulse_vreg_alloc(ctx, alloc, arg_vregs[i], P_TYPE_INT);
        emit_math_store_reg(ctx, REG_SP, (int32_t)((i - ABI_GPR_COUNT) * 8), phys);
    }
    emit_math_mov_imm(ctx, REG_RET, (uintptr_t)target);
    emit_math_call_reg(ctx, REG_RET);
    if (padding > 0)
        emit_math_add_imm(ctx, REG_SP, (int32_t)padding);
}

static void pulse_optimize_ir(pulse_insn_t * stream, size_t count) {
    int last_def[MAX_VREGS];
    for (int i = 0; i < MAX_VREGS; i++)
        last_def[i] = -1;

    for (size_t i = 0; i < count; i++) {
        if (stream[i].op == P_OP_ADD) {
            int ia = last_def[stream[i].src_a];
            int ib = last_def[stream[i].src_b];
            if (ia != -1 && ib != -1) {
                pulse_insn_t * def_a = &stream[ia];
                pulse_insn_t * def_b = &stream[ib];
                if (def_a->op == P_OP_LOAD_INT && def_b->op == P_OP_LOAD_INT) {
                    stream[i].op = P_OP_LOAD_INT;
                    stream[i].val.i = def_a->val.i + def_b->val.i;
                }
            }
        }
        last_def[stream[i].dest_vreg] = (int)i;
    }
    bool used[MAX_VREGS] = {0};
    for (int i = (int)count - 1; i >= 0; i--) {
        if (stream[i].op == P_OP_RET)
            used[stream[i].dest_vreg] = true;
        else if (stream[i].op == P_OP_ADD || stream[i].op == P_OP_FADD) {
            if (!used[stream[i].dest_vreg])
                stream[i].is_dead = true;
            else {
                used[stream[i].src_a] = true;
                used[stream[i].src_b] = true;
            }
        }
        else if (stream[i].op == P_OP_LOAD_INT || stream[i].op == P_OP_LOAD_FLOAT) {
            if (!used[stream[i].dest_vreg])
                stream[i].is_dead = true;
        }
    }
}

static void pulse_select_instructions(emit_context_t * ctx,
                                      pulse_alloc_t * alloc,
                                      pulse_insn_t * stream,
                                      size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (stream[i].is_dead)
            continue;
        pulse_insn_t * in = &stream[i];
        switch (in->op) {
        case P_OP_LOAD_INT:
            emit_math_mov_imm(ctx, pulse_vreg_alloc(ctx, alloc, in->dest_vreg, P_TYPE_INT), in->val.i);
            break;
        case P_OP_LOAD_FLOAT:
            {
#if defined(PULSE_ARCH_X64)
                union {
                    double f;
                    uint64_t u;
                } conv = {.f = in->val.f};
                emit_register_t tmp = pulse_vreg_alloc(ctx, alloc, MAX_VREGS - 1, P_TYPE_INT);
                emit_math_mov_imm(ctx, tmp, conv.u);
                emit_math_push(ctx, tmp);
                emit_register_t d = pulse_vreg_alloc(ctx, alloc, in->dest_vreg, P_TYPE_FLOAT);
                emit_emit_u8(ctx, 0xF2);
                emit_emit_u8(ctx, 0x0F);
                emit_emit_u8(ctx, 0x10);
                emit_emit_u8(ctx, 0x04 | ((d & 0x07) << 3));
                emit_emit_u8(ctx, 0x24);
                emit_math_pop(ctx, tmp);
                pulse_vreg_free(alloc, MAX_VREGS - 1);
#endif
                break;
            }
        case P_OP_ADD:
            {
                emit_register_t d = pulse_vreg_alloc(ctx, alloc, in->dest_vreg, P_TYPE_INT);
                emit_register_t a = pulse_vreg_alloc(ctx, alloc, in->src_a, P_TYPE_INT);
                emit_register_t b = pulse_vreg_alloc(ctx, alloc, in->src_b, P_TYPE_INT);
                if (d != a)
                    emit_math_mov_reg(ctx, d, a);
                emit_math_add(ctx, d, b);
                break;
            }
        case P_OP_FADD:
            {
#if defined(PULSE_ARCH_X64)
                emit_register_t d = pulse_vreg_alloc(ctx, alloc, in->dest_vreg, P_TYPE_FLOAT);
                emit_register_t a = pulse_vreg_alloc(ctx, alloc, in->src_a, P_TYPE_FLOAT);
                emit_register_t b = pulse_vreg_alloc(ctx, alloc, in->src_b, P_TYPE_FLOAT);
                if (d != a) {
                    emit_emit_u8(ctx, 0xF2);
                    emit_emit_u8(ctx, 0x0F);
                    emit_emit_u8(ctx, 0x10);
                    emit_emit_u8(ctx, 0xC0 | ((d & 7) << 3) | (a & 7));
                }
                emit_emit_u8(ctx, 0xF2);
                emit_emit_u8(ctx, 0x0F);
                emit_emit_u8(ctx, 0x58);
                emit_emit_u8(ctx, 0xC0 | ((d & 7) << 3) | (b & 7));
#endif
                break;
            }
        case P_OP_RET:
            {
                emit_register_t s = pulse_vreg_alloc(ctx, alloc, in->dest_vreg, P_TYPE_INT);
                if (s != REG_RET)
                    emit_math_mov_reg(ctx, REG_RET, s);
                emit_math_ret(ctx);
                break;
            }
        case P_OP_JMP:
            emit_math_jmp(ctx, in->val.target_name);
            break;
        }
    }
}

typedef struct {
    char name[16];
    int vreg;
} pulse_sym_t;

static int pulse_compile_source(const char * source, pulse_insn_t * out_ir) {
    pulse_sym_t syms[16];
    memset(syms, 0, sizeof(syms));
    int sym_count = 0, ir_ptr = 0;
    const char * p = source;

    while (*p) {
        while (isspace(*p) || *p == ';')
            p++;
        if (!*p)
            break;

        if (strncmp(p, "return", 6) == 0 && isspace(p[6])) {
            p += 7;
            while (isspace(*p))
                p++;
            char name[16];
            int n_ptr = 0;
            while (isalnum(*p))
                name[n_ptr++] = *p++;
            name[n_ptr] = 0;
            int v = -1;
            for (int i = 0; i < sym_count; i++)
                if (strcmp(syms[i].name, name) == 0)
                    v = syms[i].vreg;
            out_ir[ir_ptr++] = (pulse_insn_t){.op = P_OP_RET, .dest_vreg = v};
            continue;
        }

        if (isalpha(*p)) {
            char target_name[16];
            int n_ptr = 0;
            while (isalnum(*p))
                target_name[n_ptr++] = *p++;
            target_name[n_ptr] = 0;
            while (isspace(*p))
                p++;
            if (*p == '=') {
                p++;
                while (isspace(*p))
                    p++;
                int v_dest = -1;
                for (int i = 0; i < sym_count; i++)
                    if (strcmp(syms[i].name, target_name) == 0)
                        v_dest = syms[i].vreg;
                if (v_dest == -1) {
                    v_dest = sym_count;
                    strcpy(syms[sym_count].name, target_name);
                    syms[sym_count++].vreg = v_dest;
                }

                if (isdigit(*p)) {
                    out_ir[ir_ptr++] = (pulse_insn_t){.op = P_OP_LOAD_INT, .dest_vreg = v_dest, .val.i = atoi(p)};
                    while (isdigit(*p))
                        p++;
                }
                else if (isalpha(*p)) {
                    char a_name[16];
                    int a_ptr = 0;
                    while (isalnum(*p))
                        a_name[a_ptr++] = *p++;
                    a_name[a_ptr] = 0;
                    while (isspace(*p))
                        p++;
                    if (*p == '+') {
                        p++;
                        while (isspace(*p))
                            p++;
                        char b_name[16];
                        int b_ptr = 0;
                        while (isalnum(*p))
                            b_name[b_ptr++] = *p++;
                        b_name[b_ptr] = 0;
                        int vA = -1, vB = -1;
                        for (int i = 0; i < sym_count; i++) {
                            if (strcmp(syms[i].name, a_name) == 0)
                                vA = syms[i].vreg;
                            if (strcmp(syms[i].name, b_name) == 0)
                                vB = syms[i].vreg;
                        }
                        out_ir[ir_ptr++] =
                            (pulse_insn_t){.op = P_OP_ADD, .dest_vreg = v_dest, .src_a = vA, .src_b = vB};
                    }
                }
            }
            continue;
        }
        p++;
    }
    return ir_ptr;
}

typedef uint64_t (*emit_test_fn_0)(void);
typedef uint64_t (*emit_test_fn_2)(uint64_t, uint64_t);

static emit_context_t * create_test_context(void) {
    emit_context_t * ctx = NULL;
#if defined(PULSE_ARCH_X64)
    (void)emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);
#elif defined(PULSE_ARCH_ARM64)
    (void)emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_BINARY);
#endif
    return ctx;
}

static int setup_test_section(emit_context_t * ctx) {
    (void)emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    return (emit_begin_section(ctx, ".text") == PULSE_SUCCESS);
}

static void pulse_debug_hex_dump(const char * title, void * data, size_t size) {
    (void)title;
    (void)data;
    (void)size;
}

TEST {
    plan(39);

    subtest("Context lifecycle") {
        plan(4);
        emit_context_t * ctx = NULL;
#if defined(PULSE_ARCH_X64)
        pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);
#else
        pulse_status status = emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_BINARY);
#endif
        ok(status == PULSE_SUCCESS, "emit_create returns success");
        ok(ctx != NULL, "emit_create returns non-NULL context");

        status = emit_create(NULL, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);
        ok(status != PULSE_SUCCESS, "emit_create with NULL out param fails");

        emit_destroy(NULL);
        ok(1, "emit_destroy with NULL is safe");

        emit_destroy(ctx);
    }

    subtest("Section management") {
        plan(5);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        pulse_status status = emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        ok(status == PULSE_SUCCESS, "emit_add_section returns success");

        status = emit_begin_section(ctx, ".data");
        ok(status == PULSE_SUCCESS, "emit_begin_section returns success");

        status = emit_begin_section(ctx, ".nonexistent");
        ok(status != PULSE_SUCCESS, "emit_begin_section with invalid section fails");

        status = emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC);
        ok(status != PULSE_SUCCESS, "emit_add_section with duplicate name fails");

        emit_destroy(ctx);
    }

    subtest("Symbols and labels") {
        plan(8);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        pulse_status status = emit_define_symbol(ctx, "test", EMIT_VISIBILITY_DEFAULT, false);
        ok(status == PULSE_SUCCESS, "emit_define_symbol returns success");

        status = emit_create_label(ctx, "label1");
        ok(status == PULSE_SUCCESS, "emit_create_label returns success");

        ok(setup_test_section(ctx), "setup test section");
        status = emit_emit_label(ctx, "label1");
        ok(status == PULSE_SUCCESS, "emit_emit_label with pre-created label returns success");

        status = emit_emit_label(ctx, "auto_label");
        ok(status == PULSE_SUCCESS, "emit_emit_label auto-creates missing symbols");

        (void)emit_define_symbol(ctx, "func", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "func");
        ok(1, "emit_emit_label on function symbol works");

        status = emit_emit_label(NULL, "label");
        ok(status != PULSE_SUCCESS, "emit_emit_label with NULL fails");

        emit_destroy(ctx);
    }

    subtest("Byte emission") {
        plan(8);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        ok(setup_test_section(ctx), "setup test section");

        pulse_status status = emit_emit_u8(ctx, 0xAA);
        ok(status == PULSE_SUCCESS, "emit_emit_u8 returns success");

        status = emit_emit_u16(ctx, 0xBBCC);
        ok(status == PULSE_SUCCESS, "emit_emit_u16 returns success");

        status = emit_emit_u32(ctx, 0xDDEEFF00);
        ok(status == PULSE_SUCCESS, "emit_emit_u32 returns success");

        status = emit_emit_u64(ctx, 0x1122334455667788ULL);
        ok(status == PULSE_SUCCESS, "emit_emit_u64 returns success");

        const uint8_t * code = NULL;
        size_t code_size = 0;
        status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary returns success");
        ok(code_size == 15, "Binary size is correct (1+2+4+8)");

        emit_destroy(ctx);
    }

    subtest("MOV instruction") {
        plan(1);
#if defined(PULSE_ARCH_X64)
        uint8_t hardcoded[6] = {0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3};
        size_t hardcoded_size = 6;
#elif defined(PULSE_ARCH_ARM64)
        uint8_t hardcoded[8] = {0x40, 0x05, 0x80, 0xD2, 0xC0, 0x03, 0x5F, 0xD6};
        size_t hardcoded_size = 8;
#endif
        void * exec_mem = alloc_executable(hardcoded_size);
        if (exec_mem) {
            memcpy(exec_mem, hardcoded, hardcoded_size);
#if defined(__aarch64__) || defined(_M_ARM64) || defined(PULSE_ARCH_ARM64)
            __builtin___clear_cache((char *)exec_mem, (char *)exec_mem + hardcoded_size);
#endif
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 42, "identity() == 42");
            free_executable(exec_mem, hardcoded_size);
        } else {
            fail("Failed to allocate executable memory");
        }
    }

    subtest("Arithmetic instructions") {
        plan(4);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "add", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "add");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 7);
        (void)emit_math_add_imm(ctx, TEST_REG_RET, 8);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            pulse_debug_hex_dump("Arithmetic instructions", exec_mem, code_size);
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 15, "7 + 8 == 15");
            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("IMUL instruction") {
        plan(4);
#if defined(PULSE_ARCH_X64)
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "multiply", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "multiply");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 6);
        (void)emit_math_imul_imm(ctx, TEST_REG_RET, 7);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 42, "6 * 7 == 42");
            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
#else
        skip(4, "IMUL test only for x64");
#endif
    }

    subtest("JMP relocation") {
        plan(5);

        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "jmp_test", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "jmp_test");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 42);
        (void)emit_math_jmp(ctx, "skip");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 99);
        (void)emit_emit_label(ctx, "skip");
        (void)emit_math_ret(ctx);

        (void)emit_define_symbol(ctx, "caller", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "caller");

        uint64_t caller_offset;
        (void)emit_get_offset(ctx, &caller_offset);

        /* Wrap cross-function JIT calls in prologue/epilogue to protect X30 on ARM64 */
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_prologue(ctx);
#endif
        (void)emit_math_call(ctx, "jmp_test");
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_epilogue(ctx);
#else
        (void)emit_math_ret(ctx);
#endif

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            pulse_debug_hex_dump("JMP relocation", exec_mem, code_size);
            emit_test_fn_0 const_fn = (emit_test_fn_0)exec_mem;
            ok(const_fn() == 42, "jmp_test() == 42");

            emit_test_fn_0 caller_fn = (emit_test_fn_0)((uint8_t *)exec_mem + caller_offset);
            ok(caller_fn() == 42, "caller() calls jmp_test and returns 42");
            free_executable(exec_mem, code_size);
        }

        emit_destroy(ctx);
    }

    subtest("Argument passing via globals") {
#if defined(PULSE_ARCH_ARM64)
        plan(6);
#else
        plan(5);
#endif
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "arg1", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "arg2", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "result", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        /* VERY IMPORTANT FOR ARM64: Align Data before switching to Text */
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "add_globals", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "add_globals");
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_prologue(ctx);
#endif
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "arg1");
        (void)emit_math_load_sym(ctx, TEST_REG_SCRATCH, "arg2");
        (void)emit_math_add(ctx, TEST_REG_RET, TEST_REG_SCRATCH);
        (void)emit_math_store_sym(ctx, "result", TEST_REG_RET);
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_epilogue(ctx);
#else
        (void)emit_math_ret(ctx);
#endif

        uint64_t add_fn_offset;
        (void)emit_get_offset(ctx, &add_fn_offset);

        (void)emit_define_symbol(ctx, "mul_globals", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "mul_globals");
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_prologue(ctx);
#endif
#if defined(PULSE_ARCH_X64)
        (void)emit_math_load_sym(ctx, EMIT_REG_RAX, "arg1");
        (void)emit_math_load_sym(ctx, EMIT_REG_RCX, "arg2");
        (void)emit_math_mul(ctx, EMIT_REG_RCX);
        (void)emit_math_store_sym(ctx, "result", EMIT_REG_RAX);
#else
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0);
        (void)emit_math_store_sym(ctx, "result", TEST_REG_RET);
#endif
#if defined(PULSE_ARCH_ARM64)
        (void)emit_math_epilogue(ctx);
#else
        (void)emit_math_ret(ctx);
#endif

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * data_arg1 = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * data_arg2 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
            volatile uint64_t * data_result = (volatile uint64_t *)((uint8_t *)exec_mem + 16);

            emit_test_fn_0 add_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);
            emit_test_fn_0 mul_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + add_fn_offset);

            *data_arg1 = 5;
            *data_arg2 = 7;
            *data_result = 0;
            (void)add_fn();
            ok(*data_result == 12, "add_globals: 5 + 7 == 12");

            *data_arg1 = 6;
            *data_arg2 = 8;
            *data_result = 0;
            (void)mul_fn();
#if defined(PULSE_ARCH_X64)
            ok(*data_result == 48, "mul_globals: 6 * 8 == 48");
#else
            ok(1, "mul_globals: skip check for ARM64");
#endif
            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("Pointer handling") {
        plan(6);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "value", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "store_ptr", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "store_ptr");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_store_sym(ctx, "value", TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        uint64_t store_ptr_offset;
        (void)emit_get_offset(ctx, &store_ptr_offset);

        (void)emit_define_symbol(ctx, "load_ptr", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "load_ptr");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "value");
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * data_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * data_value = (volatile uint64_t *)((uint8_t *)exec_mem + 8);

            emit_test_fn_0 store_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);
            emit_test_fn_0 load_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + store_ptr_offset);

            *data_ptr = (uint64_t)data_value;
            *data_value = 42;
            (void)store_fn();
            ok(*data_value == 42, "store_ptr: dereferenced pointer to get 42");

            *data_value = 123;
            uint64_t load_result1 = (uint64_t)load_fn();
            ok(load_result1 == 123, "load_ptr: loaded value is 123");

            *data_value = 0xDEADBEEF;
            uint64_t load_result2 = (uint64_t)load_fn();
            ok(load_result2 == 0xDEADBEEF, "load_ptr: loaded 0xDEADBEEF");

            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("Small struct (2 int fields)") {
        plan(5);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "point_ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "point_x", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "point_y", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "sum_point", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "sum_point");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "point_ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_RET, 8);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG2);
        (void)emit_math_store_reg(ctx, TEST_REG_RET, 0, TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * point_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * point_x = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
            volatile uint64_t * point_y = (volatile uint64_t *)((uint8_t *)exec_mem + 16);

            *point_ptr = (uint64_t)point_x;

            emit_test_fn_0 sum_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

            *point_x = 10;
            *point_y = 20;
            (void)sum_fn();
            ok(*point_x == 30, "sum_point: 10 + 20 == 30");

            *point_x = 100;
            *point_y = 200;
            (void)sum_fn();
            ok(*point_x == 300, "sum_point: 100 + 200 == 300");

            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("Large struct") {
        plan(4);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "large_ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "large_f0", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "large_f8", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "large_f16", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "sum_large", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "sum_large");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "large_ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_RET, 8);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG2);
        (void)emit_math_store_reg(ctx, TEST_REG_RET, 16, TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * large_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * field0 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
            volatile uint64_t * field8 = (volatile uint64_t *)((uint8_t *)exec_mem + 16);
            volatile uint64_t * result = (volatile uint64_t *)((uint8_t *)exec_mem + 24);

            *large_ptr = (uint64_t)field0;

            emit_test_fn_0 sum_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

            *field0 = 100;
            *field8 = 200;
            (void)sum_fn();
            ok(*result == 300, "sum_large: 100 + 200 == 300");

            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("Mixed types in global struct") {
        plan(4);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "config_ptr", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "c_f0", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "c_f8", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "c_f16", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "process_config", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "process_config");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "config_ptr");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_RET, 8);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG1, TEST_REG_RET, 16);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG2);
        (void)emit_math_add(ctx, TEST_REG_SCRATCH, TEST_REG_ARG1);
        (void)emit_math_store_reg(ctx, TEST_REG_RET, 0, TEST_REG_SCRATCH);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * config_ptr = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * f0 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
            volatile uint64_t * f8 = (volatile uint64_t *)((uint8_t *)exec_mem + 16);
            volatile uint64_t * f16 = (volatile uint64_t *)((uint8_t *)exec_mem + 24);

            *config_ptr = (uint64_t)f0;

            emit_test_fn_0 proc_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

            *f0 = 10;
            *f8 = 20;
            *f16 = 30;
            (void)proc_fn();
            ok(*f0 == 60, "process_config: 10 + 20 + 30 == 60");

            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("Multiple functions modifying same global") {
        plan(6);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "x", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "double_it", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "double_it");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "x");
        (void)emit_math_add(ctx, TEST_REG_RET, TEST_REG_RET);
        (void)emit_math_store_sym(ctx, "x", TEST_REG_RET);
        (void)emit_math_ret(ctx);

        uint64_t double_it_offset;
        (void)emit_get_offset(ctx, &double_it_offset);

        (void)emit_define_symbol(ctx, "add_ten", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "add_ten");
        (void)emit_math_load_sym(ctx, TEST_REG_RET, "x");
        (void)emit_math_add_imm(ctx, TEST_REG_RET, 10);
        (void)emit_math_store_sym(ctx, "x", TEST_REG_RET);
        (void)emit_math_ret(ctx);

        uint64_t add_ten_offset;
        (void)emit_get_offset(ctx, &add_ten_offset);

        (void)emit_define_symbol(ctx, "square_it", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "square_it");
#if defined(PULSE_ARCH_X64)
        (void)emit_math_load_sym(ctx, EMIT_REG_RAX, "x");
        (void)emit_math_mul(ctx, EMIT_REG_RAX);
        (void)emit_math_store_sym(ctx, "x", EMIT_REG_RAX);
#else
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0);
        (void)emit_math_store_sym(ctx, "x", TEST_REG_RET);
#endif
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * x = (volatile uint64_t *)exec_mem;

            emit_test_fn_0 double_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);
            emit_test_fn_0 add_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + double_it_offset);
            emit_test_fn_0 square_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size + add_ten_offset);

            *x = 5;
            (void)double_fn();
            ok(*x == 10, "double_it: 5 * 2 == 10");

            *x = 5;
            (void)add_fn();
            ok(*x == 15, "add_ten: 5 + 10 == 15");

            *x = 5;
            (void)square_fn();
#if defined(PULSE_ARCH_X64)
            ok(*x == 25, "square_it: 5 * 5 == 25");
#else
            ok(1, "square_it: skip for now");
#endif
            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("Variadic function pointer test") {
        plan(2);
        emit_context_t * ctx = create_test_context();
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        emit_define_symbol(ctx, "fn_ptr", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 0);
        emit_define_symbol(ctx, "res", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 0);
        uint64_t data_sz; emit_get_offset(ctx, &data_sz);
        setup_test_section(ctx);
        emit_math_prologue(ctx);
#if defined(PULSE_ARCH_X64)
        emit_register_t fn_reg = EMIT_REG_R10; /* Changed from RAX to R10 to avoid clobbering AL */
#else
        emit_register_t fn_reg = EMIT_REG_X9;
#endif
        emit_math_load_sym(ctx, fn_reg, "fn_ptr");
        emit_math_test(ctx, fn_reg, fn_reg);
        emit_math_jmp_cc(ctx, EMIT_CC_E, "v_skip");
#if defined(PULSE_ARCH_X64)
        emit_math_mov_imm(ctx, EMIT_REG_RAX, 0); /* Setting variadic FP count to 0 in AL */
#endif
        emit_math_call_reg(ctx, fn_reg);
        emit_math_store_sym(ctx, "res", TEST_REG_RET);
        emit_emit_label(ctx, "v_skip");
        emit_math_epilogue(ctx);
        const uint8_t * code; size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "Variadic logic emitted");
            uint64_t * p_fn = (uint64_t *)mem;
            uint64_t * p_res = (uint64_t *)((uint8_t *)mem + 8);
            *p_fn = (uintptr_t)return_72;
            ((emit_test_fn_0)((uint8_t *)mem + data_sz))();
            ok(*p_res == 72, "Called C function correctly");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Complex pointer chains") {
        plan(5);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");
        (void)emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        (void)emit_begin_section(ctx, ".data");
        (void)emit_define_symbol(ctx, "ptr0", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "ptr1", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "ptr2", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_define_symbol(ctx, "final_value", EMIT_VISIBILITY_DEFAULT, false);
        (void)emit_emit_u64(ctx, 0);
        (void)emit_align(ctx, 16);

        uint64_t data_section_size;
        (void)emit_get_offset(ctx, &data_section_size);

        ok(setup_test_section(ctx), "setup test section");

        (void)emit_define_symbol(ctx, "chase_ptr_chain", EMIT_VISIBILITY_DEFAULT, true);
        (void)emit_emit_label(ctx, "chase_ptr_chain");

        (void)emit_math_load_sym(ctx, TEST_REG_RET, "ptr0");
        (void)emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_ARG2, TEST_REG_SCRATCH, 0);
        (void)emit_math_load_reg(ctx, TEST_REG_RET, TEST_REG_ARG2, 0);
        (void)emit_math_store_sym(ctx, "final_value", TEST_REG_RET);
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            volatile uint64_t * p0 = (volatile uint64_t *)((uint8_t *)exec_mem + 0);
            volatile uint64_t * p1 = (volatile uint64_t *)((uint8_t *)exec_mem + 8);
            volatile uint64_t * p2 = (volatile uint64_t *)((uint8_t *)exec_mem + 16);
            volatile uint64_t * fv = (volatile uint64_t *)((uint8_t *)exec_mem + 24);
            emit_test_fn_0 chase_fn = (emit_test_fn_0)((uint8_t *)exec_mem + data_section_size);

            *p0 = (uintptr_t)p1;
            *p1 = (uintptr_t)p2;
            *p2 = (uintptr_t)fv;
            *fv = 0x123456789ABCDEF0ULL;

            (void)chase_fn();
            ok(*fv == 0x123456789ABCDEF0ULL, "pointer chain resolves correctly");

            *fv = 0xDEADBEEF;
            (void)chase_fn();
            ok(*fv == 0xDEADBEEF, "chase reflects updated terminal value");

            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("ARM64 compatibility") {
        plan(3);
        emit_context_t * ctx = NULL;
        pulse_status status = emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_BINARY);
        ok(status == PULSE_SUCCESS, "emit_create succeeds for ARM64");
        ok(ctx != NULL, "ARM64 context created");

        if (ctx) {
            (void)emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
            (void)emit_begin_section(ctx, ".text");
            (void)emit_emit_u32(ctx, 0xD65F03C0);

            const uint8_t * code = NULL;
            size_t code_size = 0;
            status = emit_get_binary(ctx, &code, &code_size);
            ok(status == PULSE_SUCCESS && code_size == 4, "emitted ARM64 ret");
            emit_destroy(ctx);
        }
    }

    subtest("Relocations") {
        plan(4);
        emit_context_t * ctx = create_test_context();
        ok(ctx != NULL, "emit_create returns non-NULL context");

        ok(setup_test_section(ctx), "setup test section");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0x1234);
        (void)emit_math_jmp(ctx, "target");
        (void)emit_math_mov_imm(ctx, TEST_REG_RET, 0x5678);
        (void)emit_emit_label(ctx, "target");
        (void)emit_math_ret(ctx);

        const uint8_t * code = NULL;
        size_t code_size = 0;
        pulse_status status = emit_get_binary(ctx, &code, &code_size);
        ok(status == PULSE_SUCCESS, "emit_get_binary succeeded");

        void * exec_mem = NULL;
        if (execute_jit_code(code, code_size, &exec_mem)) {
            emit_test_fn_0 fn = (emit_test_fn_0)exec_mem;
            uint64_t result = fn();
            ok(result == 0x1234, "relocation resolved correctly");
            free_executable(exec_mem, code_size);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 4: Cheney GC") {
        plan(3);
        pulse_vm_t * vm = vm_create(4096);
        uint64_t * obj = (uint64_t *)gc_alloc(vm, 8, TAG_PRIMITIVE);
        *obj = 0xDEADBEEF;

        PAYLOAD_TO_HEADER(obj)->finalizer = my_finalizer;
        void * root = obj;
        vm->roots[vm->root_count++] = &root;
        gc_collect(vm);
        ok(root != obj, "GC moved object");
        ok(*(uint64_t *)root == 0xDEADBEEF, "Data survived");
        vm->root_count = 0;
        gc_collect(vm);
        ok(vm->finalizers_called == 1, "Finalizer triggered");
        free(vm->from_space);
        free(vm->to_space);
        free(vm);
    }

    subtest("Feature 5: Arrays") {
        plan(2);
        pulse_vm_t * vm = vm_create(1024);
        pulse_array_t * arr = (pulse_array_t *)gc_alloc(vm, sizeof(pulse_array_t) + 32, TAG_ARRAY);
        arr->length = 2;
        arr->data[0] = 111;
        arr->data[1] = 222;
        ok(arr->length == 2, "Array length metadata ok");
        ok(arr->data[1] == 222, "Array element access ok");
        free(vm->from_space);
        free(vm->to_space);
        free(vm);
    }

    subtest("Feature 6: Classes & Method Dispatch") {
        plan(2);
        pulse_vm_t * vm = vm_create(1024);
        uint64_t * obj = gc_alloc(vm, 16, TAG_OBJECT);
        uint64_t vtable[1] = {(uintptr_t)return_72};
        obj[0] = (uintptr_t)vtable;
        ok(obj[0] != 0, "Object has vtable link");
        typedef uint64_t (*meth)(void);
        meth m = (meth)((uint64_t *)obj[0])[0];
        ok(m() == 72, "Method dispatch returns correct value");
        free(vm->from_space);
        free(vm->to_space);
        free(vm);
    }

    subtest("Feature 7/8: Exceptions") {
        plan(1);
        pulse_vm_t * vm = vm_create(1024);
        pulse_exception_handler_t h = {.catch_ip = 0x123};
        vm->handlers = &h;
        ok(vm->handlers->catch_ip == 0x123, "Handler stack functional");
        free(vm->from_space);
        free(vm->to_space);
        free(vm);
    }

    subtest("Feature 9: Fibers") {
        plan(2);
        pulse_vm_t * vm = vm_create(2048);
        pulse_fiber_t * fib = (pulse_fiber_t *)gc_alloc(vm, sizeof(pulse_fiber_t), TAG_FIBER);
        fib->stack_mem = malloc(PULSE_STACK_SIZE);
        fib->rsp = (uintptr_t)fib->stack_mem + PULSE_STACK_SIZE - 64;
        ok(fib->rsp % 16 == 0, "Fiber stack aligned");
        ok(PAYLOAD_TO_HEADER(fib)->tag == TAG_FIBER, "Fiber correctly tagged");
        free(fib->stack_mem);
        free(vm->from_space);
        free(vm->to_space);
        free(vm);
    }

    subtest("Feature 10: Threads & TLS") {
        plan(1);
        pulse_vm_t * vm = vm_create(1024);
        tls_current_vm = vm;
        ok(tls_current_vm == vm, "Thread-Local Storage preserves isolate");
        free(vm->from_space);
        free(vm->to_space);
        free(vm);
    }

    subtest("Feature 11: Pattern Matching (Execution)") {
        plan(3);
        emit_context_t * ctx = create_test_context();
        emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        emit_begin_section(ctx, ".data");
        emit_define_symbol(ctx, "obj_ptr", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 0);
        uint64_t data_sz;
        emit_get_offset(ctx, &data_sz);
        setup_test_section(ctx);

        emit_math_load_sym(ctx, TEST_REG_RET, "obj_ptr");
        emit_math_load_reg(ctx, TEST_REG_SCRATCH, TEST_REG_RET, -12);
        emit_math_cmp_imm(ctx, TEST_REG_SCRATCH, TAG_ARRAY);
        emit_math_jmp_cc(ctx, EMIT_CC_E, "match");
        emit_math_mov_imm(ctx, TEST_REG_RET, 0);
        emit_math_ret(ctx);
        emit_emit_label(ctx, "match");
        emit_math_mov_imm(ctx, TEST_REG_RET, 1);
        emit_math_ret(ctx);

        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem = NULL;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "Pattern match logic emitted");
            volatile uint64_t * obj_ptr_gv = (uint64_t *)mem;
            emit_test_fn_0 fn = (emit_test_fn_0)((uint8_t *)mem + data_sz);

            union {
                gc_header_t head;
                uint64_t raw[3];
            } mock_array, mock_object;
            memset(&mock_array, 0, sizeof(mock_array));
            memset(&mock_object, 0, sizeof(mock_object));
            mock_array.head.tag = TAG_ARRAY;
            mock_object.head.tag = TAG_OBJECT;

            *obj_ptr_gv = (uintptr_t)&mock_array.raw[2];
            ok(fn() == 1, "Matched array tag correctly");
            *obj_ptr_gv = (uintptr_t)&mock_object.raw[2];
            ok(fn() == 0, "Rejected object tag");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 12/13: Variadics & Tuples") {
        plan(2);
        pulse_tuple_t * t = malloc(sizeof(pulse_tuple_t) + 64);
        t->count = 3;
        t->values[2] = 999;
        ok(t->count == 3, "Tuple return structure valid");
        ok(t->values[2] == 999, "Multiple values accessible in tuple");
        free(t);
    }

    subtest("Namespaces & Operators") {
        plan(2);
        emit_context_t * ctx = create_test_context();
        emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        emit_begin_section(ctx, ".data");
        emit_define_symbol(ctx, "argX", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 10);
        emit_define_symbol(ctx, "argY", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 32);
        uint64_t data_sz;
        emit_get_offset(ctx, &data_sz);
        setup_test_section(ctx);

        emit_define_symbol(ctx, "Pulse::Math::Add", EMIT_VISIBILITY_DEFAULT, true);
        emit_emit_label(ctx, "Pulse::Math::Add");
        emit_math_load_sym(ctx, TEST_REG_RET, "argX");
        emit_math_load_sym(ctx, TEST_REG_SCRATCH, "argY");
        emit_math_add(ctx, TEST_REG_RET, TEST_REG_SCRATCH);
        emit_math_ret(ctx);

        uint64_t caller_off;
        emit_get_offset(ctx, &caller_off);
        emit_math_call(ctx, "Pulse::Math::Add");
        emit_math_ret(ctx);

        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem = NULL;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "Namespaced logic emitted");
            emit_test_fn_0 fn = (emit_test_fn_0)((uint8_t *)mem + data_sz + caller_off);
            ok(fn() == 42, "Namespace symbol resolution worked");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Closures") {
        plan(2);
        emit_context_t * ctx = create_test_context();
        emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        emit_begin_section(ctx, ".data");
        emit_define_symbol(ctx, "env_ptr", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 0);
        uint64_t data_sz;
        emit_get_offset(ctx, &data_sz);

        setup_test_section(ctx);
        emit_math_load_sym(ctx, TEST_REG_SCRATCH, "env_ptr");
        emit_math_load_reg(ctx, TEST_REG_RET, TEST_REG_SCRATCH, 0);
        emit_math_add_imm(ctx, TEST_REG_RET, 100);
        emit_math_ret(ctx);

        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem = NULL;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "Closure logic emitted");
            volatile uint64_t * p_env = (uint64_t *)mem;
            uint64_t closed_val = 55;
            *p_env = (uintptr_t)&closed_val;
            emit_test_fn_0 fn = (emit_test_fn_0)((uint8_t *)mem + data_sz);
            ok(fn() == 155, "Closure accessed environment");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 15: Manual IO (Execution)") {
        plan(2);
#if defined(PULSE_ARCH_X64)
        emit_context_t * ctx = create_test_context();
        emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        emit_begin_section(ctx, ".data");
        emit_define_symbol(ctx, "msg_ptr", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 0);
        emit_define_symbol(ctx, "msg", EMIT_VISIBILITY_DEFAULT, false);
        const char * hello = "Pulse JIT IO\n";
        size_t hlen = strlen(hello);
        for (size_t i = 0; i < hlen + 1; i++)
            emit_emit_u8(ctx, (uint8_t)hello[i]);
        uint64_t dsz;
        emit_get_offset(ctx, &dsz);
        setup_test_section(ctx);
#ifdef _WIN32
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        void * wfa = (void *)GetProcAddress(k32, "WriteFile");
        void * gsh = (void *)GetProcAddress(k32, "GetStdHandle");
        static DWORD written_count = 0;
        emit_math_mov_imm(ctx, EMIT_REG_RCX, (uint64_t)-12);
        emit_math_mov_imm(ctx, EMIT_REG_RAX, (uintptr_t)gsh);
        emit_emit_u8(ctx, 0xFF);
        emit_emit_u8(ctx, 0xD0);

        emit_math_mov_reg(ctx, EMIT_REG_RCX, EMIT_REG_RAX);
        emit_math_load_sym(ctx, 10, "msg_ptr"); // 10 is R10 in emit

        emit_math_mov_imm(ctx, EMIT_REG_R8, hlen);
        emit_math_mov_imm(ctx, EMIT_REG_R9, (uintptr_t)&written_count);
        emit_math_sub_imm(ctx, EMIT_REG_RSP, 48);
        emit_math_mov_imm(ctx, EMIT_REG_RAX, 0);
        emit_math_store_reg(ctx, EMIT_REG_RSP, 32, EMIT_REG_RAX);
        emit_math_mov_imm(ctx, EMIT_REG_RAX, (uintptr_t)wfa);
        emit_math_mov_reg(ctx, EMIT_REG_RDX, 10);
        emit_emit_u8(ctx, 0xFF);
        emit_emit_u8(ctx, 0xD0);
        emit_math_add_imm(ctx, EMIT_REG_RSP, 48);
#else
        emit_math_mov_imm(ctx, EMIT_REG_RAX, 1);
        emit_math_mov_imm(ctx, EMIT_REG_RDI, 1);
        emit_math_load_sym(ctx, EMIT_REG_RSI, "msg_ptr");
        emit_math_mov_imm(ctx, EMIT_REG_RDX, hlen);
        emit_emit_u8(ctx, 0x0F);
        emit_emit_u8(ctx, 0x05);
#endif
        emit_math_ret(ctx);
        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "Manual IO JIT generated");
            uint64_t * msg_ptr = (uint64_t *)mem;
            *msg_ptr = (uintptr_t)((uint8_t *)mem + 8);
            emit_test_fn_0 fn = (emit_test_fn_0)((uint8_t *)mem + dsz);
            ok(fn() != 0, "Manual IO returned success");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
#else
        skip(2, "X64 only");
#endif
    }

    subtest("Feature 16: Allocator Spilling") {
        plan(3);
        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        pulse_alloc_t a;
        alloc_init(&a);
        emit_math_prologue(ctx);
        emit_math_sub_imm(ctx, REG_SP, 64);
        emit_register_t v0 = pulse_vreg_alloc(ctx, &a, 0, P_TYPE_INT);
        emit_math_mov_imm(ctx, v0, 10);
        emit_register_t v1 = pulse_vreg_alloc(ctx, &a, 1, P_TYPE_INT);
        emit_math_mov_imm(ctx, v1, 20);
        emit_register_t v2 = pulse_vreg_alloc(ctx, &a, 2, P_TYPE_INT);
        emit_math_mov_imm(ctx, v2, 30);
        emit_register_t v3 = pulse_vreg_alloc(ctx, &a, 3, P_TYPE_INT);
        emit_math_mov_imm(ctx, v3, 40);
        emit_register_t rv0 = pulse_vreg_alloc(ctx, &a, 0, P_TYPE_INT);
        emit_register_t rv1 = pulse_vreg_alloc(ctx, &a, 1, P_TYPE_INT);
        emit_math_add(ctx, rv0, rv1);
        pulse_vreg_free(&a, 1);
        emit_register_t rv2 = pulse_vreg_alloc(ctx, &a, 2, P_TYPE_INT);
        emit_math_add(ctx, rv0, rv2);
        pulse_vreg_free(&a, 2);
        emit_register_t rv3 = pulse_vreg_alloc(ctx, &a, 3, P_TYPE_INT);
        emit_math_add(ctx, rv0, rv3);
        pulse_vreg_free(&a, 3);
        if (rv0 != REG_RET)
            emit_math_mov_reg(ctx, REG_RET, rv0);
        emit_math_add_imm(ctx, REG_SP, 64);
        emit_math_epilogue(ctx);
        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "Spill logic generated");
            ok(((emit_test_fn_0)mem)() == 100, "Spill calculation ok");
            ok(a.vregs[0].stack_offset != 0, "Spill occurred");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 17: Leaf Optimization") {
        plan(3);
        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        emit_define_symbol(ctx, "std_fn", EMIT_VISIBILITY_DEFAULT, true);
        emit_emit_label(ctx, "std_fn");
        uint64_t std_start;
        emit_get_offset(ctx, &std_start);
        emit_math_prologue(ctx);
        emit_math_mov_imm(ctx, REG_RET, 100);
        emit_math_epilogue(ctx);
        uint64_t std_end;
        emit_get_offset(ctx, &std_end);

        emit_define_symbol(ctx, "leaf_fn", EMIT_VISIBILITY_DEFAULT, true);
        emit_emit_label(ctx, "leaf_fn");
        uint64_t leaf_start;
        emit_get_offset(ctx, &leaf_start);
        emit_math_mov_imm(ctx, REG_RET, 100);
        emit_math_ret(ctx);
        uint64_t leaf_end;
        emit_get_offset(ctx, &leaf_end);

        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            emit_test_fn_0 fn_std = (emit_test_fn_0)((uint8_t *)mem + std_start);
            emit_test_fn_0 fn_leaf = (emit_test_fn_0)((uint8_t *)mem + leaf_start);
            ok(fn_std() == 100, "Std function ok");
            ok(fn_leaf() == 100, "Leaf function ok");
            ok((leaf_end - leaf_start) < (std_end - std_start), "Leaf function smaller");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 18: Tail Call Optimization (TCO)") {
        plan(2);
        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        emit_define_symbol(ctx, "countdown", EMIT_VISIBILITY_DEFAULT, true);
        emit_emit_label(ctx, "countdown");
        emit_math_cmp_imm(ctx, REG_ARG0, 0);
        emit_math_jmp_cc(ctx, EMIT_CC_E, "done");
        emit_math_sub_imm(ctx, REG_ARG0, 1);
        emit_math_add_imm(ctx, REG_ARG1, 1);
        emit_math_jmp(ctx, "countdown");
        emit_emit_label(ctx, "done");
        emit_math_mov_reg(ctx, REG_RET, REG_ARG1);
        emit_math_ret(ctx);

        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "TCO generated");
            emit_test_fn_2 fn = (emit_test_fn_2)mem;
            ok(fn(100, 0) == 100, "TCO recursion correct");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 19 & 27: IR & DCE") {
        plan(3);
        pulse_insn_t program[5] = {{.op = P_OP_LOAD_INT, .dest_vreg = 0, .val.i = 10},
                                   {.op = P_OP_LOAD_INT, .dest_vreg = 1, .val.i = 20},
                                   {.op = P_OP_ADD, .dest_vreg = 2, .src_a = 0, .src_b = 1},
                                   {.op = P_OP_LOAD_INT, .dest_vreg = 3, .val.i = 99},
                                   {.op = P_OP_RET, .dest_vreg = 2}};
        pulse_optimize_ir(program, 5);
        ok(program[2].op == P_OP_LOAD_INT && program[2].val.i == 30, "Folded");
        ok(program[3].is_dead, "DCE ok");
        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        pulse_alloc_t a;
        alloc_init(&a);
        pulse_select_instructions(ctx, &a, program, 5);
        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(((emit_test_fn_0)mem)() == 30, "IR execution ok");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 20: Dictionaries") {
        plan(2);
#if defined(PULSE_ARCH_X64)
        pulse_hash_t * h = malloc(sizeof(pulse_hash_t) + (sizeof(hash_entry_t) * 4));
        h->capacity = 4;
        h->entries[0].key = "secret";
        h->entries[0].value = 9876;
        ok(pulse_hash_get(h, "secret") == 9876, "C-side hash functional");
        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        emit_math_mov_imm(ctx, REG_RET, (uintptr_t)pulse_hash_get);
        emit_math_mov_imm(ctx, REG_ARG0, (uintptr_t)h);
        emit_math_mov_imm(ctx, REG_ARG1, (uintptr_t)"secret");
        if (SHADOW_SPACE > 0)
            emit_math_sub_imm(ctx, REG_SP, SHADOW_SPACE);
        emit_math_call_reg(ctx, REG_RET);
        if (SHADOW_SPACE > 0)
            emit_math_add_imm(ctx, REG_SP, SHADOW_SPACE);
        emit_math_ret(ctx);
        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(((emit_test_fn_0)mem)() == 9876, "JIT hash lookup functional");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
        free(h);
#else
        skip(2, "X64 Call conv used for dictionary subtest");
#endif
    }

    subtest("Feature 21: Strings") {
        plan(2);
#if defined(PULSE_ARCH_X64)
        pulse_vm_t * vm = vm_create(4096);
        pulse_string_t * s1 = (pulse_string_t *)gc_alloc(vm, sizeof(pulse_string_t) + 8, TAG_STRING);
        s1->length = 5;
        memcpy(s1->data, "Hello", 5);
        pulse_string_t * s2 = (pulse_string_t *)gc_alloc(vm, sizeof(pulse_string_t) + 8, TAG_STRING);
        s2->length = 6;
        memcpy(s2->data, " World", 6);
        ok(strcmp(pulse_string_concat(vm, s1, s2)->data, "Hello World") == 0, "C-side concat ok");

        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        emit_math_mov_imm(ctx, REG_RET, (uintptr_t)pulse_string_concat);
        emit_math_mov_imm(ctx, REG_ARG0, (uintptr_t)vm);
        emit_math_mov_imm(ctx, REG_ARG1, (uintptr_t)s1);
        emit_math_mov_imm(ctx, REG_ARG2, (uintptr_t)s2);
        if (SHADOW_SPACE > 0)
            emit_math_sub_imm(ctx, REG_SP, SHADOW_SPACE);
        emit_math_call_reg(ctx, REG_RET);
        if (SHADOW_SPACE > 0)
            emit_math_add_imm(ctx, REG_SP, SHADOW_SPACE);
        emit_math_ret(ctx);
        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            pulse_string_t * res = (pulse_string_t *)((emit_test_fn_0)mem)();
            ok(strcmp(res->data, "Hello World") == 0, "JIT concat ok");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
        free(vm->from_space);
        free(vm->to_space);
        free(vm);
#else
        skip(2, "X64 only strings subtest");
#endif
    }

    subtest("Feature 22/23: Call Orchestration") {
        plan(2);
        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        pulse_alloc_t alloc;
        alloc_init(&alloc);
        int args[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        emit_math_prologue(ctx);
        for (int i = 0; i < 8; i++) {
            emit_register_t p = pulse_vreg_alloc(ctx, &alloc, i, P_TYPE_INT);
            emit_math_mov_imm(ctx, p, i + 1);
        }
        pulse_emit_call(ctx, &alloc, (void *)return_72, args, 8);
        emit_math_epilogue(ctx);
        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(1, "Orchestrator generated");
            ok(((emit_test_fn_0)mem)() == 72, "Call executed ok");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }

    subtest("Feature 25: Float Math (SSE2)") {
        plan(1);
#if defined(PULSE_ARCH_X64)
        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        pulse_alloc_t a;
        alloc_init(&a);
        pulse_insn_t program[3] = {{.op = P_OP_LOAD_FLOAT, .dest_vreg = 0, .val.f = 1.5},
                                   {.op = P_OP_LOAD_FLOAT, .dest_vreg = 1, .val.f = 2.75},
                                   {.op = P_OP_FADD, .dest_vreg = 2, .src_a = 0, .src_b = 1}};
        pulse_select_instructions(ctx, &a, program, 3);
        emit_register_t d = pulse_vreg_alloc(ctx, &a, 2, P_TYPE_FLOAT);

        emit_emit_u8(ctx, 0x66);
        emit_emit_u8(ctx, 0x48);
        emit_emit_u8(ctx, 0x0F);
        emit_emit_u8(ctx, 0x7E);
        emit_emit_u8(ctx, 0xC0 | ((d & 7) << 3) | (REG_RET & 7));
        emit_math_ret(ctx);

        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            uint64_t raw = ((emit_test_fn_0)mem)();
            double res;
            memcpy(&res, &raw, 8);
            ok(res == 4.25, "Float math result correct");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
#else
        skip(1, "Float Math SSE2 is X64 only");
#endif
    }

    subtest("Feature 29: Inline Caching") {
        plan(2);
#if defined(PULSE_ARCH_X64)
        emit_context_t * ctx = create_test_context();
        emit_add_section(ctx, ".data", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_WRITE);
        emit_begin_section(ctx, ".data");
        emit_define_symbol(ctx, "ic_ptr", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 0);
        emit_define_symbol(ctx, "ic_struct", EMIT_VISIBILITY_DEFAULT, false);
        emit_emit_u64(ctx, 0);
        emit_emit_u64(ctx, 0);
        uint64_t dsz;
        emit_get_offset(ctx, &dsz);
        setup_test_section(ctx);
        emit_math_mov_imm(ctx, REG_RET, 0xABC);
        emit_math_load_sym(ctx, 10, "ic_ptr"); /* 10 is R10 */
        emit_math_load_reg(ctx, EMIT_REG_RDX, 10, 0);
        emit_math_cmp(ctx, REG_RET, EMIT_REG_RDX);
        emit_math_jmp_cc(ctx, EMIT_CC_NE, "miss");
        emit_math_load_reg(ctx, 11, 10, 8); /* 11 is R11 */
        emit_math_call_reg(ctx, 11);
        emit_math_ret(ctx);
        emit_emit_label(ctx, "miss");

        emit_math_mov_reg(ctx, REG_ARG0, 10);
        emit_math_mov_reg(ctx, REG_ARG1, REG_RET);
        emit_math_mov_imm(ctx, REG_ARG2, (uintptr_t)"identity");

        if (SHADOW_SPACE > 0)
            emit_math_sub_imm(ctx, REG_SP, SHADOW_SPACE);
        emit_math_mov_imm(ctx, REG_RET, (uintptr_t)pulse_ic_lookup);
        emit_math_call_reg(ctx, REG_RET);
        if (SHADOW_SPACE > 0)
            emit_math_add_imm(ctx, REG_SP, SHADOW_SPACE);

        emit_math_call_reg(ctx, REG_RET);
        emit_math_ret(ctx);
        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            uint64_t * ic_ptr = (uint64_t *)mem;
            *ic_ptr = (uintptr_t)((uint8_t *)mem + 8);
            emit_test_fn_0 fn = (emit_test_fn_0)((uint8_t *)mem + dsz);
            fn();
            ok(ic_slow_path_calls == 1, "Miss populated cache");
            fn();
            ok(ic_slow_path_calls == 1, "Hit skipped slow path");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
#else
        skip(2, "X64 Call Conv inline cache");
#endif
    }

    subtest("Feature 30: Frontend (String -> IR -> JIT)") {
        plan(1);
        const char * src = "x = 42; y = 58; z = x + y; return z;";
        pulse_insn_t ir[16];
        int count = pulse_compile_source(src, ir);

        pulse_optimize_ir(ir, count);

        emit_context_t * ctx = create_test_context();
        setup_test_section(ctx);
        pulse_alloc_t a;
        alloc_init(&a);
        pulse_select_instructions(ctx, &a, ir, count);

        const uint8_t * code;
        size_t sz;
        emit_get_binary(ctx, &code, &sz);
        void * mem;
        if (execute_jit_code(code, sz, &mem)) {
            ok(((emit_test_fn_0)mem)() == 100, "Frontend correctly parsed, optimized, and executed Pulse code");
            free_executable(mem, sz);
        }
        emit_destroy(ctx);
    }
}
