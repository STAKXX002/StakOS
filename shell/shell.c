#include "shell.h"
#include "commands.h"
#include "../kernel/vga.h"
#include "../drivers/keyboard.h"

#define INPUT_MAX 256
#define ARGV_MAX   16

static char buf[INPUT_MAX];
static int  buf_len = 0;

static int sh_strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int parse(char* line, char** argv, int max_args) {
    int argc = 0;
    char* p  = line;
    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

static void dispatch(char* line) {
    char* argv[ARGV_MAX];
    int   argc = parse(line, argv, ARGV_MAX);
    if (argc == 0) return;

    for (int i = 0; commands[i].name != NULL; i++) {
        if (sh_strcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }

    vga_set_color(VGA_COLOR_LIGHT_RED);
    kprint("unknown command: ");
    kprint(argv[0]);
    kprint("  (type 'help')\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

static void print_prompt(void) {
    vga_set_color(VGA_COLOR_WHITE);
    kprint("> ");
}

void shell_run(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("\nType 'help' for available commands.\n\n");
    print_prompt();

    while (1) {
        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\b') {
            if (buf_len > 0) { buf_len--; vga_putchar('\b'); }
        } else if (c == '\n') {
            vga_putchar('\n');
            buf[buf_len] = '\0';
            dispatch(buf);
            buf_len = 0;
            print_prompt();
        } else if (buf_len < INPUT_MAX - 1) {
            buf[buf_len++] = c;
            vga_putchar(c);
        }
    }
}
