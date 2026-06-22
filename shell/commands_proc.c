#include "commands.h"
#include "sh_util.h"
#include "../kernel/vga.h"
#include "../kernel/process.h"
#include "../kernel/scheduler.h"
#include "../kernel/paging.h"
#include "../kernel/pit.h"
#include <stdint.h>

/* ---- ps ---- */

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