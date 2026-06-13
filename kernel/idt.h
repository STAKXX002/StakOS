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

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} registers_t;

typedef void (*irq_handler_t)(registers_t*);

void idt_init(void);
void irq_register(uint8_t irq, irq_handler_t handler);

#endif
