; context_switch(process_t* old, process_t* new)
;
; C calling convention (cdecl) on x86:
;   [esp+4]  = old  (process_t* of the process being switched away from)
;   [esp+8]  = new  (process_t* of the process being switched to)
;
; process_t layout (must match process.h):
;   offset 0  : pid          (uint32_t)
;   offset 4  : name[32]     (char[32])
;   offset 36 : state        (uint32_t enum)
;   offset 40 : esp          (uint32_t)  ← saved kernel stack pointer
;   offset 44 : cr3          (uint32_t)  ← page directory physical address
;
; The trick: we don't save every register individually into the struct.
; Instead we PUSH them all onto the current stack, then save ESP.
; On restore we load the new ESP, then POP everything back.
; This way the full CPU context lives on each process's own kernel stack.
;
; Stack layout after our pushes (growing downward):
;   [esp]    eflags
;   [esp+4]  eax
;   [esp+8]  ecx
;   [esp+12] edx
;   [esp+16] ebx
;   [esp+20] ebp  (original, before our frame)
;   [esp+24] esi
;   [esp+28] edi
;   [esp+32] eip  (return address — already on stack from the 'call' instruction)
;
; When we later restore and ret, eip pops off and execution resumes
; exactly where the old process called context_switch.

global context_switch
context_switch:
    ; ---- SAVE old process ----

    ; At entry, the call instruction already pushed EIP (return address).
    ; Now save all callee-saved + caller-saved registers onto current stack.
    pushfd              ; save EFLAGS
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi

    ; Get `old` pointer from stack.
    ; Stack right now: edi, esi, ebp, ebx, edx, ecx, eax, eflags, ret_eip, old*, new*
    ; That's 8 pushes (32 bytes) + ret_eip (4 bytes) = 36 bytes above old*
    mov eax, [esp + 36]     ; eax = old (process_t*)

    ; Save ESP into old->esp (offset 40)
    mov [eax + 40], esp

    ; ---- SWITCH ADDRESS SPACE ----
    ; Load new process's CR3 (offset 44) before touching new stack,
    ; so any stack access after this point uses the new address space.
    mov eax, [esp + 40]     ; eax = new (process_t*)
    mov ecx, [eax + 44]     ; ecx = new->cr3
    mov edx, cr3
    cmp ecx, edx
    je  .same_cr3           ; skip reload if same PD (e.g. two kernel threads)
    mov cr3, ecx            ; loads new PD, flushes TLB
.same_cr3:

    ; ---- RESTORE new process ----
    ; eax already = new proc pointer; reload esp from it
    mov esp, [eax + 40]

    ; Pop saved registers in reverse push order
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    popfd               ; restore EFLAGS

    ; ret pops EIP — resumes wherever the new process last called context_switch
    ; (or the entry point if this is its first run — see process_create setup)
    ret