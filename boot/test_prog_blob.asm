; Embeds build/userland/test_prog.elf directly into the kernel image.
; The ELF loader (stage 10b) reads it from here as a byte array —
; there's no filesystem yet, so this is the only way to get a real
; ELF binary into the kernel's address space for now.
;
; IMPORTANT: this file must be assembled AFTER test_prog.elf exists.
; The Makefile builds userland/test_prog.c into build/userland/test_prog.elf
; before this file is assembled — see the build dependency order.

section .rodata
align 4

global test_prog_blob
test_prog_blob:
    incbin "build/userland/test_prog.elf"

global test_prog_blob_end
test_prog_blob_end:

global test_prog_blob_size
test_prog_blob_size:
    dd test_prog_blob_end - test_prog_blob