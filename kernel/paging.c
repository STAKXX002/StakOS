#include "paging.h"
#include "pmm.h"
#include "vga.h"
#include <stddef.h>

/* Kernel page directory — one static, 4KB-aligned array.
   All per-process PDs are cloned from this. */
uint32_t kernel_pd[PD_ENTRIES] __attribute__((aligned(4096)));
uint32_t kernel_pd_phys;   /* its physical address (== virtual while identity-mapped) */

/* One page table per 4MB window we want to identity-map.
   32MB / 4MB = 8 page tables.  Adjust IDENTITY_PT_COUNT if you raise -m. */
#define IDENTITY_PT_COUNT  8
static uint32_t identity_pts[IDENTITY_PT_COUNT][PT_ENTRIES] __attribute__((aligned(4096)));

/* Assembly helper: loads CR3 and enables paging */
static void paging_enable(uint32_t pd_phys) {
    __asm__ volatile(
        "mov %0, %%cr3\n\t"         /* load page directory physical address */
        "mov %%cr0, %%eax\n\t"
        "or  $0x80000000, %%eax\n\t"/* set PG bit */
        "mov %%eax, %%cr0\n\t"
        : : "r"(pd_phys) : "eax"
    );
}

/*
 * Invalidate a single TLB entry.
 * Needed after modifying a PTE while paging is already active.
 */
static inline void tlb_flush(uint32_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void paging_init(void) {
    /* Identity-map 0 – (IDENTITY_PT_COUNT × 4MB) */
    for (uint32_t t = 0; t < IDENTITY_PT_COUNT; t++) {
        for (uint32_t i = 0; i < PT_ENTRIES; i++)
            identity_pts[t][i] = (t * PT_ENTRIES * PAGE_SIZE + i * PAGE_SIZE)
                                 | PTE_PRESENT | PTE_WRITABLE;
        kernel_pd[t] = (uint32_t)identity_pts[t] | PDE_PRESENT | PDE_WRITABLE;
    }
    /* Remaining PD entries stay 0 (not present). */

    kernel_pd_phys = (uint32_t)kernel_pd;   /* identity-mapped, so virt == phys */
    paging_enable(kernel_pd_phys);

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] Paging enabled (identity 0 - ");
    kprint_int(IDENTITY_PT_COUNT * 4);
    kprint("MB)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}

void paging_map(uint32_t virt, uint32_t phys) {
    uint32_t pd_idx = virt >> 22;           /* top 10 bits */
    uint32_t pt_idx = (virt >> 12) & 0x3FF;/* middle 10 bits */

    /* If the page table for this PD entry doesn't exist, create one */
    if (!(kernel_pd[pd_idx] & PDE_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) {
            kpanic("paging_map: out of physical memory", "");
            return;
        }
        /* Zero the new page table */
        uint32_t* pt = (uint32_t*)(uintptr_t)pt_phys;
        for (int i = 0; i < PT_ENTRIES; i++) pt[i] = 0;

        kernel_pd[pd_idx] = pt_phys | PDE_PRESENT | PDE_WRITABLE;
        tlb_flush(virt);
    }

    /* Get the page table virtual address (identity-mapped, so virt == phys here) */
    uint32_t* pt = (uint32_t*)(uintptr_t)(kernel_pd[pd_idx] & ~0xFFF);
    pt[pt_idx]   = (phys & ~0xFFF) | PTE_PRESENT | PTE_WRITABLE;
    tlb_flush(virt);
}

uint32_t paging_create_user_pd(void) {
    uint32_t pd_phys = pmm_alloc_frame();
    if (!pd_phys) return 0;

    /* The new PD lives at pd_phys which is identity-mapped, so we can
       write to it directly using its physical address as a pointer. */
    uint32_t* pd = (uint32_t*)(uintptr_t)pd_phys;

    /* Copy kernel mappings (identity-mapped region, entries 0..IDENTITY_PT_COUNT-1).
       User entries (IDENTITY_PT_COUNT .. 1023) stay 0 — not present. */
    for (uint32_t i = 0; i < PD_ENTRIES; i++)
        pd[i] = (i < IDENTITY_PT_COUNT) ? kernel_pd[i] : 0;

    return pd_phys;
}

void paging_free_pd(uint32_t pd_phys) {
    if (pd_phys) pmm_free_frame(pd_phys);
}

/*
 * Mark an already-mapped page as user-accessible (sets the U/S bit on
 * both the PDE and PTE). Used for stage 9 test code that lives inside
 * the kernel's identity-mapped region but needs to run at CPL=3 —
 * without this, every identity-mapped page is supervisor-only and any
 * ring-3 instruction fetch there takes a protection-violation page
 * fault (err_code bit 2 = user, bit 0 = protection, not not-present).
 *
 * This only touches kernel_pd, the address space active right now.
 * Real user processes (stage 10+) should map their pages as user-
 * accessible from the start via paging_map_into(), not retrofit them.
 */
void paging_mark_user(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    /*
     * IMPORTANT: modify the *currently active* page directory (via
     * live CR3), not the static kernel_pd template.
     *
     * Each process gets its own PD, cloned from kernel_pd at
     * process_create() time (see paging_create_user_pd). That clone
     * copies PDE *values* — if we later OR a bit into kernel_pd here,
     * any process whose PD was cloned before this call still has the
     * old PDE value and never sees the change. Since this function is
     * always called from the process that's about to use the mapping
     * (right before its own enter_usermode), reading CR3 here always
     * gives the right page directory to patch.
     */
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint32_t* pd = (uint32_t*)(uintptr_t)cr3;

    if (!(pd[pd_idx] & PDE_PRESENT)) {
        kpanic("paging_mark_user: page not mapped", "");
        return;
    }

    pd[pd_idx] |= PDE_USER;

    /* The page table itself IS shared across all per-process PD clones
       (the PDE copy points at the same physical table), so this PTE
       write is visible everywhere regardless of which PD we patched
       above — but we patch via `pd` for consistency and clarity. */
    uint32_t* pt = (uint32_t*)(uintptr_t)(pd[pd_idx] & ~0xFFF);
    if (!(pt[pt_idx] & PTE_PRESENT)) {
        kpanic("paging_mark_user: page not mapped", "");
        return;
    }
    pt[pt_idx] |= PTE_USER;

    tlb_flush(virt);
}