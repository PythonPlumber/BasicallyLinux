#include "elf_loader.h"
#include "util.h"

int elf_loader_parse(const uint8_t* data, uint32_t size, elf_image_t* out) {
    if (!data || size < sizeof(Elf32_Ehdr) || !out) {
        return 0;
    }
    
    const Elf32_Ehdr* hdr = (const Elf32_Ehdr*)data;
    
    // Check Magic
    if (hdr->e_ident[0] != 0x7F || hdr->e_ident[1] != 'E' || 
        hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F') {
        return 0;
    }
    
    // Check Arch (i386 = 3)
    if (hdr->e_machine != 3) {
        return 0;
    }
    
    out->entry = hdr->e_entry;
    
    // Find image base and size
    uint32_t min_addr = 0xFFFFFFFF;
    uint32_t max_addr = 0;
    
    const Elf32_Phdr* phdr = (const Elf32_Phdr*)(data + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_vaddr < min_addr) min_addr = phdr[i].p_vaddr;
            if (phdr[i].p_vaddr + phdr[i].p_memsz > max_addr) max_addr = phdr[i].p_vaddr + phdr[i].p_memsz;
        }
    }
    
    if (min_addr < max_addr) {
        out->image_base = min_addr;
        out->image_size = max_addr - min_addr;
    } else {
        out->image_base = 0;
        out->image_size = 0;
    }
    
    return 1;
}

int elf_loader_load(const uint8_t* data, uint32_t size, elf_image_t* out) {
    if (!elf_loader_parse(data, size, out)) {
        return 0;
    }
    
    const Elf32_Ehdr* hdr = (const Elf32_Ehdr*)data;
    const Elf32_Phdr* phdr = (const Elf32_Phdr*)(data + hdr->e_phoff);
    
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // Ensure we don't read past input buffer
            if (phdr[i].p_offset + phdr[i].p_filesz > size) {
                return 0;
            }
            
            // Copy file data to memory
            // Note: This assumes the destination memory is mapped and writable!
            if (phdr[i].p_filesz > 0) {
                memcpy((void*)phdr[i].p_vaddr, data + phdr[i].p_offset, phdr[i].p_filesz);
            }
            
            // Zero out BSS (memory size > file size)
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((void*)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }
    
    return 1;
}
