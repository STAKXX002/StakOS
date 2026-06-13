#include "keyboard.h"
#include "../kernel/vga.h"
#include <stdint.h>

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_BUF_SIZE  256

/* Scancode set 1 - lowercase */
static const char sc_ascii[] = {
    0,    0,   '1', '2', '3', '4', '5', '6',  /* 0x00-0x07 */
    '7', '8', '9', '0', '-', '=',  '\b', '\t', /* 0x08-0x0F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   /* 0x10-0x17 */
    'o', 'p', '[', ']', '\n',  0,  'a', 's',   /* 0x18-0x1F */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 0x20-0x27 */
    '\'', '`',  0,  '\\','z', 'x', 'c', 'v',  /* 0x28-0x2F */
    'b', 'n', 'm', ',', '.', '/',   0,  '*',   /* 0x30-0x37 */
     0,  ' ',   0,   0,   0,   0,   0,   0,    /* 0x38-0x3F */
     0,   0,   0,   0,   0,   0,   0,  '7',    /* 0x40-0x47 */
    '8', '9', '-', '4', '5', '6', '+', '1',    /* 0x48-0x4F */
    '2', '3', '0', '.',  0,   0,   0,   0,     /* 0x50-0x57 */
};

/* Scancode set 1 - uppercase/shifted */
static const char sc_ascii_shift[] = {
    0,    0,   '!', '@', '#', '$', '%', '^',   /* 0x00-0x07 */
    '&', '*', '(', ')', '_', '+', '\b', '\t',  /* 0x08-0x0F */
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',   /* 0x10-0x17 */
    'O', 'P', '{', '}', '\n',  0,  'A', 'S',   /* 0x18-0x1F */
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   /* 0x20-0x27 */
    '"',  '~',  0,  '|', 'Z', 'X', 'C', 'V',  /* 0x28-0x2F */
    'B', 'N', 'M', '<', '>', '?',   0,  '*',   /* 0x30-0x37 */
     0,  ' ',   0,   0,   0,   0,   0,   0,    /* 0x38-0x3F */
     0,   0,   0,   0,   0,   0,   0,  '7',    /* 0x40-0x47 */
    '8', '9', '-', '4', '5', '6', '+', '1',    /* 0x48-0x4F */
    '2', '3', '0', '.',  0,   0,   0,   0,     /* 0x50-0x57 */
};

/* Circular input buffer */
static char    kb_buf[KEYBOARD_BUF_SIZE];
static uint8_t kb_head = 0;  /* write position */
static uint8_t kb_tail = 0;  /* read position  */
static uint8_t shift   = 0;  /* shift state    */

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void kb_buf_push(char c) {
    uint8_t next = (kb_head + 1) % KEYBOARD_BUF_SIZE;
    if (next != kb_tail) {   /* drop if full */
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

void keyboard_handler(registers_t* r) {
    (void)r;
    uint8_t sc = inb(KEYBOARD_DATA_PORT);

    /* Key release - bit 7 set */
    if (sc & 0x80) {
        uint8_t released = sc & 0x7F;
        if (released == 0x2A || released == 0x36)
            shift = 0;
        return;
    }

    /* Shift pressed */
    if (sc == 0x2A || sc == 0x36) {
        shift = 1;
        return;
    }

    /* Translate scancode to ASCII */
    if (sc < sizeof(sc_ascii)) {
        char c = shift ? sc_ascii_shift[sc] : sc_ascii[sc];
        if (c) kb_buf_push(c);
    }
}

/* Non-blocking read - returns 0 if buffer empty */
char keyboard_getchar(void) {
    if (kb_head == kb_tail) return 0;
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KEYBOARD_BUF_SIZE;
    return c;
}

void keyboard_init(void) {
    /* Nothing to configure for basic PS/2 - IRQ1 handler does the work */
}
