/*
 * Minimal SHA-256 (FIPS 180-4), size over speed.  The only hash this
 * server needs: the exchange hash, the key derivation and the ECDSA
 * message digest are all SHA-256, so the SHA-512 core is gone.
 *
 * The round constants and initial state are generated at startup: K[i] is
 * the first 32 fractional bits of cbrt(prime_i) and H0[i] of sqrt(prime_i).
 * v26-genk..v28 extracted them bit by bit with integer arithmetic; here
 * they come from double precision, which is exact enough (all 72 words
 * were checked against the integer generator and FIPS 180-4): the cube
 * root is the fixed point of x = sqrt(q / x), one divsd and one sqrtsd per
 * step, and the same recurrence stopped after two steps is sqrt(q).
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

/* K[0..63] followed by the initial state (first 8 of 64 used: bss is free
 * and filling all 64 saves an i < 8 test) in one array */
static uint32_t sha256_t[128];
#define sha256_k  sha256_t
#define sha256_h0 (sha256_t + 64)

/* The one context.  Every hash this server takes runs to completion
 * before the next one starts, so the routines work on this static
 * context and no caller passes (or keeps reloading) a pointer to it. */
static sha256_ctx hctx;

/* one sqrtsd, whatever the math flags (the unit test builds without them) */
static inline double sqrtsd(double v) {
    __asm__("sqrtsd %1, %0" : "=x"(v) : "x"(v));
    return v;
}

static void sha_gentables(void) {
    int q = 1, d, n = 99, k;   /* signed: cdq/idiv and the 32-bit cvtsi2sd */
    uint32_t *p = sha256_t;
    double x;

    /* Two passes over the primes 2..311, one store per prime: x = sqrt(q / x)
     * from x = q gives 1, sqrt(q), ... converging (error halved per step) to
     * cbrt(q), so pass one (n = 99 steps) fills the round constants and pass
     * two (n = 2) the initial state; one loop body serves both. */
    do {
        /* d ends on q's largest proper divisor: 1 iff q is prime */
        do {
            q++;
            d = q;
            while (q % --d)
                ;
        } while (d > 1);
        x = q;
        for (k = 0; k < n; k++)
            x = sqrtsd(q / x);
        /* times 2^32 by doubling: no rodata constant */
        for (k = 0; k < 32; k++)
            x += x;
        /* *p++ = (uint32_t)x as one stosd: the value is already in rax */
        __asm__ volatile("stosl" : "+D"(p) : "a"((int64_t)x) : "memory");
        /* the 64th prime, 311 = 0x137, is the only one up to there whose
         * low byte is 0x37 (55 = 5 * 11): restart the primes for pass two
         * (n goes 99, 2, -95) */
        if ((uint8_t)q == 0x37) {
            q = 1;
            n -= 97;
        }
    } while (n >= 0);
}

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static inline void sha256_transform(void) {
    sha256_ctx *ctx = &hctx;
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

static inline void sha256_init(void) {
    sha256_ctx *ctx = &hctx;
    memcpy(ctx->state, sha256_h0, sizeof(ctx->state));
    ctx->bitlen = 0;
    ctx->buflen = 0;
}

static inline void sha256_update(const uint8_t *data, uint32_t len) {
    sha256_ctx *ctx = &hctx;
    for (uint32_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == SHA256_BLOCK_SIZE) {
            sha256_transform();
            ctx->bitlen += 512;
            ctx->buflen = 0;
        }
    }
}

static inline void sha256_final(uint8_t *hash) {
    sha256_ctx *ctx = &hctx;
    uint32_t i = ctx->buflen;

    ctx->buffer[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0;
        sha256_transform();
        i = 0;
    }
    while (i < 56) ctx->buffer[i++] = 0;

    ctx->bitlen += ctx->buflen * 8;
    *(u64a *)(ctx->buffer + 56) = __builtin_bswap64(ctx->bitlen);
    sha256_transform();

    for (i = 0; i < 8; i++)
        *(u32a *)(hash + i * 4) = __builtin_bswap32(ctx->state[i]);
    sha256_init();   /* ready for the next hash */
}

/* Constant-time comparison: zero iff the two n-byte values are equal. */
static inline int ct_diff(const uint8_t *x, const uint8_t *y, size_t n) {
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++)
        d |= x[i] ^ y[i];
    return d;
}

#endif /* SHA256_H */
