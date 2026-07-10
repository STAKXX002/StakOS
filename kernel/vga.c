#include "vga.h"

#define VGA_ADDRESS 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static uint16_t *vga_buf = (uint16_t *)VGA_ADDRESS;
static size_t vga_row = 0;
static size_t vga_col = 0;
static uint8_t vga_color = 0;

static uint8_t vga_make_color(uint8_t fg, uint8_t bg) { return fg | (bg << 4); }

static uint16_t vga_make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_ROWS; y++)
        for (size_t x = 0; x < VGA_COLS; x++)
            vga_buf[(y - 1) * VGA_COLS + x] = vga_buf[y * VGA_COLS + x];
    for (size_t x = 0; x < VGA_COLS; x++)
        vga_buf[(VGA_ROWS - 1) * VGA_COLS + x] = vga_make_entry(' ', vga_color);
    vga_row = VGA_ROWS - 1;
}

void vga_init(void) {
    vga_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (size_t y = 0; y < VGA_ROWS; y++)
        for (size_t x = 0; x < VGA_COLS; x++)
            vga_buf[y * VGA_COLS + x] = vga_make_entry(' ', vga_color);
    vga_row = 0;
    vga_col = 0;
}

void vga_set_color(uint8_t fg) { vga_color = vga_make_color(fg, VGA_COLOR_BLACK); }

void vga_putchar(char c) {
    if (c == '\b') {
        if (vga_col > 0)
            vga_col--;
        else if (vga_row > 0) {
            vga_row--;
            vga_col = VGA_COLS - 1;
        }
        vga_buf[vga_row * VGA_COLS + vga_col] = vga_make_entry(' ', vga_color);
        return;
    }
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row == VGA_ROWS)
            vga_scroll();
        return;
    }
    vga_buf[vga_row * VGA_COLS + vga_col] = vga_make_entry(c, vga_color);
    if (++vga_col == VGA_COLS) {
        vga_col = 0;
        if (++vga_row == VGA_ROWS)
            vga_scroll();
    }
}

void kprint(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++)
        vga_putchar(str[i]);
}

void kprint_hex(uint32_t val) {
    kprint("0x");
    char buf[8];
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = val & 0xF;
        buf[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        val >>= 4;
    }
    for (int i = 0; i < 8; i++)
        vga_putchar(buf[i]);
}

void kprint_int(int32_t val) {
    if (val < 0) {
        vga_putchar('-');
        val = -val;
    }
    if (val == 0) {
        vga_putchar('0');
        return;
    }
    char buf[10];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--)
        vga_putchar(buf[j]);
}

void kpanic(const char *msg, const char *detail) {
    vga_set_color(VGA_COLOR_RED);
    kprint("\n[KERNEL PANIC] ");
    kprint(msg);
    if (detail)
        kprint(detail);
    kprint("\n--- System Halted ---\n");
    __asm__ volatile("cli; hlt");
}

uint8_t vga_get_col(void) { return (uint8_t)vga_col; }
uint8_t vga_get_row(void) { return (uint8_t)vga_row; }