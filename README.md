# SCAMPEREDOS

A small x86 operating system built from scratch in C and NASM assembly: custom bootloader, a multiboot-compliant kernel, a FAT12 filesystem driver, and a simple text-mode terminal shell.

## What it does

- Boots via GRUB (multiboot) into a 32-bit protected-mode kernel
- Loads a FAT12 disk image (`rootfs.img`) as a GRUB module and mounts it as an in-memory ("RAM disk") filesystem, no BIOS disk I/O
- Draws a VGA text-mode taskbar with three tabs, **Desktop**, **Terminal**, **Shutdown**, navigated with the Left/Right arrow keys
- The Terminal tab runs a tiny shell with its own in-memory file table, supporting:
  - `write <file>`: create/append a file, line by line, until `.stop`
  - `read <file>`: print a file's contents
  - `delete <file>`: remove a file
  - `ls`: list files
  - `shutdown`: halt the CPU

## Prerequisites

This builds on Linux (or WSL on Windows, since plain `-m32` GCC/`grub-mkrescue` aren't readily available on native Windows):

- `nasm`
- `gcc` with 32-bit (`-m32`) support (install the multilib package, e.g. `gcc-multilib` on Debian/Ubuntu)
- `grub-pc-bin` + `xorriso` (provides `grub-mkrescue`)
- `qemu-system-i386` (or `qemu-system-x86_64`)

On Ubuntu/Debian/WSL:

```bash
sudo apt install nasm gcc-multilib grub-pc-bin xorriso qemu-system-x86
```

## Build

```bash
make
```

This compiles the bootloader and kernel, links `build/kernel.bin`, stages it together with `iso/boot/grub/grub.cfg` and `rootfs.img` into `isofiles/`, and runs `grub-mkrescue` to produce a bootable `scampered.iso`.

## Run (QEMU)

```bash
qemu-system-i386 -cdrom scampered.iso
```

GRUB will boot straight into the kernel (`set timeout=0`), which initializes the FAT12 driver from the `rootfs.img` module and drops you into the desktop/taskbar UI.

## Clean

```bash
make clean
```

## Project structure

```
SCAMOS/
├── Makefile
├── src/
│   ├── boot.s          # multiboot header + entry point (NASM)
│   ├── kernel.c         # kernel_main: VGA output, keyboard input, taskbar, shell
│   ├── fat12.c/.h       # FAT12 filesystem driver
│   ├── disk.c/.h        # in-memory "disk" backed by the GRUB module
│   ├── string.c/.h      # freestanding string helpers (no libc)
│   └── linker.ld        # multiboot ELF link script
├── iso/boot/grub/
│   └── grub.cfg         # GRUB menu config
└── rootfs.img           # prebuilt FAT12 image loaded as the GRUB module
```
