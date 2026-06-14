#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"
#include "../shell/shell.h"
#include "../drivers/keyboard.h"

#define MULTIBOOT2_MAGIC 0x36D76289

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

    shell_run();   /* never returns */
}