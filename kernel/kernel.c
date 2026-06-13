#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "../drivers/keyboard.h"

void kernel_main(void) {
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

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("\nReady.\n");

    /* Input line buffer */
    char    input[256];
    (void)input;
    uint8_t input_len = 0;

    /* Print first prompt and record cursor position as the min */
    vga_set_color(VGA_COLOR_WHITE);
    kprint("> ");
    uint8_t prompt_col = vga_get_col();
    uint8_t prompt_row = vga_get_row();

    while (1) {
        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\b') {
            /* Only backspace if we have input to delete */
            if (input_len > 0) {
                input_len--;
                vga_putchar('\b');
            }
        } else if (c == '\n') {
            vga_putchar('\n');
            /* TODO: parse input buffer here (Stage 4 shell) */
            input_len = 0;
            vga_set_color(VGA_COLOR_WHITE);
            kprint("> ");
            prompt_col = vga_get_col();
            prompt_row = vga_get_row();
        } else if (input_len < 255) {
            input[input_len++] = c;
            vga_putchar(c);
        }
    }
    (void)prompt_col;
    (void)prompt_row;
}
