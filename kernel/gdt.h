#ifndef GDT_H
#define GDT_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} gdt_ptr_t;

/*
 * Task State Segment — x86 hardware requires this to exist (even though
 * we don't use hardware task-switching) because esp0/ss0 are read from
 * here automatically on any ring 3 -> ring 0 transition (interrupt,
 * syscall, exception). We update esp0 on every context switch so it
 * always points at the *current* process's kernel stack.
 */
typedef struct __attribute__((packed)) {
    uint32_t prev_tss;
    uint32_t esp0;       /* kernel stack pointer to load on ring 3->0 */
    uint32_t ss0;         /* kernel stack segment — always our kernel data selector */
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} tss_entry_t;

void gdt_init(void);

/* Called on every context switch — points the TSS at the new
   process's kernel stack so interrupts/syscalls land correctly. */
void tss_set_kernel_stack(uint32_t esp0);

#endif