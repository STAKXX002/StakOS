#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

/*
 * Initialise the scheduler.
 * Called once after process_init().
 */
void scheduler_init(void);

/*
 * Add a process to the ready queue.
 * Called when a process is created or unblocked.
 */
void scheduler_enqueue(process_t* proc);

/*
 * Remove a process from the ready queue.
 * Called when a process blocks or exits.
 */
void scheduler_dequeue(process_t* proc);

/*
 * Called by the PIT IRQ0 handler every timer tick (~10ms).
 * Decrements ticks_remaining; when it hits 0 performs a context switch
 * to the next READY process.
 * Also handles waking sleeping processes (sleep_ticks countdown).
 */
void scheduler_tick(void);

/*
 * Immediately yield the CPU to the next READY process.
 * The calling process stays READY (it will run again later).
 */
void scheduler_yield(void);

/*
 * Assembly routine — performs the actual CPU context switch.
 * Saves registers of `old`, restores registers of `new`.
 * Defined in boot/context_switch.asm.
 */
void context_switch(process_t* old, process_t* new);

#endif