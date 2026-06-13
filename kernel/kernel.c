#include "vga.h"
#include "gdt.h"
#include "idt.h"

void kernel_main(void) {
    vga_init();
    gdt_init();
    idt_init();

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("StakOS\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("----------------------------------------\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] GDT initialized\n");
    kprint("[OK] IDT initialized\n");
    kprint("[OK] PIC remapped\n");
    kprint("[OK] Interrupts enabled\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("\nReady.\n");

    for(;;);
}
