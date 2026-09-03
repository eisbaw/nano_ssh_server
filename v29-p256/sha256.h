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

/* K[0..63] followed by the initial state (first 8 of 64 used: bss is free
 * and filling all 64 saves an i < 8 test) in one array */
static uint32_t sha256_t[128];
#define sha256_k  sha256_t
#define sha256_h0 (sha256_t + 64)


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

static void sha256_transform(uint32_t *state, const uint8_t *p) {
    uint32_t m[64], t1, t2;

    for (int i = 0; i < 16; i++, p += 4)
        m[i] = __builtin_bswap32(*(const u32a *)p);
    for (int i = 16; i < 64; i++)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    /* Working variables a..h as a window sliding down an array: the
     * per-round rotation h=g, g=f, ... b=a is the window moving one word
     * down, so a round only writes the new a and adds t1 into d (the new
     * e); after 64 rounds the window is v[0..7]. */
    uint32_t v[72], *s = v + 64;
    memcpy(s, state, 32);
    for (int i = 0; i < 64; i++, s--) {
        t1 = s[7] + EP1(s[4]) + CH(s[4], s[5], s[6]) + sha256_k[i] + m[i];
        t2 = EP0(s[0]) + MAJ(s[0], s[1], s[2]);
        s[3] += t1;
        s[-1] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) state[i] += v[i];
}

/* One-shot hash of a contiguous message, padded IN PLACE: every input
 * this server hashes is assembled in a buffer first, with at least 72
 * bytes free behind it (the padding is 9..72 bytes), so there is no
 * streaming context, no partial-block copy and no separate final step -
 * 0x80, zeros to 56 mod 64 and the 64-bit bit length are written after
 * the message and the whole thing is run through the transform.  The
 * output may overlap the padding (the exchange hash is written where its
 * own padding was). */
static void sha256(uint8_t *out, uint8_t *m, size_t len) {
    uint32_t st[8];
    size_t i, n = len;

    memcpy(st, sha256_h0, sizeof(st));
    m[n] = 0x80;
    do m[++n] = 0;      /* the last zero lands in the length's slot */
    while (n % SHA256_BLOCK_SIZE != 56);
    *(u64a *)(m + n) = __builtin_bswap64((uint64_t)len << 3);
    for (const uint8_t *end = m + n + 8; m < end; m += SHA256_BLOCK_SIZE)
        sha256_transform(st, m);

    for (i = 0; i < 8; i++)
        *(u32a *)(out + i * 4) = __builtin_bswap32(st[i]);
}

/* Constant-time comparison of two 16-byte values (the Poly1305 tag): zero
 * iff equal, as the OR of two 64-bit XORs - no loop, no early exit. */
static inline int ct_diff(const uint8_t *x, const uint8_t *y, size_t n) {
    (void)n;
    return (*(const u64a *)x ^ *(const u64a *)y) |
           (*(const u64a *)(x + 8) ^ *(const u64a *)(y + 8)) ? 1 : 0;
}

#endif /* SHA256_H */
