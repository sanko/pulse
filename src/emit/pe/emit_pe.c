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

#define PE_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

#define IMAGE_DOS_SIGNATURE 0x5A4D
#ifndef IMAGE_NT_SIGNATURE
#define IMAGE_NT_SIGNATURE 0x4550
#endif

#define IMAGE_SIZEOF_FILE_HEADER 20
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

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
    uint64_t BaseOfCode;
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

#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define IMAGE_SCN_MEM_WRITE 0x80000000

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

static void write_dos_stub(uint8_t * buf) {
    uint8_t * dos_stub = buf + 0x40;

    dos_stub[0] = 0x0E; /* push cs */
    dos_stub[1] = 0x1F; /* pop ds */
    dos_stub[2] = 0xB8; /* mov ax, ... */
    dos_stub[3] = 0x01; /* 0x4C01 = exit with code 1 */
    dos_stub[4] = 0x4C;
    dos_stub[5] = 0xCD; /* int 0x21 */

    strcpy((char *)(dos_stub + 6), "This program cannot be run in DOS mode.\r\r\n$");

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
    hdr->e_lfanew = sizeof(image_dos_header_t);
}

static void write_nt_headers(uint8_t * buf,
                             emit_architecture_t arch,
                             uint32_t image_size,
                             uint32_t code_size,
                             uint16_t num_sections,
                             uint32_t entry_rva) {
    uint32_t * nt_sig = (uint32_t *)(buf + sizeof(image_dos_header_t));
    *nt_sig = IMAGE_NT_SIGNATURE;

    image_file_header_t * file_hdr = (image_file_header_t *)(buf + sizeof(image_dos_header_t) + 4);
    file_hdr->Machine = pe_get_machine(arch);
    file_hdr->NumberOfSections = num_sections;
    file_hdr->TimeDateStamp = (uint32_t)time(NULL);
    file_hdr->PointerToSymbolTable = 0;
    file_hdr->NumberOfSymbols = 0;
    file_hdr->SizeOfOptionalHeader =
        (arch == EMIT_ARCH_X86_64) ? sizeof(image_optional_header64_t) : sizeof(image_optional_header32_t);
    file_hdr->Characteristics = 0x0102;

    if (arch == EMIT_ARCH_X86_64) {
        image_optional_header64_t * opt =
            (image_optional_header64_t *)(buf + sizeof(image_dos_header_t) + 4 + IMAGE_SIZEOF_FILE_HEADER);
        opt->Magic = 0x020B;
        opt->MajorLinkerVersion = 1;
        opt->MinorLinkerVersion = 0;
        opt->SizeOfCode = code_size;
        opt->SizeOfInitializedData = 0;
        opt->SizeOfUninitializedData = 0;
        opt->AddressOfEntryPoint = entry_rva;
        opt->BaseOfCode = 0x1000;
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
        opt->SizeOfHeaders =
            PE_ALIGN(sizeof(image_dos_header_t) + 4 + IMAGE_SIZEOF_FILE_HEADER + file_hdr->SizeOfOptionalHeader, 0x200);
        opt->CheckSum = 0;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8160;
        opt->SizeOfStackReserve = 0x100000;
        opt->SizeOfStackCommit = 0x1000;
        opt->SizeOfHeapReserve = 0x100000;
        opt->SizeOfHeapCommit = 0x1000;
        opt->LoaderFlags = 0;
        opt->NumberOfRvaAndSizes = 16;
    }
    else {
        image_optional_header32_t * opt =
            (image_optional_header32_t *)(buf + sizeof(image_dos_header_t) + 4 + IMAGE_SIZEOF_FILE_HEADER);
        opt->Magic = 0x010B;
        opt->MajorLinkerVersion = 1;
        opt->MinorLinkerVersion = 0;
        opt->SizeOfCode = code_size;
        opt->SizeOfInitializedData = 0;
        opt->SizeOfUninitializedData = 0;
        opt->AddressOfEntryPoint = entry_rva;
        opt->BaseOfCode = 0x1000;
        opt->BaseOfData = 0x2000;
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
        opt->SizeOfHeaders =
            PE_ALIGN(sizeof(image_dos_header_t) + 4 + IMAGE_SIZEOF_FILE_HEADER + file_hdr->SizeOfOptionalHeader, 0x200);
        opt->CheckSum = 0;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->SizeOfStackReserve = 0x100000;
        opt->SizeOfStackCommit = 0x1000;
        opt->SizeOfHeapReserve = 0x100000;
        opt->SizeOfHeapCommit = 0x1000;
        opt->LoaderFlags = 0;
        opt->NumberOfRvaAndSizes = 16;
    }
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

pulse_status emit_write_pe(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;

    size_t num_sections = 0;
    size_t code_size = 0;
    size_t data_size = 0;
    uint32_t entry_rva = 0x1000;

    emit_section_t * sec = ctx->sections;
    while (sec) {
        num_sections++;
        if (sec->flags & EMIT_SECTION_FLAG_EXECUTE)
            code_size += PE_ALIGN(sec->size, 0x200);
        else
            data_size += PE_ALIGN(sec->size, 0x200);
        sec = sec->next;
    }

    if (num_sections == 0) {
        *out_data = NULL;
        *out_size = 0;
        return PULSE_SUCCESS;
    }

    size_t header_size = sizeof(image_dos_header_t) + 4 + IMAGE_SIZEOF_FILE_HEADER +
        ((ctx->arch == EMIT_ARCH_X86_64) ? sizeof(image_optional_header64_t) : sizeof(image_optional_header32_t)) +
        num_sections * sizeof(image_section_header_t);
    header_size = PE_ALIGN(header_size, 0x200);

    size_t image_size = 0x1000 + PE_ALIGN(code_size, 0x1000) + PE_ALIGN(data_size, 0x1000);
    size_t total_size = header_size + PE_ALIGN(code_size, 0x200) + PE_ALIGN(data_size, 0x200);

    uint8_t * buf = (uint8_t *)calloc(1, total_size);
    if (!buf)
        return PULSE_ERROR_ALLOCATION_FAILED;

    write_dos_stub(buf);
    write_nt_headers(buf, ctx->arch, (uint32_t)image_size, (uint32_t)code_size, (uint16_t)num_sections, entry_rva);

    image_section_header_t * sections =
        (image_section_header_t *)(buf + header_size - num_sections * sizeof(image_section_header_t));

    uint32_t sec_idx = 0;
    uint32_t rva = 0x1000;
    uint32_t file_off = (uint32_t)header_size;

    sec = ctx->sections;
    while (sec) {
        uint32_t vsize = (uint32_t)sec->size;
        uint32_t raw_size = (uint32_t)PE_ALIGN(sec->size, 0x200);
        uint32_t flags = 0;

        if (sec->flags & EMIT_SECTION_FLAG_EXECUTE)
            flags = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
        else if (sec->flags & EMIT_SECTION_FLAG_WRITE)
            flags = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
        else
            flags = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

        if (sec_idx == 0 && (flags & IMAGE_SCN_CNT_CODE))
            entry_rva = rva;

        write_section_header(&sections[sec_idx], sec->name, vsize, rva, raw_size, file_off, flags);

        if (sec->data && sec->size > 0)
            memcpy(buf + file_off, sec->data, sec->size);

        rva += PE_ALIGN(vsize, 0x1000);
        file_off += raw_size;
        sec_idx++;
        sec = sec->next;
    }

    *out_data = buf;
    *out_size = total_size;
    return PULSE_SUCCESS;
}
