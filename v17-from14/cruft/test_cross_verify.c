/* Test if libsodium can verify c25519 signatures */
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include "edsign.h"

int main() {
    const char *msg = "Test message";
    size_t msg_len = strlen(msg);

    /* Initialize libsodium */
    if (sodium_init() < 0) {
        printf("libsodium init failed\n");
        return 1;
    }

    /* Generate key with c25519 */
    uint8_t c25_secret[32];
    uint8_t c25_public[32];

    /* Use libsodium's randombytes for consistency */
    randombytes_buf(c25_secret, 32);
    edsign_sec_to_pub(c25_public, c25_secret);

    printf("Generated key with c25519:\n");
    printf("  Secret (first 16 bytes): ");
    for (int i = 0; i < 16; i++) printf("%02x", c25_secret[i]);
    printf("...\n  Public: ");
    for (int i = 0; i < 32; i++) printf("%02x", c25_public[i]);
    printf("\n\n");

    /* Sign with c25519 */
    uint8_t c25_sig[64];
    edsign_sign(c25_sig, c25_public, c25_secret, (uint8_t*)msg, msg_len);

    printf("Signature (c25519): ");
    for (int i = 0; i < 64; i++) printf("%02x", c25_sig[i]);
    printf("\n\n");

    /* Verify with c25519 (should pass) */
    uint8_t c25_verify = edsign_verify(c25_sig, c25_public, (uint8_t*)msg, msg_len);
    printf("c25519 verifies c25519 sig: %s\n", c25_verify ? "✅ PASS" : "❌ FAIL");

    /* Now try to verify with libsodium */
    int ls_verify = crypto_sign_ed25519_verify_detached(c25_sig, (uint8_t*)msg, msg_len, c25_public);
    printf("libsodium verifies c25519 sig: %s\n", ls_verify == 0 ? "✅ PASS" : "❌ FAIL");

    if (ls_verify != 0) {
        printf("\n❌❌❌ FOUND THE BUG! ❌❌❌\n");
        printf("Libsodium CANNOT verify c25519 signatures!\n");
        printf("Even though c25519 can verify its own signatures.\n\n");

        /* Now let's do the reverse: libsodium key, libsodium sign, c25519 verify */
        uint8_t ls_pk[32], ls_sk[64];
        crypto_sign_ed25519_keypair(ls_pk, ls_sk);

        uint8_t ls_sig[64];
        unsigned long long ls_siglen;
        crypto_sign_ed25519_detached(ls_sig, &ls_siglen, (uint8_t*)msg, msg_len, ls_sk);

        uint8_t c25_verify_ls = edsign_verify(ls_sig, ls_pk, (uint8_t*)msg, msg_len);
        printf("c25519 verifies libsodium sig: %s\n", c25_verify_ls ? "✅ PASS" : "❌ FAIL");
    } else {
        printf("\n✅ Libsodium CAN verify c25519 signatures.\n");
        printf("The bug must be elsewhere.\n");
    }

    return 0;
}
