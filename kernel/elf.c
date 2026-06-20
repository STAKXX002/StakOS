#include "elf.h"
#include "paging.h"
#include "pmm.h"
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

int elf32_load(const uint8_t* data, uint32_t pd_phys) {
    uint32_t count = elf32_phdr_count(data);

    for (uint32_t i = 0; i < count; i++) {
        const elf32_phdr_t* ph = elf32_get_phdr(data, i);
        if (!ph || ph->type != PT_LOAD) continue;
        if (ph->memsz == 0) continue;

        uint32_t seg_start = ph->vaddr & ~0xFFF;
        uint32_t seg_end   = (ph->vaddr + ph->memsz + 0xFFF) & ~0xFFF;

        uint32_t flags = PTE_PRESENT | PTE_USER;
        if (ph->flags & PF_W) flags |= PTE_WRITABLE;

        for (uint32_t va = seg_start; va < seg_end; va += PAGE_SIZE) {
            uint32_t frame = pmm_alloc_frame();
            if (!frame) return 0;

            uint8_t* dst = (uint8_t*)(uintptr_t)frame;
            for (uint32_t j = 0; j < PAGE_SIZE; j++) dst[j] = 0;

            uint32_t page_va_start = va;
            uint32_t page_va_end   = va + PAGE_SIZE;
            uint32_t seg_va_start  = ph->vaddr;
            uint32_t seg_va_end    = ph->vaddr + ph->filesz;

            uint32_t copy_start = (page_va_start > seg_va_start) ? page_va_start : seg_va_start;
            uint32_t copy_end   = (page_va_end   < seg_va_end)   ? page_va_end   : seg_va_end;

            if (copy_start < copy_end) {
                uint32_t file_off = ph->offset + (copy_start - ph->vaddr);
                uint32_t dst_off  = copy_start - page_va_start;
                uint32_t copy_len = copy_end - copy_start;

                for (uint32_t j = 0; j < copy_len; j++)
                    dst[dst_off + j] = data[file_off + j];
            }

            paging_map_into(pd_phys, va, frame, flags);
        }
    }

    return 1;
}