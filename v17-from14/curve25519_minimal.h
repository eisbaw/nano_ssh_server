/*
 * Minimal Curve25519 implementation
 * Based on RFC 7748 - Elliptic Curves for Security
 * Implements X25519 function for ECDH key exchange
 */
#ifndef CURVE25519_MINIMAL_H
#define CURVE25519_MINIMAL_H

#include <stdint.h>
#include <string.h>

/* Field element: value mod 2^255-19 represented as 10 limbs of 26/25 bits */
typedef int64_t fe[10];

/* Curve25519 base point (u=9) */
static const uint8_t curve25519_basepoint[32] = {9};

/* Load a 32-byte little-endian value into field element */
static void fe_frombytes(fe h, const uint8_t s[32]) {
    int64_t h0 = s[0] | ((int64_t)s[1] << 8) | ((int64_t)s[2] << 16) | ((int64_t)s[3] << 24);
    int64_t h1 = s[4] | ((int64_t)s[5] << 8) | ((int64_t)s[6] << 16) | ((int64_t)s[7] << 24);
    int64_t h2 = s[8] | ((int64_t)s[9] << 8) | ((int64_t)s[10] << 16) | ((int64_t)s[11] << 24);
    int64_t h3 = s[12] | ((int64_t)s[13] << 8) | ((int64_t)s[14] << 16) | ((int64_t)s[15] << 24);
    int64_t h4 = s[16] | ((int64_t)s[17] << 8) | ((int64_t)s[18] << 16) | ((int64_t)s[19] << 24);
    int64_t h5 = s[20] | ((int64_t)s[21] << 8) | ((int64_t)s[22] << 16) | ((int64_t)s[23] << 24);
    int64_t h6 = s[24] | ((int64_t)s[25] << 8) | ((int64_t)s[26] << 16) | ((int64_t)s[27] << 24);
    int64_t h7 = s[28] | ((int64_t)s[29] << 8) | ((int64_t)s[30] << 16) | ((int64_t)s[31] << 24);

    h[0] = h0 & 0x3ffffff; h[1] = (h0 >> 26) | ((h1 << 6) & 0x1ffffff);
    h[2] = (h1 >> 19) & 0x3ffffff; h[3] = (h1 >> 45) | ((h2 << 13) & 0x1ffffff);
    h[4] = (h2 >> 12) & 0x3ffffff; h[5] = (h2 >> 38) | ((h3 << 20) & 0x1ffffff);
    h[6] = (h3 >> 5) & 0x3ffffff; h[7] = (h3 >> 31) | ((h4 << 27) & 0x1ffffff);
    h[8] = (h4 >> 4) & 0x3ffffff; h[9] = (h5 >> 30) | ((h6 << 8) & 0x1ffffff) | ((h7 << 14) & 0x1ffffff);
}

/* Store field element to 32-byte little-endian */
static void fe_tobytes(uint8_t s[32], const fe h) {
    fe t;
    int64_t carry;
    int i;

    for (i = 0; i < 10; i++) t[i] = h[i];

    /* Reduce modulo 2^255-19 */
    for (i = 0; i < 2; i++) {
        carry = t[9] >> 25; t[0] += carry * 19; t[9] &= 0x1ffffff;
        carry = t[0] >> 26; t[1] += carry; t[0] &= 0x3ffffff;
        carry = t[1] >> 25; t[2] += carry; t[1] &= 0x1ffffff;
        carry = t[2] >> 26; t[3] += carry; t[2] &= 0x3ffffff;
        carry = t[3] >> 25; t[4] += carry; t[3] &= 0x1ffffff;
        carry = t[4] >> 26; t[5] += carry; t[4] &= 0x3ffffff;
        carry = t[5] >> 25; t[6] += carry; t[5] &= 0x1ffffff;
        carry = t[6] >> 26; t[7] += carry; t[6] &= 0x3ffffff;
        carry = t[7] >> 25; t[8] += carry; t[7] &= 0x1ffffff;
        carry = t[8] >> 26; t[9] += carry; t[8] &= 0x3ffffff;
    }

    s[0] = t[0] & 0xff; s[1] = (t[0] >> 8) & 0xff; s[2] = (t[0] >> 16) & 0xff; s[3] = (t[0] >> 24) | ((t[1] << 2) & 0xff);
    s[4] = (t[1] >> 6) & 0xff; s[5] = (t[1] >> 14) & 0xff; s[6] = (t[1] >> 22) | ((t[2] << 3) & 0xff);
    s[7] = (t[2] >> 5) & 0xff; s[8] = (t[2] >> 13) & 0xff; s[9] = (t[2] >> 21) | ((t[3] << 5) & 0xff);
    s[10] = (t[3] >> 3) & 0xff; s[11] = (t[3] >> 11) & 0xff; s[12] = (t[3] >> 19) | ((t[4] << 6) & 0xff);
    s[13] = (t[4] >> 2) & 0xff; s[14] = (t[4] >> 10) & 0xff; s[15] = (t[4] >> 18) & 0xff;
    s[16] = t[5] & 0xff; s[17] = (t[5] >> 8) & 0xff; s[18] = (t[5] >> 16) & 0xff; s[19] = (t[5] >> 24) | ((t[6] << 1) & 0xff);
    s[20] = (t[6] >> 7) & 0xff; s[21] = (t[6] >> 15) & 0xff; s[22] = (t[6] >> 23) | ((t[7] << 3) & 0xff);
    s[23] = (t[7] >> 5) & 0xff; s[24] = (t[7] >> 13) & 0xff; s[25] = (t[7] >> 21) | ((t[8] << 4) & 0xff);
    s[26] = (t[8] >> 4) & 0xff; s[27] = (t[8] >> 12) & 0xff; s[28] = (t[8] >> 20) | ((t[9] << 6) & 0xff);
    s[29] = (t[9] >> 2) & 0xff; s[30] = (t[9] >> 10) & 0xff; s[31] = (t[9] >> 18) & 0xff;
}

/* Field element operations */
static void fe_copy(fe h, const fe f) { int i; for (i = 0; i < 10; i++) h[i] = f[i]; }
static void fe_0(fe h) { memset(h, 0, sizeof(fe)); }
static void fe_1(fe h) { fe_0(h); h[0] = 1; }

static void fe_add(fe h, const fe f, const fe g) {
    int i; for (i = 0; i < 10; i++) h[i] = f[i] + g[i];
}

static void fe_sub(fe h, const fe f, const fe g) {
    int i; for (i = 0; i < 10; i++) h[i] = f[i] - g[i];
}

static void fe_mul(fe h, const fe f, const fe g) {
    int64_t f0=f[0], f1=f[1], f2=f[2], f3=f[3], f4=f[4], f5=f[5], f6=f[6], f7=f[7], f8=f[8], f9=f[9];
    int64_t g0=g[0], g1=g[1], g2=g[2], g3=g[3], g4=g[4], g5=g[5], g6=g[6], g7=g[7], g8=g[8], g9=g[9];
    int64_t g1_19 = 19 * g1, g2_19 = 19 * g2, g3_19 = 19 * g3, g4_19 = 19 * g4;
    int64_t g5_19 = 19 * g5, g6_19 = 19 * g6, g7_19 = 19 * g7, g8_19 = 19 * g8, g9_19 = 19 * g9;
    int64_t h0, h1, h2, h3, h4, h5, h6, h7, h8, h9;
    int64_t c;

    h0 = f0*g0 + f1*g9_19 + f2*g8_19 + f3*g7_19 + f4*g6_19 + f5*g5_19 + f6*g4_19 + f7*g3_19 + f8*g2_19 + f9*g1_19;
    h1 = f0*g1 + f1*g0    + f2*g9_19 + f3*g8_19 + f4*g7_19 + f5*g6_19 + f6*g5_19 + f7*g4_19 + f8*g3_19 + f9*g2_19;
    h2 = f0*g2 + f1*g1    + f2*g0    + f3*g9_19 + f4*g8_19 + f5*g7_19 + f6*g6_19 + f7*g5_19 + f8*g4_19 + f9*g3_19;
    h3 = f0*g3 + f1*g2    + f2*g1    + f3*g0    + f4*g9_19 + f5*g8_19 + f6*g7_19 + f7*g6_19 + f8*g5_19 + f9*g4_19;
    h4 = f0*g4 + f1*g3    + f2*g2    + f3*g1    + f4*g0    + f5*g9_19 + f6*g8_19 + f7*g7_19 + f8*g6_19 + f9*g5_19;
    h5 = f0*g5 + f1*g4    + f2*g3    + f3*g2    + f4*g1    + f5*g0    + f6*g9_19 + f7*g8_19 + f8*g7_19 + f9*g6_19;
    h6 = f0*g6 + f1*g5    + f2*g4    + f3*g3    + f4*g2    + f5*g1    + f6*g0    + f7*g9_19 + f8*g8_19 + f9*g7_19;
    h7 = f0*g7 + f1*g6    + f2*g5    + f3*g4    + f4*g3    + f5*g2    + f6*g1    + f7*g0    + f8*g9_19 + f9*g8_19;
    h8 = f0*g8 + f1*g7    + f2*g6    + f3*g5    + f4*g4    + f5*g3    + f6*g2    + f7*g1    + f8*g0    + f9*g9_19;
    h9 = f0*g9 + f1*g8    + f2*g7    + f3*g6    + f4*g5    + f5*g4    + f6*g3    + f7*g2    + f8*g1    + f9*g0;

    /* Reduce carries */
    c = h0 >> 26; h1 += c; h0 -= c << 26;
    c = h4 >> 26; h5 += c; h4 -= c << 26;
    c = h1 >> 25; h2 += c; h1 -= c << 25;
    c = h5 >> 25; h6 += c; h5 -= c << 25;
    c = h2 >> 26; h3 += c; h2 -= c << 26;
    c = h6 >> 26; h7 += c; h6 -= c << 26;
    c = h3 >> 25; h4 += c; h3 -= c << 25;
    c = h7 >> 25; h8 += c; h7 -= c << 25;
    c = h8 >> 26; h9 += c; h8 -= c << 26;
    c = h9 >> 25; h0 += c * 19; h9 -= c << 25;
    c = h0 >> 26; h1 += c; h0 -= c << 26;

    h[0]=h0; h[1]=h1; h[2]=h2; h[3]=h3; h[4]=h4; h[5]=h5; h[6]=h6; h[7]=h7; h[8]=h8; h[9]=h9;
}

static void fe_sq(fe h, const fe f) { fe_mul(h, f, f); }

/* Conditional swap in constant time */
static void fe_cswap(fe f, fe g, unsigned int b) {
    int i;
    int64_t mask = -(int64_t)b;
    for (i = 0; i < 10; i++) {
        int64_t x = f[i] ^ g[i];
        x &= mask;
        f[i] ^= x;
        g[i] ^= x;
    }
}

/* Compute inverse via Fermat's little theorem: a^(p-2) mod p */
static void fe_invert(fe out, const fe z) {
    fe t0, t1, t2, t3;
    int i;

    fe_sq(t0, z);
    fe_sq(t1, t0); fe_sq(t1, t1);
    fe_mul(t1, z, t1);
    fe_mul(t0, t0, t1);
    fe_sq(t2, t0);
    fe_mul(t1, t1, t2);
    fe_sq(t2, t1); for (i = 1; i < 5; i++) fe_sq(t2, t2);
    fe_mul(t1, t2, t1);
    fe_sq(t2, t1); for (i = 1; i < 10; i++) fe_sq(t2, t2);
    fe_mul(t2, t2, t1);
    fe_sq(t3, t2); for (i = 1; i < 20; i++) fe_sq(t3, t3);
    fe_mul(t2, t3, t2);
    fe_sq(t2, t2); for (i = 1; i < 10; i++) fe_sq(t2, t2);
    fe_mul(t1, t2, t1);
    fe_sq(t2, t1); for (i = 1; i < 50; i++) fe_sq(t2, t2);
    fe_mul(t2, t2, t1);
    fe_sq(t3, t2); for (i = 1; i < 100; i++) fe_sq(t3, t3);
    fe_mul(t2, t3, t2);
    fe_sq(t2, t2); for (i = 1; i < 50; i++) fe_sq(t2, t2);
    fe_mul(t1, t2, t1);
    fe_sq(t1, t1); for (i = 1; i < 5; i++) fe_sq(t1, t1);
    fe_mul(out, t1, t0);
}

/* Montgomery ladder for scalar multiplication */
static void crypto_scalarmult_curve25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) {
    uint8_t e[32];
    fe x1, x2, z2, x3, z3, tmp0, tmp1;
    unsigned int swap = 0;
    int i, j;

    /* Clamp scalar per RFC 7748 */
    memcpy(e, scalar, 32);
    e[0] &= 248;
    e[31] &= 127;
    e[31] |= 64;

    /* Montgomery ladder */
    fe_frombytes(x1, point);
    fe_1(x2);
    fe_0(z2);
    fe_copy(x3, x1);
    fe_1(z3);

    for (i = 254; i >= 0; i--) {
        unsigned int b = (e[i >> 3] >> (i & 7)) & 1;
        swap ^= b;
        fe_cswap(x2, x3, swap);
        fe_cswap(z2, z3, swap);
        swap = b;

        /* Montgomery differential addition and doubling */
        fe_add(tmp0, x2, z2);
        fe_sub(tmp1, x2, z2);
        fe_add(x2, x3, z3);
        fe_sub(z2, x3, z3);
        fe_mul(z3, tmp0, z2);
        fe_mul(z2, tmp1, x2);
        fe_sq(x2, tmp1);
        fe_sq(tmp1, tmp0);
        fe_add(x3, z3, z2);
        fe_sub(z2, z3, z2);
        fe_mul(tmp0, tmp1, x2);
        fe_sub(tmp1, tmp1, x2);
        fe_sq(z2, z2);
        fe_mul(z3, tmp1, (fe){121666});
        fe_sq(x3, x3);
        fe_add(tmp1, x2, z3);
        fe_mul(z3, x1, z2);
        fe_mul(z2, tmp1, tmp0);
    }

    fe_cswap(x2, x3, swap);
    fe_cswap(z2, z3, swap);

    /* Affine conversion: x = X/Z */
    fe_invert(z2, z2);
    fe_mul(x2, x2, z2);
    fe_tobytes(out, x2);
}

/* Generate public key from private key (multiply by base point) */
static inline void crypto_scalarmult_base(uint8_t public_key[32], const uint8_t private_key[32]) {
    crypto_scalarmult_curve25519(public_key, private_key, curve25519_basepoint);
}

/* Compute shared secret (ECDH) */
static inline int crypto_scalarmult(uint8_t shared[32], const uint8_t private_key[32], const uint8_t peer_public[32]) {
    crypto_scalarmult_curve25519(shared, private_key, peer_public);
    return 0;
}

#endif /* CURVE25519_MINIMAL_H */
