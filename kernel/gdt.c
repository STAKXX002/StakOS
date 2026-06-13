#include "gdt.h"

#define GDT_ENTRIES 3

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* Declared in gdt_flush.asm */
extern void gdt_flush(uint32_t);

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[i].base_low    = (base & 0xFFFF);
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;

    gdt[i].limit_low   = (limit & 0xFFFF);
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

    gdt[i].access      = access;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_set_entry(0, 0, 0,          0x00, 0x00); /* Null descriptor      */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* Kernel code segment  */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* Kernel data segment  */

    gdt_flush((uint32_t)&gdt_ptr);
}
