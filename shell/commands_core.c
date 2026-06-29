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