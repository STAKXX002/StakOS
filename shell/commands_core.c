#include "../kernel/io.h"
#include "../kernel/vga.h"
#include "commands.h"
#include <stdint.h>

/* ---- help ---- */

void cmd_help(int argc, char **argv) {
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

void cmd_clear(int argc, char **argv) {
  (void)argc;
  (void)argv;
  vga_init(); /* re-initialising VGA resets the cursor and blanks the screen */
}

/* ---- echo ---- */

void cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    kprint(argv[i]);
    if (i < argc - 1)
      kprint(" ");
  }
  kprint("\n");
}

/* ---- shutdown ---- */

void cmd_shutdown(int argc, char **argv) {
  (void)argc; // Fixes unused parameter warning
  (void)argv; // Fixes unused parameter warning

  vga_set_color(VGA_COLOR_LIGHT_RED);
  kprint("Shutting down StakOS...\n");

  // QEMU ACPI Shutdown
  outw(0x604, 0x2000);

  // VirtualBox / Older QEMU ACPI Shutdown
  outw(0x4004, 0x3400);

  // Bochs / Old QEMU Shutdown
  outw(0xB004, 0x2000);

  // Fallback safe halt
  vga_set_color(VGA_COLOR_LIGHT_GREY);
  kprint("Shutdown failed. It is now safe to turn off your computer.\n");

  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}