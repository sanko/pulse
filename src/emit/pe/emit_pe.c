/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_pe.c
 * @brief PE (Portable Executable) binary format support for emit system.
 */
#define PULSE_BUILDING
#include "emit_pe.h"
#include "../emit_internals.h"
#include "common/compat_c23.h"
#include "pulse/emit/emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma pack(push, 1)

#define PE_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define PE_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

#define IMAGE_DOS_SIGNATURE 0x5A4D
#ifndef IMAGE_NT_SIGNATURE
#define IMAGE_NT_SIGNATURE 0x4550
#endif

#define IMAGE_SIZEOF_FILE_HEADER 20
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

#define IMAGE_DIRECTORY_ENTRY_IMPORT 1
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5

typedef struct {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t e_lfanew;
} image_dos_header_t;

typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint32_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} image_file_header_t;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} image_data_directory_t;

typedef struct {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    image_data_directory_t DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} image_optional_header32_t;

typedef struct {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    image_data_directory_t DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} image_optional_header64_t;

typedef struct {
    uint8_t Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} image_section_header_t;

typedef struct {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
} image_export_directory_t;

typedef struct {
    uint32_t OriginalFirstThunk;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
} image_import_descriptor_t;

typedef struct {
    uint16_t Offset;
    uint16_t Type : 4;
    uint16_t RelocType : 4;
    uint16_t : 8;
} image_relocation_entry_t;

typedef struct {
    uint32_t PageRVA;
    uint32_t BlockSize;
} image_relocation_block_t;

#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define IMAGE_SCN_MEM_WRITE 0x80000000
#define IMAGE_SCN_ALIGN_16BYTES 0x00500000

#define IMAGE_REL_BASED_ABSOLUTE 0
#define IMAGE_REL_BASED_HIGHLOW 3
#define IMAGE_REL_BASED_DIR64 10

static uint16_t pe_get_machine(emit_architecture_t arch) {
    switch (arch) {
    case EMIT_ARCH_X86_64:
        return 0x8664;
    case EMIT_ARCH_AARCH64:
        return 0xAA64;
    default:
        return 0x8664;
    }
}

static void write_dos_stub(uint8_t * buf, uint32_t pe_offset) {
    image_dos_header_t * hdr = (image_dos_header_t *)buf;
    hdr->e_magic = IMAGE_DOS_SIGNATURE;
    hdr->e_cblp = 0x90;
    hdr->e_cp = 3;
    hdr->e_cparhdr = 4;
    hdr->e_minalloc = 0xFFFF;
    hdr->e_maxalloc = 0xFFFF;
    hdr->e_ss = 0x0000;
    hdr->e_sp = 0x00B8;
    hdr->e_ip = 0x0000;
    hdr->e_cs = 0x0000;
    hdr->e_lfarlc = 0x0040;
    hdr->e_lfanew = pe_offset;

    uint8_t * dos_stub = buf + 0x40;
    dos_stub[0] = 0x0E;
    dos_stub[1] = 0x1F;
    dos_stub[2] = 0xB8;
    dos_stub[3] = 0x01;
    dos_stub[4] = 0x4C;
    dos_stub[5] = 0xCD;
    dos_stub[6] = 0x21;

    strcpy((char *)(dos_stub + 7), "This program cannot be run in DOS mode.\r\r\n$");
}

static void write_section_header(image_section_header_t * sec,
                                 const char * name,
                                 uint32_t vsize,
                                 uint32_t vaddr,
                                 uint32_t raw_size,
                                 uint32_t raw_ptr,
                                 uint32_t flags) {
    memset(sec, 0, sizeof(image_section_header_t));
    strncpy((char *)sec->Name, name, 8);
    sec->VirtualSize = vsize;
    sec->VirtualAddress = vaddr;
    sec->SizeOfRawData = PE_ALIGN(raw_size, 0x200);
    sec->PointerToRawData = raw_ptr;
    sec->Characteristics = flags;
}

static uint8_t * build_import_table(
    uint64_t image_base, uint32_t * out_size, uint32_t * out_rva, uint32_t iat_rva, int is_x64) {
    size_t total_size = 0;
    size_t pos = 0;

    const char * dll_name = "KERNEL32.dll";
    const char * func_name = "ExitProcess";

    size_t dll_name_len = strlen(dll_name) + 1;
    size_t func_name_len = strlen(func_name) + 1;

    size_t hint_size = 2 + func_name_len + 2;
    size_t ilt_size = is_x64 ? 8 : 4;
    size_t iat_size = is_x64 ? 8 : 4;
    size_t import_desc_size = sizeof(image_import_descriptor_t);

    size_t aligned_hint = PE_ALIGN(hint_size, is_x64 ? 8 : 4);
    size_t aligned_dll = PE_ALIGN(dll_name_len, is_x64 ? 8 : 2);

    total_size = aligned_hint + aligned_dll + ilt_size + iat_size + import_desc_size + (is_x64 ? 8 : 4);

    uint8_t * buf = (uint8_t *)calloc(1, total_size);
    if (!buf)
        return NULL;

    pos = 0;

    uint8_t * hint = buf + pos;
    *(uint16_t *)hint = 0;
    strcpy((char *)(hint + 2), func_name);
    pos += aligned_hint;

    uint8_t * dll_name_data = buf + pos;
    strcpy((char *)dll_name_data, dll_name);
    pos += aligned_dll;

    uint32_t hint_rva = (uint32_t)(image_base + 0x3000 + pos - aligned_hint);
    (void)hint_rva;

    image_import_descriptor_t * desc = (image_import_descriptor_t *)(buf + pos);
    desc->OriginalFirstThunk = iat_rva;
    desc->TimeDateStamp = 0;
    desc->ForwarderChain = 0;
    desc->Name = (uint32_t)(image_base + 0x3000 + pos - aligned_hint - aligned_dll + aligned_hint);
    desc->FirstThunk = iat_rva;
    pos += sizeof(image_import_descriptor_t);

    uint8_t * null_desc = buf + pos;
    memset(null_desc, 0, is_x64 ? 8 : 4);
    pos += is_x64 ? 8 : 4;

    uint8_t * ilt = buf + pos;
    if (is_x64)
        *(uint64_t *)ilt = (uint32_t)(image_base + 0x3000);
    else
        *(uint32_t *)ilt = (uint32_t)(image_base + 0x3000);
    pos += ilt_size;

    uint8_t * null_ilt = buf + pos;
    memset(null_ilt, 0, is_x64 ? 8 : 4);
    pos += is_x64 ? 8 : 4;

    *out_size = (uint32_t)total_size;
    *out_rva = (uint32_t)(0x3000 + total_size - hint_size - aligned_dll - ilt_size - iat_size - import_desc_size -
                          (is_x64 ? 8 : 4));

    return buf;
}

static uint8_t * build_code_with_exitcall(uint64_t return_value, int is_x64, size_t * out_size) {
    size_t size = is_x64 ? 24 : 14;
    uint8_t * buf = (uint8_t *)malloc(size);
    if (!buf)
        return NULL;

    size_t pos = 0;

    if (is_x64) {
        buf[pos++] = 0x48;
        buf[pos++] = 0xB8;
        *(uint64_t *)(buf + pos) = return_value;
        pos += 8;
        buf[pos++] = 0x48;
        buf[pos++] = 0x89;
        buf[pos++] = 0xC1;
        buf[pos++] = 0x48;
        buf[pos++] = 0x8B;
        buf[pos++] = 0x04;
        buf[pos++] = 0x24;
        buf[pos++] = 0xFF;
        buf[pos++] = 0x10;
        buf[pos++] = 0xCC;
    }
    else {
        buf[pos++] = 0xB8;
        *(uint32_t *)(buf + pos) = (uint32_t)return_value;
        pos += 4;
        buf[pos++] = 0x50;
        buf[pos++] = 0xFF;
        buf[pos++] = 0x54;
        buf[pos++] = 0x24;
        buf[pos++] = 0x04;
        buf[pos++] = 0xCC;
    }

    *out_size = size;
    return buf;
}

pulse_status emit_write_pe(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;

    int is_x64 = (ctx->arch == EMIT_ARCH_X86_64);

    size_t num_sections = 0;
    size_t total_code_size = 0;
    size_t total_data_size = 0;

    emit_section_t * sec = ctx->sections;
    while (sec) {
        num_sections++;
        if (sec->flags & EMIT_SECTION_FLAG_EXECUTE)
            total_code_size += PE_ALIGN(sec->size, 0x200);
        else
            total_data_size += PE_ALIGN(sec->size, 0x200);
        sec = sec->next;
    }

    if (num_sections == 0) {
        *out_data = NULL;
        *out_size = 0;
        return PULSE_SUCCESS;
    }

    size_t dos_hdr_size = sizeof(image_dos_header_t);
    size_t pe_sig_size = 4;
    size_t file_hdr_size = IMAGE_SIZEOF_FILE_HEADER;
    size_t opt_hdr_size = is_x64 ? sizeof(image_optional_header64_t) : sizeof(image_optional_header32_t);

    num_sections = num_sections + 1 + 1;

    size_t header_total =
        dos_hdr_size + pe_sig_size + file_hdr_size + opt_hdr_size + num_sections * sizeof(image_section_header_t);
    size_t header_size = PE_ALIGN(header_total, 0x200);

    uint32_t text_rva = 0x1000;
    uint32_t text_file_off = (uint32_t)header_size;

    size_t rdata_size = is_x64 ? 320 : 200;
    uint32_t rdata_rva = text_rva + PE_ALIGN(total_code_size, 0x1000);
    uint32_t rdata_file_off = text_file_off + PE_ALIGN(total_code_size, 0x200);

    size_t image_size = 0x1000 + PE_ALIGN(total_code_size, 0x1000) + PE_ALIGN(rdata_size, 0x1000);
    size_t total_size = header_size + PE_ALIGN(total_code_size, 0x200) + PE_ALIGN(rdata_size, 0x200);

    uint8_t * buf = (uint8_t *)calloc(1, total_size);
    if (!buf)
        return PULSE_ERROR_ALLOCATION_FAILED;

    uint32_t pe_offset = sizeof(image_dos_header_t);
    write_dos_stub(buf, pe_offset);

    uint32_t * nt_sig = (uint32_t *)(buf + pe_offset);
    *nt_sig = IMAGE_NT_SIGNATURE;

    image_file_header_t * file_hdr = (image_file_header_t *)(buf + pe_offset + 4);
    file_hdr->Machine = pe_get_machine(ctx->arch);
    file_hdr->NumberOfSections = (uint16_t)num_sections;
    file_hdr->TimeDateStamp = (uint32_t)time(NULL);
    file_hdr->PointerToSymbolTable = 0;
    file_hdr->NumberOfSymbols = 0;
    file_hdr->SizeOfOptionalHeader = (uint32_t)opt_hdr_size;
    file_hdr->Characteristics = 0x0002;

    if (is_x64) {
        image_optional_header64_t * opt = (image_optional_header64_t *)(buf + pe_offset + 4 + file_hdr_size);
        opt->Magic = 0x020B;
        opt->MajorLinkerVersion = 1;
        opt->MinorLinkerVersion = 0;
        opt->SizeOfCode = (uint32_t)total_code_size;
        opt->SizeOfInitializedData = 0;
        opt->SizeOfUninitializedData = 0;
        opt->AddressOfEntryPoint = text_rva + 8;
        opt->BaseOfCode = text_rva;
        opt->ImageBase = 0x140000000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorOperatingSystemVersion = 6;
        opt->MinorOperatingSystemVersion = 0;
        opt->MajorImageVersion = 0;
        opt->MinorImageVersion = 0;
        opt->MajorSubsystemVersion = 6;
        opt->MinorSubsystemVersion = 0;
        opt->Win32VersionValue = 0;
        opt->SizeOfImage = PE_ALIGN(image_size, 0x1000);
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->CheckSum = 0;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->SizeOfStackReserve = 0x100000;
        opt->SizeOfStackCommit = 0x1000;
        opt->SizeOfHeapReserve = 0x100000;
        opt->SizeOfHeapCommit = 0x1000;
        opt->LoaderFlags = 0;
        opt->NumberOfRvaAndSizes = 16;

        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = rdata_rva;
        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = (uint32_t)rdata_size;
    }
    else {
        image_optional_header32_t * opt = (image_optional_header32_t *)(buf + pe_offset + 4 + file_hdr_size);
        opt->Magic = 0x010B;
        opt->MajorLinkerVersion = 1;
        opt->MinorLinkerVersion = 0;
        opt->SizeOfCode = (uint32_t)total_code_size;
        opt->SizeOfInitializedData = 0;
        opt->SizeOfUninitializedData = 0;
        opt->AddressOfEntryPoint = text_rva + 8;
        opt->BaseOfCode = text_rva;
        opt->BaseOfData = rdata_rva;
        opt->ImageBase = 0x00400000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorOperatingSystemVersion = 6;
        opt->MinorOperatingSystemVersion = 0;
        opt->MajorImageVersion = 0;
        opt->MinorImageVersion = 0;
        opt->MajorSubsystemVersion = 6;
        opt->MinorSubsystemVersion = 0;
        opt->Win32VersionValue = 0;
        opt->SizeOfImage = PE_ALIGN(image_size, 0x1000);
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->CheckSum = 0;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->SizeOfStackReserve = 0x100000;
        opt->SizeOfStackCommit = 0x1000;
        opt->SizeOfHeapReserve = 0x100000;
        opt->SizeOfHeapCommit = 0x1000;
        opt->LoaderFlags = 0;
        opt->NumberOfRvaAndSizes = 16;

        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = rdata_rva;
        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = (uint32_t)rdata_size;
    }

    image_section_header_t * sections = (image_section_header_t *)(buf + pe_offset + 4 + file_hdr_size + opt_hdr_size);

    write_section_header(&sections[0],
                         ".text",
                         (uint32_t)total_code_size,
                         text_rva,
                         (uint32_t)total_code_size,
                         text_file_off,
                         IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);

    write_section_header(&sections[1],
                         ".rdata",
                         (uint32_t)rdata_size,
                         rdata_rva,
                         (uint32_t)rdata_size,
                         rdata_file_off,
                         IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ);

    size_t code_pos = text_file_off;

    sec = ctx->sections;
    while (sec) {
        if (sec->data && sec->size > 0)
            memcpy(buf + code_pos, sec->data, sec->size);
        code_pos += PE_ALIGN(sec->size, 0x200);
        sec = sec->next;
    }

    size_t stub_code_size;
    uint8_t * stub_code = build_code_with_exitcall(0, is_x64, &stub_code_size);
    if (stub_code) {
        memcpy(buf + code_pos, stub_code, stub_code_size);
        free(stub_code);
    }

    size_t iat_rva = rdata_rva + 200;
    uint8_t * rdata = buf + rdata_file_off;

    size_t import_desc_offset = 0;
    image_import_descriptor_t * desc = (image_import_descriptor_t *)(rdata + import_desc_offset);
    desc->OriginalFirstThunk = iat_rva;
    desc->Name = rdata_rva + 200 + sizeof(image_import_descriptor_t) + 16;
    desc->FirstThunk = iat_rva;

    size_t dll_name_offset = 200 + sizeof(image_import_descriptor_t) + 16;
    strcpy((char *)(rdata + dll_name_offset), "KERNEL32.dll");

    size_t hint_offset = 200 + sizeof(image_import_descriptor_t) + 16 + 16;
    *(uint16_t *)(rdata + hint_offset) = 0;
    strcpy((char *)(rdata + hint_offset + 2), "ExitProcess");

    size_t iat_offset = 200;
    if (is_x64) {
        *(uint64_t *)(rdata + iat_offset) = rdata_rva + hint_offset;
        *(uint64_t *)(rdata + iat_offset + 8) = 0;
        *(uint64_t *)(rdata + iat_offset + 16) = 0;
    }
    else {
        *(uint32_t *)(rdata + iat_offset) = rdata_rva + hint_offset;
        *(uint32_t *)(rdata + iat_offset + 4) = 0;
        *(uint32_t *)(rdata + iat_offset + 8) = 0;
    }

    *out_data = buf;
    *out_size = total_size;
    return PULSE_SUCCESS;
}

pulse_status emit_write_pe_exec(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size, uint64_t return_value) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;

    int is_x64 = (ctx->arch == EMIT_ARCH_X86_64);

    size_t num_user_sections = 0;
    size_t total_code_size = 0;
    size_t total_data_size = 0;

    emit_section_t * sec = ctx->sections;
    while (sec) {
        num_user_sections++;
        if (sec->flags & EMIT_SECTION_FLAG_EXECUTE)
            total_code_size += PE_ALIGN(sec->size, 0x200);
        else
            total_data_size += PE_ALIGN(sec->size, 0x200);
        sec = sec->next;
    }

    size_t stub_code_size;
    uint8_t * stub_code = build_code_with_exitcall(return_value, is_x64, &stub_code_size);

    if (num_user_sections == 0 && !stub_code) {
        *out_data = NULL;
        *out_size = 0;
        return PULSE_SUCCESS;
    }

    size_t num_sections = num_user_sections + (stub_code ? 1 : 0) + 1;

    size_t dos_hdr_size = sizeof(image_dos_header_t);
    size_t pe_sig_size = 4;
    size_t file_hdr_size = IMAGE_SIZEOF_FILE_HEADER;
    size_t opt_hdr_size = is_x64 ? sizeof(image_optional_header64_t) : sizeof(image_optional_header32_t);

    size_t header_total =
        dos_hdr_size + pe_sig_size + file_hdr_size + opt_hdr_size + num_sections * sizeof(image_section_header_t);
    size_t header_size = PE_ALIGN(header_total, 0x200);

    uint32_t text_rva = 0x1000;
    uint32_t text_file_off = (uint32_t)header_size;

    size_t rdata_size = is_x64 ? 320 : 200;
    uint32_t rdata_rva =
        text_rva + PE_ALIGN(PE_ALIGN(total_code_size, 0x200) + (stub_code ? stub_code_size : 0), 0x1000);
    uint32_t rdata_file_off =
        text_file_off + PE_ALIGN(PE_ALIGN(total_code_size, 0x200) + (stub_code ? stub_code_size : 0), 0x200);

    size_t image_size = 0x1000 + PE_ALIGN(PE_ALIGN(total_code_size, 0x200) + (stub_code ? stub_code_size : 0), 0x1000) +
        PE_ALIGN(rdata_size, 0x1000);
    size_t total_file_size = header_size +
        PE_ALIGN(PE_ALIGN(total_code_size, 0x200) + (stub_code ? stub_code_size : 0), 0x200) +
        PE_ALIGN(rdata_size, 0x200);

    uint8_t * buf = (uint8_t *)calloc(1, total_file_size);
    if (!buf) {
        if (stub_code)
            free(stub_code);
        return PULSE_ERROR_ALLOCATION_FAILED;
    }

    uint32_t pe_offset = sizeof(image_dos_header_t);
    write_dos_stub(buf, pe_offset);

    uint32_t * nt_sig = (uint32_t *)(buf + pe_offset);
    *nt_sig = IMAGE_NT_SIGNATURE;

    image_file_header_t * file_hdr = (image_file_header_t *)(buf + pe_offset + 4);
    file_hdr->Machine = pe_get_machine(ctx->arch);
    file_hdr->NumberOfSections = (uint16_t)num_sections;
    file_hdr->TimeDateStamp = (uint32_t)time(NULL);
    file_hdr->PointerToSymbolTable = 0;
    file_hdr->NumberOfSymbols = 0;
    file_hdr->SizeOfOptionalHeader = (uint32_t)opt_hdr_size;
    file_hdr->Characteristics = 0x0002;

    uint32_t entry_rva = text_rva;
    size_t all_code_size = PE_ALIGN(total_code_size, 0x200) + (stub_code ? stub_code_size : 0);

    if (is_x64) {
        image_optional_header64_t * opt = (image_optional_header64_t *)(buf + pe_offset + 4 + file_hdr_size);
        opt->Magic = 0x020B;
        opt->MajorLinkerVersion = 1;
        opt->MinorLinkerVersion = 0;
        opt->SizeOfCode = (uint32_t)all_code_size;
        opt->SizeOfInitializedData = 0;
        opt->SizeOfUninitializedData = 0;
        opt->AddressOfEntryPoint = entry_rva + (uint32_t)total_code_size;
        opt->BaseOfCode = text_rva;
        opt->ImageBase = 0x140000000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorOperatingSystemVersion = 6;
        opt->MinorOperatingSystemVersion = 0;
        opt->MajorImageVersion = 0;
        opt->MinorImageVersion = 0;
        opt->MajorSubsystemVersion = 6;
        opt->MinorSubsystemVersion = 0;
        opt->Win32VersionValue = 0;
        opt->SizeOfImage = PE_ALIGN(image_size, 0x1000);
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->CheckSum = 0;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->SizeOfStackReserve = 0x100000;
        opt->SizeOfStackCommit = 0x1000;
        opt->SizeOfHeapReserve = 0x100000;
        opt->SizeOfHeapCommit = 0x1000;
        opt->LoaderFlags = 0;
        opt->NumberOfRvaAndSizes = 16;

        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = rdata_rva;
        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = (uint32_t)rdata_size;
    }
    else {
        image_optional_header32_t * opt = (image_optional_header32_t *)(buf + pe_offset + 4 + file_hdr_size);
        opt->Magic = 0x010B;
        opt->MajorLinkerVersion = 1;
        opt->MinorLinkerVersion = 0;
        opt->SizeOfCode = (uint32_t)all_code_size;
        opt->SizeOfInitializedData = 0;
        opt->SizeOfUninitializedData = 0;
        opt->AddressOfEntryPoint = entry_rva + (uint32_t)total_code_size;
        opt->BaseOfCode = text_rva;
        opt->BaseOfData = rdata_rva;
        opt->ImageBase = 0x00400000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorOperatingSystemVersion = 6;
        opt->MinorOperatingSystemVersion = 0;
        opt->MajorImageVersion = 0;
        opt->MinorImageVersion = 0;
        opt->MajorSubsystemVersion = 6;
        opt->MinorSubsystemVersion = 0;
        opt->Win32VersionValue = 0;
        opt->SizeOfImage = PE_ALIGN(image_size, 0x1000);
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->CheckSum = 0;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->SizeOfStackReserve = 0x100000;
        opt->SizeOfStackCommit = 0x1000;
        opt->SizeOfHeapReserve = 0x100000;
        opt->SizeOfHeapCommit = 0x1000;
        opt->LoaderFlags = 0;
        opt->NumberOfRvaAndSizes = 16;

        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = rdata_rva;
        opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = (uint32_t)rdata_size;
    }

    image_section_header_t * sections = (image_section_header_t *)(buf + pe_offset + 4 + file_hdr_size + opt_hdr_size);

    write_section_header(&sections[0],
                         ".text",
                         (uint32_t)all_code_size,
                         text_rva,
                         (uint32_t)all_code_size,
                         text_file_off,
                         IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);

    write_section_header(&sections[1],
                         ".rdata",
                         (uint32_t)rdata_size,
                         rdata_rva,
                         (uint32_t)rdata_size,
                         rdata_file_off,
                         IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ);

    size_t code_pos = text_file_off;

    sec = ctx->sections;
    while (sec) {
        if (sec->data && sec->size > 0)
            memcpy(buf + code_pos, sec->data, sec->size);
        code_pos += PE_ALIGN(sec->size, 0x200);
        sec = sec->next;
    }

    if (stub_code) {
        memcpy(buf + code_pos, stub_code, stub_code_size);
        free(stub_code);
    }

    size_t iat_rva = rdata_rva + 200;
    uint8_t * rdata = buf + rdata_file_off;

    image_import_descriptor_t * desc = (image_import_descriptor_t *)(rdata + 0);
    desc->OriginalFirstThunk = iat_rva;
    desc->Name = rdata_rva + 200 + sizeof(image_import_descriptor_t) + 16;
    desc->FirstThunk = iat_rva;

    strcpy((char *)(rdata + 200 + sizeof(image_import_descriptor_t) + 16), "KERNEL32.dll");

    *(uint16_t *)(rdata + 200 + sizeof(image_import_descriptor_t) + 16 + 16) = 0;
    strcpy((char *)(rdata + 200 + sizeof(image_import_descriptor_t) + 16 + 16 + 2), "ExitProcess");

    size_t iat_offset = 200;
    if (is_x64) {
        *(uint64_t *)(rdata + iat_offset) = rdata_rva + 200 + sizeof(image_import_descriptor_t) + 16 + 16 + 2;
        *(uint64_t *)(rdata + iat_offset + 8) = 0;
        *(uint64_t *)(rdata + iat_offset + 16) = 0;
    }
    else {
        *(uint32_t *)(rdata + iat_offset) = rdata_rva + 200 + sizeof(image_import_descriptor_t) + 16 + 16 + 2;
        *(uint32_t *)(rdata + iat_offset + 4) = 0;
        *(uint32_t *)(rdata + iat_offset + 8) = 0;
    }

    *out_data = buf;
    *out_size = total_file_size;
    return PULSE_SUCCESS;
}

#pragma pack(pop)
