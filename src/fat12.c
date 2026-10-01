#include "fat12.h"
#include "disk.h"
#include "string.h"
#include <stdint.h>
#include <string.h>   // ADD THIS


/* Minimal, conservative FAT12 reader/writer. Not production-grade
   but suitable for floppy-style FAT12 images. */

#define SECTOR_SIZE 512
#define MAX_FAT_BYTES 65536   /* safety limit */

static uint16_t bytes_per_sector;
static uint8_t  sectors_per_cluster;
static uint16_t reserved_sectors;
static uint8_t  num_fats;
static uint16_t max_root_entries;
static uint32_t total_sectors;
static uint16_t sectors_per_fat;

static uint32_t root_dir_sector;
static uint32_t root_dir_sectors;
static uint32_t first_data_sector;
static uint32_t fat_start_sector;
static uint32_t data_sectors;
static uint32_t total_clusters;

/* FAT copy buffer (we'll store only one FAT copy in RAM then write to all copies). */
static uint8_t fat_buf[MAX_FAT_BYTES];
static uint32_t fat_bytes = 0;

/* Helpers to read sector(s) */
static int read_sector_u32(uint32_t lba, void* buf, uint32_t count) {
    return disk_read(lba, buf, count * SECTOR_SIZE);
}
static int write_sector_u32(uint32_t lba, const void* buf, uint32_t count) {
    return disk_write(lba, buf, count * SECTOR_SIZE);
}

/* Read BPB from sector 0 and initialize computed values */
int fat12_init(void) {
    uint8_t sec[SECTOR_SIZE];
    if (read_sector_u32(0, sec, 1) != 0) return -1;

    bytes_per_sector = (uint16_t)sec[11] | ((uint16_t)sec[12] << 8);
    sectors_per_cluster = sec[13];
    reserved_sectors = (uint16_t)sec[14] | ((uint16_t)sec[15] << 8);
    num_fats = sec[16];
    max_root_entries = (uint16_t)sec[17] | ((uint16_t)sec[18] << 8);
    uint16_t tot16 = (uint16_t)sec[19] | ((uint16_t)sec[20] << 8);
    sectors_per_fat = (uint16_t)sec[22] | ((uint16_t)sec[23] << 8);
    uint32_t tot32 = (uint32_t)sec[32] | ((uint32_t)sec[33] << 8) | ((uint32_t)sec[34] << 16) | ((uint32_t)sec[35] << 24);

    total_sectors = (tot16 != 0) ? tot16 : tot32;

    if (bytes_per_sector != SECTOR_SIZE) return -1; /* safety */

    fat_start_sector = reserved_sectors;
    /* Above line has accidental space: fix below */
    (void)0; /* placeholder */
    /* recompute properly: */
    fat_start_sector = reserved_sectors;

    /* root dir sectors */
    root_dir_sectors = ((max_root_entries * 32) + (bytes_per_sector - 1)) / bytes_per_sector;

    first_data_sector = reserved_sectors + (num_fats * sectors_per_fat) + root_dir_sectors;
    root_dir_sector = reserved_sectors + (num_fats * sectors_per_fat);
    data_sectors = total_sectors - (reserved_sectors + (num_fats * sectors_per_fat) + root_dir_sectors);

    total_clusters = data_sectors / sectors_per_cluster;
    if (total_clusters < 1) return -1;

    /* load FAT (first copy) into fat_buf */
    fat_bytes = (uint32_t)sectors_per_fat * bytes_per_sector;
    if (fat_bytes > MAX_FAT_BYTES) return -1;
    if (read_sector_u32(reserved_sectors, fat_buf, sectors_per_fat) != 0) return -1;

    return 0;
}

/* Convert "NAME.EXT" (case-insensitive) into raw 11-byte FAT name */
static void to_raw11(const char* name, char out[11]) {
    int i;
    for (i = 0; i < 11; i++) out[i] = ' ';
    int p = 0;
    /* copy name upto '.' */
    int j = 0;
    while(name[j] && name[j] != '.' && p < 8) {
        char c = name[j++];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[p++] = c;
    }
    if (name[j] == '.') {
        j++;
        int q = 8;
        while(name[j] && q < 11) {
            char c = name[j++];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[q++] = c;
        }
    }
}

/* Read FAT12 12-bit entry */
static uint16_t fat12_get(uint16_t cluster) {
    uint32_t off = (cluster * 3) / 2;
    if (off + 1 >= fat_bytes) return 0xFFF;
    uint16_t a = fat_buf[off];
    uint16_t b = fat_buf[off + 1];
    uint16_t val;
    if (cluster & 1) {
        val = ((b << 8) | a) >> 4;
    } else {
        val = ((b << 8) | a) & 0x0FFF;
    }
    return val;
}

/* Set FAT12 12-bit entry */
static void fat12_set(uint16_t cluster, uint16_t value) {
    uint32_t off = (cluster * 3) / 2;
    if (off + 1 >= fat_bytes) return;
    uint16_t a = fat_buf[off];
    uint16_t b = fat_buf[off + 1];
    uint16_t entry = (b << 8) | a;
    if (cluster & 1) {
        /* replace high 12 bits */
        entry &= 0x000F;
        entry |= (value << 4) & 0xFFF0;
    } else {
        /* replace low 12 bits */
        entry &= 0xF000;
        entry |= (value & 0x0FFF);
    }
    fat_buf[off] = entry & 0xFF;
    fat_buf[off + 1] = (entry >> 8) & 0xFF;
}

/* Find a root directory entry by raw 11-byte name */
static int find_root_entry_by_raw(const char raw[11], uint32_t* entry_sector_out, uint32_t* entry_offset_out, uint16_t* first_cluster_out, uint32_t* size_out) {
    uint8_t buf[SECTOR_SIZE];
    uint32_t entries = max_root_entries;
    uint32_t e = 0;
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (read_sector_u32(root_dir_sector + s, buf, 1) != 0) return -1;
        for (uint32_t off = 0; off + 32 <= SECTOR_SIZE; off += 32) {
            if (e >= entries) return -1;
            /* if entry is empty (first byte 0x00) -> end */
            if (buf[off] == 0x00) return -1;
            /* skip deleted (0xE5) or volume labels (attr & 0x08) */
            if ((uint8_t)buf[off] == 0xE5) { e++; continue; }
            uint8_t attr = buf[off + 11];
            if (attr & 0x08) { e++; continue; }
            if (memcmp(raw, (char*)&buf[off], 11) == 0) {
                uint16_t clust = (uint16_t)buf[off + 26] | ((uint16_t)buf[off + 27] << 8);
                uint32_t fsize = (uint32_t)buf[off + 28] | ((uint32_t)buf[off + 29] << 8) | ((uint32_t)buf[off + 30] << 16) | ((uint32_t)buf[off + 31] << 24);
                *entry_sector_out = root_dir_sector + s;
                *entry_offset_out = off;
                *first_cluster_out = clust;
                *size_out = fsize;
                return 0;
            }
            e++;
        }
    }
    return -1;
}

/* Read cluster into buffer (cluster number >=2) */
static int read_cluster(uint16_t cluster, void* out_buf) {
    if (cluster < 2) return -1;
    uint32_t first_sector_of_cluster = first_data_sector + ((cluster - 2) * sectors_per_cluster);
    return read_sector_u32(first_sector_of_cluster, out_buf, sectors_per_cluster);
}

/* Write cluster from buffer */
static int write_cluster(uint16_t cluster, const void* in_buf) {
    if (cluster < 2) return -1;
    uint32_t first_sector_of_cluster = first_data_sector + ((cluster - 2) * sectors_per_cluster);
    return write_sector_u32(first_sector_of_cluster, in_buf, sectors_per_cluster);
}

/* Read full file by raw name into dest */
int fat12_read_file_by_rawname(const char raw11[11], void* dest, uint32_t dest_size) {
    uint32_t entry_sector, entry_offset, size;
    uint16_t first_cluster;
    if (find_root_entry_by_raw(raw11, &entry_sector, &entry_offset, &first_cluster, &size) != 0) return -1;
    uint8_t cluster_buf[SECTOR_SIZE *  (8)]; /* max cluster buffer: sectors_per_cluster used dynamically */
    uint32_t bytes_remaining = size;
    uint32_t dest_pos = 0;
    uint16_t cluster = first_cluster;
    while (bytes_remaining > 0) {
        if (cluster >= 0xFF8) break; /* end */
        /* read cluster sectors one by one into a local small buffer sized for sectors_per_cluster */
        uint8_t tmp[SECTOR_SIZE * 4]; /* conservative; typical cluster size small */
        /* to be safe, read sector-by-sector */
        uint32_t first_sector = first_data_sector + (cluster - 2) * sectors_per_cluster;
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            if (read_sector_u32(first_sector + s, tmp + s * SECTOR_SIZE, 1) != 0) return -1;
        }
        uint32_t copy = sectors_per_cluster * SECTOR_SIZE;
        if (copy > bytes_remaining) copy = bytes_remaining;
        if (dest_pos + copy > dest_size) return -1;
        for (uint32_t i = 0; i < copy; i++) ((uint8_t*)dest)[dest_pos + i] = tmp[i];
        dest_pos += copy;
        bytes_remaining -= copy;
        /* next cluster */
        uint16_t next = fat12_get(cluster);
        if (next >= 0xFF8) break;
        if (next == 0x000) break;
        cluster = next;
    }
    return (int)size;
}

/* Accept "NAME.EXT" and call rawname reader */
int fat12_read_file(const char* name_dot_ext, void* dest, uint32_t dest_size) {
    char raw[11];
    to_raw11(name_dot_ext, raw);
    return fat12_read_file_by_rawname(raw, dest, dest_size);
}

/* Find a free cluster by searching FAT for 0 */
static int find_free_cluster(uint16_t* out_cluster) {
    /* cluster numbering from 2..total_clusters+1 */
    for (uint16_t c = 2; c < (uint16_t)(total_clusters + 2); c++) {
        if (fat12_get(c) == 0x000) {
            *out_cluster = c;
            return 0;
        }
    }
    return -1;
}

/* Write FAT buffer back to all FAT copies and flush */
int fat12_flush(void) {
    /* write primary FAT */
    if (write_sector_u32(reserved_sectors, fat_buf, sectors_per_fat) != 0) return -1;
    /* write additional copies */
    for (uint8_t i = 1; i < num_fats; i++) {
        uint32_t s = reserved_sectors + i * sectors_per_fat;
        if (write_sector_u32(s, fat_buf, sectors_per_fat) != 0) return -1;
    }
    return 0;
}

/* Create or overwrite a root dir entry for name; allocate clusters and write data */
int fat12_write_file(const char* name_dot_ext, const void* data, uint32_t size) {
    char raw[11];
    to_raw11(name_dot_ext, raw);
    /* check for existing entry */
    uint32_t entry_sector=0, entry_offset=0;
    uint16_t first_cluster = 0;
    uint32_t existing_size = 0;
    int exists = (find_root_entry_by_raw(raw, &entry_sector, &entry_offset, &first_cluster, &existing_size) == 0);

    /* allocate needed clusters */
    uint32_t bytes_per_cluster = bytes_per_sector * sectors_per_cluster;
    uint32_t need_clusters = (size + bytes_per_cluster - 1) / bytes_per_cluster;
    if (need_clusters == 0) need_clusters = 1;

    uint16_t clusters[4096];
    if (need_clusters > 4096) return -1; /* too big */

    for (uint32_t i = 0; i < need_clusters; i++) {
        uint16_t freec;
        if (find_free_cluster(&freec) != 0) return -1;
        /* mark as used temporarily to avoid reuse during this allocation */
        fat12_set(freec, 0xFF7); /* mark bad temporarily */
        clusters[i] = freec;
    }

    /* link clusters in FAT */
    for (uint32_t i = 0; i < need_clusters; i++) {
        uint16_t next = (i + 1 < need_clusters) ? clusters[i + 1] : 0xFFF;
        fat12_set(clusters[i], next);
    }

    /* write cluster data */
    const uint8_t* src = (const uint8_t*)data;
    uint8_t tmp[SECTOR_SIZE];
    for (uint32_t i = 0; i < need_clusters; i++) {
        /* build cluster buffer of sectors_per_cluster sectors */
        uint8_t clusterbuf[SECTOR_SIZE * 4]; /* conservative */
        uint32_t tocopy = bytes_per_cluster;
        if (i == need_clusters - 1) {
            uint32_t remaining = size - (i * bytes_per_cluster);
            if (remaining < tocopy) tocopy = remaining;
        }
        /* zero the cluster buffer */
        for (uint32_t b = 0; b < (uint32_t)bytes_per_cluster; b++) clusterbuf[b] = 0;
        for (uint32_t b = 0; b < tocopy; b++) clusterbuf[b] = src[i * bytes_per_cluster + b];
        /* write sector-by-sector */
        uint32_t first_sector = first_data_sector + (clusters[i] - 2) * sectors_per_cluster;
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            if (write_sector_u32(first_sector + s, clusterbuf + s * SECTOR_SIZE, 1) != 0) return -1;
        }
    }

    /* write FAT back to disk */
    if (fat12_flush() != 0) return -1;

    /* update (or create) root dir entry */
    uint8_t dirsec[SECTOR_SIZE];
    uint32_t found_spot_sector = 0;
    uint32_t found_spot_offset = 0;
    uint8_t got_spot = 0;
    uint32_t e = 0;
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (read_sector_u32(root_dir_sector + s, dirsec, 1) != 0) return -1;
        for (uint32_t off = 0; off + 32 <= SECTOR_SIZE; off += 32) {
            if (dirsec[off] == 0x00 || dirsec[off] == 0xE5) {
                /* free slot */
                if (!got_spot) {
                    found_spot_sector = root_dir_sector + s;
                    found_spot_offset = off;
                    got_spot = 1;
                }
                if (dirsec[off] == 0x00) break;
            }
            e++;
        }
        if (got_spot) break;
    }
    if (!got_spot && !exists) return -1;

    /* fill entry buffer */
    uint8_t entry[32];
    for (int i = 0; i < 11; i++) entry[i] = (uint8_t)raw[i];
    entry[11] = 0x20; /* archive */
    /* reserved/time fields zeros */
    for (int i = 12; i < 26; i++) entry[i] = 0;
    uint16_t lowcl = clusters[0];
    entry[26] = lowcl & 0xFF;
    entry[27] = (lowcl >> 8) & 0xFF;
    entry[28] = size & 0xFF;
    entry[29] = (size >> 8) & 0xFF;
    entry[30] = (size >> 16) & 0xFF;
    entry[31] = (size >> 24) & 0xFF;

    /* write entry into root dir sector */
    if (read_sector_u32(found_spot_sector, dirsec, 1) != 0) return -1;
    for (int i = 0; i < 32; i++) dirsec[found_spot_offset + i] = entry[i];
    if (write_sector_u32(found_spot_sector, dirsec, 1) != 0) return -1;

    return 0;
}
