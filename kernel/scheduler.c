#include "scheduler.h"
#include "process.h"
#include "vga.h"


/*
 * The run queue: a circular singly-linked list of READY processes.
 * queue_head = next process to run.
 * queue_tail = last process (tail->next = queue_head for circular wrap).
 */
static process_t* queue_head = NULL;
static process_t* queue_tail = NULL;
static uint32_t   queue_size = 0;

/* ---- queue operations ---- */

void scheduler_enqueue(process_t* proc) {
    if (!proc) return;
    __asm__ volatile("cli");

    proc->state = PROCESS_READY;
    proc->next  = NULL;
    if (!queue_head) {
        queue_head = proc;
        queue_tail = proc;
    } else {
        queue_tail->next = proc;
        queue_tail       = proc;
    }
    queue_size++;

    __asm__ volatile("sti");
}

void scheduler_dequeue(process_t* proc) {
    if (!proc || !queue_head) return;

    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));

    process_t* prev = NULL;
    process_t* cur  = queue_head;
    while (cur) {
        if (cur == proc) {
            if (prev)              prev->next  = cur->next;
            else                   queue_head  = cur->next;
            if (cur == queue_tail) queue_tail  = prev;
            cur->next = NULL;
            queue_size--;
            break;
        }
        prev = cur;
        cur  = cur->next;
    }

    __asm__ volatile("push %0; popf" : : "r"(flags));
}

/* ---- scheduler_init ---- */

void scheduler_init(void) {
    queue_head = NULL;
    queue_tail = NULL;
    queue_size = 0;

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] Scheduler initialized (round-robin + priority)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

/* ---- pick next ---- */

/*
 * Pick the next READY process from the queue.
 * Simple round-robin: take head, move it to tail.
 * Priority is implemented via ticks_remaining: a process with priority N
 * gets N consecutive ticks before yielding.
 */
static process_t* pick_next(void) {
    if (!queue_head) return NULL;

    /* Rotate: take from head, put at tail */
    process_t* next = queue_head;
    queue_head = next->next;
    if (!queue_head) queue_tail = NULL;
    next->next = NULL;

    /* Re-enqueue at tail */
    if (!queue_head) {
        queue_head = next;
        queue_tail = next;
    } else {
        queue_tail->next = next;
        queue_tail       = next;
    }

    return next;
}

/* ---- do_switch ---- */

static void do_switch(process_t* next) {
    process_t* old = process_current();
    if (old == next) return;   /* nothing to do */

    /* Update states */
    if (old->state == PROCESS_RUNNING)
        old->state = PROCESS_READY;

    next->state           = PROCESS_RUNNING;
    next->ticks_remaining = next->priority;   /* reset quantum */

    process_set_current(next);
    context_switch(old, next);
    /* execution resumes HERE when old is switched back in */
}

/* ---- scheduler_tick ---- */

/*
 * Called by PIT IRQ0 every timer tick.
 * 1. Decrement sleep_ticks for any sleeping process — wake it if done.
 * 2. Decrement current process's ticks_remaining.
 * 3. If quantum expired, switch to next READY process.
 */
void scheduler_tick(void) {
    process_t* cur = process_current();
    if (!cur) return;

    /* Wake any sleeping processes in the queue */
    process_t* p = queue_head;
    while (p) {
        if (p->state == PROCESS_BLOCKED && p->sleep_ticks > 0) {
            p->sleep_ticks--;
            if (p->sleep_ticks == 0)
                p->state = PROCESS_READY;   /* will be picked up next tick */
        }
        p = p->next;
    }

    /* Decrement current process quantum */
    if (cur->ticks_remaining > 0)
        cur->ticks_remaining--;

    /* If quantum expired and there's something else to run, switch */
    if (cur->ticks_remaining == 0 && queue_size > 0) {
        process_t* next = pick_next();
        if (next && next != cur)
            do_switch(next);
        else
            cur->ticks_remaining = cur->priority;   /* reset, run again */
    }
}

/* ---- scheduler_yield ---- */

void scheduler_yield(void) {
    process_t* cur  = process_current();
    process_t* next = pick_next();

    if (!next || next == cur) return;
    do_switch(next);
}