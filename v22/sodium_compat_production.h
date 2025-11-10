/* Libsodium compatibility layer - Production version
 * 100% standalone: c25519 Ed25519 + curve25519-donna (public domain)
 * NO libsodium or OpenSSL dependencies
 *
 * Uses curve25519-donna-c64.c (public domain, from agl/curve25519-donna)
 * Derived from Daniel J. Bernstein's original work
 */
#ifndef SODIUM_COMPAT_H
#define SODIUM_COMPAT_H

#include <stdint.h>
#include <string.h>
#include "random_minimal.h"
#include "edsign.h"
#include "curve25519_donna.h"

/* Override Ed25519 functions with c25519 implementations */

/* crypto_sign_keypair: Generate Ed25519 keypair using c25519 */
static inline int my_crypto_sign_keypair(uint8_t *pk, uint8_t *sk) {
    /* Generate 32-byte secret */
    randombytes_buf(sk, 32);

    /* Derive public key using c25519 */
    edsign_sec_to_pub(pk, sk);

    /* libsodium format: store public in second half of sk */
    memcpy(sk + 32, pk, 32);

    return 0;
}
#define crypto_sign_keypair my_crypto_sign_keypair

/* crypto_sign_detached: Sign message using c25519 */
static inline int my_crypto_sign_detached(uint8_t *sig, unsigned long long *siglen_p,
                                          const uint8_t *m, unsigned long long mlen,
                                          const uint8_t *sk) {
    const uint8_t *secret = sk;      /* First 32 bytes */
    const uint8_t *public = sk + 32; /* Second 32 bytes */

    /* Sign with c25519 Ed25519 */
    edsign_sign(sig, public, secret, m, (size_t)mlen);

    if (siglen_p) {
        *siglen_p = 64;
    }

    return 0;
}
#define crypto_sign_detached my_crypto_sign_detached

#endif /* SODIUM_COMPAT_H */
