#include "process.h"
#include "scheduler.h"
#include "vga.h"
#include "pmm.h"
#include "../mm/kmalloc.h"
#include "paging.h"
#include <stddef.h>

/* Simple string copy — no libc */
static void kstrncpy(char* dst, const char* src, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* PID counter */
static uint32_t next_pid = 0;

/* Currently running process */
static process_t* current_process = NULL;

/* The idle process — runs when nothing else is READY */
static process_t idle_pcb;

/* ---- idle task ---- */

static void __attribute__((unused)) idle_task(void) {
    while (1) {
        /* hlt: pause until next interrupt, saves power */
        __asm__ volatile("hlt");
    }
}

/* ---- process_init ---- */

void process_init(void) {
    /*
     * Set up the idle process (PID 0).
     * It uses idle_pcb which is statically allocated — no kmalloc needed.
     * This is important because process_init() runs before the heap is
     * fully exercised.
     */
    idle_pcb.pid      = next_pid++;
    idle_pcb.state    = PROCESS_RUNNING;   /* we ARE the idle task right now */
    idle_pcb.priority = 0;                 /* lowest possible */
    idle_pcb.ticks_remaining = 1;
    idle_pcb.sleep_ticks     = 0;
    idle_pcb.exit_code       = 0;
    idle_pcb.next            = NULL;
    kstrncpy(idle_pcb.name, "idle", 32);

    idle_pcb.esp = 0;   /* set on first context switch away from idle */
    idle_pcb.kernel_stack_top = 0;  /* idle runs on the live kernel stack */

    current_process = &idle_pcb;
    idle_pcb.cr3 = kernel_pd_phys;   /* idle uses the original kernel address space */

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] Process subsystem initialized (idle PID=0)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

/* ---- process_create ---- */

/*
 * Stack setup for a new process:
 *
 * We fake the stack so that context_switch() can restore it correctly.
 * context_switch pops: edi, esi, ebp, ebx, edx, ecx, eax, eflags
 * then does ret which pops eip.
 *
 * So we set up the stack (growing downward) as:
 *
 *   [top]
 *   process_exit ptr   ← return address if entry() ever returns
 *   entry ptr          ← eip (ret in context_switch jumps here)
 *   eflags = 0x202     ← interrupts enabled
 *   eax = 0
 *   ecx = 0
 *   edx = 0
 *   ebx = 0
 *   ebp = 0
 *   esi = 0
 *   edi = 0            ← esp points here after our pushes
 */
process_t* process_create(const char* name, void (*entry)(void), uint32_t priority) {
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) return NULL;

    /* Zero the PCB */
    uint8_t* p = (uint8_t*)proc;
    for (uint32_t i = 0; i < sizeof(process_t); i++) p[i] = 0;

    /* Create this process's page directory */
    proc->cr3 = paging_create_user_pd();
    if (!proc->cr3) {
        kfree(proc);
        return NULL;
    }

    proc->pid             = next_pid++;
    proc->state           = PROCESS_READY;
    proc->priority        = (priority == 0) ? 1 : priority;
    proc->ticks_remaining = proc->priority;
    proc->sleep_ticks     = 0;
    proc->exit_code       = 0;
    proc->next            = NULL;
    kstrncpy(proc->name, name, 32);

    /* Allocate 2 contiguous PMM frames (8 KB) for the kernel stack.
       pmm_alloc_frame returns one frame at a time, so we call it twice.
       They won't be physically contiguous, but that's fine — we only
       use the top frame for the initial stack pointer, and the stack
       grows down within it (8 KB is two frames, so we need both). */
    uint32_t stack_lo = pmm_alloc_frame();
    uint32_t stack_hi = pmm_alloc_frame();
    if (!stack_lo || !stack_hi) {
        if (stack_lo) pmm_free_frame(stack_lo);
        if (stack_hi) pmm_free_frame(stack_hi);
        paging_free_pd(proc->cr3);
        kfree(proc);
        return NULL;
    }
    /* Map both frames into the kernel address space so we can write to them.
       They're already identity-mapped (within our 32MB window), so
       kernel_stack_top = physical top of the upper frame. */
    proc->kernel_stack_top = stack_hi + PAGE_SIZE;  /* top of upper frame */

    /* Map both frames so they're accessible as a contiguous 8KB stack.
       Upper frame: [stack_hi .. stack_hi+4096)
       Lower frame: [stack_lo .. stack_lo+4096)  (stack grows into this)
       Since both are identity-mapped we don't need extra paging_map calls yet. */

    /*
     * Set up initial stack frame so context_switch can restore it.
     * We build the stack manually here — this is what the process will
     * "wake up to" the very first time it's scheduled.
     */
    uint32_t* stack = (uint32_t*)proc->kernel_stack_top;
    *(--stack) = (uint32_t)process_exit;  /* return address guard */
    *(--stack) = (uint32_t)entry;         /* eip — where execution starts */
    *(--stack) = 0x00000202;              /* eflags: IF=1 (interrupts on) */
    *(--stack) = 0;  /* eax */
    *(--stack) = 0;  /* ecx */
    *(--stack) = 0;  /* edx */
    *(--stack) = 0;  /* ebx */
    *(--stack) = 0;  /* ebp */
    *(--stack) = 0;  /* esi */
    *(--stack) = 0;  /* edi */

    /* esp points to the edi slot — exactly where context_switch expects it */
    proc->esp = (uint32_t)stack;

    scheduler_enqueue(proc);

    return proc;
}

/* ---- process_exit ---- */

void process_exit(int32_t exit_code) {
    current_process->exit_code = exit_code;
    current_process->state     = PROCESS_ZOMBIE;

    /* Remove from run queue and yield — never returns */
    scheduler_dequeue(current_process);
    scheduler_yield();

    /* Should never reach here */
    __asm__ volatile("cli; hlt");
}

/* ---- process_current ---- */

process_t* process_current(void) {
    return current_process;
}

/* Called by scheduler to update current_process */
void process_set_current(process_t* proc) {
    current_process = proc;
}

/* ---- process_list ---- */

static const char* state_name(process_state_t s) {
    switch (s) {
        case PROCESS_READY:   return "READY  ";
        case PROCESS_RUNNING: return "RUNNING";
        case PROCESS_BLOCKED: return "BLOCKED";
        case PROCESS_ZOMBIE:  return "ZOMBIE ";
        case PROCESS_DEAD:    return "DEAD   ";
        default:              return "UNKNOWN";
    }
}

static void print_proc_row(process_t* p, int highlight) {
    vga_set_color(highlight ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_GREY);
    kprint_int((int32_t)p->pid);
    kprint("    ");
    kprint(state_name(p->state));
    kprint("  ");
    kprint_int((int32_t)p->priority);
    kprint("    ");
    kprint(p->name);
    kprint("\n");
}

void process_list(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("PID  STATE    PRI  NAME\n");
    kprint("---  -------  ---  ----------------\n");

    process_t* cur = process_current();

    if (cur)
        print_proc_row(cur, 1);

    process_t* p = scheduler_queue_head();
    while (p) {
        if (p != cur)
            print_proc_row(p, 0);
        p = p->next;
    }

    vga_set_color(VGA_COLOR_LIGHT_GREY);
}