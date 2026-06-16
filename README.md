# StakOS

StakOS is a 32-bit x86 operating system written from scratch in C and Assembly. The project focuses on learning and exploring systems programming by building kernel components from the ground up.

## Features

* Multiboot support
* GDT and IDT
* Interrupt handling
* PIT timer
* Physical memory manager
* Paging
* Kernel heap (`kmalloc`)
* Process scheduler
* Context switching
* VGA text console
* Keyboard driver
* Interactive shell

## Build

### Requirements

* i686-elf-gcc
* NASM
* GRUB
* QEMU

### Building

Build the bootable ISO:

```bash id="1qf2zx"
make iso
```

Run StakOS in QEMU:

```bash id="8jg4lw"
make run
```

Remove build artifacts:

```bash id="3c9mvn"
make clean
```

## Planned Features

* File system support
* System calls
* User mode
* More drivers

StakOS is developed as a project to understand operating systems from the hardware level upward.
