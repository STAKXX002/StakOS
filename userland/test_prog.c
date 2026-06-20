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
    syscall3(1 /* SYS_EXIT */, 0, 0, 0);

    for (;;) { }  /* unreachable */
}