#include "gdt.h"

#define GDT_ENTRIES 6

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr;
static tss_entry_t tss __attribute__((aligned(4)));

extern void gdt_flush(uint32_t);
extern void tss_flush(void);

static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low = (base & 0xFFFF);
    gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;

    gdt[i].limit_low = (limit & 0xFFFF);
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

    gdt[i].access = access;
}

static void write_tss(int i, uint16_t ss0) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss_entry_t) - 1; /* size, not an address */

    /* TSS descriptor access byte 0x89: present, DPL=0, type=32-bit TSS */
    gdt_set_entry(i, base, limit, 0x89, 0x00);

    for (uint32_t j = 0; j < sizeof(tss_entry_t); j++)
        ((uint8_t *)&tss)[j] = 0;

    tss.ss0 = ss0;
    tss.iomap_base = sizeof(tss_entry_t);
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint32_t)&gdt;

    gdt_set_entry(0, 0, 0, 0x00, 0x00);          /* Null                */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* Kernel code (ring0)  */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* Kernel data (ring0)  */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* User code   (ring3)  */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* User data   (ring3)  */
    write_tss(5, 0x10);                          /* TSS — ss0 = kernel data selector */

    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();
}

void tss_set_kernel_stack(uint32_t esp0) { tss.esp0 = esp0; }