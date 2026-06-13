#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE       4096
#define PMM_BITS_PER_ENTRY 32

/*
 * Multiboot2 info block header (we only need the first field to skip it,
 * then walk the tags that follow it).
 */
typedef struct __attribute__((packed)) {
    uint32_t total_size;
    uint32_t reserved;
    /* tags follow immediately */
} mb2_info_t;

/* Generic Multiboot2 tag header */
typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

/* Memory map tag (type = 6) */
typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    /* entries follow */
} mb2_tag_mmap_t;

/* One memory map entry */
typedef struct __attribute__((packed)) {
    uint64_t base_addr;
    uint64_t length;
    uint32_t mem_type;   /* 1 = available RAM */
    uint32_t reserved;
} mb2_mmap_entry_t;

#define MB2_MEMORY_AVAILABLE 1

/*
 * Initialise the PMM using the Multiboot2 memory map.
 * mboot_ptr is the physical address GRUB stored in ebx.
 */
void pmm_init(uint32_t mboot_ptr);

/* Allocate one 4KB physical page. Returns 0 on failure. */
uint32_t pmm_alloc_frame(void);

/* Free a previously allocated page. */
void pmm_free_frame(uint32_t phys_addr);

/* Diagnostic: print stats to VGA. */
void pmm_print_stats(void);

#endif