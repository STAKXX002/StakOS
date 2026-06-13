#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

/* Exactly what's on the stack when isr_common_stub calls isr_handler.
   No ss/useresp — those are only pushed on privilege level change (ring3->ring0).
   We are already in ring 0 so the CPU never pushes them. */
typedef struct {
    uint32_t ds;                                    /* pushed by us        */
    uint32_t edi,esi,ebp,esp,ebx,edx,ecx,eax;      /* pusha               */
    uint32_t int_no, err_code;                      /* pushed by ISR macro */
    uint32_t eip, cs, eflags;                       /* CPU auto-push       */
} registers_t;

void idt_init(void);

#endif
