#include "commands.h"
#include "../kernel/vga.h"
#include "../kernel/process.h"
#include "../kernel/paging.h"
#include "../kernel/gdt.h"
#include "../kernel/usermode.h"
#include "../kernel/syscall_wrapper.h"
#include "../kernel/elf.h"
#include <stdint.h>

/* ---- synctest ---- */

/*
 * cmd_synctest — exercises the int 0x80 syscall gate end to end.
 * Proves: the IDT gate reaches syscall_handler, arguments survive
 * the ebx/ecx/edx convention, and the dispatcher's return value
 * comes back through iret into eax.
 */
void cmd_synctest(int argc, char** argv) {
    (void)argc; (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("Syscall gate test\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);

    const char* msg = "  hello from sys_write via int 0x80\n";
    uint32_t len = 0;
    while (msg[len]) len++;

    uint32_t written = do_sys_write(1, msg, len);

    kprint("  sys_write returned: ");
    kprint_int((int32_t)written);
    kprint(written == len ? "  [OK]\n" : "  [MISMATCH]\n");

    kprint("  calling sys_yield()...\n");
    do_sys_yield();
    kprint("  back from sys_yield  [OK]\n");
}

/*
 * ---- usertest: real ring-3 execution via the syscall gate ----
 *
 * user_test_fn runs at CPL=3. It cannot call kprint, vga_set_color,
 * or any kernel function directly — those addresses aren't mapped
 * as user-accessible, and even if they were, calling kernel code
 * from ring 3 without a proper gate is exactly the hole syscalls
 * exist to close. The ONLY way this function can do anything
 * observable is through int 0x80, via the do_sys_* wrappers — which
 * work unchanged at CPL=3 because the IDT gate's DPL=3 permits it.
 *
 * This is a plain function, not `naked` — it needs a normal prologue
 * so the do_sys_write()/do_sys_exit() calls (which themselves compile
 * to inline `int 0x80`) have a valid stack frame to work from. We
 * jump here via iret, not via `call`, so there's no return address
 * expectation to worry about — the function just never returns.
 */
static uint8_t user_test_stack[4096] __attribute__((aligned(4096)));

static void user_test_fn(void) {
    const char* msg = "  hello from ring 3 via int 0x80\n";
    uint32_t len = 0;
    while (msg[len]) len++;

    do_sys_write(1, msg, len);
    do_sys_exit(0);

    /* unreachable — do_sys_exit calls process_exit(), which never returns */
    for (;;) { }
}

/*
 * usertest_launcher runs as a normal kernel-mode process (created via
 * process_create, scheduled like any other process). Its only job is
 * to mark the test code/stack pages as user-accessible and perform
 * the ring-3 jump. It never returns — user_test_fn ends with
 * do_sys_exit(), which calls process_exit() and removes this whole
 * process from the scheduler cleanly.
 */
static void usertest_launcher(void) {
    tss_set_kernel_stack(process_current()->kernel_stack_top);

    uint32_t fn_page = (uint32_t)user_test_fn & ~0xFFF;
    for (int i = 0; i < 4; i++)
        paging_mark_user(fn_page + i * 0x1000);

    /* user_test_stack is 4096 bytes but not necessarily page-aligned at
        its start (it's just a static array), so the region can span two
        physical pages. Mark both — the top of the stack (where esp starts
        and the first pushes land) is the part that actually gets touched
        first, and it's easy to miss if the array's base isn't page-aligned. */
    uint32_t stack_lo = (uint32_t)user_test_stack & ~0xFFF;
    uint32_t stack_hi = ((uint32_t)user_test_stack + sizeof(user_test_stack) - 1) & ~0xFFF;
    paging_mark_user(stack_lo);
    if (stack_hi != stack_lo)
        paging_mark_user(stack_hi);

    enter_usermode((uint32_t)user_test_fn,
                    (uint32_t)(user_test_stack + sizeof(user_test_stack)));

    /* unreachable */
}

void cmd_usertest(int argc, char** argv) {
    (void)argc; (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("Ring-3 user mode test\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("  spawning usertest process...\n");

    process_t* p = process_create("usertest", usertest_launcher, 5);
    if (!p) {
        kprint("  process_create failed\n");
        return;
    }

    kprint("  spawned pid=");
    kprint_int((int32_t)p->pid);
    kprint(" - watch for its output on the next scheduler tick\n");
}

/* ---- elftest ---- */

/* Embedded test ELF — see boot/test_prog_blob.asm */
extern const uint8_t test_prog_blob[];

/*
 * cmd_elftest — the real pipeline. Unlike usertest (which hand-built
 * a CPL=3 function and stack from kernel-side static arrays),
 * this loads an actual ELF32 binary via process_create_from_elf(),
 * proving elf32_validate + elf32_load + paging_map_into + the ring-3
 * trampoline all work together end to end.
 */
void cmd_elftest(int argc, char** argv) {
    (void)argc; (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("ELF loader test\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("  loading embedded test_prog.elf...\n");

    process_t* p = process_create_from_elf("elftest", test_prog_blob, 5);
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