#include "process.h"
#include "scheduler.h"
#include "vga.h"
#include "pmm.h"
#include "../mm/kmalloc.h"
#include "paging.h"
#include "elf.h"
#include "gdt.h"
#include "usermode.h"
#include <stddef.h>

/* Defined in boot/boot.asm — top of the 16KB boot-time kernel stack.
   Used as idle's kernel_stack_top so the TSS esp0 is always valid,
   even before idle has ever been switched away from. */
extern uint8_t stack_top;

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
    idle_pcb.kernel_stack_top = (uint32_t)&stack_top;  /* the real boot-time stack */

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
    proc->stack_frame_lo   = stack_lo;
    proc->stack_frame_hi   = stack_hi;
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

/* ---- user_mode_trampoline ---- */

/*
 * Entry function for every process started via process_create_from_elf().
 * Runs once, in kernel mode, the first time the scheduler switches to
 * this process — same as any other process_create() entry function.
 *
 * Its only job is what usertest_launcher did by hand in stage 9: set
 * the TSS's esp0 (belt-and-suspenders — do_switch already did this,
 * but it's free insurance against future scheduler changes), mark the
 * already-loaded ELF segments and user stack as ring-3 accessible
 * in THIS process's own page directory (paging_mark_user patches the
 * live CR3, which is correct here since we ARE that process by the
 * time this runs), then jump to user_entry via enter_usermode.
 *
 * Reads user_entry/user_stack_top from current_process rather than
 * taking parameters, since process_create()'s entry signature is a
 * fixed void(*)(void) — there's no other way to pass this data through
 * the existing context-switch machinery without changing that ABI.
 */
static void user_mode_trampoline(void) {
    process_t* self = process_current();

    tss_set_kernel_stack(self->kernel_stack_top);

    /* The ELF's segments and the user stack were mapped by
       process_create_from_elf() with PTE_USER already set, but the
       PDE-level USER bit only gets set on tables created AFTER that
       flag was known — paging_map_into() handles this correctly at
       map time, so no separate paging_mark_user() pass is needed
       here the way stage 9's hand-built test required it. */

    enter_usermode(self->user_entry, self->user_stack_top);
    /* unreachable */
}

/* ---- process_create_from_elf ---- */

#define USER_STACK_PAGES 4   /* 16 KB user stack */
#define USER_STACK_BASE  0x500000  /* arbitrary — well above where any
                                       PT_LOAD segment in our test binary
                                       lives (0x400000), no collision */

process_t* process_create_from_elf(const char* name, const uint8_t* elf_data,
                                    uint32_t priority) {
    uint32_t entry;
    if (!elf32_validate(elf_data, &entry)) return NULL;

    process_t* proc = process_create(name, user_mode_trampoline, priority);
    if (!proc) return NULL;

    if (!elf32_load(elf_data, proc->cr3)) {
        /* Resources allocated by process_create so far (PCB, kernel
           stack, page directory) are still valid — let the normal
           exit/reap path handle them rather than duplicating cleanup
           here. Marking ZOMBIE directly (not via process_exit, since
           we're not running AS this process) and leaving it for the
           reaper is simplest. */
        proc->state = PROCESS_ZOMBIE;
        scheduler_mark_zombie(proc);
        return NULL;
    }

    /* Map a user stack right after the segments — separate frames,
       separate mapping call, same target page directory. */
    uint32_t stack_top = USER_STACK_BASE;
    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) {
            proc->state = PROCESS_ZOMBIE;
            scheduler_mark_zombie(proc);
            return NULL;
        }
        uint32_t va = USER_STACK_BASE + i * PAGE_SIZE;
        paging_map_into(proc->cr3, va, frame,
                         PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        stack_top = va + PAGE_SIZE;
    }

    proc->user_entry     = entry;
    proc->user_stack_top = stack_top;  /* top of the LAST mapped page */

    return proc;
}

/* ---- process_exit ---- */

void process_exit(int32_t exit_code) {
    current_process->exit_code = exit_code;
    current_process->state     = PROCESS_ZOMBIE;

    /* Move to the zombie list instead of just dequeuing — we can't
       free our own PCB/stack/page directory while still running on
       them, so scheduler_tick() reaps us later from a safe context. */
    scheduler_mark_zombie(current_process);
    scheduler_yield();

    /* Should never reach here */
    __asm__ volatile("cli; hlt");
}

/* ---- process_free ---- */

/*
 * Frees everything process_create() allocated: the two kernel-stack
 * frames, the cloned page directory, and the PCB itself. Only ever
 * called from scheduler_reap_zombies(), which guarantees we're never
 * running as the process being freed.
 */
void process_free(process_t* proc) {
    if (!proc) return;

    pmm_free_frame(proc->stack_frame_lo);
    pmm_free_frame(proc->stack_frame_hi);
    paging_free_pd(proc->cr3);

    kfree(proc);
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