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
    shell/shell.c \
    shell/commands.c \
    mm/kmalloc.c \
    drivers/keyboard.c

ASM_SRCS = \
	boot/boot.asm \
	boot/gdt_flush.asm \
	boot/isr.asm \
	boot/context_switch.asm \
	boot/usermode.asm

# Object files inside build/
C_OBJS   = $(patsubst %.c,build/%.o,$(C_SRCS))
ASM_OBJS = $(patsubst %.asm,build/%.o,$(ASM_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS)

# Output
KERNEL = build/stakos.elf
ISO    = build/stakos.iso

.PHONY: all clean iso run

all: $(KERNEL)

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