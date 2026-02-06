#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "types.h"

// ELF Header Types
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;

#define EI_NIDENT 16

typedef struct {
    uint8_t     e_ident[EI_NIDENT];
    Elf32_Half  e_type;
    Elf32_Half  e_machine;
    Elf32_Word  e_version;
    Elf32_Addr  e_entry;
    Elf32_Off   e_phoff;
    Elf32_Off   e_shoff;
    Elf32_Word  e_flags;
    Elf32_Half  e_ehsize;
    Elf32_Half  e_phentsize;
    Elf32_Half  e_phnum;
    Elf32_Half  e_shentsize;
    Elf32_Half  e_shnum;
    Elf32_Half  e_shstrndx;
} Elf32_Ehdr;

// Program Header Types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

#define PF_X       1
#define PF_W       2
#define PF_R       4

typedef struct {
    Elf32_Word  p_type;
    Elf32_Off   p_offset;
    Elf32_Addr  p_vaddr;
    Elf32_Addr  p_paddr;
    Elf32_Word  p_filesz;
    Elf32_Word  p_memsz;
    Elf32_Word  p_flags;
    Elf32_Word  p_align;
} Elf32_Phdr;

typedef struct {
    uint32_t entry;
    uint32_t image_base;
    uint32_t image_size;
} elf_image_t;

// Parse ELF header and validate
// Returns 1 on success, 0 on failure
int elf_loader_parse(const uint8_t* data, uint32_t size, elf_image_t* out);

// Load ELF segments into current address space
// Returns 1 on success, 0 on failure
int elf_loader_load(const uint8_t* data, uint32_t size, elf_image_t* out);

#endif // ELF_LOADER_H
