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
#define PLATFORM_NAME "Windows"
#elif defined(__linux__)
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#define EXECUTABLE_EXT ""
#define PLATFORM_NAME "Linux"
#else
#define EXECUTABLE_EXT ""
#define PLATFORM_NAME "Unknown"
#endif

static const char * test_exe_name = "pulse_emit_test" EXECUTABLE_EXT;
static const char * test_elf_rel_name = "t_pulse_emit_test.o";
static const char * test_elf_exec_name = "t_pulse_emit_test_elf";

static void cleanup_test_files(void) {
#ifdef _WIN32
    DeleteFileA(test_exe_name);
#else
    unlink(test_exe_name);
    unlink(test_elf_rel_name);
    unlink(test_elf_exec_name);
#endif
}

static emit_context_t * create_pe_context(void) {
    emit_context_t * ctx = NULL;
    pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_PE);
    if (status != PULSE_SUCCESS)
        return NULL;
    return ctx;
}

static emit_context_t * create_elf_relocatable_context(void) {
    emit_context_t * ctx = NULL;
    pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_ELF);
    if (status != PULSE_SUCCESS)
        return NULL;
    return ctx;
}

static emit_context_t * create_elf_executable_context(void) {
    emit_context_t * ctx = NULL;
    pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_ELF_EXEC);
    if (status != PULSE_SUCCESS)
        return NULL;
    return ctx;
}

static int write_simple_pe_exe(uint64_t return_value) {
    emit_context_t * ctx = create_pe_context();
    if (!ctx)
        return 0;

    pulse_status status = emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    if (status != PULSE_SUCCESS) { emit_destroy(ctx); return 0; }

    status = emit_begin_section(ctx, ".text");
    if (status != PULSE_SUCCESS) { emit_destroy(ctx); return 0; }

    emit_define_symbol(ctx, "main", EMIT_VISIBILITY_DEFAULT, true);
    emit_emit_label(ctx, "main");
    emit_math_prologue(ctx);
    emit_math_mov_imm(ctx, EMIT_REG_RAX, return_value);
    emit_math_epilogue(ctx);

    status = emit_write_file(ctx, test_exe_name);
    emit_destroy(ctx);
    return status == PULSE_SUCCESS;
}

static int run_executable_and_check_exit_code(const char * exe_path, int expected_exit_code) {
#ifdef _WIN32
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    BOOL result = CreateProcessA(exe_path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!result)
        return 0;

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exit_code == (DWORD)expected_exit_code);
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./%s", exe_path);
    int exit_code = system(cmd);
    return WIFEXITED(exit_code) && WEXITSTATUS(exit_code) == expected_exit_code;
#endif
}

TEST {
    plan(10);

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

        if (write_simple_pe_exe(42)) {
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

    subtest("write and verify PE file structure") {
        plan(4);

        if (write_simple_pe_exe(0)) {
            ok(1, "PE file written");

            FILE * f = fopen(test_exe_name, "rb");
            if (f) {
                unsigned char buf[512];
                size_t read = fread(buf, 1, 512, f);
                fclose(f);

                ok(read > 64, "PE file has content");

                int lfanew = *(int *)(buf + 0x3C);
                ok(lfanew == 0x40, "PE signature at offset 0x40");

                unsigned int pe_sig = *(unsigned int *)(buf + lfanew);
                ok(pe_sig == 0x00004550, "Valid PE signature");
            }
            else {
                fail("Cannot open PE file");
            }
        }
        else {
            fail("Failed to write PE file");
        }
    }

    subtest("write PE with different return values") {
        plan(3);

        ok(write_simple_pe_exe(0), "wrote PE returning 0");
        ok(write_simple_pe_exe(42), "wrote PE returning 42");
        ok(write_simple_pe_exe(255), "wrote PE returning 255");
    }

#ifdef __linux__
    subtest("write ELF relocatable file") {
        plan(3);

        emit_context_t * ctx = create_elf_relocatable_context();
        if (!ctx) {
            fail("Failed to create ELF relocatable context");
        }
        else {
            pulse_status status = emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
            if (status != PULSE_SUCCESS) {
                emit_destroy(ctx);
                fail("Failed to add section");
            }
            else {
                status = emit_begin_section(ctx, ".text");
                if (status != PULSE_SUCCESS) {
                    emit_destroy(ctx);
                    fail("Failed to begin section");
                }
                else {
                    emit_define_symbol(ctx, "_start", EMIT_VISIBILITY_DEFAULT, true);
                    emit_emit_label(ctx, "_start");
                    emit_emit_u8(ctx, 0xb8); emit_emit_u32(ctx, 42); emit_emit_u8(ctx, 0xc3);

                    status = emit_write_file(ctx, test_elf_rel_name);
                    emit_destroy(ctx);

                    if (status == PULSE_SUCCESS) {
                        ok(1, "ELF relocatable file written successfully");

                        FILE * f = fopen(test_elf_rel_name, "rb");
                        if (f) {
                            unsigned char header[4];
                            size_t read = fread(header, 1, 4, f);
                            fclose(f);
                            ok(read == 4, "Read 4 bytes from ELF file");
                            ok(header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F',
                               "ELF file has valid signature");
                        }
                        else {
                            fail("Could not open ELF file");
                        }
                    }
                    else {
                        fail("Failed to write ELF file");
                    }
                }
            }
        }
    }

    subtest("link and execute ELF relocatable returning 42") {
        plan(1);

        emit_context_t * ctx = create_elf_relocatable_context();
        if (!ctx) {
            fail("Failed to create ELF relocatable context");
        }
        else {
            pulse_status status = emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
            if (status == PULSE_SUCCESS) {
                emit_begin_section(ctx, ".text");
                emit_define_symbol(ctx, "_start", EMIT_VISIBILITY_DEFAULT, true);
                emit_emit_label(ctx, "_start");
                emit_emit_u8(ctx, 0xb8); emit_emit_u32(ctx, 60);
                emit_emit_u8(ctx, 0xbf); emit_emit_u32(ctx, 42);
                emit_emit_u8(ctx, 0x0f); emit_emit_u8(ctx, 0x05);

                status = emit_write_file(ctx, test_elf_rel_name);
            }
            emit_destroy(ctx);

            if (status == PULSE_SUCCESS) {
                char cmd[256];
                snprintf(cmd, sizeof(cmd), "ld -o %s %s -nostdlib -e _start 2>/dev/null", test_elf_exec_name, test_elf_rel_name);
                int link_result = system(cmd);

                if (link_result == 0) {
                    ok(run_executable_and_check_exit_code(test_elf_exec_name, 42),
                       "Linked ELF returns exit code 42");
                }
                else {
                    fail("Failed to link ELF file (exit code %d)", WEXITSTATUS(link_result));
                }
            }
            else {
                fail("Failed to write ELF file");
            }
        }
    }

    subtest("write and execute ELF executable returning 42") {
        plan(1);

        emit_context_t * ctx = create_elf_executable_context();
        if (!ctx) {
            fail("Failed to create ELF executable context");
        }
        else {
            pulse_status status = emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
            if (status == PULSE_SUCCESS) {
                emit_begin_section(ctx, ".text");
                emit_define_symbol(ctx, "_start", EMIT_VISIBILITY_DEFAULT, true);
                emit_emit_label(ctx, "_start");
                emit_emit_u8(ctx, 0xb8); emit_emit_u32(ctx, 60);
                emit_emit_u8(ctx, 0xbf); emit_emit_u32(ctx, 42);
                emit_emit_u8(ctx, 0x0f); emit_emit_u8(ctx, 0x05);

                status = emit_write_file(ctx, test_elf_exec_name);
            }
            emit_destroy(ctx);

            if (status == PULSE_SUCCESS) {
                char chmod_cmd[256];
                snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s 2>/dev/null", test_elf_exec_name);
                system(chmod_cmd);
                ok(run_executable_and_check_exit_code(test_elf_exec_name, 42),
                   "ELF executable returns exit code 42");
            }
            else {
                fail("Failed to write ELF executable");
            }
        }
    }

    subtest("write and execute ELF executable with arithmetic") {
        plan(1);

        emit_context_t * ctx = create_elf_executable_context();
        if (!ctx) {
            fail("Failed to create ELF executable context");
        }
        else {
            pulse_status status = emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
            if (status == PULSE_SUCCESS) {
                emit_begin_section(ctx, ".text");
                emit_define_symbol(ctx, "_start", EMIT_VISIBILITY_DEFAULT, true);
                emit_emit_label(ctx, "_start");
                emit_emit_u8(ctx, 0xb8); emit_emit_u32(ctx, 10);
                emit_emit_u8(ctx, 0x05); emit_emit_u32(ctx, 20);
                emit_emit_u8(ctx, 0x69); emit_emit_u8(ctx, 0xc0); emit_emit_u32(ctx, 3);
                emit_emit_u8(ctx, 0xb8); emit_emit_u32(ctx, 60);
                emit_emit_u8(ctx, 0x89); emit_emit_u8(ctx, 0xc7);
                emit_emit_u8(ctx, 0x0f); emit_emit_u8(ctx, 0x05);

                status = emit_write_file(ctx, test_elf_exec_name);
            }
            emit_destroy(ctx);

            if (status == PULSE_SUCCESS) {
                char chmod_cmd[256];
                snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s 2>/dev/null", test_elf_exec_name);
                system(chmod_cmd);
                ok(run_executable_and_check_exit_code(test_elf_exec_name, 90),
                   "ELF executable computes (10 + 20) * 3 = 90");
            }
            else {
                fail("Failed to write ELF executable");
            }
        }
    }
#else
    subtest("write ELF relocatable file (skipped on non-Linux)") {
        plan(1);
        skip(1, "ELF tests only run on Linux");
    }

    subtest("write and execute ELF executable (skipped on non-Linux)") {
        plan(1);
        skip(1, "ELF tests only run on Linux");
    }

    subtest("write and execute ELF with arithmetic (skipped on non-Linux)") {
        plan(1);
        skip(1, "ELF tests only run on Linux");
    }
#endif

    subtest("cleanup") {
        plan(1);
        cleanup_test_files();
        ok(1, "Cleanup complete");
    }
}
