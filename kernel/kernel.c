#include <stdint.h>
#include <stddef.h>
#include "gdt.h"

/* VGA text mode buffer */
#define VGA_ADDRESS     0xB8000
#define VGA_COLS        80
#define VGA_ROWS        25

/* Colors */
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_WHITE         15
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_YELLOW        14

static uint16_t* vga_buf = (uint16_t*)VGA_ADDRESS;
static size_t    vga_row = 0;
static size_t    vga_col = 0;
static uint8_t   vga_color = 0;

static uint8_t vga_make_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static uint16_t vga_make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void vga_clear(void) {
    vga_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (size_t y = 0; y < VGA_ROWS; y++)
        for (size_t x = 0; x < VGA_COLS; x++)
            vga_buf[y * VGA_COLS + x] = vga_make_entry(' ', vga_color);
    vga_row = 0;
    vga_col = 0;
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_ROWS; y++)
        for (size_t x = 0; x < VGA_COLS; x++)
            vga_buf[(y-1) * VGA_COLS + x] = vga_buf[y * VGA_COLS + x];
    for (size_t x = 0; x < VGA_COLS; x++)
        vga_buf[(VGA_ROWS-1) * VGA_COLS + x] = vga_make_entry(' ', vga_color);
    vga_row = VGA_ROWS - 1;
}

static void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row == VGA_ROWS) vga_scroll();
        return;
    }
    vga_buf[vga_row * VGA_COLS + vga_col] = vga_make_entry(c, vga_color);
    if (++vga_col == VGA_COLS) {
        vga_col = 0;
        if (++vga_row == VGA_ROWS) vga_scroll();
    }
}

static void vga_set_color(uint8_t fg) {
    vga_color = vga_make_color(fg, VGA_COLOR_BLACK);
}

static void kprint(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++)
        vga_putchar(str[i]);
}

/* ── Kernel entry point ─────────────────────────────────────── */
void kernel_main(void) {
    gdt_init();
    vga_clear();

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("StakOS\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("----------------------------------------\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] Kernel loaded\n");
    kprint("[OK] VGA text mode initialized\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("\nReady.\n");

    /* Hang forever */
    for(;;);
}
