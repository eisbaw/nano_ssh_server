/*
 * nolibc.c - Implementations + program entry for the freestanding build.
 *
 * Provides: _start and the handful of mem/str functions still referenced
 * (GCC emits calls to memcpy/memset even in a freestanding build).
 */
#include "nolibc.h"

/* ------------------------------------------------------------------ */
/* mem / str                                                           */
/* ------------------------------------------------------------------ */
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

/* ------------------------------------------------------------------ */
/* Program entry                                                       */
/* ------------------------------------------------------------------ */
extern int main(void);

/* main() ignores argc/argv, so _start does not unpack them: align the
 * stack, call main, hand its result to exit_group. */
__attribute__((naked, noreturn, used))
void _start(void) {
    __asm__ volatile (
        "xor %%ebp, %%ebp\n\t"     /* clear frame pointer (ABI) */
        "and $-16, %%rsp\n\t"      /* align stack to 16 bytes */
        "call main\n\t"
        "mov %%eax, %%edi\n\t"     /* exit_group(main(...)) */
        "mov $231, %%eax\n\t"
        "syscall\n\t"
        ::: "memory");
}
