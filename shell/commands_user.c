#include "../kernel/elf.h"
#include "../kernel/gdt.h"
#include "../kernel/paging.h"
#include "../kernel/process.h"
#include "../kernel/syscall_wrapper.h"
#include "../kernel/usermode.h"
#include "../kernel/vga.h"
#include "commands.h"
#include <stdint.h>

/* ---- synctest ---- */

/*
 * cmd_synctest — exercises the int 0x80 syscall gate end to end.
 * Proves: the IDT gate reaches syscall_handler, arguments survive
 * the ebx/ecx/edx convention, and the dispatcher's return value
 * comes back through iret into eax.
 */
void cmd_synctest(int argc, char **argv) {
  (void)argc;
  (void)argv;

  vga_set_color(VGA_COLOR_LIGHT_CYAN);
  kprint("Syscall gate test\n");
  vga_set_color(VGA_COLOR_LIGHT_GREY);

  const char *msg = "  hello from sys_write via int 0x80\n";
  uint32_t len = 0;
  while (msg[len])
    len++;

  uint32_t written = do_sys_write(1, msg, len);

  kprint("  sys_write returned: ");
  kprint_int((int32_t)written);
  kprint(written == len ? "  [OK]\n" : "  [MISMATCH]\n");

  kprint("  calling sys_yield()...\n");
  do_sys_yield();
  kprint("  back from sys_yield  [OK]\n");
}

/*
 * ---- elftest ---- */

/* Embedded test ELF — see boot/test_prog_blob.asm */
extern const uint8_t test_prog_blob[];

/*
 * cmd_elftest — the real pipeline. Unlike a hand-built CPL=3 function
 * and stack from kernel-side static arrays (the old stage-9 approach,
 * retired — it retrofitted user-accessibility onto a fixed, hardcoded
 * page range that broke as the kernel binary grew),
 * this loads an actual ELF32 binary via process_create_from_elf(),
 * proving elf32_validate + elf32_load + paging_map_into + the ring-3
 * trampoline all work together end to end.
 */
void cmd_elftest(int argc, char **argv) {
  (void)argc;
  (void)argv;

  vga_set_color(VGA_COLOR_LIGHT_CYAN);
  kprint("ELF loader test\n");
  vga_set_color(VGA_COLOR_LIGHT_GREY);
  kprint("  loading embedded test_prog.elf...\n");

  process_t *p = process_create_from_elf("elftest", test_prog_blob, 5);
  if (!p) {
    kprint("  process_create_from_elf failed\n");
    return;
  }

  kprint("  spawned pid=");
  kprint_int((int32_t)p->pid);
  kprint(" entry=");
  kprint_hex(p->user_entry);
  kprint(" - watch for its output on the next scheduler tick\n");
}

/* ---- fstest ---- */

/*
 * cmd_fstest — exercises SYS_OPEN/SYS_READ/SYS_CLOSE from kernel
 * mode, against the same ramfs that elftest's binary is registered
 * in. Proves the fd table and ramfs lookup work before testing the
 * same path from genuine ring 3 in a later stage.
 */
void cmd_fstest(int argc, char **argv) {
  (void)argc;
  (void)argv;

  vga_set_color(VGA_COLOR_LIGHT_CYAN);
  kprint("Filesystem test\n");
  vga_set_color(VGA_COLOR_LIGHT_GREY);

  int fd = do_sys_open("test.elf");
  kprint("  open(\"test.elf\") = ");
  kprint_int(fd);
  kprint(fd >= 0 ? "  [OK]\n" : "  [FAIL]\n");
  if (fd < 0)
    return;

  uint8_t buf[16];
  uint32_t n = do_sys_read(fd, buf, sizeof(buf));
  kprint("  read() = ");
  kprint_int((int32_t)n);
  kprint(" bytes: ");
  for (uint32_t i = 0; i < n; i++) {
    char hc = "0123456789abcdef"[(buf[i] >> 4) & 0xF];
    char lc = "0123456789abcdef"[buf[i] & 0xF];
    vga_putchar(hc);
    vga_putchar(lc);
    vga_putchar(' ');
  }
  kprint("\n");

  int closed = do_sys_close(fd);
  kprint("  close() = ");
  kprint_int(closed);
  kprint(closed == 0 ? "  [OK]\n" : "  [FAIL]\n");
}