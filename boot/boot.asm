; Multiboot2 constants
MAGIC       equ 0xE85250D6
ARCH        equ 0           ; i386 protected mode
HEADER_LEN  equ (multiboot_end - multiboot_start)
CHECKSUM    equ -(MAGIC + ARCH + HEADER_LEN)

section .multiboot_header
align 8
multiboot_start:
    dd MAGIC
    dd ARCH
    dd HEADER_LEN
    dd CHECKSUM

    ; End tag - required, tells GRUB "no more tags"
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
multiboot_end:

section .bss
align 16
stack_bottom:
    resb 16384      ; 16KB stack
stack_top:

section .text
global _start
_start:
    ; Set up the stack pointer
    mov esp, stack_top

    ; Call our C kernel entry point
    extern kernel_main
    call kernel_main

    ; If kernel_main ever returns, hang forever
.hang:
    cli         ; disable interrupts
    hlt         ; halt the CPU
    jmp .hang
