#include "process.h"
#include "scheduler.h"
#include "vga.h"
#include "../mm/kmalloc.h"
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

    idle_pcb.kernel_stack_top =
        (uint32_t)&idle_pcb.kernel_stack[STACK_SIZE];

    /*
     * ESP for idle: we don't actually set it up like a normal process
     * because it becomes current immediately without a context switch.
     * The idle loop runs in the existing kernel stack until the first
     * real process is scheduled.
     */
    idle_pcb.esp = 0;   /* set on first context switch away from idle */

    current_process = &idle_pcb;

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

    proc->pid             = next_pid++;
    proc->state           = PROCESS_READY;
    proc->priority        = (priority == 0) ? 1 : priority;
    proc->ticks_remaining = proc->priority;
    proc->sleep_ticks     = 0;
    proc->exit_code       = 0;
    proc->next            = NULL;
    kstrncpy(proc->name, name, 32);

    proc->kernel_stack_top = (uint32_t)&proc->kernel_stack[STACK_SIZE];

    /*
     * Set up initial stack frame so context_switch can restore it.
     * We build the stack manually here — this is what the process will
     * "wake up to" the very first time it's scheduled.
     */
    uint32_t* stack = (uint32_t*)proc->kernel_stack_top;

    /* If entry() returns it lands in process_exit(0) */
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

    /* Add to scheduler queue */
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

void process_list(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("PID  STATE    PRI  NAME\n");
    kprint("---  -------  ---  ----------------\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);

    /* Show current process */
    process_t* cur = process_current();
    if (cur) {
        vga_set_color(VGA_COLOR_WHITE);
        kprint_int((int32_t)cur->pid);
        kprint("    ");
        kprint(state_name(cur->state));
        kprint("  ");
        kprint_int((int32_t)cur->priority);
        kprint("    ");
        kprint(cur->name);
        kprint("\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
    }
}