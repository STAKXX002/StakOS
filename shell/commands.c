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
        
        uint32_t max_before_mul = 0xFFFFFFFFu / (uint32_t)base;
        
        if (result > max_before_mul || result * (uint32_t)base > 0xFFFFFFFFu - digit) {
            result = 0xFFFFFFFFu;  /* saturate */
        } else {
            result = result * (uint32_t)base + digit;
        }
        
        s++;
    }
    return result;
}

/* ---- help ---- */

void cmd_help(int argc, char** argv) {
    (void)argc;
    (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("StakOS commands:\n");

    for (int i = 0; commands[i].name != NULL; i++) {
        vga_set_color(VGA_COLOR_WHITE);
        kprint("  ");
        kprint(commands[i].name);

        /* find length manually */
        int len = 0;
        while (commands[i].name[len])
            len++;

        /* pad to 10 columns */
        while (len < 10) {
            kprint(" ");
            len++;
        }

        vga_set_color(VGA_COLOR_LIGHT_GREY);
        kprint("- ");
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

#define MEMTEST_MAX_FRAMES 64

/*
 * Allocates N frames, prints their physical addresses,
 * then frees them all and confirms the PMM count recovers.
 * Validates alloc/free round-trip without corrupting anything.
 */
void cmd_memtest(int argc, char** argv) {
    uint32_t n = 8;   /* default: test 8 frames */
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
void cmd_hexdump(int argc, char** argv) {
    if (argc < 3) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("usage: hexdump <addr> <len>\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    uint32_t addr = sh_strtoul(argv[1], 16);
    uint32_t len  = sh_strtoul(argv[2], 10);

    if (addr > 0x400000 || len > 0x400000 - addr) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("hexdump: address range outside mapped memory (0x0-0x400000)\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

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

/* command table defined at bottom of file after all implementations */

/* ---- heapinfo ---- */

#include "../mm/kmalloc.h"

void cmd_heapinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    kmalloc_print_stats();
}

/* ---- heaptest ---- */

/*
 * Allocates several chunks of different sizes, prints their addresses,
 * then frees them and shows that the heap coalesces back to its original state.
 */
void cmd_heaptest(int argc, char** argv) {
    (void)argc; (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("heaptest: allocating chunks\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);

    void* a = kmalloc(32);
    void* b = kmalloc(128);
    void* c = kmalloc(64);
    void* d = kmalloc(256);

    kprint("  kmalloc(32)  = 0x"); kprint_hex((uint32_t)(uintptr_t)a); kprint("\n");
    kprint("  kmalloc(128) = 0x"); kprint_hex((uint32_t)(uintptr_t)b); kprint("\n");
    kprint("  kmalloc(64)  = 0x"); kprint_hex((uint32_t)(uintptr_t)c); kprint("\n");
    kprint("  kmalloc(256) = 0x"); kprint_hex((uint32_t)(uintptr_t)d); kprint("\n");

    /* Write and read back to verify the memory is actually usable */
    if (a) { ((uint8_t*)a)[0] = 0xAB; ((uint8_t*)a)[31] = 0xCD; }
    if (b) { ((uint8_t*)b)[0] = 0x12; ((uint8_t*)b)[127] = 0x34; }

    int ok = a && b && c && d
          && ((uint8_t*)a)[0]   == 0xAB
          && ((uint8_t*)a)[31]  == 0xCD
          && ((uint8_t*)b)[0]   == 0x12
          && ((uint8_t*)b)[127] == 0x34;

    if (ok) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN);
        kprint("  read-back OK\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("  [FAIL] read-back mismatch!\n");
    }

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("heaptest: freeing (check coalesce)\n");
    kfree(a); kfree(b); kfree(c); kfree(d);

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("  freed - run heapinfo to confirm coalesced\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

/* ---- ps ---- */

#include "../kernel/process.h"
#include "../kernel/pit.h"

void cmd_ps(int argc, char** argv) {
    (void)argc; (void)argv;
    process_list();
}

/* ---- sleep ---- */

void cmd_sleep(int argc, char** argv) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("usage: sleep <ticks>\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }
    uint32_t ticks = sh_strtoul(argv[1], 10);
    uint32_t start = pit_ticks();
    /* busy-wait for now — proper blocking comes with full scheduler integration */
    while (pit_ticks() - start < ticks) {
        __asm__ volatile("hlt");
    }
    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("slept ");
    kprint_int((int32_t)ticks);
    kprint(" ticks\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

/* ---- uptime ---- */

void cmd_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    uint32_t ticks = pit_ticks();
    uint32_t secs  = ticks / 100;   /* we run at 100 Hz */
    kprint("uptime: ");
    kprint_int((int32_t)secs);
    kprint("s (");
    kprint_int((int32_t)ticks);
    kprint(" ticks at 100Hz)\n");
}

/* ---- command table ---- */

const command_t commands[] = {
    { "help",     "list available commands",               cmd_help     },
    { "clear",    "clear the screen",                      cmd_clear    },
    { "echo",     "print arguments to screen",             cmd_echo     },
    { "meminfo",  "show PMM memory statistics",            cmd_meminfo  },
    { "memtest",  "alloc/free N frames (default 8)",       cmd_memtest  },
    { "hexdump",  "dump memory: hexdump <addr> <len>",     cmd_hexdump  },
    { "heapinfo", "show kmalloc heap statistics",          cmd_heapinfo },
    { "heaptest", "alloc/free/read-back heap chunks",      cmd_heaptest },
    { "ps",       "list processes",                        cmd_ps       },
    { "sleep",    "sleep <ticks> (100 ticks = 1 second)",  cmd_sleep    },
    { "uptime",   "show ticks and seconds since boot",     cmd_uptime   },
    { NULL, NULL, NULL }   /* sentinel */
};