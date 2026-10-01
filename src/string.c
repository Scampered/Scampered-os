#include "string.h"

/* strlen: return length of NUL-terminated string */
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* strcpy: copy NUL-terminated string */
char* strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/* strcmp: lexicographic comparison */
int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == 0)
            return 0;
    }
    return 0;
}

/* memcpy: copy n bytes (non-overlapping or overlapping not handled specially) */
void* memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

int memcmp(const void* a, const void* b, size_t size) {
    const unsigned char* x = a;
    const unsigned char* y = b;

    for (size_t i = 0; i < size; i++) {
        if (x[i] != y[i])
            return x[i] - y[i];
    }
    return 0;
}


/* memset: set n bytes to val */
void* memset(void *dest, int val, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    unsigned char v = (unsigned char)val;
    for (size_t i = 0; i < n; i++) d[i] = v;
    return dest;
}
