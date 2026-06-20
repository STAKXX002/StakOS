# Toolchain
CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-gcc

# Flags
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
ASFLAGS = -f elf32
LDFLAGS = -ffreestanding -O2 -nostdlib -T linker.ld

# Sources
C_SRCS = \
    kernel/kernel.c \
    kernel/gdt.c \
    kernel/idt.c \
    kernel/vga.c \
    kernel/pmm.c \
    kernel/paging.c \
    kernel/process.c \
    kernel/scheduler.c \
    kernel/pit.c \
    kernel/syscall.c \
    kernel/elf.c \
    shell/shell.c \
    shell/commands.c \
    mm/kmalloc.c \
    drivers/keyboard.c

ASM_SRCS = \
	boot/boot.asm \
	boot/gdt_flush.asm \
	boot/isr.asm \
	boot/context_switch.asm \
	boot/usermode.asm \
	boot/test_prog_blob.asm

# Object files inside build/
C_OBJS   = $(patsubst %.c,build/%.o,$(C_SRCS))
ASM_OBJS = $(patsubst %.asm,build/%.o,$(ASM_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS)

# Output
KERNEL = build/stakos.elf
ISO    = build/stakos.iso

.PHONY: all clean iso run

all: $(KERNEL)

# ---- Userland test program (stage 10) ----
# A minimal freestanding ELF32 binary, embedded into the kernel image
# via incbin so the ELF loader has something real to parse and run
# before a filesystem exists.
USER_CFLAGS = -ffreestanding -nostdlib -m32

build/userland/test_prog.o: userland/test_prog.c
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/userland/test_prog.elf: build/userland/test_prog.o userland/test_prog.ld
	i686-elf-ld -m elf_i386 -T userland/test_prog.ld $< -o $@

# The blob object MUST be built after test_prog.elf exists, since
# test_prog_blob.asm directly incbins it. This explicit dependency
# is what makes `make` build the userland ELF first automatically.
build/boot/test_prog_blob.o: boot/test_prog_blob.asm build/userland/test_prog.elf
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Compile C
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble
build/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Link
$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

# Build ISO
iso: $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/stakos.elf
	cp iso/boot/grub/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

# Run in QEMU
run: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M

# Clean
clean:
	rm -rf build isodir