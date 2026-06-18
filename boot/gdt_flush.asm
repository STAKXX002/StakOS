global gdt_flush

gdt_flush:
    mov eax, [esp+4]    ; Get pointer to GDT from argument
    lgdt [eax]          ; Load it into GDTR

    ; Far jump to reload CS (code segment register)
    ; 0x08 = offset of kernel code segment in GDT (entry 1 × 8 bytes)
    jmp 0x08:.flush

.flush:
    ; Reload all data segment registers with kernel data segment
    ; 0x10 = offset of kernel data segment (entry 2 × 8 bytes)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global idt_flush
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

global tss_flush
tss_flush:
    ; TSS is GDT entry 5 → selector = 5*8 = 0x28
    ; Bit 2 set would mean LDT, not relevant here (we use 0x00 in access)
    mov ax, 0x28
    ltr ax
    ret