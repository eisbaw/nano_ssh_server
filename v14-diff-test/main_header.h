/*
 * Nano SSH Server - v14-diff-test
 * Minimal diff testing: Replace libsodium functions one at a time
 *
 * Compilation flags to test each replacement:
 * -DTEST_RANDOMBYTES - Replace randombytes_buf with custom
 * -DTEST_CURVE25519 - Replace Curve25519 with custom
 * -DTEST_KEYGEN - Replace crypto_sign_keypair with c25519
 * -DTEST_SIGNING - Replace crypto_sign_detached with c25519
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Conditional includes based on test flags */
#if defined(TEST_RANDOMBYTES) || defined(TEST_CURVE25519) || defined(TEST_KEYGEN) || defined(TEST_SIGNING)
    /* Testing mode: include custom implementations */
    #ifdef TEST_RANDOMBYTES
        #include "random_minimal.h"
    #endif

    #ifdef TEST_CURVE25519
        #include "curve25519_minimal.h"
    #endif

    #if defined(TEST_KEYGEN) || defined(TEST_SIGNING)
        #include "edsign.h"
        #include "random_minimal.h"  /* Needed for keypair generation */
    #endif

    /* Still need libsodium for functions we're not replacing */
    #include <sodium.h>
#else
    /* Baseline: 100% libsodium */
    #include <sodium.h>
#endif

#include "aes128_minimal.h"
#include "sha256_minimal.h"

/* Wrapper functions for conditional replacement */

#ifdef TEST_KEYGEN
/* Replace crypto_sign_keypair with c25519 version */
static inline int my_crypto_sign_keypair(uint8_t *pk, uint8_t *sk) {
    fprintf(stderr, ">>> USING C25519 KEY GENERATION <<<\n");
    /* Generate 32-byte secret */
    randombytes_buf(sk, 32);
    /* Derive public key */
    edsign_sec_to_pub(pk, sk);
    /* libsodium format: store public in second half */
    memcpy(sk + 32, pk, 32);
    return 0;
}
#define crypto_sign_keypair my_crypto_sign_keypair
#endif

#ifdef TEST_SIGNING
/* Replace crypto_sign_detached with c25519 version */
static inline int my_crypto_sign_detached(uint8_t *sig, unsigned long long *siglen_p,
                                          const uint8_t *m, unsigned long long mlen,
                                          const uint8_t *sk) {
    fprintf(stderr, ">>> USING C25519 SIGNING <<<\n");
    const uint8_t *secret = sk;      /* First 32 bytes */
    const uint8_t *public = sk + 32; /* Second 32 bytes */
    edsign_sign(sig, public, secret, m, (size_t)mlen);
    if (siglen_p) *siglen_p = 64;
    return 0;
}
#define crypto_sign_detached my_crypto_sign_detached
#endif

/* Forward declarations */
ssize_t send_data(int fd, const void *buf, size_t len);
ssize_t recv_data(int fd, void *buf, size_t len);
