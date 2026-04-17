/**
 * @file emit_elf.h
 * @brief Internal ELF format structures and helpers.
 */
#ifndef PULSE_EMIT_ELF_H
#define PULSE_EMIT_ELF_H

#include <stdint.h>
#include <stddef.h>

#define ELF_MAGIC "\x7F" "ELF"

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1

#define ET_REL 1
#define ET_EXEC 2

#define EM_X86_64 62
#define EM_AARCH64 183

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4

#define SHF_ALLOC 2
#define SHF_EXECINSTR 4
#define SHF_WRITE 1

#define ELF64_ST_TYPE(info) (((uint32_t)(info)) & 0xF)
#define ELF64_ST_INFO(bind, type) (((uint8_t)(bind) << 4) | ((type)&0xF))

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2

typedef struct {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

typedef struct {
    uint64_t r_offset;
    uint64_t r_addend;
    uint32_t r_sym;
    uint32_t r_type;
} Elf64_Rela;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7

#define PF_X (1 << 0)
#define PF_W (1 << 1)
#define PF_R (1 << 2)

#define ELF_R_X86_64_NONE 0
#define ELF_R_X86_64_64 1
#define ELF_R_X86_64_PC32 2
#define ELF_R_X86_64_PLT32 4
#define ELF_R_X86_64_GOTPCREL 9
#define ELF_R_X86_64_GOTPCRELX 33

#define ELF_R_AARCH64_NONE 256
#define ELF_R_AARCH64_JUMP_SLOT 512

#endif
