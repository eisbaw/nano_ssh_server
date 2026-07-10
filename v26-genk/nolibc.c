/*
 * nolibc.c - Implementations + program entry for the freestanding build.
 *
 * Provides: _start, errno, mem/str functions, and a tiny heap allocator.
 */
#include "nolibc.h"

/* errno storage */
int errno;

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

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
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

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* ------------------------------------------------------------------ */
/* Heap: bump allocator over an mmap'd region. The SSH server's        */
/* malloc/free calls are bounded (<= MAX_PACKET_SIZE) and balanced     */
/* within a single packet operation. We track outstanding allocations  */
/* with a counter; when it drops to zero we reset the bump pointer.    */
/* This keeps memory flat across the long-running connection loop.     */
/* ------------------------------------------------------------------ */
#define HEAP_SIZE (1u << 20)   /* 1 MiB arena, plenty for 35 KB packets */

#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define MAP_PRIVATE    0x02
#define MAP_ANONYMOUS  0x20

static unsigned char *heap_base = 0;
static size_t heap_off = 0;
static size_t heap_live = 0;   /* number of outstanding allocations */

static int heap_init(void) {
    long r = __syscall6(SYS_mmap, 0, HEAP_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (r < 0 && r > -4096) return -1;
    heap_base = (unsigned char *)r;
    heap_off = 0;
    heap_live = 0;
    return 0;
}

void *malloc(size_t n) {
    if (!heap_base) {
        if (heap_init() != 0) return 0;
    }
    /* 16-byte align */
    n = (n + 15u) & ~(size_t)15u;
    if (heap_off + n > HEAP_SIZE) {
        /* Out of arena space: if nothing live, reset; else fail. */
        if (heap_live == 0) {
            heap_off = 0;
        } else {
            return 0;
        }
        if (n > HEAP_SIZE) return 0;
    }
    void *p = heap_base + heap_off;
    heap_off += n;
    heap_live++;
    return p;
}

void free(void *p) {
    if (!p) return;
    if (heap_live > 0) heap_live--;
    if (heap_live == 0) {
        /* All freed: reclaim the whole arena. */
        heap_off = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Program entry                                                       */
/* ------------------------------------------------------------------ */
extern int main(int argc, char **argv);

/* _start receives argc/argv on the stack. We grab them with a tiny    */
/* asm stub and tail into _start_c. */
__attribute__((noreturn, used, externally_visible, noinline))
void _start_c(long *sp) {
    int argc = (int)sp[0];
    char **argv = (char **)(sp + 1);
    int ret = main(argc, argv);
    _exit_group(ret);
    for (;;) { }  /* unreachable; silences noreturn warning */
}

__attribute__((naked, noreturn, used))
void _start(void) {
    __asm__ volatile (
        "xor %%rbp, %%rbp\n\t"   /* clear frame pointer (ABI) */
        "mov %%rsp, %%rdi\n\t"   /* pass stack pointer to _start_c */
        "and $-16, %%rsp\n\t"    /* align stack to 16 bytes */
        "call _start_c\n\t"
        ::: "memory");
}
