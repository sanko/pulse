/**
 * Copyright (c) 2025 Sanko Robinson
 *
 * This source code is dual-licensed under the Artistic License 2.0 or the MIT License.
 * You may choose to use the code under the terms of either license.
 *
 * SPDX-License-Identifier: (Artistic-2.0 OR MIT)
 */
/**
 * @file emit_elf.h
 * @brief ELF (Executable and Linkable Format) structures and constants.
 *
 * This header defines ELF64 structures and constants for generating
 * ELF binaries on Unix-like systems. It follows the ELF64 specification
 * for little-endian 64-bit systems.
 */
#ifndef PULSE_EMIT_ELF_H
#define PULSE_EMIT_ELF_H

#include <stddef.h>
#include <stdint.h>

/** @brief ELF magic number signature */
#define ELF_MAGIC \
    "\x7F"        \
    "ELF"

/** @brief 64-bit object file class */
#define ELFCLASS64 2
/** @brief Little-endian byte order */
#define ELFDATA2LSB 1
/** @brief Current ELF version */
#define EV_CURRENT 1

/** @brief Relocatable object file */
#define ET_REL 1
/** @brief Executable file */
#define ET_EXEC 2

/** @brief x86-64 architecture */
#define EM_X86_64 62
/** @brief ARM64 architecture */
#define EM_AARCH64 183

/** @brief Section header type: undefined */
#define SHT_NULL 0
/** @brief Section header type: program data */
#define SHT_PROGBITS 1
/** @brief Section header type: symbol table */
#define SHT_SYMTAB 2
/** @brief Section header type: string table */
#define SHT_STRTAB 3
/** @brief Section header type: relocation entries with addend */
#define SHT_RELA 4

/** @brief Section flag: occupies memory during execution */
#define SHF_ALLOC 2
/** @brief Section flag: contains executable instructions */
#define SHF_EXECINSTR 4
/** @brief Section flag: writable */
#define SHF_WRITE 1

/** @brief Extract symbol type from st_info */
#define ELF64_ST_TYPE(info) (((uint32_t)(info)) & 0xF)
/** @brief Create st_info value from binding and type */
#define ELF64_ST_INFO(bind, type) (((uint8_t)(bind) << 4) | ((type) & 0xF))

/** @brief Symbol binding: local scope */
#define STB_LOCAL 0
/** @brief Symbol binding: global scope */
#define STB_GLOBAL 1
/** @brief Symbol binding: weak (overridable) */
#define STB_WEAK 2

/** @brief Symbol type: no type */
#define STT_NOTYPE 0
/** @brief Symbol type: data object */
#define STT_OBJECT 1
/** @brief Symbol type: function */
#define STT_FUNC 2

/**
 * @brief ELF64 file header.
 *
 * Located at the beginning of every ELF file, this structure contains
 * basic file information and pointers to other parts of the file.
 */
typedef struct {
    uint8_t e_ident[16];  /**< Magic number and other info */
    uint16_t e_type;      /**< Object file type */
    uint16_t e_machine;   /**< Architecture */
    uint32_t e_version;   /**< Object file version */
    uint64_t e_entry;     /**< Entry point virtual address */
    uint64_t e_phoff;     /**< Program header table file offset */
    uint64_t e_shoff;     /**< Section header table file offset */
    uint32_t e_flags;     /**< Processor-specific flags */
    uint16_t e_ehsize;    /**< ELF header size */
    uint16_t e_phentsize; /**< Program header table entry size */
    uint16_t e_phnum;     /**< Program header table entry count */
    uint16_t e_shentsize; /**< Section header table entry size */
    uint16_t e_shnum;     /**< Section header table entry count */
    uint16_t e_shstrndx;  /**< Section header string table index */
} Elf64_Ehdr;

/**
 * @brief ELF64 section header.
 *
 * Describes a section in the ELF file, including its type, flags,
 * and location in the file.
 */
typedef struct {
    uint32_t sh_name;      /**< Section name (string table index) */
    uint32_t sh_type;      /**< Section type */
    uint64_t sh_flags;     /**< Section flags */
    uint64_t sh_addr;      /**< Section virtual address at execution */
    uint64_t sh_offset;    /**< Section file offset */
    uint64_t sh_size;      /**< Section size in bytes */
    uint32_t sh_link;      /**< Link to another section */
    uint32_t sh_info;      /**< Additional section information */
    uint64_t sh_addralign; /**< Section alignment */
    uint64_t sh_entsize;   /**< Entry size if section holds table */
} Elf64_Shdr;

/**
 * @brief ELF64 symbol table entry.
 *
 * Represents a symbol (function or variable) in the object file.
 */
typedef struct {
    uint32_t st_name;  /**< Symbol name (string table index) */
    uint8_t st_info;   /**< Symbol type and binding */
    uint8_t st_other;  /**< Symbol visibility */
    uint16_t st_shndx; /**< Section index */
    uint64_t st_value; /**< Symbol value (address or offset) */
    uint64_t st_size;  /**< Associated object size */
} Elf64_Sym;

/**
 * @brief ELF64 relocation entry with addend.
 *
 * Describes a relocation to be performed during linking.
 */
typedef struct {
    uint64_t r_offset; /**< Address of the relocation */
    uint64_t r_addend; /**< Addend used to compute the new value */
    uint32_t r_sym;    /**< Symbol index */
    uint32_t r_type;   /**< Relocation type */
} Elf64_Rela;

/**
 * @brief ELF64 program header.
 *
 * Describes a segment (loadable or non-loadable) in the executable.
 */
typedef struct {
    uint32_t p_type;   /**< Segment type */
    uint32_t p_flags;  /**< Segment flags */
    uint64_t p_offset; /**< Segment file offset */
    uint64_t p_vaddr;  /**< Segment virtual address */
    uint64_t p_paddr;  /**< Segment physical address */
    uint64_t p_filesz; /**< Segment size in file */
    uint64_t p_memsz;  /**< Segment size in memory */
    uint64_t p_align;  /**< Segment alignment */
} Elf64_Phdr;

/** @brief Program header type: unused */
#define PT_NULL 0
/** @brief Program header type: loadable segment */
#define PT_LOAD 1
/** @brief Program header type: dynamic linking information */
#define PT_DYNAMIC 2
/** @brief Program header type: interpreter path */
#define PT_INTERP 3
/** @brief Program header type: auxiliary information */
#define PT_NOTE 4
/** @brief Program header type: reserved */
#define PT_SHLIB 5
/** @brief Program header type: program header table itself */
#define PT_PHDR 6
/** @brief Program header type: thread-local storage template */
#define PT_TLS 7

/** @brief Segment flags: executable */
#define PF_X (1 << 0)
/** @brief Segment flags: writable */
#define PF_W (1 << 1)
/** @brief Segment flags: readable */
#define PF_R (1 << 2)

/** @brief x86-64 relocation: no operation */
#define ELF_R_X86_64_NONE 0
/** @brief x86-64 relocation: direct 64-bit */
#define ELF_R_X86_64_64 1
/** @brief x86-64 relocation: PC-relative 32-bit signed */
#define ELF_R_X86_64_PC32 2
/** @brief x86-64 relocation: PC-relative call to PLT */
#define ELF_R_X86_64_PLT32 4
/** @brief x86-64 relocation: PC-relative GOT entry */
#define ELF_R_X86_64_GOTPCREL 9
/** @brief x86-64 relocation: authenticated GOT entry */
#define ELF_R_X86_64_GOTPCRELX 33

/** @brief ARM64 relocation: no operation */
#define ELF_R_AARCH64_NONE 256
/** @brief ARM64 relocation: PLT entry */
#define ELF_R_AARCH64_JUMP_SLOT 512

#include "../emit_internals.h"

/**
 * @brief Writes ELF object file to a buffer.
 * @param[in] ctx The emit context.
 * @param[out] out_data Pointer to the allocated buffer.
 * @param[out] out_size Pointer to the buffer size.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_write_elf(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size);

/**
 * @brief Writes ELF executable to a buffer.
 * @param[in] ctx The emit context.
 * @param[out] out_data Pointer to the allocated buffer.
 * @param[out] out_size Pointer to the buffer size.
 * @return PULSE_SUCCESS on success.
 */
pulse_status emit_write_elf_exec(emit_context_t * ctx, uint8_t ** out_data, size_t * out_size);

#endif
