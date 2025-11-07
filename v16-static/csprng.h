/*
 * Cryptographically Secure Pseudo-Random Number Generator
 * Uses /dev/urandom for entropy
 */

#ifndef CSPRNG_H
#define CSPRNG_H

#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Fill buffer with cryptographically secure random bytes
 * Uses /dev/urandom which is non-blocking and suitable for crypto
 *
 * Returns: 0 on success, -1 on error
 */
static inline int random_bytes(uint8_t *buf, size_t len) {
    int fd;
    ssize_t n;
    size_t total = 0;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    while (total < len) {
        n = read(fd, buf + total, len - total);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue;  /* Retry on interrupt */
            }
            close(fd);
            return -1;
        }
        total += n;
    }

    close(fd);
    return 0;
}

#endif /* CSPRNG_H */
