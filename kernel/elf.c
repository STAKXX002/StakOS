#include "elf.h"
#include <stddef.h>

int elf32_validate(const uint8_t* data, uint32_t* out_entry) {
    const elf32_header_t* hdr = (const elf32_header_t*)data;

    if (hdr->magic != ELF_MAGIC)                return 0;
    if (hdr->ei_class != ELF_CLASS_32)          return 0;
    if (hdr->ei_data  != ELF_DATA_LE)           return 0;
    if (hdr->type   != ELF_TYPE_EXEC)           return 0;
    if (hdr->machine != ELF_MACHINE_386)        return 0;
    if (hdr->phentsize != sizeof(elf32_phdr_t)) return 0;
    if (hdr->phnum == 0)                        return 0;

    if (out_entry) *out_entry = hdr->entry;
    return 1;
}

const elf32_phdr_t* elf32_get_phdr(const uint8_t* data, uint32_t i) {
    const elf32_header_t* hdr = (const elf32_header_t*)data;
    if (i >= hdr->phnum) return NULL;

    const uint8_t* table = data + hdr->phoff;
    return (const elf32_phdr_t*)(table + i * hdr->phentsize);
}

uint32_t elf32_phdr_count(const uint8_t* data) {
    const elf32_header_t* hdr = (const elf32_header_t*)data;
    return hdr->phnum;
}