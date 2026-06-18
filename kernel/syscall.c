#include "syscall.h"
#include "vga.h"
#include "process.h"
#include "scheduler.h"
#include <stddef.h>

static uint32_t sys_write(int32_t fd, const char* buf, uint32_t len) {
    /* Only fd 1 (stdout) is wired up for now — no fd table yet. */
    if (fd != 1 || !buf) return (uint32_t)-1;

    for (uint32_t i = 0; i < len; i++)
        vga_putchar(buf[i]);

    return len;
}

static uint32_t sys_exit(int32_t code) {
    process_exit(code);
    return 0;  /* unreachable — process_exit() never returns */
}

static uint32_t sys_yield(void) {
    scheduler_yield();
    return 0;
}

uint32_t syscall_handler(registers_t* r) {
    switch (r->eax) {
        case SYS_WRITE:
            return sys_write((int32_t)r->ebx, (const char*)r->ecx, r->edx);
        case SYS_EXIT:
            return sys_exit((int32_t)r->ebx);
        case SYS_YIELD:
            return sys_yield();
        default:
            vga_set_color(VGA_COLOR_LIGHT_RED);
            kprint("[syscall] unknown number: ");
            kprint_int((int32_t)r->eax);
            kprint("\n");
            vga_set_color(VGA_COLOR_LIGHT_GREY);
            return (uint32_t)-1;
    }
}