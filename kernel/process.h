#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define STACK_SIZE      8192    /* 8 KB per-process kernel stack */
#define MAX_PROCESSES   16      /* max concurrent processes for now */

typedef enum {
    PROCESS_READY,      /* in run queue, wants CPU                      */
    PROCESS_RUNNING,    /* currently executing on CPU                   */
    PROCESS_BLOCKED,    /* waiting for event (I/O, sleep, mutex)        */
    PROCESS_ZOMBIE,     /* exited, waiting for parent to reap           */
    PROCESS_DEAD        /* fully cleaned up, PCB slot reclaimable       */
} process_state_t;

/*
 * Saved CPU context — exactly the registers we push/pop on context switch.
 * Order MUST match the push order in context_switch.asm.
 */
typedef struct {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip;       /* return address — pushed implicitly by call   */
    uint32_t eflags;
} cpu_context_t;

/*
 * Process Control Block.
 * kmalloc() allocates one of these per process.
 */
typedef struct process {
    /* Identity */
    uint32_t         pid;
    char             name[32];
    process_state_t  state;

    /* CPU context — valid when process is not RUNNING */
    uint32_t         esp;           /* saved stack pointer                  */
    uint32_t         cr3;           /* physical address of page dir     */

    /* Scheduling */
    uint32_t         priority;      /* 1 (low) – 10 (high); default 5      */
    uint32_t         ticks_remaining; /* counts down each PIT tick          */

    /* Sleep support */
    uint32_t         sleep_ticks;   /* >0 means BLOCKED waiting for timer   */

    /* Exit */
    int32_t          exit_code;     /* set by process_exit()                */

    /* Per-process kernel stack — 2 PMM frames (8 KB), allocated in process_create */
    uint32_t         kernel_stack_top; /* virtual address of top of stack    */

    /* Intrusive linked list — scheduler queue */
    struct process*  next;
} process_t;

/* ---- process API ---- */

/* Initialise the process subsystem and create the idle process (PID 0). */
void process_init(void);

/*
 * Create a new process that will start executing `entry`.
 * Returns the new process_t* or NULL if out of memory.
 */
process_t* process_create(const char* name, void (*entry)(void), uint32_t priority);

/*
 * Terminate the current process with `exit_code`.
 * Marks it ZOMBIE and yields to the scheduler.
 * Never returns.
 */
void process_exit(int32_t exit_code);

/* Return the currently running process. */
process_t* process_current(void);

/* Pretty-print all processes to VGA (for a 'ps' command). */
void process_list(void);
void process_set_current(process_t* proc);

#endif