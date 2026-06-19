; enter_usermode(uint32_t entry, uint32_t user_stack)
;
; Drops CPL from 0 to 3 by building a fake interrupt return frame and
; executing iret. This is the standard x86 trick: iret doesn't just
; "return" — it loads CS/EIP/EFLAGS (and SS/ESP, since this is a
; privilege-level change) from whatever is on the stack, with no
; verification that an actual interrupt put them there.
;
; GDT selectors (see gdt.c):
;   entry 3 = user code, entry 4 = user data
;   selector = index * 8, with low 2 bits = requested privilege level
;   0x18 = (3*8) | 3   user code,  RPL=3
;   0x20 = (4*8) | 3   user data,  RPL=3
;
; Stack layout iret expects, pushed in this exact order (top to bottom
; as written, so push in REVERSE — last pushed is on top):
;   [esp+0]  EIP            <- where execution resumes (entry)
;   [esp+4]  CS             <- 0x18 (user code, RPL=3)
;   [esp+8]  EFLAGS         <- 0x202 (IF=1)
;   [esp+12] ESP            <- new ring-3 stack pointer (user_stack)
;   [esp+16] SS             <- 0x20 (user data, RPL=3)
;
; iret pops all five when it detects a privilege-level change (CS's
; RPL differs from the current CPL), switching SS/ESP atomically.

global enter_usermode
enter_usermode:
    ; [esp+4] = entry, [esp+8] = user_stack  (cdecl args)
    mov eax, [esp+4]     ; eax = entry point
    mov ebx, [esp+8]     ; ebx = user stack pointer

    ; Reload segment registers with user data selector | RPL 3.
    ; Must happen BEFORE the iret frame is built, while we're still
    ; in a context where mov-to-segment-register is allowed.
    mov cx, 0x20 | 3
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    ; Note: ss is NOT reloaded here — iret will set it from the frame.

    ; Build the iret frame, pushing in reverse order so the layout
    ; above ends up correct (push order: SS, ESP, EFLAGS, CS, EIP —
    ; iret pops EIP first, so EIP must be pushed last / on top).
    push dword (0x20 | 3)   ; SS  = user data, RPL 3
    push ebx                 ; ESP = user_stack
    push dword 0x202         ; EFLAGS: IF=1, reserved bit 1 always set
    push dword (0x18 | 3)   ; CS  = user code, RPL 3
    push eax                 ; EIP = entry

    iret
    ; Never returns — execution continues in user mode at `entry`.