/*
 * Minimal SHA-256 implementation
 * Optimized for size, not speed
 * Based on FIPS 180-4
 */

#ifndef SHA256_MINIMAL_H
#define SHA256_MINIMAL_H

#include <stdint.h>
#include "nolibc.h"
#include "sha512.h"


#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint8_t buffer[SHA256_BLOCK_SIZE];
    uint64_t bitlen;
    uint32_t buflen;
} sha256_ctx;

/* SHA-256 constants K: per FIPS 180-4 these are the first 32 fractional
 * bits of cbrt(prime), i.e. exactly the top halves of the SHA-512 K table
 * that sha_gentables() generates at startup. */
#define K(i) ((uint32_t)(sha512_kgen[i] >> 32))

/* Rotate right */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* SHA-256 functions */
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static inline void sha256_transform(sha256_ctx *ctx) {
    uint32_t m[64], t1, t2;
    uint8_t *p = ctx->buffer;

    /* Prepare message schedule */
    for (int i = 0; i < 16; i++, p += 4)
        m[i] = __builtin_bswap32(*(const u32a *)p);
    for (int i = 16; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    /* Working variables a..h as one array: the per-round rotation
     * h=g, g=f, ... b=a is a shift of the array instead of seven moves. */
    uint32_t s[8];
    memcpy(s, ctx->state, sizeof(s));
    for (int i = 0; i < 64; i++) {
        t1 = s[7] + EP1(s[4]) + CH(s[4], s[5], s[6]) + K(i) + m[i];
        t2 = EP0(s[0]) + MAJ(s[0], s[1], s[2]);
        for (int j = 7; j > 0; j--) s[j] = s[j - 1];
        s[4] += t1;
        s[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) ctx->state[i] += s[i];
}

static inline void sha256_init(sha256_ctx *ctx) {
    /* Initial state = first 32 fractional bits of sqrt(prime) = top halves
     * of the startup-generated SHA-512 initial state. */
    for (int i = 0; i < 8; i++)
        ctx->state[i] = (uint32_t)(sha512_initial_state.h[i] >> 32);
    ctx->bitlen = 0;
    ctx->buflen = 0;
}

static inline void sha256_update(sha256_ctx *ctx, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx);
            ctx->bitlen += 512;
            ctx->buflen = 0;
        }
    }
}

static inline void sha256_final(sha256_ctx *ctx, uint8_t *hash) {
    uint32_t i = ctx->buflen;

    /* Pad with 0x80 followed by zeros */
    ctx->buffer[i++] = 0x80;

    /* Pad to 56 bytes (leaving 8 bytes for length) */
    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0;
        sha256_transform(ctx);
        i = 0;
    }
    while (i < 56) ctx->buffer[i++] = 0;

    /* Append bit length */
    ctx->bitlen += ctx->buflen * 8;
    *(u64a *)(ctx->buffer + 56) = __builtin_bswap64(ctx->bitlen);
    sha256_transform(ctx);

    /* Produce final hash */
    for (i = 0; i < 8; i++)
        *(u32a *)(hash + i * 4) = __builtin_bswap32(ctx->state[i]);
}

/* Constant-time comparison: zero if the two n-byte values are equal,
 * non-zero otherwise. Callers only test against zero, so the difference
 * accumulator is returned directly. */
static inline int ct_diff(const uint8_t *x, const uint8_t *y, size_t n) {
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++) {
        d |= x[i] ^ y[i];
    }
    return d;
}

#endif /* SHA256_MINIMAL_H */
