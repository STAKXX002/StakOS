#include "idt.h"
#include "vga.h"

void syscall_handler(registers_t* r) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("[syscall] gate fired, eax=");
    kprint_int((int32_t)r->eax);
    kprint("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}