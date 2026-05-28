/*
 * Minimal CSPRNG implementation using /dev/urandom
 * Replaces libsodium's randombytes_buf()
 */
#ifndef RANDOM_MINIMAL_H
#define RANDOM_MINIMAL_H

#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>

/* Generate secure random bytes using /dev/urandom */
static inline void randombytes_buf(void *buf, size_t len) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        /* Fallback should never happen on modern Unix systems */
        return;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (unsigned char *)buf + total, len - total);
        if (n <= 0) break;
        total += n;
    }

    close(fd);
}

#endif /* RANDOM_MINIMAL_H */
