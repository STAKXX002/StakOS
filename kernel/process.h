#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define STACK_SIZE 8192  /* 8 KB per-process kernel stack */
#define MAX_PROCESSES 16 /* max concurrent processes for now */
#define MAX_FDS_PER_PROCESS                                                                        \
    8 /* small and fixed for now - no dynamic growth                                               \
       */

typedef enum {
    PROCESS_READY,   /* in run queue, wants CPU                      */
    PROCESS_RUNNING, /* currently executing on CPU                   */
    PROCESS_BLOCKED, /* waiting for event (I/O, sleep, mutex)        */
    PROCESS_ZOMBIE,  /* exited, waiting for parent to reap           */
    PROCESS_DEAD     /* fully cleaned up, PCB slot reclaimable       */
} process_state_t;

/*
 * A single open-file entry in a process's fd table.
 * file_idx is the ramfs file table index (see kernel/ramfs.h);
 * offset is how many bytes have been read so far via this fd.
 * in_use == 0 means this slot is free.
 */
typedef struct {
    int in_use;
    int file_idx;
    uint32_t offset;
} fd_entry_t;

/*
 * Saved CPU context - exactly the registers we push/pop on context switch.
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
    uint32_t eip; /* return address - pushed implicitly by call   */
    uint32_t eflags;
} cpu_context_t;

/*
 * Process Control Block.
 * kmalloc() allocates one of these per process.
 */
typedef struct process {
    /* Identity */
    uint32_t pid;
    char name[32];
    process_state_t state;

    /* CPU context - valid when process is not RUNNING */
    uint32_t esp;        /* saved stack pointer */
    uint32_t cr3;        /* physical address of page dir */
    uint32_t parent_pid; /* track who spawned this process */

    /* Scheduling */
    uint32_t priority;        /* 1 (low) – 10 (high); default 5 */
    uint32_t ticks_remaining; /* counts down each PIT tick */

    /* Sleep support */
    uint32_t sleep_ticks; /* >0 means BLOCKED waiting for timer */

    /* Exit */
    int32_t exit_code; /* set by process_exit() */

    /* Per-process kernel stack - 2 PMM frames (8 KB), allocated in process_create
     */
    uint32_t kernel_stack_top; /* virtual address of top of stack    */
    uint32_t stack_frame_lo;   /* physical frame - needed to free    */
    uint32_t stack_frame_hi;   /* physical frame - needed to free    */

    /*
     * Set only for processes started via process_create_from_elf().
     * user_entry is 0 for ordinary kernel-mode processes (idle, the
     * stage 8/9 test processes) - the trampoline checks this to know
     * whether it should jump to ring 3 at all.
     */
    uint32_t user_entry;     /* ELF entry point (virtual addr)     */
    uint32_t user_stack_top; /* top of the mapped user stack       */

    /*
     * Per-process file descriptor table. fd 0/1/2 are reserved by
     * convention (stdin/stdout/stderr) but not populated here yet -
     * SYS_WRITE still special-cases fd==1 directly rather than going
     * through this table. Real files opened via SYS_OPEN start at the
     * first free slot, which in practice means fd 3 onward for now.
     */
    fd_entry_t fds[MAX_FDS_PER_PROCESS];

    /* Intrusive linked list - scheduler queue */
    struct process *next;
} process_t;

/* ---- process API ---- */

/* Initialise the process subsystem and create the idle process (PID 0). */
void process_init(void);

/*
 * Create a new process that will start executing `entry`.
 * Returns the new process_t* or NULL if out of memory.
 */
process_t *process_create(const char *name, void (*entry)(void), uint32_t priority);

/*
 * Creates a process that runs `elf_data` at ring 3.
 *
 * Loads every PT_LOAD segment into a fresh page directory (via
 * elf32_load), maps a 4-page user stack alongside it, and arranges
 * for the process's first scheduled run to jump straight to CPL=3 at
 * the ELF's entry point - replacing the old hand-written stage-9
 * ring-3 test pattern with a real, data-driven loader.
 *
 * Returns the new process_t*, or NULL on invalid ELF / OOM.
 */
process_t *process_create_from_elf(const char *name, const uint8_t *elf_data, uint32_t priority);

/*
 * Terminate the current process with `exit_code`.
 * Marks it ZOMBIE and yields to the scheduler.
 * Never returns.
 */
void process_exit(int32_t exit_code);

/*
 * Frees a process's resources: kernel stack frames, page directory,
 * and the PCB itself. Only safe to call on a process that is not
 * currently running - used by the scheduler's zombie reaper.
 */
void process_free(process_t *proc);

/*
 * Look up a process by pid, independent of scheduler/queue state.
 * Returns NULL if no such pid exists (never created, or already freed).
 * This is the authoritative "does this process still exist" check -
 * used by is_pid_alive() and later by wait().
 */
process_t *process_lookup(uint32_t pid);

/* Return the currently running process. */
process_t *process_current(void);

/* Pretty-print all processes to VGA (for a 'ps' command). */
void process_list(void);
void process_set_current(process_t *proc);

#endif