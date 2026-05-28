/*
 * Minimal CSPRNG implementation using the Linux getrandom(2) syscall.
 * Replaces libsodium's randombytes_buf(). Freestanding (no libc).
 */
#ifndef RANDOM_MINIMAL_H
#define RANDOM_MINIMAL_H

#include "nolibc.h"

/* Generate secure random bytes using getrandom(2). */
static inline void randombytes_buf(void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        long n = __syscall3(SYS_getrandom,
                            (unsigned char *)buf + total,
                            len - total, 0);
        if (n <= 0) {
            if (n == -EINTR) continue;
            break;
        }
        total += (size_t)n;
    }
}

#endif /* RANDOM_MINIMAL_H */
