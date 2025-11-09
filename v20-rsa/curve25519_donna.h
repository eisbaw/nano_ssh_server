/*
 * Curve25519 wrapper for curve25519-donna-c64.c
 *
 * Adapts the public domain curve25519-donna implementation
 * to the libsodium API used by our SSH server.
 *
 * Original curve25519-donna: Public domain
 * Derived from Daniel J. Bernstein's work
 */

#ifndef CURVE25519_DONNA_H
#define CURVE25519_DONNA_H

#include <stdint.h>
#include <string.h>

/* Forward declare the function from curve25519-donna-c64.c */
int curve25519_donna(uint8_t *mypublic, const uint8_t *secret, const uint8_t *basepoint);

/* Curve25519 base point (generator) */
static const uint8_t curve25519_basepoint[32] = {9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* crypto_scalarmult_base: Multiply secret scalar by the generator point
 * Compatible with libsodium API
 */
static inline int crypto_scalarmult_base(uint8_t *public_key, const uint8_t *private_key) {
    return curve25519_donna(public_key, private_key, curve25519_basepoint);
}

/* crypto_scalarmult: Compute ECDH shared secret
 * Compatible with libsodium API
 * Returns 0 on success, -1 on failure
 */
static inline int crypto_scalarmult(uint8_t *shared, const uint8_t *private_key, const uint8_t *peer_public) {
    return curve25519_donna(shared, private_key, peer_public);
}

#endif /* CURVE25519_DONNA_H */
