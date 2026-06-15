#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/*
 * Initialise the PIT (Programmable Interval Timer 8253/8254).
 * Programs channel 0 to fire IRQ0 at `hz` times per second.
 * Typical values: 100 (10ms tick), 250 (4ms), 1000 (1ms).
 */
void pit_init(uint32_t hz);

/* Returns the total number of ticks since pit_init(). */
uint32_t pit_ticks(void);

#endif