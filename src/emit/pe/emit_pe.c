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
    uint16_t SizeOfOptionalHeader;  // Fixed: was uint32_t
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
    uint32_t OriginalFirstThunk;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
} image_import_descriptor_t;

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

static uint8_t * build_code_with_exitcall(
    uint64_t return_value, int arch, uint32_t exitprocess_iat_rva, uint32_t text_rva, size_t * out_size) {
    size_t size = 64; // Safe buffer
    uint8_t * buf = (uint8_t *)calloc(1, size);
    if (!buf)
        return NULL;
    size_t pos = 0;
    if (arch == EMIT_ARCH_X86_64) {
        buf[pos++] = 0x48;
        buf[pos++] = 0x83;
        buf[pos++] = 0xEC;
        buf[pos++] = 0x28;  // sub rsp, 40
        buf[pos++] = 0x48;
        buf[pos++] = 0xB9;  // mov rcx, imm64
        *(uint64_t *)(buf + pos) = return_value;
        pos += 8;
        buf[pos++] = 0xFF;
        buf[pos++] = 0x15;  // call [rip+disp]
        uint32_t rip = text_rva + (uint32_t)pos + 4;
        *(uint32_t *)(buf + pos) = (uint32_t)(exitprocess_iat_rva - rip);
        pos += 4;
        buf[pos++] = 0xC3;
    }
    else if (arch == EMIT_ARCH_AARCH64) {
        /* mov x0, #return_value (limited to 16-bit for simplicity in this stub) */
        uint32_t mov_x0 = 0xD2800000 | ((return_value & 0xFFFF) << 5) | 0;
        *(uint32_t *)(buf + pos) = mov_x0;
        pos += 4;
        /* adrp x8, exitprocess_iat_rva */
        uint32_t adrp = 0x90000008;
        int64_t diff = (int64_t)exitprocess_iat_rva - (int64_t)text_rva;
        int64_t pagediff = (diff >> 12);
        adrp |= ((pagediff & 0x3) << 29) | ((pagediff & 0x1FFFFC) << 3);
        *(uint32_t *)(buf + pos) = adrp;
        pos += 4;
        /* ldr x8, [x8, #:lo12:exitprocess_iat_rva] */
        uint32_t ldr = 0xF9400108 | ((exitprocess_iat_rva & 0xFFF) >> 3) << 10;
        *(uint32_t *)(buf + pos) = ldr;
        pos += 4;
        /* blr x8 */
        *(uint32_t *)(buf + pos) = 0xD63F0100;
        pos += 4;
    }
    else {
        buf[pos++] = 0x68;  // push imm32
        *(uint32_t *)(buf + pos) = (uint32_t)return_value;
        pos += 4;
        buf[pos++] = 0xFF;
        buf[pos++] = 0x15;  // call [addr]
        *(uint32_t *)(buf + pos) = 0x00400000 + exitprocess_iat_rva;
        pos += 4;
    }
    *out_size = pos;
    return buf;
}

pulse_status emit_write_pe(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;
    int is_x64 = (ctx->arch == EMIT_ARCH_X86_64);
    size_t combined_user_size = 0;
    emit_section_t * sec = ctx->sections;
    while (sec) {
        combined_user_size += PE_ALIGN(sec->size, 0x200);
        sec = sec->next;
    }
    if (combined_user_size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return PULSE_SUCCESS;
    }

    uint32_t pe_offset = 128;
    size_t opt_hdr_size = is_x64 ? sizeof(image_optional_header64_t) : sizeof(image_optional_header32_t);
    size_t header_total = pe_offset + 4 + 20 + opt_hdr_size + 2 * 32;
    size_t header_size = PE_ALIGN(header_total, 0x200);
    uint32_t text_rva = 0x1000;
    uint32_t rdata_rva = text_rva + PE_ALIGN(combined_user_size, 0x1000);
    size_t total_size = header_size + PE_ALIGN(combined_user_size, 0x200) + PE_ALIGN(128, 0x200);

    uint8_t * buf = (uint8_t *)calloc(1, total_size);
    if (!buf)
        return PULSE_ERROR_ALLOCATION_FAILED;
    write_dos_stub(buf, pe_offset);
    *(uint32_t *)(buf + pe_offset) = IMAGE_NT_SIGNATURE;
    image_file_header_t * file_hdr = (image_file_header_t *)(buf + pe_offset + 4);
    file_hdr->Machine = pe_get_machine(ctx->arch);
    file_hdr->NumberOfSections = 2;
    file_hdr->TimeDateStamp = (uint32_t)time(NULL);
    file_hdr->SizeOfOptionalHeader = (uint16_t)opt_hdr_size;
    file_hdr->Characteristics = 0x0002;

    if (is_x64) {
        image_optional_header64_t * opt = (image_optional_header64_t *)(buf + pe_offset + 24);
        opt->Magic = 0x020B;
        opt->AddressOfEntryPoint = text_rva;
        opt->BaseOfCode = text_rva;
        opt->ImageBase = 0x140000000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorSubsystemVersion = 6;
        opt->SizeOfImage = (uint32_t)PE_ALIGN(rdata_rva + 128, 0x1000);
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->NumberOfRvaAndSizes = 16;
        opt->DataDirectory[1].VirtualAddress = rdata_rva;
        opt->DataDirectory[1].Size = 128;
    }
    else {
        image_optional_header32_t * opt = (image_optional_header32_t *)(buf + pe_offset + 24);
        opt->Magic = 0x010B;
        opt->AddressOfEntryPoint = text_rva;
        opt->BaseOfCode = text_rva;
        opt->ImageBase = 0x00400000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorSubsystemVersion = 6;
        opt->SizeOfImage = (uint32_t)PE_ALIGN(rdata_rva + 128, 0x1000);
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->NumberOfRvaAndSizes = 16;
        opt->DataDirectory[1].VirtualAddress = rdata_rva;
        opt->DataDirectory[1].Size = 128;
    }

    image_section_header_t * sections = (image_section_header_t *)(buf + pe_offset + 24 + opt_hdr_size);
    write_section_header(&sections[0],
                         ".text",
                         (uint32_t)combined_user_size,
                         text_rva,
                         (uint32_t)combined_user_size,
                         (uint32_t)header_size,
                         0x60000020);
    write_section_header(&sections[1],
                         ".rdata",
                         128,
                         rdata_rva,
                         128,
                         (uint32_t)(header_size + PE_ALIGN(combined_user_size, 0x200)),
                         0x40000040);

    size_t code_pos = header_size;
    for (sec = ctx->sections; sec; sec = sec->next) {
        if (sec->size > 0)
            memcpy(buf + code_pos, sec->data, sec->size);
        code_pos += PE_ALIGN(sec->size, 0x200);
    }
    uint8_t * rdata = buf + (header_size + PE_ALIGN(combined_user_size, 0x200));
    image_import_descriptor_t * desc = (image_import_descriptor_t *)rdata;
    desc->OriginalFirstThunk = rdata_rva + 40;
    desc->Name = rdata_rva + 72;
    desc->FirstThunk = rdata_rva + 56;
    if (is_x64) {
        *(uint64_t *)(rdata + 40) = rdata_rva + 88;
        *(uint64_t *)(rdata + 56) = rdata_rva + 88;
    }
    else {
        *(uint32_t *)(rdata + 40) = rdata_rva + 88;
        *(uint32_t *)(rdata + 56) = rdata_rva + 88;
    }
    strcpy((char *)(rdata + 72), "KERNEL32.dll");
    strcpy((char *)(rdata + 90), "ExitProcess");
    *out_data = buf;
    *out_size = total_size;
    return PULSE_SUCCESS;
}

pulse_status emit_write_pe_exec_internal(emit_context_t * ctx,
                                         uint8_t ** out_data,
                                         size_t * out_size,
                                         uint64_t return_value) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;
    int is_x64 = (ctx->arch == EMIT_ARCH_X86_64);
    uint32_t pe_offset = 128;
    size_t opt_hdr_size = is_x64 ? sizeof(image_optional_header64_t) : sizeof(image_optional_header32_t);
    size_t header_size = PE_ALIGN(pe_offset + 24 + opt_hdr_size + 64, 0x200);
    uint32_t text_rva = 0x1000;
    uint32_t rdata_rva = 0x2000;
    size_t stub_size;
    uint8_t * stub = build_code_with_exitcall(return_value, ctx->arch, rdata_rva + 56, text_rva, &stub_size);
    if (!stub)
        return PULSE_ERROR_ALLOCATION_FAILED;
    size_t total_size = header_size + 0x200 + 0x200;
    uint8_t * buf = (uint8_t *)calloc(1, total_size);
    write_dos_stub(buf, pe_offset);
    *(uint32_t *)(buf + pe_offset) = IMAGE_NT_SIGNATURE;
    image_file_header_t * file_hdr = (image_file_header_t *)(buf + pe_offset + 4);
    file_hdr->Machine = pe_get_machine(ctx->arch);
    file_hdr->NumberOfSections = 2;
    file_hdr->SizeOfOptionalHeader = (uint16_t)opt_hdr_size;
    file_hdr->Characteristics = 0x0002;
    if (is_x64) {
        image_optional_header64_t * opt = (image_optional_header64_t *)(buf + pe_offset + 24);
        opt->Magic = 0x020B;
        opt->AddressOfEntryPoint = text_rva;
        opt->BaseOfCode = text_rva;
        opt->ImageBase = 0x140000000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorSubsystemVersion = 6;
        opt->SizeOfImage = 0x3000;
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->NumberOfRvaAndSizes = 16;
        opt->DataDirectory[1].VirtualAddress = rdata_rva;
        opt->DataDirectory[1].Size = 128;
    }
    else {
        image_optional_header32_t * opt = (image_optional_header32_t *)(buf + pe_offset + 24);
        opt->Magic = 0x010B;
        opt->AddressOfEntryPoint = text_rva;
        opt->BaseOfCode = text_rva;
        opt->ImageBase = 0x00400000;
        opt->SectionAlignment = 0x1000;
        opt->FileAlignment = 0x200;
        opt->MajorSubsystemVersion = 6;
        opt->SizeOfImage = 0x3000;
        opt->SizeOfHeaders = (uint32_t)header_size;
        opt->Subsystem = 3;
        opt->DllCharacteristics = 0x8140;
        opt->NumberOfRvaAndSizes = 16;
        opt->DataDirectory[1].VirtualAddress = rdata_rva;
        opt->DataDirectory[1].Size = 128;
    }
    image_section_header_t * sections = (image_section_header_t *)(buf + pe_offset + 24 + opt_hdr_size);
    write_section_header(
        &sections[0], ".text", (uint32_t)stub_size, text_rva, (uint32_t)stub_size, (uint32_t)header_size, 0x60000020);
    write_section_header(&sections[1], ".rdata", 128, rdata_rva, 128, (uint32_t)(header_size + 0x200), 0x40000040);
    memcpy(buf + header_size, stub, stub_size);
    free(stub);
    uint8_t * rdata = buf + header_size + 0x200;
    image_import_descriptor_t * desc = (image_import_descriptor_t *)rdata;
    desc->OriginalFirstThunk = rdata_rva + 40;
    desc->Name = rdata_rva + 72;
    desc->FirstThunk = rdata_rva + 56;
    if (is_x64) {
        *(uint64_t *)(rdata + 40) = rdata_rva + 88;
        *(uint64_t *)(rdata + 56) = rdata_rva + 88;
    }
    else {
        *(uint32_t *)(rdata + 40) = rdata_rva + 88;
        *(uint32_t *)(rdata + 56) = rdata_rva + 88;
    }
    strcpy((char *)(rdata + 72), "KERNEL32.dll");
    strcpy((char *)(rdata + 90), "ExitProcess");
    *out_data = buf;
    *out_size = total_size;
    return PULSE_SUCCESS;
}
#pragma pack(pop)
