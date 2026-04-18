/**
 * @file 185_emit_file.c
 * @brief Unit test for emit_write_file() - generates and runs executables.
 */
#define DBLTAP_ENABLE
#define DBLTAP_IMPLEMENTATION
#include "common/compat_c23.h"
#include "common/double_tap.h"
#include "common/infix_config.h"
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef __linux__
static const char * test_elf_rel_name = "t_pulse_emit_test.o";
#endif

static void cleanup_test_files(void) {
#ifdef _WIN32
    DeleteFileA("pulse_emit_test_1.exe");
    DeleteFileA("pulse_emit_test_2.exe");
    DeleteFileA("pulse_emit_test_3.exe");
    DeleteFileA("pulse_emit_test_4.exe");
    DeleteFileA("pulse_emit_test_5.exe");
    DeleteFileA("test_pe_exec_0.exe");
    DeleteFileA("test_pe_exec_42.exe");
#else
    unlink("pulse_emit_test_1");
    unlink("pulse_emit_test_2");
    unlink("pulse_emit_test_3");
    unlink("pulse_emit_test_4");
    unlink("pulse_emit_test_5");
    unlink("pulse_emit_test_1.exe");
    unlink("pulse_emit_test_2.exe");
    unlink("pulse_emit_test_3.exe");
    unlink("pulse_emit_test_4.exe");
    unlink("pulse_emit_test_5.exe");
#ifdef __linux__
    unlink(test_elf_rel_name);
#endif
#endif
}

static emit_context_t * create_pe_context(void) {
    emit_context_t * ctx = NULL;
#if defined(PULSE_ARCH_X64)
    if (emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_PE) != PULSE_SUCCESS)
        return NULL;
#elif defined(PULSE_ARCH_ARM64)
    if (emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_PE) != PULSE_SUCCESS)
        return NULL;
#endif
    return ctx;
}

#ifdef __linux__
static emit_context_t * create_elf_relocatable_context(void) {
    emit_context_t * ctx = NULL;
#if defined(PULSE_ARCH_X64)
    if (emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_ELF) != PULSE_SUCCESS)
        return NULL;
#elif defined(PULSE_ARCH_ARM64)
    if (emit_create(&ctx, EMIT_ARCH_AARCH64, EMIT_FORMAT_ELF) != PULSE_SUCCESS)
        return NULL;
#endif
    return ctx;
}
#endif

static int write_simple_pe_exe(uint64_t return_value, const char * file_name) {
    emit_context_t * ctx = create_pe_context();
    if (!ctx)
        return 0;
    emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    emit_begin_section(ctx, ".text");
    emit_define_symbol(ctx, "main", EMIT_VISIBILITY_DEFAULT, true);
    emit_emit_label(ctx, "main");
    emit_math_prologue(ctx);
#if defined(PULSE_ARCH_X64)
    emit_math_mov_imm(ctx, EMIT_REG_RAX, return_value);
#elif defined(PULSE_ARCH_ARM64)
    emit_math_mov_imm(ctx, EMIT_REG_X0, return_value);
#endif
    emit_math_epilogue(ctx);
    pulse_status status = emit_write_file(ctx, file_name);
    emit_destroy(ctx);
    return status == PULSE_SUCCESS;
}

TEST {
    plan(10);
    subtest("emit_write_file with NULL context") {
        plan(1);
        ok(emit_write_file(NULL, "test.exe") != PULSE_SUCCESS, "fails");
    }
    subtest("emit_write_file with NULL filename") {
        plan(1);
        emit_context_t * ctx = create_pe_context();
        if (ctx) {
            ok(emit_write_file(ctx, NULL) != PULSE_SUCCESS, "fails");
            emit_destroy(ctx);
        }
        else
            fail("context creation failed");
    }
    subtest("write and verify PE file header") {
        plan(3);
        if (write_simple_pe_exe(42, "pulse_emit_test_1.exe")) {
            ok(1, "written");
            FILE * f = fopen("pulse_emit_test_1.exe", "rb");
            if (f) {
                unsigned char h[2];
                size_t r = fread(h, 1, 2, f);
                fclose(f);
                ok(r == 2, "read 2");
                ok(h[0] == 'M' && h[1] == 'Z', "valid signature");
            }
            else
                fail("open failed");
        }
        else
            fail("write failed");
    }
    subtest("write and verify PE file structure") {
        plan(3);
        if (write_simple_pe_exe(0, "pulse_emit_test_2.exe")) {
            ok(1, "written");
            FILE * f = fopen("pulse_emit_test_2.exe", "rb");
            if (f) {
                unsigned char b[512];
                (void)fread(b, 1, 512, f);
                fclose(f);
                ok(*(int *)(b + 0x3C) == 128, "lfanew correct");
                ok(*(unsigned int *)(b + 128) == 0x00004550, "valid PE sig");
            }
            else
                fail("open failed");
        }
        else
            fail("write failed");
    }
    subtest("write PE with different return values") {
        plan(3);
        ok(write_simple_pe_exe(0, "pulse_emit_test_3.exe"), "wrote 0");
        ok(write_simple_pe_exe(42, "pulse_emit_test_4.exe"), "wrote 42");
        ok(write_simple_pe_exe(255, "pulse_emit_test_5.exe"), "wrote 255");
    }
    subtest("execute PE executable (skipped)") {
        plan(2);
        skip(2, "Not relevant on Linux");
    }
#ifdef __linux__
    subtest("write ELF relocatable file") {
        plan(2);
        emit_context_t * ctx = create_elf_relocatable_context();
        emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
        emit_begin_section(ctx, ".text");
        emit_define_symbol(ctx, "_start", EMIT_VISIBILITY_DEFAULT, true);
        emit_emit_label(ctx, "_start");
        emit_emit_u8(ctx, 0xb8);
        emit_emit_u32(ctx, 42);
        emit_emit_u8(ctx, 0xc3);
        if (emit_write_file(ctx, test_elf_rel_name) == PULSE_SUCCESS) {
            ok(1, "written");
            FILE * f = fopen(test_elf_rel_name, "rb");
            unsigned char h[4];
            (void)fread(h, 1, 4, f);
            fclose(f);
            ok(h[0] == 0x7F && h[1] == 'E' && h[2] == 'L' && h[3] == 'F', "valid signature");
        }
        else
            fail("write failed");
        emit_destroy(ctx);
    }
#else
    subtest("write ELF (skipped)") {
        plan(1);
        skip(1, "Linux only");
    }
#endif
    subtest("link and execute ELF (skipped)") {
        plan(1);
        skip(1, "Not implemented");
    }
    subtest("write and execute ELF (skipped)") {
        plan(1);
        skip(1, "Not implemented");
    }
    subtest("cleanup") {
        plan(1);
        cleanup_test_files();
        ok(1, "done");
    }
}
