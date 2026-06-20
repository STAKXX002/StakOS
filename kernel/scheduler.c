#include "scheduler.h"
#include "process.h"
#include "vga.h"
#include "gdt.h"


/*
 * The run queue: a circular singly-linked list of READY processes.
 * queue_head = next process to run.
 * queue_tail = last process (tail->next = queue_head for circular wrap).
 */
static process_t* queue_head = NULL;
static process_t* queue_tail = NULL;
static uint32_t   queue_size = 0;

/*
 * Zombie list — processes that called process_exit() but whose
 * resources (PCB, kernel stack frames, page directory) can't be
 * freed yet because they were still running on/using them at the
 * moment they exited. scheduler_tick() reaps this list from a
 * different process's context, where freeing is actually safe.
 */
static process_t* zombie_head = NULL;

/* ---- queue operations ---- */

void scheduler_enqueue(process_t* proc) {
    if (!proc) return;

    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));

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

    __asm__ volatile("push %0; popf" : : "r"(flags));
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

void scheduler_mark_zombie(process_t* proc) {
    if (!proc) return;

    /* Take it out of the run queue first (same logic as dequeue),
       then push it onto the zombie list instead of discarding it. */
    scheduler_dequeue(proc);

    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));

    proc->next  = zombie_head;
    zombie_head = proc;

    __asm__ volatile("push %0; popf" : : "r"(flags));
}

/* ---- scheduler_reap_zombies ---- */

/*
 * Frees every process currently on the zombie list. Called from
 * scheduler_tick(), which always runs as whichever process the timer
 * interrupted — never as the zombie itself, so freeing its stack and
 * page directory here is safe.
 */
static void scheduler_reap_zombies(void) {
    while (zombie_head) {
        process_t* z = zombie_head;
        zombie_head  = z->next;

        process_free(z);   /* frees PCB, kernel stack frames, page dir */
    }
}

void scheduler_init(void) {
    queue_head = NULL;
    queue_tail = NULL;
    queue_size = 0;

    /*
     * process_init() (run just before this) sets current_process to the
     * idle PCB directly, bypassing the queue entirely. If we don't enqueue
     * it here, idle sits outside the round-robin set: the instant anything
     * else is created and idle's first (1-tick) quantum expires, idle is
     * marked READY but can never be picked again — the system gets stuck
     * running whatever just preempted it, forever.
     */
    process_t* idle = process_current();
    if (idle) {
        scheduler_enqueue(idle);
        idle->state = PROCESS_RUNNING;   /* it really is running right now */
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] Scheduler initialized (round-robin + priority)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

process_t* scheduler_queue_head(void) {
    return queue_head;
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

    /*
     * Point the TSS at next's kernel stack top. The CPU reads this
     * (esp0/ss0) automatically on any ring 3 -> ring 0 transition
     * (interrupt, exception, or int 0x80) — it has nothing to do with
     * our own software context_switch below, but it MUST be correct
     * before any process runs at CPL=3, or the first interrupt while
     * that process is running will load a stale or zero stack pointer.
     */
    tss_set_kernel_stack(next->kernel_stack_top);

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

    /* Free any processes that exited since the last tick. Safe here —
       we're never running as the zombie itself at this point. */
    scheduler_reap_zombies();

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