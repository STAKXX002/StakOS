#ifndef SYSCALL_WRAPPER_H
#define SYSCALL_WRAPPER_H

#include <stdint.h>
#include "syscall.h"

/*
 * Kernel-side syscall trampoline — issues `int 0x80` with the classic
 * eax=number, ebx/ecx/edx=args convention and returns whatever
 * syscall_handler placed in eax.
 *
 * Callable from kernel mode right now. Stage 9 adds the ring-3 jump;
 * once a process actually runs at CPL=3, this exact same int 0x80
 * sequence works unchanged — the instruction doesn't care which ring
 * the caller was in, only that the gate's DPL (already 3) permits it.
 */
static inline uint32_t syscall3(uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a), "c"(b), "d"(c)
        : "memory"
    );
    return ret;
}

static inline uint32_t do_sys_write(int32_t fd, const char* buf, uint32_t len) {
    return syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, len);
}

static inline void do_sys_exit(int32_t code) {
    syscall3(SYS_EXIT, (uint32_t)code, 0, 0);
}

static inline void do_sys_yield(void) {
    syscall3(SYS_YIELD, 0, 0, 0);
}

#endif