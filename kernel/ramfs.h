#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

#define RAMFS_MAX_FILES 16
#define RAMFS_MAX_NAME_LEN 32

/*
 * A single file entry. `data` points directly at wherever the bytes
 * already live — usually a kernel-image blob (incbin) or a kmalloc'd
 * buffer — ramfs does not own or copy this memory, it just indexes
 * it. That means a ramfs_register()'d buffer must outlive the file
 * table entry; there's no refcounting yet.
 */
typedef struct {
  char name[RAMFS_MAX_NAME_LEN];
  const uint8_t *data;
  uint32_t size;
  int in_use;
} ramfs_file_t;

/* Resets the file table to empty. Call once during boot. */
void ramfs_init(void);

/*
 * Registers a file under `name`, backed by `data`/`size`. Returns the
 * file's index (>= 0) on success, or -1 if the name is too long, the
 * table is full, or the name is already registered.
 */
int ramfs_register(const char *name, const uint8_t *data, uint32_t size);

/*
 * Looks up a file by exact name (no path parsing — flat namespace).
 * Returns its index, or -1 if not found.
 */
int ramfs_lookup(const char *name);

/* Returns the file_t at `index`, or NULL if index is out of range or unused. */
const ramfs_file_t *ramfs_get(int index);

/*
 * Returns the highest valid index + 1 — i.e. callers can iterate
 * `for (int i = 0; i < ramfs_capacity(); i++)` and check ramfs_get(i)
 * for NULL to skip unused slots. Used by the shell's `ls` command.
 */
int ramfs_capacity(void);

#endif