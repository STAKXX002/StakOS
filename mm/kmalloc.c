#include "kmalloc.h"
#include "../kernel/pmm.h"
#include "../kernel/vga.h"

/*
 * Block header — sits immediately before every allocation.
 * The `data` region starts right after the header.
 *
 * Memory layout of one block:
 *   [ block_t header (12 bytes) | data bytes... ]
 *
 * `size` is the number of usable data bytes (NOT including the header).
 * `free` is 1 if the block is available, 0 if in use.
 * `next` links to the next block header in the arena (NULL = last block).
 */
typedef struct block {
    uint32_t magic;
    uint32_t size;
    uint32_t free;
    struct block *next;
} block_t;

#define BLOCK_MAGIC 0xB10C0DE0
#define BLOCK_HEADER_SIZE sizeof(block_t) /* 12 bytes */

/*
 * Minimum split threshold.
 * Only split a block if the remainder is large enough to hold
 * a header plus at least this many bytes of usable space.
 * Prevents creating useless tiny blocks.
 */
#define MIN_SPLIT_SIZE 8

/* The heap: a contiguous physical region allocated from the PMM at init. */
static block_t *heap_start = NULL;
static uint32_t heap_total_bytes = 0;

/* ---- init ---- */

void kmalloc_init(uint32_t pages) {
    if (pages == 0)
        return;

    /*
     * Allocate `pages` contiguous-ish frames from the PMM.
     * Because our PMM allocates one frame at a time and the frames
     * may not be physically contiguous, we take a simpler approach:
     * allocate just ONE large block of `pages` worth of space by
     * grabbing the first page and building our entire arena there.
     *
     * For a real OS you'd want a contiguous physical allocator.
     * For StakOS Stage 5, one PMM frame per page stitched into a
     * linked free-list works fine — we just set up each page as its
     * own block and link them.
     */
    block_t *prev = NULL;
    heap_total_bytes = 0;

    for (uint32_t i = 0; i < pages; i++) {
        uint32_t phys = pmm_alloc_frame();
        if (!phys) {
            /* Out of physical memory — stop here */
            break;
        }

        /* Each frame becomes one big free block */
        block_t *blk = (block_t *)(uintptr_t)phys;
        blk->magic = BLOCK_MAGIC;
        blk->size = PAGE_SIZE - BLOCK_HEADER_SIZE;
        blk->free = 1;
        blk->next = NULL;

        if (!heap_start)
            heap_start = blk;
        if (prev)
            prev->next = blk;

        prev = blk;
        heap_total_bytes += PAGE_SIZE;
    }
}

/* ---- split ---- */

/*
 * Split block `blk` so that the first part holds exactly `size` bytes,
 * and the remainder becomes a new free block (if large enough).
 */
static void split(block_t *blk, size_t size) {
    /* Only split if there's room for a new header + MIN_SPLIT_SIZE bytes */
    if (blk->size <= size + BLOCK_HEADER_SIZE + MIN_SPLIT_SIZE)
        return;

    /* Carve out a new block in the remainder */
    block_t *new_blk = (block_t *)((uint8_t *)blk + BLOCK_HEADER_SIZE + size);
    new_blk->magic = BLOCK_MAGIC;
    new_blk->size = blk->size - size - BLOCK_HEADER_SIZE;
    new_blk->free = 1;
    new_blk->next = blk->next;

    blk->size = size;
    blk->next = new_blk;
}

/* ---- coalesce ---- */

/*
 * Merge adjacent free blocks starting from `blk`.
 * We do a single forward pass: if blk and blk->next are both free,
 * absorb next into blk (repeat until no more merges possible).
 */
static void coalesce(void) {
    block_t *cur = heap_start;
    while (cur && cur->next) {
        uint8_t *end_of_cur = (uint8_t *)cur + BLOCK_HEADER_SIZE + cur->size;
        if (cur->free && cur->next->free && (uint8_t *)cur->next == end_of_cur) {
            cur->size += BLOCK_HEADER_SIZE + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

/* ---- kmalloc ---- */

void *kmalloc(size_t size) {
    if (size == 0 || !heap_start)
        return NULL;

    /* Align size to 4 bytes so all allocations are 4-byte aligned */
    size = (size + 3) & ~(size_t)3;

    /* First-fit search */
    block_t *cur = heap_start;
    while (cur) {
        if (cur->free && cur->size >= size) {
            split(cur, size);
            cur->free = 0;
            /* Return pointer to data region, just after the header */
            return (void *)((uint8_t *)cur + BLOCK_HEADER_SIZE);
        }
        cur = cur->next;
    }

    return NULL; /* heap exhausted */
}

/* ---- kfree ---- */

void kfree(void *ptr) {
    if (!ptr)
        return;
    block_t *blk = (block_t *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);
    if (blk->magic != BLOCK_MAGIC || blk->free) {
        kpanic("kfree: invalid pointer or double-free", "");
        return;
    }
    blk->free = 1;
    coalesce();
}

/* ---- stats ---- */

void kmalloc_print_stats(void) {
    uint32_t total_blocks = 0;
    uint32_t free_blocks = 0;
    uint32_t used_blocks = 0;
    uint32_t free_bytes = 0;
    uint32_t used_bytes = 0;

    block_t *cur = heap_start;
    while (cur) {
        total_blocks++;
        if (cur->free) {
            free_blocks++;
            free_bytes += cur->size;
        } else {
            used_blocks++;
            used_bytes += cur->size;
        }
        cur = cur->next;
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] kmalloc heap initialized\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("     Heap size   : ");
    kprint_int((int32_t)heap_total_bytes);
    kprint(" bytes (");
    kprint_int((int32_t)(heap_total_bytes / 1024));
    kprint(" KB)\n");
    kprint("     Free        : ");
    kprint_int((int32_t)free_bytes);
    kprint(" bytes in ");
    kprint_int((int32_t)free_blocks);
    kprint(" block(s)\n");
    kprint("     Used        : ");
    kprint_int((int32_t)used_bytes);
    kprint(" bytes in ");
    kprint_int((int32_t)used_blocks);
    kprint(" block(s)\n");
    kprint("     Total       : ");
    kprint_int((int32_t)total_blocks);
    kprint(" block(s)\n");
}