#include "ramfs.h"
#include <stddef.h>

static ramfs_file_t files[RAMFS_MAX_FILES];

static int str_eq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static uint32_t str_len(const char* s) {
    uint32_t n = 0;
    while (s[n] && n < RAMFS_MAX_NAME_LEN - 1) n++;
    return n;
}

void ramfs_init(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        files[i].in_use = 0;
}

int ramfs_register(const char* name, const uint8_t* data, uint32_t size) {
    uint32_t len = str_len(name);
    if (len == 0 || name[len] != '\0') return -1;   /* name too long */

    if (ramfs_lookup(name) >= 0) return -1;          /* already registered */

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].in_use) {
            for (uint32_t j = 0; j <= len; j++)
                files[i].name[j] = name[j];
            files[i].data   = data;
            files[i].size   = size;
            files[i].in_use = 1;
            return i;
        }
    }
    return -1;   /* table full */
}

int ramfs_lookup(const char* name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].in_use && str_eq(files[i].name, name))
            return i;
    }
    return -1;
}

const ramfs_file_t* ramfs_get(int index) {
    if (index < 0 || index >= RAMFS_MAX_FILES) return NULL;
    if (!files[index].in_use) return NULL;
    return &files[index];
}