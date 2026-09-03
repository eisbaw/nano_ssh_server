/*
 * nolibc.c - Implementations + program entry for the freestanding build.
 *
 * Provides _start and the handful of mem/str functions still referenced
 * (GCC emits calls to memcpy/memset even in a freestanding build).
 */
#include "nolibc.h"

/* ------------------------------------------------------------------ */
/* mem / str                                                           */
/* ------------------------------------------------------------------ */
/* rep movsb: the arguments already sit in rdi/rsi, so the body is a move
 * of the count and the string instruction.  noinline: inlined at its 22
 * call sites the count setup grows more than the calls shrink */
__attribute__((noinline))
void *memcpy(void *dst, const void *src, size_t n) {
    void *d = dst;
    __asm__ volatile ("rep movsb" : "+D"(d), "+S"(src), "+c"(n) :: "memory");
    return dst;
}

__attribute__((noinline))
void *memset(void *dst, int c, size_t n) {
    void *d = dst;
    __asm__ volatile ("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return dst;
}

/* repe cmpsb subtracts [rdi] (a) from [rsi] (b), so at the first
 * difference CF is set when b < a; the equal case leaves the zero result,
 * otherwise cmc + sbb turn CF into -1 for a < b and the or makes the
 * other case 1. */
__attribute__((noinline))
int memcmp(const void *a, const void *b, size_t n) {
    int r = 0;
    __asm__ volatile ("repe cmpsb\n\t"
                      "je 1f\n\t"
                      "cmc\n\t"
                      "sbb %0, %0\n\t"
                      "or $1, %0\n"
                      "1:"
                      : "+r"(r), "+D"(a), "+S"(b), "+c"(n) :: "memory", "cc");
    return r;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}


/* ------------------------------------------------------------------ */
/* Program entry                                                       */
/* ------------------------------------------------------------------ */
/* The kernel enters with every register zero and rsp 16-byte aligned;
 * main() ignores argc/argv and never returns, so _start is one push,
 * which leaves rsp 8 mod 16 as after a call - the alignment GCC's frame
 * layout assumes - and falls through into main(), which tiny.ld places
 * right behind it.  Top-level asm rather than a naked function: GCC
 * appends a ud2 trap to a naked body. */
__asm__(".pushsection .text.entry,\"ax\"\n"
        ".globl _start\n"
        "_start:\n\t"
        "push %rax\n\t"
        ".popsection");
