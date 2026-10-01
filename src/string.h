#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char *s);
char* strcpy(char *dest, const char *src);
int strcmp(const char *a, const char *b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char *dest, const char *src);

void* memcpy(void *dest, const void *src, size_t n);
int memcmp(const void* a, const void* b, size_t size);
void* memset(void *dest, int val, size_t n);

#endif /* STRING_H */
