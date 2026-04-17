/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
#define PULSE_BUILDING
#include "emit_elf.h"
#include "../emit_internals.h"
#include "common/compat_c23.h"
#include "pulse/emit/emit.h"
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
    ehdr->e_type = ET_REL;
    ehdr->e_machine = elf_get_machine(arch);
    ehdr->e_version = EV_CURRENT;
    ehdr->e_ehsize = sizeof(Elf64_Ehdr);
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

static uint8_t * build_strtab(const char * strings[], size_t num_strings, size_t * out_size) {
    size_t total = 1;
    for (size_t i = 1; i < num_strings; i++)
        if (strings[i])
            total += strlen(strings[i]) + 1;
    uint8_t * buf = calloc(1, total);
    if (!buf)
        return NULL;
    size_t off = 1;
    for (size_t i = 1; i < num_strings; i++) {
        if (strings[i]) {
            size_t len = strlen(strings[i]) + 1;
            memcpy(buf + off, strings[i], len);
            off += len;
        }
    }
    *out_size = total;
    return buf;
}

static uint8_t * build_symtab(
    emit_symbol_t * symbols, size_t num_symbols, const char * strtab_buf, size_t strtab_sz, size_t * out_size) {
    size_t total = num_symbols * sizeof(Elf64_Sym);
    uint8_t * buf = calloc(1, total);
    if (!buf)
        return NULL;
    Elf64_Sym * sym = (Elf64_Sym *)buf;
    size_t sym_idx = 1;
    emit_symbol_t * s = symbols;
    while (s && sym_idx < num_symbols) {
        if (s->name) {
            const char * match = strstr(strtab_buf, s->name);
            if (match)
                sym[sym_idx].st_name = (uint32_t)(match - strtab_buf);
        }
        sym[sym_idx].st_info = ELF64_ST_INFO(STB_GLOBAL, s->is_function ? STT_FUNC : STT_OBJECT);
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
    while (r) {
        num++;
        r = r->next;
    }
    if (num == 0) {
        *out_count = 0;
        return NULL;
    }
    uint8_t * buf = calloc(num, sizeof(Elf64_Rela));
    Elf64_Rela * ent = (Elf64_Rela *)buf;
    r = relocs;
    for (size_t i = 0; i < num && r; i++, r = r->next) {
        ent[i].r_offset = r->offset;
        size_t idx = 1;
        emit_symbol_t * s = symbols;
        while (s) {
            if (strcmp(s->name, r->symbol_name) == 0)
                break;
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
    size_t num_user_secs = 0, num_syms = 0;
    emit_section_t * sec = ctx->sections;
    while (sec) {
        num_user_secs++;
        sec = sec->next;
    }
    emit_symbol_t * sym = ctx->symbols;
    while (sym) {
        num_syms++;
        sym = sym->next;
    }
    size_t num_sections = 1 + num_user_secs + 1 + (num_syms > 0 ? 2 : 0) + (ctx->relocations ? 1 : 0);

    const char * names[32] = {"", ".shstrtab"};
    size_t ni = 2;
    sec = ctx->sections;
    while (sec && ni < 30)
        names[ni++] = sec->name;
    size_t text_shdr_idx = 0;
    if (num_syms > 0) {
        names[ni++] = ".strtab";
        names[ni++] = ".symtab";
    }
    if (ctx->relocations)
        names[ni++] = ".rela.text";

    size_t shstrtab_sz = 1;
    for (size_t i = 0; i < ni; i++)
        shstrtab_sz += strlen(names[i]) + 1;
    uint8_t * shstr_buf = calloc(1, shstrtab_sz);
    size_t sh_off = 1;
    for (size_t i = 0; i < ni; i++) {
        size_t l = strlen(names[i]) + 1;
        memcpy(shstr_buf + sh_off, names[i], l);
        sh_off += l;
    }

    const char * sym_names[32] = {""};
    sym = ctx->symbols;
    for (size_t i = 1; i <= num_syms && i < 30; i++) {
        sym_names[i] = sym->name;
        sym = sym->next;
    }
    size_t strtab_sz = 0;
    uint8_t * strtab = build_strtab(sym_names, num_syms + 1, &strtab_sz);
    size_t symtab_sz = 0;
    uint8_t * symtab = build_symtab(ctx->symbols, num_syms + 1, (char *)strtab, strtab_sz, &symtab_sz);
    size_t rela_cnt = 0;
    uint8_t * rela = build_rela(ctx->relocations, ctx->symbols, &rela_cnt);
    size_t rela_sz = rela_cnt * sizeof(Elf64_Rela);

    Elf64_Shdr * shdrs = calloc(num_sections, sizeof(Elf64_Shdr));
    size_t data_off = ELF_ALIGN(sizeof(Elf64_Ehdr) + num_sections * sizeof(Elf64_Shdr), 16);
    size_t shdr_idx = 2;
    sec = ctx->sections;
    while (sec) {
        uint64_t f = 0;
        if (sec->flags & EMIT_SECTION_FLAG_ALLOC)
            f |= SHF_ALLOC;
        if (sec->flags & EMIT_SECTION_FLAG_EXECUTE)
            f |= SHF_EXECINSTR;
        if (sec->flags & EMIT_SECTION_FLAG_WRITE)
            f |= SHF_WRITE;
        write_elf_section_header(&shdrs[shdr_idx], 0, SHT_PROGBITS, f, 0, data_off, sec->size, 0, 0, 16, 0);
        const char * m = strstr((char *)shstr_buf, sec->name);
        if (m)
            shdrs[shdr_idx].sh_name = (uint32_t)(m - (char *)shstr_buf);
        if (strcmp(sec->name, ".text") == 0)
            text_shdr_idx = shdr_idx;
        data_off += ELF_ALIGN(sec->size, 16);
        shdr_idx++;
        sec = sec->next;
    }
    size_t str_idx = 0, sym_idx = 0;
    if (num_syms > 0) {
        str_idx = shdr_idx;
        write_elf_section_header(&shdrs[shdr_idx++], 0, SHT_STRTAB, 0, 0, data_off, strtab_sz, 0, 0, 1, 0);
        const char * m = strstr((char *)shstr_buf, ".strtab");
        if (m)
            shdrs[str_idx].sh_name = (uint32_t)(m - (char *)shstr_buf);
        data_off += strtab_sz;
        sym_idx = shdr_idx;
        write_elf_section_header(
            &shdrs[shdr_idx++], 0, SHT_SYMTAB, 0, 0, data_off, symtab_sz, (uint32_t)str_idx, 1, 8, 24);
        m = strstr((char *)shstr_buf, ".symtab");
        if (m)
            shdrs[sym_idx].sh_name = (uint32_t)(m - (char *)shstr_buf);
        data_off += ELF_ALIGN(symtab_sz, 8);
    }
    if (rela_cnt > 0) {
        size_t r_idx = shdr_idx;
        write_elf_section_header(&shdrs[shdr_idx++],
                                 0,
                                 SHT_RELA,
                                 0,
                                 0,
                                 data_off,
                                 rela_sz,
                                 (uint32_t)sym_idx,
                                 (uint32_t)text_shdr_idx,
                                 8,
                                 24);
        const char * m = strstr((char *)shstr_buf, ".rela.text");
        if (m)
            shdrs[r_idx].sh_name = (uint32_t)(m - (char *)shstr_buf);
        data_off += ELF_ALIGN(rela_sz, 8);
    }
    write_elf_section_header(&shdrs[1], 0, SHT_STRTAB, 0, 0, data_off, shstrtab_sz, 0, 0, 1, 0);
    const char * m = strstr((char *)shstr_buf, ".shstrtab");
    if (m)
        shdrs[1].sh_name = (uint32_t)(m - (char *)shstr_buf);
    size_t total = ELF_ALIGN(data_off + shstrtab_sz, 16);
    uint8_t * buf = calloc(1, total);
    write_elf_header(buf, num_sections, ctx->arch);
    ((Elf64_Ehdr *)buf)->e_shoff = sizeof(Elf64_Ehdr);
    memcpy(buf + sizeof(Elf64_Ehdr), shdrs, num_sections * sizeof(Elf64_Shdr));
    sec = ctx->sections;
    for (size_t i = 0; i < num_user_secs; i++) {
        memcpy(buf + shdrs[i + 2].sh_offset, sec->data, sec->size);
        sec = sec->next;
    }
    if (num_syms > 0) {
        memcpy(buf + shdrs[str_idx].sh_offset, strtab, strtab_sz);
        memcpy(buf + shdrs[sym_idx].sh_offset, symtab, symtab_sz);
    }
    if (rela_cnt > 0)
        memcpy(buf + shdrs[shdr_idx - 1].sh_offset, rela, rela_sz);
    memcpy(buf + shdrs[1].sh_offset, shstr_buf, shstrtab_sz);
    free(shdrs);
    free(shstr_buf);
    free(strtab);
    free(symtab);
    free(rela);
    *out_data = buf;
    *out_size = total;
    return PULSE_SUCCESS;
}

pulse_status emit_write_elf_exec(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size) {
    if (!ctx || !out_data || !out_size)
        return PULSE_ERROR_INVALID_ARGUMENT;
    size_t code_sz = 0;
    for (emit_section_t * s = ctx->sections; s; s = s->next)
        code_sz = ELF_ALIGN(code_sz, 16) + s->size;
    size_t total = ELF_ALIGN(0x1000 + code_sz, 4096);
    uint8_t * buf = calloc(1, total);
    Elf64_Ehdr * ehdr = (Elf64_Ehdr *)buf;
    memcpy(ehdr->e_ident, ELF_MAGIC, 4);
    ehdr->e_ident[4] = ELFCLASS64;
    ehdr->e_ident[5] = ELFDATA2LSB;
    ehdr->e_ident[6] = EV_CURRENT;
    ehdr->e_type = ET_EXEC;
    ehdr->e_machine = elf_get_machine(ctx->arch);
    ehdr->e_version = EV_CURRENT;
    ehdr->e_entry = 0x401000 + (ctx->symbols ? ctx->symbols->value : 0);
    ehdr->e_phoff = sizeof(Elf64_Ehdr);
    ehdr->e_ehsize = sizeof(Elf64_Ehdr);
    ehdr->e_phentsize = sizeof(Elf64_Phdr);
    ehdr->e_phnum = 1;
    Elf64_Phdr * phdr = (Elf64_Phdr *)(buf + sizeof(Elf64_Ehdr));
    phdr->p_type = PT_LOAD;
    phdr->p_flags = PF_R | PF_X;
    phdr->p_vaddr = 0x400000;
    phdr->p_paddr = 0x400000;
    phdr->p_filesz = total;
    phdr->p_memsz = total;
    phdr->p_align = 0x1000;
    size_t off = 0x1000;
    for (emit_section_t * s = ctx->sections; s; s = s->next) {
        off = ELF_ALIGN(off, 16);
        memcpy(buf + off, s->data, s->size);
        off += s->size;
    }
    *out_data = buf;
    *out_size = total;
    return PULSE_SUCCESS;
}
