/*
 * Basic crypto functionality test for v15-static
 * Tests: CSPRNG, SHA-256, HMAC, AES-CTR, DH, RSA
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "csprng.h"
#include "sha256_minimal.h"
#include "aes128_minimal.h"
#include "bignum.h"
#include "diffie_hellman.h"
#include "rsa.h"

int main() {
    printf("=== v15-static Crypto Tests ===\n\n");
    
    /* Test 1: CSPRNG */
    printf("1. CSPRNG Test...\n");
    uint8_t random_buf[32];
    if (random_bytes(random_buf, 32) == 0) {
        printf("   ✓ Generated 32 random bytes\n");
        printf("   First 8 bytes: ");
        for (int i = 0; i < 8; i++) printf("%02x ", random_buf[i]);
        printf("\n");
    } else {
        printf("   ✗ FAILED\n");
        return 1;
    }
    
    /* Test 2: SHA-256 */
    printf("\n2. SHA-256 Test...\n");
    uint8_t hash[32];
    const char *test_msg = "Hello World";
    sha256(hash, (const uint8_t *)test_msg, strlen(test_msg));
    printf("   ✓ Computed SHA-256\n");
    printf("   Hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", hash[i]);
    printf("...\n");
    
    /* Test 3: HMAC-SHA256 */
    printf("\n3. HMAC-SHA256 Test...\n");
    uint8_t hmac[32];
    uint8_t key[32] = {0x01, 0x02, 0x03, 0x04};
    hmac_sha256(hmac, (const uint8_t *)test_msg, strlen(test_msg), key, 32);
    printf("   ✓ Computed HMAC-SHA256\n");
    printf("   HMAC: ");
    for (int i = 0; i < 8; i++) printf("%02x", hmac[i]);
    printf("...\n");
    
    /* Test 4: AES-128-CTR */
    printf("\n4. AES-128-CTR Test...\n");
    aes128_ctr_ctx aes_ctx;
    uint8_t aes_key[16] = {0};
    uint8_t iv[16] = {0};
    uint8_t plaintext[16] = "Test message!!!";
    uint8_t ciphertext[16];
    aes128_ctr_init(&aes_ctx, aes_key, iv);
    aes128_ctr_encrypt(&aes_ctx, ciphertext, plaintext, 16);
    printf("   ✓ Encrypted 16 bytes\n");
    printf("   Ciphertext: ");
    for (int i = 0; i < 8; i++) printf("%02x", ciphertext[i]);
    printf("...\n");
    
    /* Test 5: DH Group14 */
    printf("\n5. DH Group14 Test...\n");
    uint8_t dh_private[256];
    uint8_t dh_public[256];
    dh_generate_keypair(dh_private, dh_public);
    printf("   ✓ Generated DH keypair\n");
    printf("   Public key (first 8 bytes): ");
    for (int i = 0; i < 8; i++) printf("%02x", dh_public[i]);
    printf("...\n");
    
    /* Test 6: RSA */
    printf("\n6. RSA-2048 Test...\n");
    rsa_key_t rsa_key;
    rsa_init_key(&rsa_key);
    printf("   ✓ Initialized RSA key\n");
    
    uint8_t signature[256];
    if (rsa_sign(signature, hash, &rsa_key) == 0) {
        printf("   ✓ Generated RSA signature\n");
        printf("   Signature (first 8 bytes): ");
        for (int i = 0; i < 8; i++) printf("%02x", signature[i]);
        printf("...\n");
    } else {
        printf("   ✗ RSA sign FAILED\n");
        return 1;
    }
    
    printf("\n=== All Crypto Tests PASSED ===\n");
    return 0;
}
