/* Test signature compatibility between libsodium and c25519 */
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include "edsign.h"

int main() {
    /* Test message */
    const char *msg = "Hello, World!";
    size_t msg_len = strlen(msg);

    /* Generate key with libsodium */
    uint8_t ls_pk[32], ls_sk[64];
    crypto_sign_keypair(ls_pk, ls_sk);

    printf("Libsodium public key: ");
    for (int i = 0; i < 32; i++) printf("%02x", ls_pk[i]);
    printf("\n");

    /* Sign with libsodium */
    uint8_t ls_sig[64];
    unsigned long long ls_siglen;
    crypto_sign_detached(ls_sig, &ls_siglen, (uint8_t*)msg, msg_len, ls_sk);

    printf("Libsodium signature:  ");
    for (int i = 0; i < 64; i++) printf("%02x", ls_sig[i]);
    printf("\n");

    /* Now use the same secret with c25519 */
    /* libsodium's sk[64] = seed(32) || pk(32) */
    /* But we need to use the seed part */
    const uint8_t *secret = ls_sk;  /* First 32 bytes */
    const uint8_t *public = ls_sk + 32;  /* Second 32 bytes (should match ls_pk) */

    /* Verify they match */
    if (memcmp(public, ls_pk, 32) != 0) {
        printf("ERROR: Public key mismatch!\n");
        return 1;
    }

    /* Sign with c25519 using same secret */
    uint8_t c25_sig[64];
    edsign_sign(c25_sig, public, secret, (uint8_t*)msg, msg_len);

    printf("c25519 signature:     ");
    for (int i = 0; i < 64; i++) printf("%02x", c25_sig[i]);
    printf("\n");

    /* Compare signatures */
    if (memcmp(ls_sig, c25_sig, 64) == 0) {
        printf("\n✅ Signatures MATCH - implementations are compatible\n");
    } else {
        printf("\n❌ Signatures DIFFER - implementations are incompatible\n");

        /* Verify both with c25519 */
        uint8_t verify_ls = edsign_verify(ls_sig, public, (uint8_t*)msg, msg_len);
        uint8_t verify_c25 = edsign_verify(c25_sig, public, (uint8_t*)msg, msg_len);

        printf("c25519 verifies libsodium sig: %s\n", verify_ls ? "PASS" : "FAIL");
        printf("c25519 verifies c25519 sig: %s\n", verify_c25 ? "PASS" : "FAIL");

        /* Verify both with libsodium */
        int verify_ls_by_ls = crypto_sign_verify_detached(ls_sig, (uint8_t*)msg, msg_len, ls_pk);
        int verify_c25_by_ls = crypto_sign_verify_detached(c25_sig, (uint8_t*)msg, msg_len, ls_pk);

        printf("libsodium verifies libsodium sig: %s\n", verify_ls_by_ls == 0 ? "PASS" : "FAIL");
        printf("libsodium verifies c25519 sig: %s\n", verify_c25_by_ls == 0 ? "PASS" : "FAIL");
    }

    return 0;
}
