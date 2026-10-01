#include "disk.h"
#include <stdint.h>

/*
    GRUB loads your FAT12 image as a multiboot module.
    kernel_main() must call:

        disk_init(mb_info_addr);

    After that:
        - disk_read  reads bytes from the RAM disk
        - disk_write writes bytes to the RAM disk (optional)

    NOTE:
        This replaces BIOS disk I/O completely.
*/

static uint8_t* disk_base = 0;
static uint32_t disk_size = 0;

/* ==== Multiboot structures (minimal versions) ==== */

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} multiboot_info_t;

typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;


/* ============================================================
   Initialize disk system by reading GRUB module #0
   ============================================================ */
void disk_init(uint32_t mb_info_addr)
{
    multiboot_info_t* mb = (multiboot_info_t*)mb_info_addr;

    // Multiboot flag bit 3 = modules present
    if (!(mb->flags & (1 << 3))) {
        disk_base = 0;
        disk_size = 0;
        return;
    }

    multiboot_module_t* mods = (multiboot_module_t*)mb->mods_addr;

    // Expect module 0 = rootfs.img
    disk_base = (uint8_t*)mods[0].mod_start;
    disk_size = mods[0].mod_end - mods[0].mod_start;
}


/* ============================================================
   Read LBA from RAM disk
   ============================================================ */
int disk_read(uint32_t lba, void* buf, uint32_t bytes)
{
    if (!disk_base) return -1;

    uint32_t offset = lba * 512;

    if (offset + bytes > disk_size)
        return -1;

    uint8_t* out = (uint8_t*)buf;
    uint8_t* src = disk_base + offset;

    for (uint32_t i = 0; i < bytes; i++)
        out[i] = src[i];

    return 0;
}


/* ============================================================
   Write LBA to RAM disk
   (Note: RAM disk = temporary; changes NOT persistent)
   ============================================================ */
int disk_write(uint32_t lba, const void* buf, uint32_t bytes)
{
    if (!disk_base) return -1;

    uint32_t offset = lba * 512;

    if (offset + bytes > disk_size)
        return -1;

    uint8_t* dst = disk_base + offset;
    const uint8_t* src = (const uint8_t*)buf;

    for (uint32_t i = 0; i < bytes; i++)
        dst[i] = src[i];

    return 0;
}
