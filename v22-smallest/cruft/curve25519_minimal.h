/*
 * Minimal Curve25519 implementation
 * Based on RFC 7748 - Elliptic Curves for Security
 * Implements X25519 function for ECDH key exchange
 */
#ifndef CURVE25519_MINIMAL_H
#define CURVE25519_MINIMAL_H

#include <stdint.h>
#include <string.h>
#include "fe25519.h"

/* Curve25519 base point (u=9) */
static const uint8_t curve25519_basepoint[32] = {9};

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
