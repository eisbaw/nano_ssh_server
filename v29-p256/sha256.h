/*
 * Minimal SHA-256 (FIPS 180-4), size over speed.  The only hash this
 * server needs: the exchange hash, the key derivation and the ECDSA
 * message digest are all SHA-256, so the SHA-512 core is gone.
 *
 * The round constants and initial state are generated at startup: K[i] is
 * the first 32 fractional bits of cbrt(prime_i) and H0[i] of sqrt(prime_i),
 * extracted bit by bit on a fixed-point candidate whose cube fits 128 bits
 * (v26-genk did this for SHA-512 through the 256-bit multiplier; at 32-bit
 * precision GCC's __int128 is a handful of instructions).
 */
#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include "nolibc.h"

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint8_t buffer[SHA256_BLOCK_SIZE];
    uint64_t bitlen;
    uint32_t buflen;
} sha256_ctx;

static uint32_t sha256_k[64];
static uint32_t sha256_h0[8];

/* First 32 fractional bits of q^(1/e), e in {2, 3}, q < 2^9: accept bit
 * `bit` of the 5.32 fixed-point candidate x iff x^e <= q << 32e. */
static uint32_t root32(unsigned q, int e) {
    uint64_t x = 0;
    int bit, k;

    for (bit = 36; bit >= 0; bit--) {
        unsigned __int128 t = 1;

        x |= (uint64_t)1 << bit;
        for (k = 0; k < e; k++)
            t *= x;
        if (t > (unsigned __int128)q << (32 * e))
            x ^= (uint64_t)1 << bit;
    }
    return (uint32_t)x;
}

static void sha_gentables(void) {
    unsigned q = 1;
    int i;

    for (i = 0; i < 64; i++) {
        unsigned d;

        do {
            q++;
            for (d = 2; d * d <= q && q % d; d++)
                ;
        } while (d * d <= q);
        sha256_k[i] = root32(q, 3);
        if (i < 8)
            sha256_h0[i] = root32(q, 2);
    }
}

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static inline void sha256_transform(sha256_ctx *ctx) {
    uint32_t m[64], t1, t2;
    uint8_t *p = ctx->buffer;

    for (int i = 0; i < 16; i++, p += 4)
        m[i] = __builtin_bswap32(*(const u32a *)p);
    for (int i = 16; i < 64; i++)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    /* Working variables a..h as one array: the per-round rotation
     * h=g, g=f, ... b=a is a shift of the array instead of seven moves. */
    uint32_t s[8];
    memcpy(s, ctx->state, sizeof(s));
    for (int i = 0; i < 64; i++) {
        t1 = s[7] + EP1(s[4]) + CH(s[4], s[5], s[6]) + sha256_k[i] + m[i];
        t2 = EP0(s[0]) + MAJ(s[0], s[1], s[2]);
        for (int j = 7; j > 0; j--) s[j] = s[j - 1];
        s[4] += t1;
        s[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) ctx->state[i] += s[i];
}

static inline void sha256_init(sha256_ctx *ctx) {
    memcpy(ctx->state, sha256_h0, sizeof(ctx->state));
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

    ctx->buffer[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0;
        sha256_transform(ctx);
        i = 0;
    }
    while (i < 56) ctx->buffer[i++] = 0;

    ctx->bitlen += ctx->buflen * 8;
    *(u64a *)(ctx->buffer + 56) = __builtin_bswap64(ctx->bitlen);
    sha256_transform(ctx);

    for (i = 0; i < 8; i++)
        *(u32a *)(hash + i * 4) = __builtin_bswap32(ctx->state[i]);
}

/* Constant-time comparison: zero iff the two n-byte values are equal. */
static inline int ct_diff(const uint8_t *x, const uint8_t *y, size_t n) {
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++)
        d |= x[i] ^ y[i];
    return d;
}

#endif /* SHA256_H */
