/*
 * Standalone Curve25519 implementation using f25519 field arithmetic
 * Based on RFC 7748 - Elliptic Curves for Security
 * Implements X25519 function for ECDH key exchange
 * NO libsodium dependency
 */

#ifndef CURVE25519_STANDALONE_H
#define CURVE25519_STANDALONE_H

#include <stdint.h>
#include <string.h>
#include "f25519.h"

/* Curve25519 base point */
static const uint8_t curve25519_basepoint[32] = {9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* Load field element from bytes */
static inline void fe_load(uint8_t *out, const uint8_t *in) {
    f25519_copy(out, in);
}

/* Store field element to bytes */
static inline void fe_store(uint8_t *out, const uint8_t *in) {
    f25519_normalize((uint8_t *)in);
    f25519_copy(out, in);
}

/* Square a field element */
static inline void fe_sq(uint8_t *out, const uint8_t *a) {
    f25519_mul__distinct(out, a, a);
}

/* Multiply two field elements */
static inline void fe_mul(uint8_t *out, const uint8_t *a, const uint8_t *b) {
    f25519_mul__distinct(out, a, b);
}

/* Multiply by small constant */
static inline void fe_mul_c(uint8_t *out, const uint8_t *a, uint32_t c) {
    f25519_mul_c(out, a, c);
}

/* Invert a field element */
static inline void fe_inv(uint8_t *out, const uint8_t *a) {
    f25519_inv__distinct(out, a);
}

/* Conditional copy: if c==1 copy b to a, if c==0 keep a */
static inline void fe_cond_copy(uint8_t *a, const uint8_t *b, int c) {
    uint8_t mask = -(uint8_t)c;
    int i;
    for (i = 0; i < 32; i++) {
        a[i] ^= mask & (a[i] ^ b[i]);
    }
}

/* Conditional swap in constant time */
static inline void fe_cond_swap(uint8_t *a, uint8_t *b, int c) {
    uint8_t mask = -(uint8_t)c;
    int i;
    for (i = 0; i < 32; i++) {
        uint8_t tmp = mask & (a[i] ^ b[i]);
        a[i] ^= tmp;
        b[i] ^= tmp;
    }
}

/* Curve25519 scalar multiplication using Montgomery ladder */
static void curve25519_mul(uint8_t *result, const uint8_t *scalar, const uint8_t *point) {
    uint8_t x_1[32], x_2[32], z_2[32], x_3[32], z_3[32];
    uint8_t tmp0[32], tmp1[32];
    uint8_t e[32];
    unsigned int swap = 0;
    int i;

    /* Clamp scalar per RFC 7748 */
    memcpy(e, scalar, 32);
    e[0] &= 248;
    e[31] = (e[31] & 127) | 64;

    /* Load point and initialize */
    fe_load(x_1, point);

    /* x_2 = 1 */
    memset(x_2, 0, 32);
    x_2[0] = 1;

    /* z_2 = 0 */
    memset(z_2, 0, 32);

    /* x_3 = point */
    fe_load(x_3, point);

    /* z_3 = 1 */
    memset(z_3, 0, 32);
    z_3[0] = 1;

    /* Montgomery ladder */
    for (i = 254; i >= 0; i--) {
        unsigned int b = (e[i >> 3] >> (i & 7)) & 1;
        swap ^= b;
        fe_cond_swap(x_2, x_3, swap);
        fe_cond_swap(z_2, z_3, swap);
        swap = b;

        /* Montgomery differential addition and doubling */
        /* A = x_2 + z_2 */
        memcpy(tmp0, x_2, 32);
        f25519_add(tmp0, tmp0, z_2);

        /* AA = A^2 */
        fe_sq(tmp0, tmp0);

        /* B = x_2 - z_2 */
        memcpy(tmp1, x_2, 32);
        f25519_sub(tmp1, tmp1, z_2);

        /* BB = B^2 */
        fe_sq(tmp1, tmp1);

        /* E = AA - BB */
        memcpy(x_2, tmp0, 32);
        f25519_sub(x_2, x_2, tmp1);

        /* D = x_3 - z_3 */
        memcpy(z_2, x_3, 32);
        f25519_sub(z_2, z_2, z_3);

        /* C = x_3 + z_3 */
        memcpy(z_3, x_3, 32);
        f25519_add(z_3, z_3, z_3);

        /* DA = D * B */
        fe_mul(z_2, z_2, tmp1);

        /* CB = C * A */
        fe_mul(z_3, z_3, tmp0);

        /* x_3 = (DA + CB)^2 */
        memcpy(tmp0, z_2, 32);
        f25519_add(tmp0, tmp0, z_3);
        fe_sq(x_3, tmp0);

        /* z_3 = x_1 * (DA - CB)^2 */
        memcpy(tmp1, z_2, 32);
        f25519_sub(tmp1, tmp1, z_3);
        fe_sq(tmp1, tmp1);
        fe_mul(z_3, x_1, tmp1);

        /* x_2 = AA * BB */
        fe_mul(x_2, tmp0, tmp1);

        /* z_2 = E * ((AA + 39081 * BB)) */
        memcpy(tmp0, tmp1, 32);
        f25519_mul_c(tmp0, tmp0, 39081);
        f25519_add(tmp0, tmp0, tmp1);
        fe_mul(z_2, x_2, tmp0);
    }

    fe_cond_swap(x_2, x_3, swap);
    fe_cond_swap(z_2, z_3, swap);

    /* Affine conversion: result = x_2 / z_2 = x_2 * z_2^(p-2) */
    fe_inv(z_2, z_2);
    fe_mul(result, x_2, z_2);
}

/* Generate public key from private key (multiply by base point) */
static inline void crypto_scalarmult_base(uint8_t *public_key, const uint8_t *private_key) {
    curve25519_mul(public_key, private_key, curve25519_basepoint);
}

/* Compute shared secret (ECDH) */
static inline int crypto_scalarmult(uint8_t *shared, const uint8_t *private_key, const uint8_t *peer_public) {
    curve25519_mul(shared, private_key, peer_public);
    return 0;
}

#endif /* CURVE25519_STANDALONE_H */
