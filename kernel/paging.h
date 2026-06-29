#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "pmm.h"   /* for PAGE_SIZE, used by IDENTITY_MAPPED_BYTES below */

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

/* Number of 4MB windows identity-mapped at boot (see paging_init).
 * 32MB / 4MB = 8. This must match the -m flag passed to qemu in the
 * Makefile/CI — there's no way to derive one from the other, so if
 * you raise -m, raise this too.
 */
#define IDENTITY_PT_COUNT     8
#define IDENTITY_MAPPED_BYTES (IDENTITY_PT_COUNT * PT_ENTRIES * PAGE_SIZE)

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

/*
 * Allocate a fresh page directory for a new process.
 * Copies all kernel PD entries (the identity-mapped region) so the
 * process can run kernel code, then zeros the user half.
 * Returns the physical address of the new PD, or 0 on OOM.
 */
uint32_t paging_create_user_pd(void);

/*
 * Free a per-process page directory previously created by
 * paging_create_user_pd(). Does NOT free any user-space page tables
 * or frames — that's the job of the VM region tracker (stage 7c+).
 */
void paging_free_pd(uint32_t pd_phys);

/*
 * Maps a virtual page to a physical frame inside an ARBITRARY page
 * directory — not necessarily the one currently loaded in CR3.
 *
 * This is what the ELF loader needs: it builds a new process's
 * address space while the caller (e.g. the shell) is still running
 * on its own CR3. Allocates a new page table on demand if the PDE
 * for this virtual address isn't present yet.
 *
 * `flags` should be some combination of PTE_PRESENT | PTE_WRITABLE |
 * PTE_USER. The PDE gets PDE_USER too if PTE_USER is set, since the
 * CPU requires both to permit a ring-3 access.
 */
void paging_map_into(uint32_t pd_phys, uint32_t virt, uint32_t phys, uint32_t flags);

#endif