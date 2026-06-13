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
