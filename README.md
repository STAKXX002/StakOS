# StakOS

A 32-bit x86 kernel written from scratch in C and x86 assembly - no libc, no existing kernel code, built up one subsystem at a time. The goal is to understand operating systems from the hardware up: how a CPU boots, how interrupts actually work, how memory is managed without an MMU doing favors for you, and how a scheduler decides who runs next.

Development follows a staged progression, each stage adding one subsystem on top of a working version of the last.

## Subsystems

**Boot & core**
- Multiboot2-compliant boot, GDT with a flat memory model
- IDT with full exception handling (ISR0–31) and PIC-remapped IRQs
- VGA text-mode console driver, PS/2 keyboard driver

**Memory**
- Physical memory manager - bitmap allocator driven by the Multiboot2 memory map
- Paging - identity-mapped kernel space, **per-process page directories** with isolated address spaces and CR3 switching on every context switch
- Page-fault handler with CR2 / error-code decoding
- Kernel heap (`kmalloc`/`kfree`) - free-list allocator with block-magic corruption/double-free detection and adjacency-checked coalescing

**Processes & scheduling**
- Process control blocks with READY / RUNNING / BLOCKED / ZOMBIE states
- Per-process kernel stacks (allocated from physical memory frames, not embedded in the PCB)
- Round-robin scheduler with per-process priority quantums
- Hand-written context switching (`context_switch.asm`) - full register save/restore via the stack
- PIT-driven preemption at 100 Hz

**Syscalls**
- `int 0x80` gate with a DPL-3 IDT entry (ready for ring 3 in advance)
- Syscall ABI: `eax` = number, `ebx`/`ecx`/`edx` = arguments, return value threaded back through `iret`
- `SYS_WRITE`, `SYS_EXIT`, `SYS_YIELD` implemented and tested end-to-end from kernel mode

**Shell**
An interactive command-line shell with a readline loop, argv-style parsing, and built-in commands:

| Command | Description |
|---|---|
| `help` | list available commands |
| `clear` | clear the screen |
| `echo` | print arguments |
| `meminfo` | physical memory manager statistics |
| `memtest` | alloc/free N physical frames |
| `hexdump` | dump memory at an address |
| `heapinfo` | kernel heap statistics |
| `heaptest` | alloc/free/read-back heap chunks |
| `ps` | list running and queued processes |
| `sleep` | sleep N timer ticks |
| `uptime` | ticks and seconds since boot |
| `vminfo` | per-process virtual memory / CR3 info |
| `synctest` | exercise the `int 0x80` syscall gate round-trip |
| `usertest` | spawn a process and run it at ring 3 |

## Build

### Requirements
- [`i686-elf` cross-compiler](https://wiki.osdev.org/GCC_Cross-Compiler) (`i686-elf-gcc`) - StakOS targets bare metal, not your host OS, so a standard GCC won't work
- NASM
- GRUB (`grub-mkrescue`) for building the bootable ISO
- QEMU (`qemu-system-i386`) for running it

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

## Roadmap

- [x] Multiboot2 boot, GDT, IDT, PIC
- [x] Physical memory manager + paging
- [x] Kernel heap allocator
- [x] Interactive shell
- [x] Process model + round-robin scheduler
- [x] Per-process virtual address spaces
- [x] Syscall gate (kernel-mode)
- [x] User mode (ring 3) + TSS - running the existing syscall ABI from real user processes
- [x] ELF loader
- [x] Virtual filesystem + ramfs
- [ ] `fork` / `wait` / pipes

## Why

Most of what's "given" in a normal OS course - memory protection, scheduling fairness, a syscall boundary - is invisible until you've built the version that doesn't have it yet. StakOS exists to make those invisible guarantees visible, one stage at a time.