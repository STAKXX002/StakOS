#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_MAGIC 0x464C457F /* "\x7FELF" little-endian as u32 */
#define ELF_CLASS_32 1
#define ELF_DATA_LE 1
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_386 3

#define PT_LOAD 1 /* loadable segment — the only type we handle */

#define PF_X (1u << 0) /* executable */
#define PF_W (1u << 1) /* writable   */
#define PF_R (1u << 2) /* readable   */

typedef struct __attribute__((packed)) {
  uint32_t magic;   /* must equal ELF_MAGIC */
  uint8_t ei_class; /* 1 = 32-bit */
  uint8_t ei_data;  /* 1 = little-endian */
  uint8_t version;
  uint8_t osabi;
  uint8_t abiversion;
  uint8_t pad[7];
  uint16_t type;    /* 2 = executable */
  uint16_t machine; /* 3 = x86 */
  uint32_t version2;
  uint32_t entry; /* virtual address of the entry point */
  uint32_t phoff; /* file offset of the program header table */
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize; /* size of one program header entry */
  uint16_t phnum;     /* number of program header entries */
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf32_header_t;

typedef struct __attribute__((packed)) {
  uint32_t type;   /* PT_LOAD, etc. */
  uint32_t offset; /* offset in the file */
  uint32_t vaddr;  /* virtual address to load at */
  uint32_t paddr;  /* unused by us — ELF leftover field */
  uint32_t filesz; /* bytes to copy from the file */
  uint32_t memsz;  /* bytes to reserve in memory (>= filesz; the
                       difference is zero-filled, e.g. for .bss) */
  uint32_t flags;  /* PF_R / PF_W / PF_X */
  uint32_t align;
} elf32_phdr_t;

/*
 * Validates an ELF32 image sitting at `data` and fills `out_entry`
 * with its entry point. Returns 1 on success, 0 if the image fails
 * any check (bad magic, wrong class/machine, not executable, etc).
 * Does not load or map anything — that's elf_load() in stage 10b.
 */
int elf32_validate(const uint8_t *data, uint32_t *out_entry);

/*
 * Returns a pointer to the i'th program header, or NULL if i is out
 * of range. Caller is expected to have already validated the image.
 */
const elf32_phdr_t *elf32_get_phdr(const uint8_t *data, uint32_t i);

/* Number of program headers in the image. 0 if data is not validated. */
uint32_t elf32_phdr_count(const uint8_t *data);

/*
 * Loads every PT_LOAD segment from `data` into the page directory at
 * `pd_phys`. Allocates fresh physical frames and copies file data,
 * zero-filling memsz-filesz (e.g. .bss). pd_phys does not need to be
 * the live CR3. Returns 1 on success, 0 on OOM.
 */
int elf32_load(const uint8_t *data, uint32_t pd_phys);

#endif