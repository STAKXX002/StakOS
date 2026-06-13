#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "../kernel/idt.h"

void keyboard_init(void);
void keyboard_handler(registers_t* r);
char keyboard_getchar(void);

#endif
