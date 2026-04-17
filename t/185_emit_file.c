/**
 * @file 185_emit_file.c
 * @brief Unit test for emit_write_file() - generates and runs executables.
 * @ingroup test_suite
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
#define EXECUTABLE_EXT ".exe"
#else
#include <sys/mman.h>
#include <unistd.h>
#define EXECUTABLE_EXT ""
#endif

static const char * test_exe_name = "pulse_emit_test" EXECUTABLE_EXT;

static void cleanup_test_exe(void) {
#ifdef _WIN32
    DeleteFileA(test_exe_name);
#else
    unlink(test_exe_name);
#endif
}

static emit_context_t * create_pe_context(void) {
    emit_context_t * ctx = NULL;
    pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_PE);
    if (status != PULSE_SUCCESS)
        return NULL;
    return ctx;
}

static int write_simple_exe(const char * filename, uint64_t return_value) {
    emit_context_t * ctx = create_pe_context();
    if (!ctx)
        return 0;

    pulse_status status = emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    if (status != PULSE_SUCCESS) {
        emit_destroy(ctx);
        return 0;
    }

    status = emit_begin_section(ctx, ".text");
    if (status != PULSE_SUCCESS) {
        emit_destroy(ctx);
        return 0;
    }

    emit_define_symbol(ctx, "main", EMIT_VISIBILITY_DEFAULT, true);
    emit_emit_label(ctx, "main");

    emit_math_prologue(ctx);
    emit_math_mov_imm(ctx, EMIT_REG_RAX, return_value);
    emit_math_epilogue(ctx);

    status = emit_write_file(ctx, filename);
    emit_destroy(ctx);

    return status == PULSE_SUCCESS;
}

static int run_executable_and_check_exit_code(const char * exe_path, int expected_exit_code) {
#ifdef _WIN32
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    BOOL result = CreateProcessA(exe_path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!result) {
        DWORD err = GetLastError();
        printf("CreateProcess failed with error %lu\n", err);
        return 0;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    printf("Process exited with code %lu, expected %d\n", exit_code, expected_exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exit_code == (DWORD)expected_exit_code);
#else
    int exit_code = system(exe_path);
    return WIFEXITED(exit_code) && WEXITSTATUS(exit_code) == expected_exit_code;
#endif
}

TEST {
    plan(8);

    subtest("emit_write_file with NULL context") {
        plan(1);
        pulse_status status = emit_write_file(NULL, "test.exe");
        ok(status != PULSE_SUCCESS, "emit_write_file with NULL context fails");
    }

    subtest("emit_write_file with NULL filename") {
        plan(1);
        emit_context_t * ctx = create_pe_context();
        if (ctx) {
            pulse_status status = emit_write_file(ctx, NULL);
            ok(status != PULSE_SUCCESS, "emit_write_file with NULL filename fails");
            emit_destroy(ctx);
        }
        else {
            fail("Failed to create context");
        }
    }

    subtest("write and verify PE file header") {
        plan(3);

        if (write_simple_exe(test_exe_name, 42)) {
            ok(1, "PE file written successfully");

            FILE * f = fopen(test_exe_name, "rb");
            if (f) {
                unsigned char header[2];
                size_t read = fread(header, 1, 2, f);
                ok(read == 2, "Read 2 bytes from PE file");
                ok(header[0] == 'M' && header[1] == 'Z', "PE file has valid MZ signature");
                fclose(f);
            }
            else {
                fail("Could not open PE file for reading");
            }
        }
        else {
            fail("Failed to write PE file");
        }
    }

    subtest("write and execute PE file") {
        plan(1);

        if (write_simple_exe(test_exe_name, 0))
            ok(run_executable_and_check_exit_code(test_exe_name, 0), "PE executable created and runs");
        else
            fail("Failed to write PE file");
    }

    subtest("write PE with different return values") {
        plan(3);

        ok(write_simple_exe(test_exe_name, 0), "wrote PE returning 0");
        ok(write_simple_exe(test_exe_name, 42), "wrote PE returning 42");
        ok(write_simple_exe(test_exe_name, 255), "wrote PE returning 255");
    }

    subtest("write and execute PE file returning 42") {
        plan(1);

        if (write_simple_exe(test_exe_name, 42))
            ok(run_executable_and_check_exit_code(test_exe_name, 42), "PE executable returns exit code 42");
        else
            fail("Failed to write PE file");
    }

    subtest("write and execute PE file returning 255") {
        plan(1);

        if (write_simple_exe(test_exe_name, 255))
            ok(run_executable_and_check_exit_code(test_exe_name, 255), "PE executable returns exit code 255");
        else
            fail("Failed to write PE file");
    }

    subtest("write PE file with arithmetic operations") {
        plan(1);

        emit_context_t * ctx = create_pe_context();
        if (!ctx) {
            fail("Failed to create context");
            return;
        }

        pulse_status status = emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
        if (status != PULSE_SUCCESS) {
            emit_destroy(ctx);
            fail("Failed to add section");
            return;
        }

        status = emit_begin_section(ctx, ".text");
        if (status != PULSE_SUCCESS) {
            emit_destroy(ctx);
            fail("Failed to begin section");
            return;
        }

        emit_emit_label(ctx, "main");
        emit_math_prologue(ctx);

        emit_math_mov_imm(ctx, EMIT_REG_RAX, 10);
        emit_math_add_imm(ctx, EMIT_REG_RAX, 20);
        emit_math_imul_imm(ctx, EMIT_REG_RAX, 3);

        emit_math_epilogue(ctx);

        status = emit_write_file(ctx, test_exe_name);
        emit_destroy(ctx);

        if (status == PULSE_SUCCESS)
            ok(run_executable_and_check_exit_code(test_exe_name, 90), "PE executable computes (10 + 20) * 3 = 90");
        else
            fail("Failed to write PE file");
    }

    subtest("verify PE file size is reasonable") {
        plan(1);

        if (write_simple_exe(test_exe_name, 0)) {
            FILE * f = fopen(test_exe_name, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fclose(f);

                ok(size > 512 && size < 100000, "PE file size is reasonable (%ld bytes)", size);
            }
            else {
                fail("Could not open PE file");
            }
        }
        else {
            fail("Failed to write PE file");
        }
    }

    subtest("cleanup") {
        plan(1);
        // Don't cleanup yet - let dump_pe run
        ok(1, "Skip cleanup for debugging");
    }
}
