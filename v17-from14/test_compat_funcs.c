/* Test the compatibility layer functions directly */
#include <stdio.h>
#include <string.h>
#include <sodium.h>

/* Include the compat layer but first undefine to avoid conflicts */
#define TEST_COMPAT 1
#include "sodium_compat.h"

/* Declare real libsodium functions with different names */
extern int _real_crypto_sign_ed25519_keypair(uint8_t *pk, uint8_t *sk) __attribute__((weak, alias("crypto_sign_ed25519_keypair")));
extern int _real_crypto_sign_ed25519_detached(uint8_t *sig, unsigned long long *siglen_p,
                                               const uint8_t *m, unsigned long long mlen,
                                               const uint8_t *sk) __attribute__((weak, alias("crypto_sign_ed25519_detached")));

int main() {
    const char *msg = "Test message for SSH";
    size_t msg_len = strlen(msg);

    /* Initialize */
    if (sodium_init() < 0) {
        printf("Init failed\n");
        return 1;
    }

    printf("=== Testing compatibility layer functions ===\n\n");

    /* Test compat layer crypto_sign_keypair */
    uint8_t compat_pk[32], compat_sk[64];
    crypto_sign_keypair(compat_pk, compat_sk);

    printf("Compat layer generated key:\n");
    printf("  Public: ");
    for (int i = 0; i < 32; i++) printf("%02x", compat_pk[i]);
    printf("\n");

    /* Test compat layer crypto_sign_detached */
    uint8_t compat_sig[64];
    unsigned long long compat_siglen;
    crypto_sign_detached(compat_sig, &compat_siglen, (uint8_t*)msg, msg_len, compat_sk);

    printf("  Signature: ");
    for (int i = 0; i < 16; i++) printf("%02x", compat_sig[i]);
    printf("...\n");

    /* Verify with real libsodium */
    int verify_result = crypto_sign_ed25519_verify_detached(compat_sig, (uint8_t*)msg, msg_len, compat_pk);
    printf("  Real libsodium verifies compat sig: %s\n\n", verify_result == 0 ? "✅ PASS" : "❌ FAIL");

    if (verify_result != 0) {
        printf("❌❌❌ FOUND THE BUG! ❌❌❌\n");
        printf("The compatibility layer is producing signatures that libsodium cannot verify!\n");

        /* Debug: check key format */
        printf("\nDebug info:\n");
        printf("compat_sk[0:32] (secret): ");
        for (int i = 0; i < 16; i++) printf("%02x", compat_sk[i]);
        printf("...\n");
        printf("compat_sk[32:64] (public): ");
        for (int i = 0; i < 16; i++) printf("%02x", compat_sk[32+i]);
        printf("...\n");
        printf("compat_pk: ");
        for (int i = 0; i < 16; i++) printf("%02x", compat_pk[i]);
        printf("...\n");

        int pk_match = memcmp(compat_pk, compat_sk + 32, 32) == 0;
        printf("compat_pk matches compat_sk[32:64]: %s\n", pk_match ? "YES" : "NO");
    } else {
        printf("✅ Compatibility layer works correctly!\n");
    }

    return 0;
}
