#ifndef DISK_H
#define DISK_H

#include <stdint.h>

/* Initialize disk subsystem from GRUB multiboot info pointer (address) */
void disk_init(uint32_t mb_info_addr);

/* Read 'bytes' bytes starting at LBA 'lba' into buf.
   bytes must be a multiple of 512 (sector size).
   Returns 0 on success, -1 on error. */
int disk_read(uint32_t lba, void* buf, uint32_t bytes);

/* Write 'bytes' bytes from buf into LBA 'lba' region of RAM-disk.
   Returns 0 on success, -1 on error. */
int disk_write(uint32_t lba, const void* buf, uint32_t bytes);

#endif
