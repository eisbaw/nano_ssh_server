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
    uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;
    uint8_t *p = ctx->buffer;

    /* Prepare message schedule */
    for (int i = 0; i < 16; i++, p += 4) {
        m[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | p[3];
    }
    for (int i = 16; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    /* Initialize working variables */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    /* Compression function */
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K(i) + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Add compressed chunk to current hash value */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
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
    for (int j = 7; j >= 0; j--) {
        ctx->buffer[56 + j] = ctx->bitlen >> (8 * (7 - j));
    }
    sha256_transform(ctx);

    /* Produce final hash */
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (ctx->state[i] >> 24) & 0xff;
        hash[i * 4 + 1] = (ctx->state[i] >> 16) & 0xff;
        hash[i * 4 + 2] = (ctx->state[i] >> 8) & 0xff;
        hash[i * 4 + 3] = ctx->state[i] & 0xff;
    }
}

/* HMAC-SHA256 over seq || packet, the only MAC this server computes.
 *
 * The key is always exactly 32 bytes, so the "hash the key if it is longer
 * than a block" branch is dropped, and the outer hash is built after the
 * inner one finishes rather than being carried in a second live context -
 * one sha256_ctx and one 64-byte pad instead of an hmac_sha256_ctx pair.
 */
static inline void hmac_sha256_seq(uint8_t *mac, const uint8_t *key,
                                   uint32_t seq,
                                   const uint8_t *data, size_t len) {
    sha256_ctx ctx;
    uint8_t pad[SHA256_BLOCK_SIZE];
    uint8_t inner[SHA256_DIGEST_SIZE];
    uint8_t seq_be[4];
    int i;

    for (i = 0; i < 4; i++)
        seq_be[i] = (uint8_t)(seq >> (24 - 8 * i));

    for (i = 0; i < SHA256_BLOCK_SIZE; i++)
        pad[i] = 0x36 ^ (i < SHA256_DIGEST_SIZE ? key[i] : 0);

    sha256_init(&ctx);
    sha256_update(&ctx, pad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, seq_be, 4);
    sha256_update(&ctx, data, (uint32_t)len);
    sha256_final(&ctx, inner);

    /* 0x36 ^ 0x5c turns the ipad into the opad in place */
    for (i = 0; i < SHA256_BLOCK_SIZE; i++)
        pad[i] ^= 0x36 ^ 0x5c;

    sha256_init(&ctx);
    sha256_update(&ctx, pad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, inner, SHA256_DIGEST_SIZE);
    sha256_final(&ctx, mac);
}

/* Constant-time comparison: zero if the two 32-byte values are equal,
 * non-zero otherwise. Callers only test against zero, so the difference
 * accumulator is returned directly. */
static inline int ct_diff_32(const uint8_t *x, const uint8_t *y) {
    uint8_t d = 0;
    for (int i = 0; i < 32; i++) {
        d |= x[i] ^ y[i];
    }
    return d;
}

#endif /* SHA256_MINIMAL_H */
