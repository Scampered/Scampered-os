#ifndef FAT12_H
#define FAT12_H

#include <stdint.h>

/* Initialize FAT12 subsystem. Internally will call disk_read() to parse BPB.
   Returns 0 on success, -1 on failure. */
int fat12_init(void);

/* Read file by 8.3 name (ASCII uppercase, padded with spaces). Example names:
   "HELLO   TXT" (11 chars) or the helper function below to convert.
   dest buffer must be large enough. Returns number of bytes read or -1 on error. */
int fat12_read_file_by_rawname(const char raw11[11], void* dest, uint32_t dest_size);

/* Convenience: supply normal "NAME.EXT" string (case-insensitive),
   will be converted to raw 11 and then read.
   Returns bytes read or -1. */
int fat12_read_file(const char* name_dot_ext, void* dest, uint32_t dest_size);

/* Write/overwrite a file; creates or replaces a root dir entry.
   name_dot_ext uses "NAME.EXT" 8.3 style (will be uppercased).
   Returns 0 on success, -1 on error. */
int fat12_write_file(const char* name_dot_ext, const void* data, uint32_t size);

/* Flush any in-memory FAT buffers back to disk image (calls disk_write) */
int fat12_flush(void);

#endif
