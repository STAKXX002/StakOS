#include "syscall.h"
#include "process.h"
#include "ramfs.h"
#include "scheduler.h"
#include "vga.h"
#include <stddef.h>

static uint32_t sys_write(int32_t fd, const char *buf, uint32_t len) {
    /* Only fd 1 (stdout) is wired up for now — files are read-only
       via ramfs, there's no SYS_WRITE-to-file path yet. */
    if (fd != 1 || !buf)
        return (uint32_t)-1;

    for (uint32_t i = 0; i < len; i++)
        vga_putchar(buf[i]);

    return len;
}

static uint32_t sys_exit(int32_t code) {
    process_exit(code);
    return 0; /* unreachable — process_exit() never returns */
}

static uint32_t sys_yield(void) {
    scheduler_yield();
    return 0;
}

/*
 * Finds a free slot in the current process's fd table. Returns the
 * fd number (its index), or -1 if the table is full. fd 0/1/2 are
 * skipped deliberately — they're reserved for stdin/stdout/stderr by
 * convention, even though nothing populates them yet, so the first
 * real file opens at fd 3.
 */
static int alloc_fd(process_t *proc) {
    for (int fd = 3; fd < MAX_FDS_PER_PROCESS; fd++) {
        if (!proc->fds[fd].in_use)
            return fd;
    }
    return -1;
}

static uint32_t sys_open(const char *path) {
    if (!path)
        return (uint32_t)-1;

    int file_idx = ramfs_lookup(path);
    if (file_idx < 0)
        return (uint32_t)-1; /* no such file */

    process_t *proc = process_current();
    int fd = alloc_fd(proc);
    if (fd < 0)
        return (uint32_t)-1; /* fd table full */

    proc->fds[fd].in_use = 1;
    proc->fds[fd].file_idx = file_idx;
    proc->fds[fd].offset = 0;

    return (uint32_t)fd;
}

static uint32_t sys_read(int32_t fd, uint8_t *buf, uint32_t len) {
    if (!buf)
        return (uint32_t)-1;
    if (fd < 3 || fd >= MAX_FDS_PER_PROCESS)
        return (uint32_t)-1;

    process_t *proc = process_current();
    fd_entry_t *entry = &proc->fds[fd];
    if (!entry->in_use)
        return (uint32_t)-1;

    const ramfs_file_t *file = ramfs_get(entry->file_idx);
    if (!file)
        return (uint32_t)-1;

    uint32_t remaining = file->size - entry->offset;
    uint32_t to_copy = (len < remaining) ? len : remaining;

    for (uint32_t i = 0; i < to_copy; i++)
        buf[i] = file->data[entry->offset + i];

    entry->offset += to_copy;
    return to_copy;
}

static uint32_t sys_close(int32_t fd) {
    if (fd < 3 || fd >= MAX_FDS_PER_PROCESS)
        return (uint32_t)-1;

    process_t *proc = process_current();
    fd_entry_t *entry = &proc->fds[fd];
    if (!entry->in_use)
        return (uint32_t)-1;

    entry->in_use = 0;
    entry->file_idx = -1;
    entry->offset = 0;
    return 0;
}

uint32_t syscall_handler(registers_t *r) {
    switch (r->eax) {
    case SYS_WRITE:
        return sys_write((int32_t)r->ebx, (const char *)r->ecx, r->edx);
    case SYS_EXIT:
        return sys_exit((int32_t)r->ebx);
    case SYS_YIELD:
        return sys_yield();
    case SYS_OPEN:
        return sys_open((const char *)r->ebx);
    case SYS_READ:
        return sys_read((int32_t)r->ebx, (uint8_t *)r->ecx, r->edx);
    case SYS_CLOSE:
        return sys_close((int32_t)r->ebx);
    default:
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("[syscall] unknown number: ");
        kprint_int((int32_t)r->eax);
        kprint("\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return (uint32_t)-1;
    }
}