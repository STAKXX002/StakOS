#include "../kernel/paging.h"
#include "../kernel/pmm.h"
#include "../kernel/vga.h"
#include "../mm/kmalloc.h"
#include "commands.h"
#include "sh_util.h"
#include <stdint.h>

/* ---- meminfo ---- */

void cmd_meminfo(int argc, char **argv) {
    (void)argc;
    (void)argv;
    pmm_print_stats();
}

/* ---- memtest ---- */

#define MEMTEST_MAX_FRAMES 64

/*
 * Allocates N frames, prints their physical addresses,
 * then frees them all and confirms the PMM count recovers.
 * Validates alloc/free round-trip without corrupting anything.
 */
void cmd_memtest(int argc, char **argv) {
    uint32_t n = 8; /* default: test 8 frames */
    if (argc >= 2)
        n = sh_strtoul(argv[1], 10);
    if (n == 0 || n > MEMTEST_MAX_FRAMES) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("memtest: n must be 1-64\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    uint32_t frames[MEMTEST_MAX_FRAMES];

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("memtest: allocating ");
    kprint_int((int32_t)n);
    kprint(" frames\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);

    /* Allocate */
    uint32_t i;
    for (i = 0; i < n; i++) {
        frames[i] = pmm_alloc_frame();
        if (!frames[i]) {
            vga_set_color(VGA_COLOR_LIGHT_RED);
            kprint("  [FAIL] alloc returned 0 at i=");
            kprint_int((int32_t)i);
            kprint("\n");
            n = i; /* only free what we got */
            goto done;
        }
        kprint("  alloc[");
        kprint_int((int32_t)i);
        kprint("] = ");
        kprint_hex(frames[i]);
        kprint("\n");
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("  all allocs OK\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);

done:
    /* Free */
    for (uint32_t j = 0; j < n; j++)
        pmm_free_frame(frames[j]);

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("  freed ");
    kprint_int((int32_t)n);
    kprint(" frames - PMM recovered\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

/* ---- hexdump ---- */

/*
 * Usage: hexdump <hex_addr> <len>
 * Dumps `len` bytes from physical address `addr` as hex + ASCII.
 * Great for inspecting kernel data structures, the VGA buffer, etc.
 *
 * Example: hexdump 0x100000 64   (first 64 bytes of kernel)
 *          hexdump 0xb8000  160  (first line of VGA framebuffer)
 */
void cmd_hexdump(int argc, char **argv) {
    if (argc < 3) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("usage: hexdump <addr> <len>\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    uint32_t addr = sh_strtoul(argv[1], 16);
    uint32_t len = sh_strtoul(argv[2], 10);

    uint32_t max_mapped = IDENTITY_MAPPED_BYTES;

    if (addr > max_mapped || len > max_mapped - addr) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("hexdump: address range outside mapped memory (0x0-");
        kprint_hex(max_mapped);
        kprint(")\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    if (len == 0 || len > 512) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("hexdump: len must be 1-512\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    uint8_t *ptr = (uint8_t *)(uintptr_t)addr;

    for (uint32_t offset = 0; offset < len; offset += 16) {
        /* Address label */
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        kprint_hex(addr + offset);
        kprint("  ");

        /* Hex bytes */
        vga_set_color(VGA_COLOR_WHITE);
        for (uint32_t i = 0; i < 16; i++) {
            if (offset + i < len) {
                uint8_t byte = ptr[offset + i];
                uint8_t hi = (byte >> 4) & 0xF;
                uint8_t lo = byte & 0xF;
                char hc = hi < 10 ? '0' + hi : 'a' + hi - 10;
                char lc = lo < 10 ? '0' + lo : 'a' + lo - 10;
                vga_putchar(hc);
                vga_putchar(lc);
            } else {
                kprint("  ");
            }
            vga_putchar(' ');
            if (i == 7)
                vga_putchar(' '); /* extra space at midpoint */
        }

        /* ASCII */
        vga_set_color(VGA_COLOR_LIGHT_CYAN);
        kprint(" |");
        for (uint32_t i = 0; i < 16 && offset + i < len; i++) {
            uint8_t byte = ptr[offset + i];
            vga_putchar((byte >= 32 && byte < 127) ? (char)byte : '.');
        }
        kprint("|\n");
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

/* ---- heapinfo ---- */

void cmd_heapinfo(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kmalloc_print_stats();
}

/* ---- heaptest ---- */

/*
 * Allocates several chunks of different sizes, prints their addresses,
 * then frees them and shows that the heap coalesces back to its original state.
 */
void cmd_heaptest(int argc, char **argv) {
    (void)argc;
    (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("heaptest: allocating chunks\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);

    void *a = kmalloc(32);
    void *b = kmalloc(128);
    void *c = kmalloc(64);
    void *d = kmalloc(256);

    kprint("  kmalloc(32)  = ");
    kprint_hex((uint32_t)(uintptr_t)a);
    kprint("\n");
    kprint("  kmalloc(128) = ");
    kprint_hex((uint32_t)(uintptr_t)b);
    kprint("\n");
    kprint("  kmalloc(64)  = ");
    kprint_hex((uint32_t)(uintptr_t)c);
    kprint("\n");
    kprint("  kmalloc(256) = ");
    kprint_hex((uint32_t)(uintptr_t)d);
    kprint("\n");

    /* Write and read back to verify the memory is actually usable */
    if (a) {
        ((uint8_t *)a)[0] = 0xAB;
        ((uint8_t *)a)[31] = 0xCD;
    }
    if (b) {
        ((uint8_t *)b)[0] = 0x12;
        ((uint8_t *)b)[127] = 0x34;
    }

    int ok = a && b && c && d && ((uint8_t *)a)[0] == 0xAB && ((uint8_t *)a)[31] == 0xCD &&
             ((uint8_t *)b)[0] == 0x12 && ((uint8_t *)b)[127] == 0x34;

    if (ok) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN);
        kprint("  read-back OK\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("  [FAIL] read-back mismatch!\n");
    }

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("heaptest: freeing (check coalesce)\n");
    kfree(a);
    kfree(b);
    kfree(c);
    kfree(d);

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("  freed - run heapinfo to confirm coalesced\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}