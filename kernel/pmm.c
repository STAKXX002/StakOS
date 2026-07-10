#include "pmm.h"
#include "vga.h"
#include <stddef.h>

/*
 * Bitmap: each bit represents one 4KB physical frame.
 * Bit = 0 → frame is FREE
 * Bit = 1 → frame is USED
 *
 * We support up to 128 MB of physical RAM (128M / 4K = 32768 frames).
 * 32768 bits = 1024 uint32_t entries = 4 KB of bitmap.
 * Adjust MAX_FRAMES if you want more.
 */
#define MAX_FRAMES 32768 /* 128 MB */

static uint32_t pmm_bitmap[MAX_FRAMES / 32];
static uint32_t total_frames = 0;
static uint32_t free_frames = 0;

/* ---- bit manipulation helpers ---- */

static inline void frame_set(uint32_t frame) { pmm_bitmap[frame / 32] |= (1u << (frame % 32)); }

static inline void frame_clear(uint32_t frame) { pmm_bitmap[frame / 32] &= ~(1u << (frame % 32)); }

static inline int frame_test(uint32_t frame) {
    return (pmm_bitmap[frame / 32] >> (frame % 32)) & 1u;
}

/* ---- kernel symbol from linker script ---- */
/* We mark everything below this address as USED so we never
   hand the kernel image region to anyone as free memory. */
extern uint8_t _kernel_end; /* provided by linker.ld */

/* ---- public API ---- */

void pmm_init(uint32_t mboot_ptr) {
    /* 1. Mark everything USED by default */
    for (int i = 0; i < (int)(MAX_FRAMES / 32); i++)
        pmm_bitmap[i] = 0xFFFFFFFF;

    /* 2. Walk Multiboot2 tags to find memory map */
    mb2_info_t *info = (mb2_info_t *)(uintptr_t)mboot_ptr;
    mb2_tag_t *tag = (mb2_tag_t *)((uint8_t *)info + 8); /* skip header */

    while (tag->type != 0) { /* type 0 = end tag */
        if (tag->type == 6) {
            /* Memory map tag */
            mb2_tag_mmap_t *mmap = (mb2_tag_mmap_t *)tag;
            uint8_t *entry_ptr = (uint8_t *)mmap + sizeof(mb2_tag_mmap_t);
            uint8_t *mmap_end = (uint8_t *)mmap + mmap->size;

            while (entry_ptr < mmap_end) {
                mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)entry_ptr;

                if (e->mem_type == MB2_MEMORY_AVAILABLE) {
                    /* Free this region frame by frame */
                    uint64_t start = e->base_addr;
                    uint64_t end = start + e->length;

                    /* Round start up and end down to page boundaries */
                    uint32_t frame_start = (uint32_t)((start + PAGE_SIZE - 1) / PAGE_SIZE);
                    uint32_t frame_end = (uint32_t)(end / PAGE_SIZE);

                    for (uint32_t f = frame_start; f < frame_end && f < MAX_FRAMES; f++) {
                        frame_clear(f);
                        free_frames++;
                        total_frames++;
                    }
                }
                entry_ptr += mmap->entry_size;
            }
        }
        /* Advance to next tag (tags are 8-byte aligned) */
        uint32_t next = (uint32_t)(uintptr_t)tag + tag->size;
        next = (next + 7) & ~7u;
        tag = (mb2_tag_t *)(uintptr_t)next;
    }

    /* 3. Re-mark frame 0 as USED (never hand out physical address 0 —
          makes NULL pointer dereferences easier to catch) */
    frame_set(0);

    /* 4. Mark all frames the kernel occupies as USED */
    uint32_t kernel_end_frame = (uint32_t)((uintptr_t)&_kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t f = 0; f <= kernel_end_frame && f < MAX_FRAMES; f++) {
        if (!frame_test(f)) {
            frame_set(f);
            free_frames--;
        }
    }

    /* 5. Also mark the Multiboot2 info structure itself as USED */
    uint32_t mb_frame = mboot_ptr / PAGE_SIZE;
    if (mb_frame < MAX_FRAMES && !frame_test(mb_frame)) {
        frame_set(mb_frame);
        free_frames--;
    }
}

uint32_t pmm_alloc_frame(void) {
    if (free_frames == 0)
        return 0;

    for (uint32_t i = 0; i < MAX_FRAMES / 32; i++) {
        if (pmm_bitmap[i] == 0xFFFFFFFF)
            continue; /* all used, skip */
        /* Find first free bit */
        for (uint32_t bit = 0; bit < 32; bit++) {
            uint32_t frame = i * 32 + bit;
            if (!frame_test(frame)) {
                frame_set(frame);
                free_frames--;
                return frame * PAGE_SIZE;
            }
        }
    }
    return 0; /* out of memory */
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame = phys_addr / PAGE_SIZE;
    if (frame >= MAX_FRAMES)
        return;
    if (!frame_test(frame))
        return; /* double-free guard */
    frame_clear(frame);
    free_frames++;
}

void pmm_print_stats(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("[OK] PMM initialized\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
    kprint("     Total frames : ");
    kprint_int((int32_t)total_frames);
    kprint("  (");
    kprint_int((int32_t)(total_frames * PAGE_SIZE / 1024));
    kprint(" KB)\n");
    kprint("     Free  frames : ");
    kprint_int((int32_t)free_frames);
    kprint("  (");
    kprint_int((int32_t)(free_frames * PAGE_SIZE / 1024));
    kprint(" KB)\n");
}