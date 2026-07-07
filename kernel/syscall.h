#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"
#include <stdint.h>

/*
 * Syscall numbers — passed in eax when the caller does `int 0x80`.
 * Args follow the classic 3-register convention: ebx, ecx, edx.
 * Keep this list append-only; never renumber an existing entry.
 */
#define SYS_WRITE 0 /* sys_write(fd, buf, len)  -> bytes written */
#define SYS_EXIT 1  /* sys_exit(code)           -> never returns */
#define SYS_YIELD 2 /* sys_yield()              -> 0             */
#define SYS_OPEN 3  /* sys_open(path)           -> fd, or -1     */
#define SYS_READ 4  /* sys_read(fd, buf, len)   -> bytes read    */
#define SYS_CLOSE 5 /* sys_close(fd)            -> 0, or -1      */
#define SYS_FORK                                                               \
  6 /* sys_fork()               -> child pid (parent),                         \
                                    0 (child), or -1  */

/* Returns the value to be placed in the caller's eax after iret. */
uint32_t syscall_handler(registers_t *r);

#endif