/* Libsodium compatibility layer - TEST: Use libsodium Curve25519 + c25519 Ed25519 */
#ifndef SODIUM_COMPAT_H
#define SODIUM_COMPAT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "random_minimal.h"
/* REMOVED curve25519_minimal.h - testing if custom Curve25519 is the issue */
#include "edsign.h"

/* Include libsodium for Curve25519 functions */
#include <sodium.h>

/* Override Ed25519 functions with c25519 versions */

/* crypto_sign_keypair: Generate Ed25519 keypair using c25519 */
static inline int my_crypto_sign_keypair(uint8_t *pk, uint8_t *sk) {
    fprintf(stderr, ">>> USING C25519 KEY GENERATION <<<\n");
    /* Generate 32-byte secret */
    randombytes_buf(sk, 32);

    /* Derive public key */
    edsign_sec_to_pub(pk, sk);

    /* libsodium stores public key in second half of sk */
    memcpy(sk + 32, pk, 32);

    return 0;
}
#define crypto_sign_keypair my_crypto_sign_keypair

/* crypto_sign_detached: Sign using c25519 */
static inline int my_crypto_sign_detached(uint8_t *sig, unsigned long long *siglen_p,
                                          const uint8_t *m, unsigned long long mlen,
                                          const uint8_t *sk) {
    fprintf(stderr, ">>> USING C25519 SIGNING <<<\n");
    const uint8_t *secret = sk;      /* First 32 bytes */
    const uint8_t *public = sk + 32; /* Second 32 bytes */

    edsign_sign(sig, public, secret, m, (size_t)mlen);

    if (siglen_p) {
        *siglen_p = 64;
    }

    /* Self-verification for debugging */
    uint8_t verify_result = edsign_verify(sig, public, m, (size_t)mlen);
    fprintf(stderr, "Self-verification: %s\n", verify_result ? "PASS" : "FAIL");

    return 0;
}
#define crypto_sign_detached my_crypto_sign_detached

#endif /* SODIUM_COMPAT_H */
