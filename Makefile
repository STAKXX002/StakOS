# Toolchain
CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-gcc

# Flags
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
ASFLAGS = -f elf32
LDFLAGS = -ffreestanding -O2 -nostdlib -T linker.ld

# Sources
C_SRCS  = kernel/kernel.c kernel/gdt.c
ASM_SRCS = boot/boot.asm boot/gdt_flush.asm

# Objects
C_OBJS   = $(C_SRCS:.c=.o)
ASM_OBJS = $(ASM_SRCS:.asm=.o)
OBJS     = $(ASM_OBJS) $(C_OBJS)

# Output
KERNEL  = build/stakos.elf
ISO     = build/stakos.iso

.PHONY: all clean iso run

all: $(KERNEL)

# Compile C
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Link
$(KERNEL): $(OBJS)
	mkdir -p build
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

clean:
	rm -rf build isodir $(OBJS)
