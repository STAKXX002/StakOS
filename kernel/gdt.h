#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* A single GDT entry - 8 bytes */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;     /* Limit bits 0-15  */
    uint16_t base_low;      /* Base  bits 0-15  */
    uint8_t  base_mid;      /* Base  bits 16-23 */
    uint8_t  access;        /* Access byte      */
    uint8_t  granularity;   /* Flags + Limit 16-19 */
    uint8_t  base_high;     /* Base  bits 24-31 */
} gdt_entry_t;

/* Pointer structure loaded into GDTR register */
typedef struct __attribute__((packed)) {
    uint16_t limit;         /* Size of GDT - 1  */
    uint32_t base;          /* Address of GDT   */
} gdt_ptr_t;

void gdt_init(void);

#endif
