/* Libsodium compatibility layer using v17 custom crypto implementations */
#ifndef SODIUM_COMPAT_H
#define SODIUM_COMPAT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "random_minimal.h"
#include "curve25519_minimal.h"
#include "edsign.h"

/* Initialize (no-op for our implementations) */
static inline int sodium_init(void) {
    return 0;
}

/* crypto_sign_keypair: Generate Ed25519 keypair
 * libsodium format: pk[32], sk[64] where sk = secret||public
 * c25519 format: secret[32], pub[32] separate
 */
static inline int crypto_sign_keypair(uint8_t *pk, uint8_t *sk) {
    /* Generate 32-byte secret */
    randombytes_buf(sk, 32);

    /* Derive public key */
    edsign_sec_to_pub(pk, sk);

    /* libsodium stores public key in second half of sk */
    memcpy(sk + 32, pk, 32);

    return 0;
}

/* crypto_sign_detached: Sign a message
 * libsodium: sk[64] contains secret||public
 * c25519: needs separate secret[32] and public[32]
 */
static inline int crypto_sign_detached(uint8_t *sig, unsigned long long *siglen_p,
                                       const uint8_t *m, unsigned long long mlen,
                                       const uint8_t *sk) {
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

#endif /* SODIUM_COMPAT_H */
