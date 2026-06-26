#include "commands.h"
#include "../kernel/vga.h"
#include "../kernel/ramfs.h"
#include "../kernel/elf.h"
#include "../kernel/process.h"
#include <stdint.h>

/* ---- ls ---- */

void cmd_ls(int argc, char** argv) {
    (void)argc; (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_CYAN);
    kprint("NAME                 SIZE\n");
    kprint("----------------     ----------\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);

    int any = 0;
    int cap = ramfs_capacity();
    for (int i = 0; i < cap; i++) {
        const ramfs_file_t* f = ramfs_get(i);
        if (!f) continue;
        any = 1;

        uint32_t len = 0;
        while (f->name[len]) len++;

        kprint(f->name);
        for (uint32_t j = len; j < 20; j++) kprint(" ");
        kprint_int((int32_t)f->size);
        kprint(" bytes\n");
    }

    if (!any) kprint("(no files)\n");
}

/* ---- run ---- */

/*
 * run <filename> — looks the file up in ramfs, validates it's a real
 * ELF32 executable, and launches it via process_create_from_elf().
 * This is the general-purpose version of what `elftest` did with a
 * single hardcoded binary — any file ramfs_register()'d at boot (or,
 * once a real write path exists, created at runtime) can be run this
 * way by name.
 */
void cmd_run(int argc, char** argv) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("usage: run <filename>\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    const char* name = argv[1];
    int idx = ramfs_lookup(name);
    if (idx < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("run: no such file: ");
        kprint(name);
        kprint("\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    const ramfs_file_t* f = ramfs_get(idx);
    /* idx came straight from ramfs_lookup, which only returns indices
       of in_use entries, so f should never be NULL here — but check
       anyway rather than trust that invariant blindly. */
    if (!f) {
        kprint("run: internal error (file disappeared)\n");
        return;
    }

    uint32_t entry;
    if (!elf32_validate(f->data, &entry)) {
        vga_set_color(VGA_COLOR_LIGHT_RED);
        kprint("run: not a valid ELF32 executable: ");
        kprint(name);
        kprint("\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY);
        return;
    }

    process_t* p = process_create_from_elf(name, f->data, 5);
    if (!p) {
        kprint("run: process_create_from_elf failed\n");
        return;
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN);
    kprint("run: spawned pid=");
    kprint_int((int32_t)p->pid);
    kprint(" entry=");
    kprint_hex(p->user_entry);
    kprint("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY);
}