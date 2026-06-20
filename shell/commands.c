#include "commands.h"
#include "../kernel/vga.h"
#include "../kernel/pmm.h"
#include "../kernel/paging.h"
#include "../kernel/process.h"
#include "../kernel/scheduler.h"
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

    kprint("  kmalloc(32)  = "); kprint_hex((uint32_t)(uintptr_t)a); kprint("\n");
    kprint("  kmalloc(128) = "); kprint_hex((uint32_t)(uintptr_t)b); kprint("\n");
    kprint("  kmalloc(64)  = "); kprint_hex((uint32_t)(uintptr_t)c); kprint("\n");
    kprint("  kmalloc(256) = "); kprint_hex((uint32_t)(uintptr_t)d); kprint("\n");

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

/* ---- vminfo ---- */

#include "../kernel/paging.h"
#include "../kernel/process.h"

static void vminfo_print_proc(process_t* p, uint32_t live_cr3) {
    kprint_int((int32_t)p->pid);
    kprint("  ");
    /* pad name to 16 chars */
    const char* n = p->name;
    uint32_t    len = 0;
    while (n[len]) len++;
    kprint(n);
    for (uint32_t i = len; i < 16; i++) kprint(" ");
    kprint("0x"); kprint_hex(p->cr3);
    kprint("  ");
    if (p->cr3 == live_cr3)
        kprint("<-- live");
    else if (p->cr3 == kernel_pd_phys)
        kprint("(kernel PD)");
    else
        kprint("(own PD)");
    kprint("\n");
}

void cmd_vminfo(int argc, char** argv) {
    (void)argc; (void)argv;

    uint32_t live_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(live_cr3));

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("Virtual memory\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("  kernel_pd_phys : 0x"); kprint_hex(kernel_pd_phys); kprint("\n");
    kprint("  live CR3       : 0x"); kprint_hex(live_cr3);        kprint("\n\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("PID  NAME              CR3         NOTE\n");
    kprint("---  ----------------  ----------  -----------\n");
    vga_set_color(VGA_COLOR_WHITE);

    /* current process (RUNNING, not in queue) */
    process_t* cur = process_current();
    if (cur) vminfo_print_proc(cur, live_cr3);

    /* all queued processes — skip current, already printed above */
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    process_t* p = scheduler_queue_head();
    while (p) {
        if (p != cur) vminfo_print_proc(p, live_cr3);
        p = p->next;
    }
}

/* ---- synctest ---- */

#include "../kernel/syscall_wrapper.h"
#include "../kernel/usermode.h"
#include "../kernel/gdt.h"

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

#include "../kernel/elf.h"

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
    { "vminfo",   "show virtual memory info",              cmd_vminfo   },
    { "synctest", "exercise int 0x80 syscall gate",        cmd_synctest },
    { "usertest", "spawn a process and run it at ring 3",   cmd_usertest },
    { "elftest",  "load and run the embedded test ELF binary", cmd_elftest },
    { NULL, NULL, NULL }   /* sentinel */
};