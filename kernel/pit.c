#include "pit.h"
#include "idt.h"
#include "scheduler.h"
#include "vga.h"
#include "io.h"

/*
 * The 8254 PIT base frequency is 1,193,182 Hz.
 * To get a tick every 1/hz seconds we program divisor = 1193182 / hz.
 */
#define PIT_BASE_FREQ   1193182
#define PIT_CMD         0x43    /* command port */
#define PIT_CHANNEL0    0x40    /* channel 0 data port */

static volatile uint32_t tick_count = 0;

/* IRQ0 handler — called every timer tick */
static void pit_handler(registers_t* regs) {
    (void)regs;
    tick_count++;
    scheduler_tick();   /* let the scheduler decide if a switch is needed */
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_BASE_FREQ / hz;

    /*
     * Command byte: 0x36
     *   bits 7-6: channel 0 (00)
     *   bits 5-4: lobyte/hibyte access mode (11)
     *   bits 3-1: mode 3 — square wave generator (011)
     *   bit  0:   binary counting (0)
     */
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));         /* low byte  */
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));  /* high byte */

    /* Register our handler for IRQ0 (mapped to IDT vector 32) */
    irq_register(0, pit_handler);

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] PIT initialized (");
    kprint_int((int32_t)hz);
    kprint(" Hz)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

uint32_t pit_ticks(void) {
    return tick_count;
}