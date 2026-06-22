/*
 * Minimal freestanding test program - compiled separately from the
 * kernel, linked as a flat ELF32 executable, then embedded into the
 * kernel image for the ELF loader to parse and run.
 *
 * No libc, no stdlib, no startup files - the only thing this can do
 * is issue raw int 0x80 syscalls, exactly matching the convention
 * kernel/syscall.c implements (eax=number, ebx/ecx/edx=args).
 */

static inline unsigned int syscall3(unsigned int num, unsigned int a,
                                     unsigned int b, unsigned int c) {
    unsigned int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a), "c"(b), "d"(c)
        : "memory"
    );
    return ret;
}

void _start(void) {
    const char* msg = "hello from a real loaded ELF binary\n";
    unsigned int len = 0;
    while (msg[len]) len++;

    syscall3(0 /* SYS_WRITE */, 1, (unsigned int)msg, len);

    /*
     * Open and read this same file (test.elf) from ring 3 — proves
     * SYS_OPEN/SYS_READ correctly handle a user-mode path pointer
     * and a user-mode read buffer while the kernel-side handler runs
     * at CPL=0 after the int 0x80 transition.
     */
    const char* path = "test.elf";
    int fd = (int)syscall3(3 /* SYS_OPEN */, (unsigned int)path, 0, 0);

    if (fd >= 0) {
        char buf[4];   /* just read the first 4 bytes: ELF magic */
        unsigned int n = syscall3(4 /* SYS_READ */, (unsigned int)fd,
                                   (unsigned int)buf, sizeof(buf));

        if (n == 4 && buf[0] == 0x7f && buf[1] == 'E' &&
            buf[2] == 'L' && buf[3] == 'F') {
            const char* ok = "ring-3 file read: ELF magic OK\n";
            unsigned int ok_len = 0;
            while (ok[ok_len]) ok_len++;
            syscall3(0, 1, (unsigned int)ok, ok_len);
        } else {
            const char* bad = "ring-3 file read: MISMATCH\n";
            unsigned int bad_len = 0;
            while (bad[bad_len]) bad_len++;
            syscall3(0, 1, (unsigned int)bad, bad_len);
        }

        syscall3(5 /* SYS_CLOSE */, (unsigned int)fd, 0, 0);
    } else {
        const char* fail = "ring-3 file open: FAILED\n";
        unsigned int fail_len = 0;
        while (fail[fail_len]) fail_len++;
        syscall3(0, 1, (unsigned int)fail, fail_len);
    }

    syscall3(1 /* SYS_EXIT */, 0, 0, 0);

    for (;;) { }  /* unreachable */
}