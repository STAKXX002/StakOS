extern isr_handler
extern irq_handler
extern syscall_handler

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

IRQ  0, 32
IRQ  1, 33
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ---- Syscall gate (int 0x80) ----
global isr128
isr128:
    push dword 0          ; dummy err_code, keeps registers_t layout uniform
    push dword 128         ; int_no
    jmp syscall_common_stub

isr_common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

irq_common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

syscall_common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call syscall_handler
    add esp, 4
    ; eax now holds the syscall's return value - must survive until popa

    pop ebx               ; restore saved ds into ebx, NOT eax (eax holds retval)
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    ; Overwrite the pusha'd eax slot on the stack with the return value,
    ; so popa loads it into eax for the calling process to see after iret.
    mov [esp + 28], eax

    popa
    add esp, 8
    iret

; fork_trampoline
;
; Landing pad for a freshly-forked child's FIRST scheduling. context_switch's
; `ret` lands here (its eip came from the fake call-frame process_fork()
; built). At this exact moment, esp points at a hand-built replica of a
; syscall trap frame (ds, pusha regs with eax forced to 0, int_no, err_code,
; eip, cs, eflags, esp_user, ss_user) - see process_fork() in process.c.
;
; This is byte-for-byte the same stack shape syscall_common_stub leaves
; behind right before its own epilogue, so we just run that epilogue here
; to drop straight into ring 3 at the parent's exact post-int-0x80 address.
global fork_trampoline
fork_trampoline:
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret