#include "paging.h"
#include "pmm.h"
#include "vga.h"
#include <stddef.h>

/*
 * We statically allocate the page directory and the first page table.
 * They must be 4 KB aligned so the CPU can load them into CR3.
 *
 * __attribute__((aligned(4096))) puts them in .bss, which is zero-initialised
 * by convention (our linker script keeps .bss, so this is fine).
 */
static uint32_t page_directory[PD_ENTRIES] __attribute__((aligned(4096)));
static uint32_t identity_pt[PT_ENTRIES]    __attribute__((aligned(4096)));

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
    /*
     * Identity-map the first 4 MB:
     *   virtual 0x00000000–0x003FFFFF → physical 0x00000000–0x003FFFFF
     *
     * This covers:
     *   - Frame 0 (BIOS / real-mode IVT — we won't touch it but mapping it
     *              harmlessly avoids a page fault if anything accesses low mem)
     *   - 0x100000 (1 MB) where our kernel is loaded
     *   - VGA framebuffer at 0xB8000 (fits inside 4 MB)
     */
    for (uint32_t i = 0; i < PT_ENTRIES; i++) {
        /* Each entry: physical address | Present | Writable */
        identity_pt[i] = (i * PAGE_SIZE) | PTE_PRESENT | PTE_WRITABLE;
    }

    /* Install the identity page table into PD entry 0
       (virtual 0x00000000 – 0x003FFFFF) */
    page_directory[0] = (uint32_t)identity_pt | PDE_PRESENT | PDE_WRITABLE;

    /* All other PD entries stay 0 (not present) — page fault if accessed */

    /* Enable paging by loading CR3 and setting CR0.PG */
    paging_enable((uint32_t)page_directory);

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] Paging enabled (identity 0-4MB)\n");
}

void paging_map(uint32_t virt, uint32_t phys) {
    uint32_t pd_idx = virt >> 22;           /* top 10 bits */
    uint32_t pt_idx = (virt >> 12) & 0x3FF;/* middle 10 bits */

    /* If the page table for this PD entry doesn't exist, create one */
    if (!(page_directory[pd_idx] & PDE_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) {
            kpanic("paging_map: out of physical memory", "");
            return;
        }
        /* Zero the new page table */
        uint32_t* pt = (uint32_t*)(uintptr_t)pt_phys;
        for (int i = 0; i < PT_ENTRIES; i++) pt[i] = 0;

        page_directory[pd_idx] = pt_phys | PDE_PRESENT | PDE_WRITABLE;
        tlb_flush(virt);
    }

    /* Get the page table virtual address (identity-mapped, so virt == phys here) */
    uint32_t* pt = (uint32_t*)(uintptr_t)(page_directory[pd_idx] & ~0xFFF);
    pt[pt_idx]   = (phys & ~0xFFF) | PTE_PRESENT | PTE_WRITABLE;
    tlb_flush(virt);
}