#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <stdint.h>

/*
 * Initialise the kernel heap.
 * Requests `pages` physical frames from the PMM and sets up
 * the free-list over that region.
 * Call once, after paging_init().
 */
void kmalloc_init(uint32_t pages);

/*
 * Allocate `size` bytes from the kernel heap.
 * Returns NULL if the heap is exhausted or size == 0.
 * Returned pointer is at minimum 4-byte aligned.
 */
void *kmalloc(size_t size);

/*
 * Free a pointer previously returned by kmalloc.
 * kfree(NULL) is a safe no-op.
 * Coalesces adjacent free blocks to prevent fragmentation.
 */
void kfree(void *ptr);

/* Print heap statistics to VGA — useful for debugging. */
void kmalloc_print_stats(void);

#endif