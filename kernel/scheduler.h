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
 * Move a process from the ready queue onto the zombie list. Called by
 * process_exit() instead of scheduler_dequeue() directly — a zombie
 * can't free its own PCB/stack/page directory (it's still running on
 * them), so it stays discoverable here until scheduler_tick() reaps
 * it safely from a different process's context.
 */
void scheduler_mark_zombie(process_t* proc);

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

/*
 * Returns the head of the round-robin ready queue (NULL if empty).
 * Exposed read-only so other subsystems (e.g. 'ps') can enumerate every
 * known process instead of only whichever one is currently running.
 */
process_t* scheduler_queue_head(void);

#endif