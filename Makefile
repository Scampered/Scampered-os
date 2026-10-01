# Makefile for SCAMPEREDOS (multiboot + rootfs.img as GRUB module)

CROSS =
CC = $(CROSS)gcc
LD = $(CROSS)ld
AS = nasm

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-builtin
LDFLAGS = -m elf_i386

SRC = src
BUILD = build
ISO_DIR = isofiles

OBJS = $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/fat12.o $(BUILD)/disk.o $(BUILD)/string.o

all: scampered.iso

$(BUILD)/boot.o: $(SRC)/boot.s
	mkdir -p $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.o: $(SRC)/kernel.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fat12.o: $(SRC)/fat12.c $(SRC)/fat12.h $(SRC)/disk.h $(SRC)/string.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/disk.o: $(SRC)/disk.c $(SRC)/disk.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/string.o: $(SRC)/string.c $(SRC)/string.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Link kernel (multiboot ELF executable)
$(BUILD)/kernel.bin: $(OBJS) $(SRC)/linker.ld
	$(LD) $(LDFLAGS) -T $(SRC)/linker.ld $(OBJS) -o $@

# Build ISO contents
iso-tree: $(BUILD)/kernel.bin
	mkdir -p $(ISO_DIR)/boot/grub

	# kernel (multiboot)
	cp $(BUILD)/kernel.bin $(ISO_DIR)/boot/kernel.bin

	# GRUB CONFIG (CORRECT PATH!)
	cp iso/boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg

	# MODULE #0 rootfs.img
	cp rootfs.img $(ISO_DIR)/rootfs.img

scampered.iso: iso-tree
	grub-mkrescue -o scampered.iso $(ISO_DIR)

clean:
	rm -rf $(BUILD) $(ISO_DIR) scampered.iso

.PHONY: all clean iso-tree
