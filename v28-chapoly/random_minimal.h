/*
 * Minimal CSPRNG implementation using the Linux getrandom(2) syscall.
 * Replaces libsodium's randombytes_buf(). Freestanding (no libc).
 */
#ifndef RANDOM_MINIMAL_H
#define RANDOM_MINIMAL_H

#include "nolibc.h"

/* Generate secure random bytes using getrandom(2).
 *
 * getrandom() only returns short for requests larger than 256 bytes or when
 * a signal arrives; every request in this server is at most 64 bytes, so a
 * retry-until-complete loop needs no partial-fill bookkeeping. */
static inline void randombytes_buf(void *buf, size_t len) {
    while (__syscall3(SYS_getrandom, buf, len, 0) != (long)len)
        ;
}

#endif /* RANDOM_MINIMAL_H */
