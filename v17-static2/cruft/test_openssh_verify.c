/* Test verification the way OpenSSH does it */
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include "edsign.h"

int main() {
    const char *msg = "Exchange hash for SSH";
    size_t msg_len = strlen(msg);

    /* Initialize libsodium */
    if (sodium_init() < 0) {
        printf("libsodium init failed\n");
        return 1;
    }

    printf("=== Test 1: libsodium key, libsodium sign, libsodium verify (OpenSSH-style) ===\n");

    /* Generate key with libsodium */
    uint8_t ls_pk[32], ls_sk[64];
    crypto_sign_ed25519_keypair(ls_pk, ls_sk);

    /* Sign with libsodium (detached) */
    uint8_t ls_sig[64];
    unsigned long long ls_siglen;
    crypto_sign_ed25519_detached(ls_sig, &ls_siglen, (uint8_t*)msg, msg_len, ls_sk);

    printf("Libsodium signature: ");
    for (int i = 0; i < 16; i++) printf("%02x", ls_sig[i]);
    printf("...\n");

    /* Verify OpenSSH-style: crypto_sign_ed25519_open with sig||msg */
    size_t smlen = 64 + msg_len;
    uint8_t *sm = malloc(smlen);
    uint8_t *m = malloc(smlen);
    unsigned long long mlen = smlen;

    memcpy(sm, ls_sig, 64);           // signature first
    memcpy(sm + 64, msg, msg_len);    // then message

    int ret = crypto_sign_ed25519_open(m, &mlen, sm, smlen, ls_pk);
    printf("crypto_sign_ed25519_open result: %d\n", ret);
    printf("Verification: %s\n\n", ret == 0 ? "✅ PASS" : "❌ FAIL");

    free(sm);
    free(m);

    printf("=== Test 2: c25519 key, c25519 sign, libsodium verify (OpenSSH-style) ===\n");

    /* Generate key with c25519 */
    uint8_t c25_secret[32], c25_pk[32];
    randombytes_buf(c25_secret, 32);
    edsign_sec_to_pub(c25_pk, c25_secret);

    /* Sign with c25519 */
    uint8_t c25_sig[64];
    edsign_sign(c25_sig, c25_pk, c25_secret, (uint8_t*)msg, msg_len);

    printf("c25519 signature: ");
    for (int i = 0; i < 16; i++) printf("%02x", c25_sig[i]);
    printf("...\n");

    /* Verify OpenSSH-style */
    sm = malloc(smlen);
    m = malloc(smlen);
    mlen = smlen;

    memcpy(sm, c25_sig, 64);           // signature first
    memcpy(sm + 64, msg, msg_len);     // then message

    ret = crypto_sign_ed25519_open(m, &mlen, sm, smlen, c25_pk);
    printf("crypto_sign_ed25519_open result: %d\n", ret);
    printf("Verification: %s\n", ret == 0 ? "✅ PASS" : "❌ FAIL");

    if (ret != 0) {
        printf("\n❌❌❌ FOUND THE BUG! ❌❌❌\n");
        printf("libsodium's crypto_sign_ed25519_open CANNOT verify c25519 signatures!\n");
        printf("Even though crypto_sign_ed25519_verify_detached CAN!\n\n");

        /* Double-check with verify_detached */
        int detached_verify = crypto_sign_ed25519_verify_detached(c25_sig, (uint8_t*)msg, msg_len, c25_pk);
        printf("For comparison, crypto_sign_ed25519_verify_detached: %s\n",
               detached_verify == 0 ? "✅ PASS" : "❌ FAIL");
    }

    free(sm);
    free(m);

    return 0;
}
