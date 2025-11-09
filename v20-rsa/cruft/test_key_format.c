/* Test key format differences between libsodium and c25519 */
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include "edsign.h"

int main() {
    /* Initialize libsodium */
    if (sodium_init() < 0) {
        printf("libsodium init failed\n");
        return 1;
    }

    /* Test 1: Generate a 32-byte seed */
    uint8_t seed[32];
    randombytes_buf(seed, 32);

    printf("=== Test 1: Using raw 32-byte seed ===\n");
    printf("Seed: ");
    for (int i = 0; i < 32; i++) printf("%02x", seed[i]);
    printf("\n\n");

    /* Derive public key with c25519 from seed */
    uint8_t c25_pk[32];
    edsign_sec_to_pub(c25_pk, seed);

    printf("c25519 public key: ");
    for (int i = 0; i < 32; i++) printf("%02x", c25_pk[i]);
    printf("\n");

    /* Generate key with libsodium using crypto_sign_seed_keypair (uses same seed) */
    uint8_t ls_pk[32], ls_sk[64];
    crypto_sign_ed25519_seed_keypair(ls_pk, ls_sk, seed);

    printf("libsodium public key: ");
    for (int i = 0; i < 32; i++) printf("%02x", ls_pk[i]);
    printf("\n");

    printf("libsodium sk[0:32]: ");
    for (int i = 0; i < 32; i++) printf("%02x", ls_sk[i]);
    printf("\n");

    printf("libsodium sk[32:64]: ");
    for (int i = 0; i < 32; i++) printf("%02x", ls_sk[32+i]);
    printf("\n\n");

    /* Check if public keys match */
    int pk_match = memcmp(c25_pk, ls_pk, 32) == 0;
    printf("Public keys match: %s\n", pk_match ? "✅ YES" : "❌ NO");

    int sk_public_match = memcmp(c25_pk, ls_sk + 32, 32) == 0;
    printf("c25_pk matches ls_sk[32:64]: %s\n", sk_public_match ? "✅ YES" : "❌ NO");

    int seed_scalar_match = memcmp(seed, ls_sk, 32) == 0;
    printf("seed matches ls_sk[0:32]: %s\n", seed_scalar_match ? "✅ YES" : "❌ NO");

    printf("\n=== Test 2: Sign and verify ===\n");
    const char *msg = "Test message";
    size_t msg_len = strlen(msg);

    /* Sign with c25519 using seed */
    uint8_t c25_sig[64];
    edsign_sign(c25_sig, c25_pk, seed, (uint8_t*)msg, msg_len);

    printf("c25519 signature: ");
    for (int i = 0; i < 16; i++) printf("%02x", c25_sig[i]);
    printf("...\n");

    /* Sign with libsodium using ls_sk */
    uint8_t ls_sig[64];
    unsigned long long ls_siglen;
    crypto_sign_ed25519_detached(ls_sig, &ls_siglen, (uint8_t*)msg, msg_len, ls_sk);

    printf("libsodium signature: ");
    for (int i = 0; i < 16; i++) printf("%02x", ls_sig[i]);
    printf("...\n");

    int sig_match = memcmp(c25_sig, ls_sig, 64) == 0;
    printf("Signatures match: %s\n", sig_match ? "✅ YES" : "❌ NO");

    /* Cross-verify */
    uint8_t c25_verifies_c25 = edsign_verify(c25_sig, c25_pk, (uint8_t*)msg, msg_len);
    uint8_t c25_verifies_ls = edsign_verify(ls_sig, ls_pk, (uint8_t*)msg, msg_len);
    int ls_verifies_c25 = crypto_sign_ed25519_verify_detached(c25_sig, (uint8_t*)msg, msg_len, c25_pk);
    int ls_verifies_ls = crypto_sign_ed25519_verify_detached(ls_sig, (uint8_t*)msg, msg_len, ls_pk);

    printf("\nc25519 verifies c25519 sig: %s\n", c25_verifies_c25 ? "✅" : "❌");
    printf("c25519 verifies libsodium sig: %s\n", c25_verifies_ls ? "✅" : "❌");
    printf("libsodium verifies c25519 sig: %s\n", ls_verifies_c25 == 0 ? "✅" : "❌");
    printf("libsodium verifies libsodium sig: %s\n", ls_verifies_ls == 0 ? "✅" : "❌");

    return 0;
}
