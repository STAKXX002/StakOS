#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"
#include "process.h"
#include "scheduler.h"
#include "pit.h"
#include "io.h"
#include "../mm/kmalloc.h"
#include "../drivers/keyboard.h"
#include "../shell/shell.h"

#define MULTIBOOT2_MAGIC 0x36D76289
#define HEAP_PAGES       16    /* 16 × 4KB = 64KB kernel heap */
#define MULTIBOOT2_MAGIC 0x36D76289
#define HEAP_PAGES       16    /* 16 × 4KB = 64KB kernel heap */

/* Write a string to the QEMU/Bochs debug console (I/O port 0xE9).
   Used by CI to confirm the kernel finished booting. */
static void debug_print(const char* s) {
    while (*s) {
        outb(0xE9, (uint8_t)*s++);
    }
}

void kernel_main(uint32_t magic, uint32_t mboot_ptr) {
    vga_init();
    gdt_init();
    idt_init();
    keyboard_init();
    irq_register(1, keyboard_handler);

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("StakOS\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("----------------------------------------\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] GDT initialized\n");
    kprint("[OK] IDT initialized\n");
    kprint("[OK] PIC remapped\n");
    kprint("[OK] Interrupts enabled\n");
    kprint("[OK] Keyboard initialized\n");

    if (magic != MULTIBOOT2_MAGIC) {
        kpanic("Not booted by a Multiboot2 bootloader!", "");
        return;
    }

    pmm_init(mboot_ptr);
    pmm_print_stats();
    paging_init();
    kmalloc_init(HEAP_PAGES);
    kmalloc_print_stats();

    /* Stage 6: process subsystem */
    process_init();
    scheduler_init();
    pit_init(100);      /* 100 Hz = 10ms tick */

    debug_print("STAKOS_BOOT_OK\n");
    shell_run();        /* never returns */
}