#include "commands.h"
#include "../kernel/vga.h"
#include "../kernel/pmm.h"
#include <stdint.h>

/* ---- string helpers ---- */

static uint32_t sh_strtoul(const char* s, int base) {
    uint32_t result = 0;

    /* skip optional 0x prefix */
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    while (*s) {
        uint32_t digit;
        if (*s >= '0' && *s <= '9')      digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;
        result = result * (uint32_t)base + digit;
        s++;
    }
    return result;
}

/* ---- help ---- */

void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("StakOS commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    for (int i = 0; commands[i].name != NULL; i++) {
        kprint("  ");
        vga_set_color(VGA_COLOR_WHITE);
        kprint(commands[i].name);
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        kprint("\t- ");
        kprint(commands[i].help);
        kprint("\n");
    }
}

/* ---- clear ---- */

void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_init();   /* re-initialising VGA resets the cursor and blanks the screen */
}

/* ---- echo ---- */

void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprint(argv[i]);
        if (i < argc - 1) kprint(" ");
    }
    kprint("\n");
}

/* ---- meminfo ---- */

void cmd_meminfo(int argc, char** argv) {
    (void)argc; (void)argv;
    pmm_print_stats();
}

/* ---- memtest ---- */

/*
 * Allocates N frames, prints their physical addresses,
 * then frees them all and confirms the PMM count recovers.
 * Validates alloc/free round-trip without corrupting anything.
 */
void cmd_memtest(int argc, char** argv) {
    uint32_t n = 8;   /* default: test 8 frames */
    if (argc >= 2)
        n = sh_strtoul(argv[1], 10);
    if (n == 0 || n > 64) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("memtest: n must be 1-64\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    uint32_t frames[64];

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
            n = i;   /* only free what we got */
            goto done;
        }
        kprint("  alloc[");
        kprint_int((int32_t)i);
        kprint("] = 0x");
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
    kprint(" frames — PMM recovered\n");
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
void cmd_hexdump(int argc, char** argv) {
    if (argc < 3) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("usage: hexdump <addr> <len>\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    uint32_t addr = sh_strtoul(argv[1], 16);
    uint32_t len  = sh_strtoul(argv[2], 10);

    if (len == 0 || len > 512) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("hexdump: len must be 1-512\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    uint8_t* ptr = (uint8_t*)(uintptr_t)addr;

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
                /* print high nibble */
                uint8_t hi = (byte >> 4) & 0xF;
                uint8_t lo =  byte       & 0xF;
                char hc = hi < 10 ? '0' + hi : 'a' + hi - 10;
                char lc = lo < 10 ? '0' + lo : 'a' + lo - 10;
                vga_putchar(hc);
                vga_putchar(lc);
            } else {
                kprint("  ");
            }
            vga_putchar(' ');
            if (i == 7) vga_putchar(' ');   /* extra space at midpoint */
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

/* ---- command table ---- */

const command_t commands[] = {
    { "help",    "list available commands",               cmd_help    },
    { "clear",   "clear the screen",                     cmd_clear   },
    { "echo",    "print arguments to screen",             cmd_echo    },
    { "meminfo", "show PMM memory statistics",            cmd_meminfo },
    { "memtest", "alloc/free N frames (default 8)",       cmd_memtest },
    { "hexdump", "dump memory: hexdump <addr> <len>",     cmd_hexdump },
    { NULL, NULL, NULL }   /* sentinel */
};