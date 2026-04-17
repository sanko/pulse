/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may use this code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_elf.c
 * @brief ELF binary format support for emit system.
 */
#define PULSE_BUILDING
#include "common/compat_c23.h"
#include "pulse/emit/emit.h"
#include "emit_elf.h"
#include "../emit_internals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ELF_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

static uint16_t elf_get_machine(emit_architecture_t arch) {
    switch (arch) {
    case EMIT_ARCH_X86_64:
        return EM_X86_64;
    case EMIT_ARCH_AARCH64:
        return EM_AARCH64;
    default:
        return EM_X86_64;
    }
}

static void write_elf_header(uint8_t * buffer, size_t num_sections, emit_architecture_t arch) {
    Elf64_Ehdr * ehdr = (Elf64_Ehdr *)buffer;

    memcpy(ehdr->e_ident, ELF_MAGIC, 4);
    ehdr->e_ident[4] = ELFCLASS64;
    ehdr->e_ident[5] = ELFDATA2LSB;
    ehdr->e_ident[6] = EV_CURRENT;
    ehdr->e_ident[7] = 0;

    ehdr->e_type = ET_REL;
    ehdr->e_machine = elf_get_machine(arch);
    ehdr->e_version = EV_CURRENT;
    ehdr->e_entry = 0;
    ehdr->e_phoff = 0;
    ehdr->e_shoff = 0;
    ehdr->e_flags = 0;
    ehdr->e_ehsize = sizeof(Elf64_Ehdr);
    ehdr->e_phentsize = 0;
    ehdr->e_phnum = 0;
    ehdr->e_shentsize = sizeof(Elf64_Shdr);
    ehdr->e_shnum = (uint16_t)num_sections;
    ehdr->e_shstrndx = 1;
}

static void write_elf_section_header(Elf64_Shdr * shdr,
                                 uint32_t name_index,
                                 uint32_t sh_type,
                                 uint64_t sh_flags,
                                 uint64_t sh_addr,
                                 uint64_t sh_offset,
                                 uint64_t sh_size,
                                 uint32_t sh_link,
                                 uint32_t sh_info,
                                 uint64_t sh_addralign,
                                 uint64_t sh_entsize) {
    shdr->sh_name = name_index;
    shdr->sh_type = sh_type;
    shdr->sh_flags = sh_flags;
    shdr->sh_addr = sh_addr;
    shdr->sh_offset = sh_offset;
    shdr->sh_size = sh_size;
    shdr->sh_link = sh_link;
    shdr->sh_info = sh_info;
    shdr->sh_addralign = sh_addralign;
    shdr->sh_entsize = sh_entsize;
}

static uint8_t * build_shstrtab(const char * names[], size_t num_names, size_t * out_size) {
    size_t total = 1;
    for (size_t i = 0; i < num_names; i++)
        total += strlen(names[i]) + 1;

    uint8_t * buf = calloc(1, total);
    if (!buf)
        return NULL;

    size_t off = 1;
    for (size_t i = 0; i < num_names; i++) {
        size_t len = strlen(names[i]) + 1;
        memcpy(buf + off, names[i], len);
        off += len;
    }
    *out_size = total;
    return buf;
}

static size_t shstrtab_offset(const uint8_t * shstrtab, const char * name) {
    size_t off = 1;
    size_t i = 0;
    while (i < 1000) {
        const char * p = (const char *)(shstrtab + off);
        if (strcmp(p, name) == 0)
            return off;
        size_t len = strlen(p) + 1;
        off += len;
        i++;
        if (off >= 1024)
            break;
    }
    return 0;
}

static uint8_t * build_strtab(const char * strings[], size_t num_strings, size_t * out_size) {
    size_t total = 1;
    for (size_t i = 0; i < num_strings; i++) {
        if (strings[i])
            total += strlen(strings[i]) + 1;
    }

    uint8_t * buf = calloc(1, total);
    if (!buf)
        return NULL;

    size_t off = 1;
    for (size_t i = 0; i < num_strings; i++) {
        if (strings[i]) {
            size_t len = strlen(strings[i]) + 1;
            memcpy(buf + off, strings[i], len);
            off += len;
        }
    }
    *out_size = total;
    return buf;
}

static uint8_t * build_symtab(emit_symbol_t * symbols, size_t num_symbols, const char * strtab_strings[], size_t * out_size) {
    size_t total = num_symbols * sizeof(Elf64_Sym);
    uint8_t * buf = calloc(1, total);
    if (!buf)
        return NULL;

    Elf64_Sym * sym = (Elf64_Sym *)buf;
    sym[0].st_name = 0;
    sym[0].st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
    sym[0].st_shndx = 0;

    size_t sym_idx = 1;
    emit_symbol_t * s = symbols;
    while (s && sym_idx < num_symbols) {
        size_t off = 1;
        for (size_t i = 1; i < num_symbols && strtab_strings[i]; i++) {
            if (s->name && strcmp(strtab_strings[i], s->name) == 0) {
                sym[sym_idx].st_name = (uint32_t)off;
                break;
            }
            off += strlen(strtab_strings[i]) + 1;
        }
        if (sym[sym_idx].st_name == 0 && s->name)
            sym[sym_idx].st_name = (uint32_t)off;
        sym[sym_idx].st_info = ELF64_ST_INFO(STB_GLOBAL, s->is_function ? STT_FUNC : STT_OBJECT);
        sym[sym_idx].st_shndx = 0;
        sym[sym_idx].st_value = s->value;
        s = s->next;
        sym_idx++;
    }

    *out_size = total;
    return buf;
}

static uint8_t * build_rela(emit_relocation_t * relocs, emit_symbol_t * symbols, size_t * out_count) {
    size_t num = 0;
    emit_relocation_t * r = relocs;
    while (r) { num++; r = r->next; }

    if (num == 0) { *out_count = 0; return NULL; }

    uint8_t * buf = calloc(num, sizeof(Elf64_Rela));
    if (!buf) return NULL;

    Elf64_Rela * ent = (Elf64_Rela *)buf;
    r = relocs;
    for (size_t i = 0; i < num && r; i++, r = r->next) {
        ent[i].r_offset = r->offset;
        ent[i].r_addend = 0;
        ent[i].r_sym = 0;

        size_t idx = 1;
        emit_symbol_t * s = symbols;
        while (s) {
            if (strcmp(s->name, r->symbol_name) == 0) break;
            idx++;
            s = s->next;
        }
        ent[i].r_sym = (uint32_t)idx;
        ent[i].r_type = ELF_R_X86_64_PC32;
    }

    *out_count = num;
    return buf;
}

pulse_status emit_write_elf(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;

    size_t num_user_secs = 0;
    size_t num_syms = 0;
    emit_section_t * sec = ctx->sections;
    while (sec) { num_user_secs++; sec = sec->next; }
    emit_symbol_t * sym = ctx->symbols;
    while (sym) { num_syms++; sym = sym->next; }

    size_t num_extra = 1;
    if (num_syms > 0) num_extra += 2;
    if (ctx->relocations) num_extra++;
    size_t num_sections = 1 + num_user_secs + num_extra;

    const char * names[16];
    names[0] = "";
    names[1] = ".shstrtab";
    size_t ni = 2;
    sec = ctx->sections;
    while (sec && ni < 14) { names[ni++] = sec->name; sec = sec->next; }
    size_t text_shdr_idx = ni;
    if (num_syms > 0) { names[ni++] = ".strtab"; names[ni++] = ".symtab"; }
    size_t rela_shdr_idx = ni;
    if (ctx->relocations) names[ni++] = ".rela.text";

    size_t shstrtab_size;
    uint8_t * shstrtab = build_shstrtab(names, ni, &shstrtab_size);
    if (!shstrtab) return PULSE_ERROR_ALLOCATION_FAILED;

    const char * sym_names[16];
    sym_names[0] = "";
    sym = ctx->symbols;
    for (size_t i = 1; i <= num_syms && sym; i++) { sym_names[i] = sym->name ? sym->name : ""; sym = sym->next; }
    size_t sym_strtab_size;
    uint8_t * sym_strtab = build_strtab(sym_names, num_syms + 1, &sym_strtab_size);
    if (!sym_strtab) { free(shstrtab); return PULSE_ERROR_ALLOCATION_FAILED; }

    size_t symtab_size;
    uint8_t * symtab = build_symtab(ctx->symbols, num_syms + 1, (const char **)sym_names, &symtab_size);

    size_t rela_count = 0;
    uint8_t * rela = build_rela(ctx->relocations, ctx->symbols, &rela_count);
    size_t rela_size = rela_count * sizeof(Elf64_Rela);

    Elf64_Shdr * shdrs = calloc(num_sections, sizeof(Elf64_Shdr));
    if (!shdrs) { free(shstrtab); free(sym_strtab); free(symtab); free(rela); return PULSE_ERROR_ALLOCATION_FAILED; }

    write_elf_section_header(&shdrs[0], 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);

    size_t data_off = sizeof(Elf64_Ehdr);
    data_off = ELF_ALIGN(data_off, 16);

    size_t shdr_idx = 2;
    sec = ctx->sections;
    for (size_t i = 0; i < num_user_secs && sec; i++) {
        data_off = ELF_ALIGN(data_off, 16);
        uint64_t flags = 0;
        if (sec->flags & EMIT_SECTION_FLAG_ALLOC) flags |= SHF_ALLOC;
        if (sec->flags & EMIT_SECTION_FLAG_EXECUTE) flags |= SHF_EXECINSTR;
        if (sec->flags & EMIT_SECTION_FLAG_WRITE) flags |= SHF_WRITE;
        write_elf_section_header(&shdrs[shdr_idx], shstrtab_offset(shstrtab, sec->name), SHT_PROGBITS, flags, 0, data_off, sec->size, 0, 0, 16, 0);
        shdrs[shdr_idx].sh_offset = data_off;
        data_off += ELF_ALIGN(sec->size, 16);
        if (strcmp(sec->name, ".text") == 0) text_shdr_idx = shdr_idx;
        shdr_idx++;
    }

    size_t strtab_shdr_idx = 0, symtab_shdr_idx = 0, rela_shdr_idx_final = 0;

    if (num_syms > 0) {
        strtab_shdr_idx = shdr_idx;
        data_off = ELF_ALIGN(data_off, 1);
        write_elf_section_header(&shdrs[shdr_idx], shstrtab_offset(shstrtab, ".strtab"), SHT_STRTAB, 0, 0, data_off, sym_strtab_size, 0, 0, 1, 0);
        shdrs[shdr_idx].sh_offset = data_off;
        data_off += sym_strtab_size;
        shdr_idx++;

        symtab_shdr_idx = shdr_idx;
        data_off = ELF_ALIGN(data_off, 8);
        write_elf_section_header(&shdrs[shdr_idx], shstrtab_offset(shstrtab, ".symtab"), SHT_SYMTAB, 0, 0, data_off, symtab_size, (uint32_t)strtab_shdr_idx, 0, 8, sizeof(Elf64_Sym));
        shdrs[shdr_idx].sh_offset = data_off;
        data_off += ELF_ALIGN(symtab_size, 8);
        shdr_idx++;
    }

    if (rela && rela_count > 0) {
        rela_shdr_idx_final = shdr_idx;
        data_off = ELF_ALIGN(data_off, 8);
        write_elf_section_header(&shdrs[shdr_idx], shstrtab_offset(shstrtab, ".rela.text"), SHT_RELA, 0, 0, data_off, rela_size, (uint32_t)symtab_shdr_idx, (uint32_t)text_shdr_idx, 8, sizeof(Elf64_Rela));
        shdrs[shdr_idx].sh_offset = data_off;
        data_off += ELF_ALIGN(rela_size, 8);
        shdr_idx++;
    }

    shdr_idx = 1;
    data_off = ELF_ALIGN(data_off, 1);
    write_elf_section_header(&shdrs[shdr_idx], shstrtab_offset(shstrtab, ".shstrtab"), SHT_STRTAB, 0, 0, data_off, shstrtab_size, 0, 0, 1, 0);
    shdrs[shdr_idx].sh_offset = data_off;

    size_t sh_off = ELF_ALIGN(data_off + shstrtab_size, 8);
    size_t total = ELF_ALIGN(sh_off + num_sections * sizeof(Elf64_Shdr), 16);

    uint8_t * buf = calloc(1, total);
    if (!buf) { free(shdrs); free(shstrtab); free(sym_strtab); free(symtab); free(rela); return PULSE_ERROR_ALLOCATION_FAILED; }

    write_elf_header(buf, num_sections, ctx->arch);

    Elf64_Ehdr * ehdr = (Elf64_Ehdr *)buf;
    ehdr->e_shstrndx = 1;

    sec = ctx->sections;
    for (size_t i = 0; i < num_user_secs && sec; i++) {
        size_t off = shdrs[i + 2].sh_offset;
        if (sec->size > 0)
            memcpy(buf + off, sec->data, sec->size);
        sec = sec->next;
    }

    size_t off = shdrs[strtab_shdr_idx].sh_offset;
    memcpy(buf + off, sym_strtab, sym_strtab_size);

    off = shdrs[symtab_shdr_idx].sh_offset;
    memcpy(buf + off, symtab, symtab_size);

    if (rela && rela_count > 0) {
        off = shdrs[rela_shdr_idx_final].sh_offset;
        memcpy(buf + off, rela, rela_size);
    }

    off = shdrs[1].sh_offset;
    memcpy(buf + off, shstrtab, shstrtab_size);

    memcpy(buf + sh_off, shdrs, num_sections * sizeof(Elf64_Shdr));
    ehdr->e_shoff = sh_off;

    free(shdrs);
    free(shstrtab);
    free(sym_strtab);
    free(symtab);
    free(rela);

    *out_data = buf;
    *out_size = total;
    return PULSE_SUCCESS;
}
