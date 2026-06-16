#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/*
 * x86 32-bit paging (non-PAE):
 *   - CR3 holds the physical address of the page directory (4KB aligned)
 *   - Page directory: 1024 entries × 4 bytes = 4 KB
 *   - Each PD entry points to a page table (1024 entries × 4 bytes = 4 KB)
 *   - Each PT entry maps one 4 KB physical frame
 *
 * We identity-map the first 4 MB (one page table) which covers the kernel.
 * The kernel lives at 1 MB (0x100000), so 4 MB is plenty for now.
 */

/* Page Directory Entry flags */
#define PDE_PRESENT     (1u << 0)
#define PDE_WRITABLE    (1u << 1)
#define PDE_USER        (1u << 2)

/* Page Table Entry flags */
#define PTE_PRESENT     (1u << 0)
#define PTE_WRITABLE    (1u << 1)
#define PTE_USER        (1u << 2)

/* Number of entries in a page directory / page table */
#define PD_ENTRIES      1024
#define PT_ENTRIES      1024

/* A page table: 1024 4-byte PTEs */
typedef uint32_t page_table_t[PT_ENTRIES];

/* A page directory: 1024 4-byte PDEs */
typedef uint32_t page_dir_t[PD_ENTRIES];

/* Physical address of the kernel page directory (set by paging_init). */
extern uint32_t kernel_pd_phys;

/*
 * Initialise paging:
 *   1. Allocate page directory from the PMM
 *   2. Identity-map the first 4 MB (PD[0] → identity page table)
 *   3. Load CR3, set CR0.PG
 *
 * After this call the CPU runs in paged mode.
 */
void paging_init(void);

/* Map a single virtual page to a physical frame (supervisor, read/write). */
void paging_map(uint32_t virt, uint32_t phys);

#endif