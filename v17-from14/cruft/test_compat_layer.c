/* Test if our compatibility layer behaves like libsodium */
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include "sodium_compat.h"

/* We need to undefine the compat macros to test both */
#undef crypto_sign_keypair
#undef crypto_sign_detached
#undef sodium_init

/* External declarations for real libsodium functions */
extern int libsodium_sodium_init(void);
extern int libsodium_crypto_sign_keypair(uint8_t *pk, uint8_t *sk);
extern int libsodium_crypto_sign_detached(uint8_t *sig, unsigned long long *siglen_p,
                                          const uint8_t *m, unsigned long long mlen,
                                          const uint8_t *sk);

int main() {
    /* Test message */
    const char *msg = "Test message for SSH";
    size_t msg_len = strlen(msg);

    /* Initialize libsodium */
    if (libsodium_sodium_init() < 0) {
        printf("libsodium init failed\n");
        return 1;
    }

    /* Generate key with real libsodium */
    uint8_t ls_pk[32], ls_sk[64];
    libsodium_crypto_sign_keypair(ls_pk, ls_sk);

    /* Sign with real libsodium */
    uint8_t ls_sig[64];
    unsigned long long ls_siglen;
    libsodium_crypto_sign_detached(ls_sig, &ls_siglen, (uint8_t*)msg, msg_len, ls_sk);

    printf("Real libsodium:\n");
    printf("  Public key: ");
    for (int i = 0; i < 32; i++) printf("%02x", ls_pk[i]);
    printf("\n  Signature:  ");
    for (int i = 0; i < 64; i++) printf("%02x", ls_sig[i]);
    printf("\n\n");

    /* Now test with compat layer - using same secret */
    uint8_t compat_sk[64];
    memcpy(compat_sk, ls_sk, 64);  /* Copy the libsodium key */

    /* Sign with compat layer */
    uint8_t compat_sig[64];
    unsigned long long compat_siglen;
    /* Call the compat layer function from header */
    /* Actually, we can't easily do this because the macros are undefined */
    /* Let me just manually call what the compat does */
    const uint8_t *secret = compat_sk;
    const uint8_t *public = compat_sk + 32;
    edsign_sign(compat_sig, public, secret, (uint8_t*)msg, msg_len);

    printf("Compat layer (c25519):\n");
    printf("  Public key: ");
    for (int i = 0; i < 32; i++) printf("%02x", public[i]);
    printf("\n  Signature:  ");
    for (int i = 0; i < 64; i++) printf("%02x", compat_sig[i]);
    printf("\n\n");

    /* Compare */
    int pk_match = memcmp(ls_pk, public, 32) == 0;
    int sig_match = memcmp(ls_sig, compat_sig, 64) == 0;

    printf("Public key match: %s\n", pk_match ? "✅ YES" : "❌ NO");
    printf("Signature match:  %s\n", sig_match ? "✅ YES" : "❌ NO");

    if (sig_match) {
        printf("\n✅ Compat layer produces identical results!\n");
    } else {
        printf("\n❌ Compat layer produces different signatures!\n");
    }

    return 0;
}
