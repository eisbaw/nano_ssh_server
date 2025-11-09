/* Exactly reproduce what the server does */
#include <stdio.h>
#include <string.h>

/* First include our custom implementations */
#include "random_minimal.h"
#include "edsign.h"

/* Now manually implement the compat layer inline */
static inline int my_crypto_sign_keypair(uint8_t *pk, uint8_t *sk) {
    /* Generate 32-byte secret */
    randombytes_buf(sk, 32);

    /* Derive public key */
    edsign_sec_to_pub(pk, sk);

    /* libsodium stores public key in second half of sk */
    memcpy(sk + 32, pk, 32);

    return 0;
}

static inline int my_crypto_sign_detached(uint8_t *sig, unsigned long long *siglen_p,
                                          const uint8_t *m, unsigned long long mlen,
                                          const uint8_t *sk) {
    const uint8_t *secret = sk;      /* First 32 bytes */
    const uint8_t *public = sk + 32; /* Second 32 bytes */

    edsign_sign(sig, public, secret, m, (size_t)mlen);

    if (siglen_p) {
        *siglen_p = 64;
    }

    return 0;
}

/* Now include libsodium for verification */
#include <sodium.h>

int main() {
    const char *msg = "Exchange hash";
    size_t msg_len = strlen(msg);

    /* Initialize libsodium */
    if (sodium_init() < 0) {
        printf("libsodium init failed\n");
        return 1;
    }

    printf("=== Exact reproduction of server flow ===\n\n");

    /* Generate key using compat layer (as server does) */
    uint8_t pk[32], sk[64];
    my_crypto_sign_keypair(pk, sk);

    printf("Generated key:\n");
    printf("  Public key: ");
    for (int i = 0; i < 32; i++) printf("%02x", pk[i]);
    printf("\n");
    printf("  sk[0:32]: ");
    for (int i = 0; i < 16; i++) printf("%02x", sk[i]);
    printf("...\n");
    printf("  sk[32:64]: ");
    for (int i = 0; i < 16; i++) printf("%02x", sk[32+i]);
    printf("...\n\n");

    /* Verify format */
    int pk_matches_sk_second_half = memcmp(pk, sk + 32, 32) == 0;
    printf("pk matches sk[32:64]: %s\n\n", pk_matches_sk_second_half ? "✅ YES" : "❌ NO");

    /* Sign using compat layer (as server does) */
    uint8_t sig[64];
    unsigned long long siglen;
    my_crypto_sign_detached(sig, &siglen, (uint8_t*)msg, msg_len, sk);

    printf("Signature: ");
    for (int i = 0; i < 16; i++) printf("%02x", sig[i]);
    printf("...\n\n");

    /* Verify using detached verify (sanity check) */
    int detached_result = crypto_sign_ed25519_verify_detached(sig, (uint8_t*)msg, msg_len, pk);
    printf("crypto_sign_ed25519_verify_detached: %s\n", detached_result == 0 ? "✅ PASS" : "❌ FAIL");

    /* Verify using OpenSSH-style open (as OpenSSH does) */
    size_t smlen = 64 + msg_len;
    uint8_t *sm = malloc(smlen);
    uint8_t *m = malloc(smlen);
    unsigned long long mlen = smlen;

    memcpy(sm, sig, 64);
    memcpy(sm + 64, msg, msg_len);

    int open_result = crypto_sign_ed25519_open(m, &mlen, sm, smlen, pk);
    printf("crypto_sign_ed25519_open: %s\n", open_result == 0 ? "✅ PASS" : "❌ FAIL");

    if (open_result != 0) {
        printf("\n❌❌❌ THIS IS THE BUG! ❌❌❌\n");
        printf("The compat layer produces signatures that fail crypto_sign_ed25519_open!\n");
        printf("But pass crypto_sign_ed25519_verify_detached!\n");
    } else {
        printf("\n✅ Everything works! Bug must be elsewhere.\n");
    }

    free(sm);
    free(m);

    return 0;
}
