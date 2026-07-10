#include "scheduler.h"
#include "gdt.h"
#include "process.h"
#include "vga.h"

/*
 * The run queue: a circular singly-linked list of READY processes.
 * queue_head = next process to run.
 * queue_tail = last process (tail->next = queue_head for circular wrap).
 */
static process_t *queue_head = NULL;
static process_t *queue_tail = NULL;
static uint32_t queue_size = 0;

/*
 * Zombie list - processes that called process_exit() but whose
 * resources (PCB, kernel stack frames, page directory) can't be
 * freed yet because they were still running on/using them at the
 * moment they exited. scheduler_tick() reaps this list from a
 * different process's context, where freeing is actually safe.
 */
static process_t *zombie_head = NULL;

/* ---- queue operations ---- */

void scheduler_enqueue(process_t *proc) {
    if (!proc)
        return;

    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));

    proc->state = PROCESS_READY;
    proc->next = NULL;
    if (!queue_head) {
        queue_head = proc;
        queue_tail = proc;
    } else {
        queue_tail->next = proc;
        queue_tail = proc;
    }
    queue_size++;

    __asm__ volatile("push %0; popf" : : "r"(flags));
}

void scheduler_dequeue(process_t *proc) {
    if (!proc || !queue_head)
        return;

    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));

    process_t *prev = NULL;
    process_t *cur = queue_head;
    while (cur) {
        if (cur == proc) {
            if (prev)
                prev->next = cur->next;
            else
                queue_head = cur->next;
            if (cur == queue_tail)
                queue_tail = prev;
            cur->next = NULL;
            queue_size--;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    __asm__ volatile("push %0; popf" : : "r"(flags));
}

void scheduler_mark_zombie(process_t *proc) {
    if (!proc)
        return;

    /* Take it out of the run queue first (same logic as dequeue),
       then push it onto the zombie list instead of discarding it. */
    scheduler_dequeue(proc);

    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));

    proc->next = zombie_head;
    zombie_head = proc;

    __asm__ volatile("push %0; popf" : : "r"(flags));
}

/* ---- scheduler_reap_zombies ---- */

/*
 * Helper function to check if a process ID still exists as a live
 * entity (READY, RUNNING, or BLOCKED) that could eventually call
 * wait() to claim a zombie child.
 *
 * Looks the pid up in process_table via process_lookup() rather than
 * walking the ready queue. This matters because "in the ready queue"
 * is NOT the same thing as "alive" - a process blocked indefinitely
 * on an event (wait() in 12b, a pipe read/write in 12c) will be
 * scheduler_dequeue()'d out of the ready queue entirely while still
 * very much alive, and a queue-traversal check would wrongly report
 * it as dead, causing scheduler_reap_zombies() to free a zombie's
 * PCB/stack/page-directory out from under a parent that's about to
 * wait() for it.
 *
 * NOTE: pid 0 (idle) is deliberately excluded here even though
 * process_lookup(0) will find it (it's registered in process_init()).
 * "Alive" is being used as a proxy for "will eventually call wait()
 * to claim this zombie" - and the shell runs as pid 0 (kernel_main ->
 * shell_run() executes as the initial idle_pcb context, see
 * process_init), but has no SYS_WAIT path yet (stage 12c). So every
 * shell-spawned process (run/elftest) has parent_pid == 0, and if we
 * let the lookup answer for pid 0, it'll always say yes (idle always
 * exists) and those zombies will never be reaped.
 *
 * Treating pid 0 as a non-claiming parent for now means shell-spawned
 * zombies get swept immediately, same as pre-orphan-protection
 * behavior. Once the shell can issue a real wait() for foreground
 * children, remove this and let pid 0 go through the normal path -
 * at that point it legitimately becomes a parent that can claim.
 */
static int is_pid_alive(uint32_t pid) {
    if (pid == 0)
        return 0; /* idle/shell can't call wait() yet - treat as non-claiming.
                     Remove this special case once pid 0 gets a real SYS_WAIT
                     path (stage 12c) and can legitimately claim zombies. */

    process_t *p = process_lookup(pid);
    if (!p)
        return 0; /* never existed, or already freed */

    return p->state != PROCESS_ZOMBIE && p->state != PROCESS_DEAD;
}

/*
 * Walks the zombie list and reaps orphan processes whose parents have already
 * exited. If a zombie's parent is still alive, its state, PCB metadata, and
 * exit code are preserved intact so the parent can collect them via wait()
 * later.
 *
 * Called from scheduler_tick(), which always runs in the interrupt context of
 * whichever process the timer suspended-never as the zombie itself. This
 * guarantees that freeing its kernel stack and page directory is completely
 * safe.
 */
static void scheduler_reap_zombies(void) {
    process_t *prev = NULL;
    process_t *cur = zombie_head;

    while (cur) {
        if (!is_pid_alive(cur->parent_pid)) {
            /* Parent is dead or missing, this is an orphan. Auto-reap resources
             * safely */
            process_t *orphan = cur;
            cur = cur->next;

            if (prev) {
                prev->next = cur;
            } else {
                zombie_head = cur;
            }

            process_free(orphan);
        } else {
            /* Parent is still active; retain the zombie state for a future wait()
             * call */
            prev = cur;
            cur = cur->next;
        }
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
     * marked READY but can never be picked again - the system gets stuck
     * running whatever just preempted it, forever.
     */
    process_t *idle = process_current();
    if (idle) {
        scheduler_enqueue(idle);
        idle->state = PROCESS_RUNNING; /* it really is running right now */
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] Scheduler initialized (round-robin + priority)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

process_t *scheduler_queue_head(void) { return queue_head; }

/* ---- pick next ---- */

/*
 * Pick the next READY process from the queue.
 * Simple round-robin: take head, move it to tail.
 * Priority is implemented via ticks_remaining: a process with priority N
 * gets N consecutive ticks before yielding.
 */
static process_t *pick_next(void) {
    if (!queue_head)
        return NULL;

    uint32_t checked = 0;

    /* Rotate through the queue elements to find a task with a READY state status
     */
    while (checked < queue_size) {
        process_t *next = queue_head;

        /* Pop current head */
        queue_head = next->next;
        if (!queue_head)
            queue_tail = NULL;
        next->next = NULL;

        /* Re-enqueue immediately at the tail to maintain round-robin circulation */
        if (!queue_head) {
            queue_head = next;
            queue_tail = next;
        } else {
            queue_tail->next = next;
            queue_tail = next;
        }

        checked++;

        /* If this process is genuinely READY to execute, return it */
        if (next->state == PROCESS_READY) {
            return next;
        }
    }

    /* If nothing else is explicitly READY, return NULL (scheduler switches back
     * to current or idle) */
    return NULL;
}

/* ---- do_switch ---- */

static void do_switch(process_t *next) {
    process_t *old = process_current();
    if (old == next)
        return; /* nothing to do */

    /* Update states */
    if (old->state == PROCESS_RUNNING)
        old->state = PROCESS_READY;

    next->state = PROCESS_RUNNING;
    next->ticks_remaining = next->priority; /* reset quantum */

    /*
     * Point the TSS at next's kernel stack top. The CPU reads this
     * (esp0/ss0) automatically on any ring 3 -> ring 0 transition
     * (interrupt, exception, or int 0x80) - it has nothing to do with
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
 * 1. Decrement sleep_ticks for any sleeping process - wake it if done.
 * 2. Decrement current process's ticks_remaining.
 * 3. If quantum expired, switch to next READY process.
 */
void scheduler_tick(void) {
    process_t *cur = process_current();
    if (!cur)
        return;

    /* Free any processes that exited since the last tick. Safe here -
       we're never running as the zombie itself at this point. */
    scheduler_reap_zombies();

    /* Wake any sleeping processes in the queue */
    process_t *p = queue_head;
    while (p) {
        if (p->state == PROCESS_BLOCKED && p->sleep_ticks > 0) {
            p->sleep_ticks--;
            if (p->sleep_ticks == 0)
                p->state = PROCESS_READY; /* will be picked up next tick */
        }
        p = p->next;
    }

    /* Decrement current process quantum */
    if (cur->ticks_remaining > 0)
        cur->ticks_remaining--;

    /* If quantum expired and there's something else to run, switch */
    if (cur->ticks_remaining == 0 && queue_size > 0) {
        process_t *next = pick_next();
        if (next && next != cur)
            do_switch(next);
        else
            cur->ticks_remaining = cur->priority; /* reset, run again */
    }
}

/* ---- scheduler_yield ---- */

void scheduler_yield(void) {
    process_t *cur = process_current();
    process_t *next = pick_next();

    if (!next || next == cur)
        return;
    do_switch(next);
}