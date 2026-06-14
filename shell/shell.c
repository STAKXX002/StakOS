#include "shell.h"
#include "../kernel/vga.h"
#include "../kernel/pmm.h"
#include "../drivers/keyboard.h"

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Minimal string helpers (no libc in freestanding land)              */
/* ------------------------------------------------------------------ */

static int k_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ------------------------------------------------------------------ */
/*  Input line buffer                                                   */
/* ------------------------------------------------------------------ */

#define INPUT_MAX 256
#define ARGV_MAX   16

static char input_buf[INPUT_MAX];
static int  input_len = 0;

static void input_reset(void) {
    input_len = 0;
    input_buf[0] = '\0';
}

/* ------------------------------------------------------------------ */
/*  Prompt                                                              */
/* ------------------------------------------------------------------ */

static void print_prompt(void) {
    vga_set_color(VGA_COLOR_WHITE);
    kprint("> ");
}

/* ------------------------------------------------------------------ */
/*  Command implementations                                             */
/* ------------------------------------------------------------------ */

static void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("StakOS commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("  help               - show this message\n");
    kprint("  clear              - clear the screen\n");
    kprint("  echo <text>        - print text\n");
    kprint("  meminfo            - physical memory stats\n");
    kprint("  memtest [n]        - allocate/free n frames (default 16)\n");
    kprint("  hexdump <addr> [n] - dump n bytes at hex address (default 64)\n");
}

static void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_init();   /* re-initialise = clear + reset cursor */
}

static void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprint(argv[i]);
        if (i + 1 < argc) vga_putchar(' ');
    }
    vga_putchar('\n');
}

static void cmd_meminfo(int argc, char** argv) {
    (void)argc; (void)argv;
    pmm_print_stats();
}

/* Parse a decimal string → uint32_t.  Returns 0 on empty/invalid. */
static uint32_t parse_dec(const char* s) {
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

/* Parse a hex string (with or without 0x prefix) → uint32_t. */
static uint32_t parse_hex(const char* s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint32_t v = 0;
    while (1) {
        char c = *s++;
        if      (c >= '0' && c <= '9') v = (v << 4) | (c - '0');
        else if (c >= 'a' && c <= 'f') v = (v << 4) | (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = (v << 4) | (c - 'A' + 10);
        else break;
    }
    return v;
}

static void cmd_memtest(int argc, char** argv) {
    uint32_t n = (argc >= 2) ? parse_dec(argv[1]) : 16;
    if (n == 0 || n > 512) {
        vga_set_color(VGA_COLOR_YELLOW);
        kprint("memtest: n must be 1-512\n");
        return;
    }

    /* Use a fixed-size on-stack array; 512 uint32_t = 2 KB */
    uint32_t frames[512];
    uint32_t allocated = 0;

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("Allocating ");
    kprint_int((int32_t)n);
    kprint(" frames... ");

    for (uint32_t i = 0; i < n; i++) {
        uint32_t f = pmm_alloc_frame();
        if (!f) { kprint("\nOut of memory after "); kprint_int((int32_t)i); kprint(" frames\n"); break; }
        frames[allocated++] = f;
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("OK\n");

    /* Write a canary to each frame and verify */
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("Writing canary values... ");
    for (uint32_t i = 0; i < allocated; i++) {
        uint32_t* p = (uint32_t*)(uintptr_t)frames[i];
        *p = 0xDEADBEEF ^ frames[i];   /* unique value per frame */
    }
    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("OK\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("Verifying canary values... ");
    uint32_t bad = 0;
    for (uint32_t i = 0; i < allocated; i++) {
        uint32_t* p   = (uint32_t*)(uintptr_t)frames[i];
        uint32_t  exp = 0xDEADBEEF ^ frames[i];
        if (*p != exp) bad++;
    }
    if (bad == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN);
        kprint("OK\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("FAIL (");
        kprint_int((int32_t)bad);
        kprint(" bad frames)\n");
    }

    /* Free all frames */
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("Freeing... ");
    for (uint32_t i = 0; i < allocated; i++)
        pmm_free_frame(frames[i]);
    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("OK\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("memtest: ");
    kprint_int((int32_t)allocated);
    kprint(" frames tested, ");
    kprint_int((int32_t)(allocated - bad));
    kprint(" passed\n");
}

static void cmd_hexdump(int argc, char** argv) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_YELLOW);
        kprint("usage: hexdump <addr> [nbytes]\n");
        return;
    }
    uint32_t addr = parse_hex(argv[1]);
    uint32_t len  = (argc >= 3) ? parse_dec(argv[2]) : 64;
    if (len > 256) len = 256;   /* cap to avoid flooding the screen */

    uint8_t* p = (uint8_t*)(uintptr_t)addr;

    for (uint32_t i = 0; i < len; i += 16) {
        /* Address column */
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        kprint_hex(addr + i);
        kprint("  ");

        /* Hex bytes */
        vga_set_color(VGA_COLOR_WHITE);
        for (uint32_t j = 0; j < 16; j++) {
            if (i + j < len) {
                uint8_t byte = p[i + j];
                /* manual 2-digit hex */
                uint8_t hi = byte >> 4, lo = byte & 0xF;
                vga_putchar(hi < 10 ? '0' + hi : 'A' + hi - 10);
                vga_putchar(lo < 10 ? '0' + lo : 'A' + lo - 10);
            } else {
                kprint("  ");
            }
            vga_putchar(' ');
            if (j == 7) vga_putchar(' ');   /* extra space mid-row */
        }

        /* ASCII column */
        vga_set_color(VGA_COLOR_LIGHT_CYAN);
        kprint(" |");
        for (uint32_t j = 0; j < 16 && i + j < len; j++) {
            uint8_t c = p[i + j];
            vga_putchar(c >= 32 && c < 127 ? (char)c : '.');
        }
        kprint("|\n");
    }
}

/* ------------------------------------------------------------------ */
/*  Dispatch table                                                      */
/* ------------------------------------------------------------------ */

typedef void (*cmd_fn_t)(int argc, char** argv);

typedef struct {
    const char* name;
    cmd_fn_t    fn;
} command_t;

static const command_t commands[] = {
    { "help",    cmd_help    },
    { "clear",   cmd_clear   },
    { "echo",    cmd_echo    },
    { "meminfo", cmd_meminfo },
    { "memtest", cmd_memtest },
    { "hexdump", cmd_hexdump },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/* ------------------------------------------------------------------ */
/*  Parser: split input_buf into argv tokens (space-delimited)         */
/* ------------------------------------------------------------------ */

static void dispatch(void) {
    if (input_len == 0) return;

    char*    argv[ARGV_MAX];
    int      argc = 0;
    char*    p    = input_buf;

    while (*p && argc < ARGV_MAX) {
        /* Skip leading spaces */
        while (*p == ' ') p++;
        if (!*p) break;
        /* Token starts here */
        argv[argc++] = p;
        /* Advance to next space or end */
        while (*p && *p != ' ') p++;
        if (*p == ' ') *p++ = '\0';   /* null-terminate the token */
    }

    if (argc == 0) return;

    /* Look up and call */
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (k_strcmp(argv[0], commands[i].name) == 0) {
            vga_set_color(VGA_COLOR_LIGHT_GREY);
            commands[i].fn(argc, argv);
            return;
        }
    }

    /* Unknown command */
    vga_set_color(VGA_COLOR_LIGHT_RED);
    kprint("unknown command: ");
    kprint(argv[0]);
    kprint("  (type 'help')\n");
}

/* ------------------------------------------------------------------ */
/*  Main shell loop                                                     */
/* ------------------------------------------------------------------ */

void shell_run(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("Type 'help' for available commands.\n\n");
    print_prompt();
    input_reset();

    while (1) {
        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                input_buf[input_len] = '\0';
                vga_putchar('\b');
            }
        } else if (c == '\n') {
            vga_putchar('\n');
            input_buf[input_len] = '\0';
            dispatch();
            input_reset();
            print_prompt();
        } else if (input_len < INPUT_MAX - 1) {
            input_buf[input_len++] = c;
            input_buf[input_len]   = '\0';
            vga_putchar(c);
        }
    }
}