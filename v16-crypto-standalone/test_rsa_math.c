/*
 * Test RSA mathematics: verify (m^d)^e mod n == m
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t m, c, m2;
    uint8_t m_bytes[256], c_bytes[256], m2_bytes[256];

    /* Initialize key */
    rsa_init_key(&key);

    /* Create a test message (small number) */
    memset(m_bytes, 0, 256);
    m_bytes[255] = 0x42;  /* m = 66 */
    bn_from_bytes(&m, m_bytes, 256);

    printf("=== Testing RSA Math ===\n");
    printf("Original message m: ");
    for (int i = 252; i < 256; i++) {
        printf("%02x ", m_bytes[i]);
    }
    printf("\n");

    /* Encrypt: c = m^e mod n */
    printf("\nStep 1: Encrypt c = m^e mod n\n");
    bn_modexp(&c, &m, &key.e, &key.n);
    bn_to_bytes(&c, c_bytes, 256);
    printf("Ciphertext c (last 8 bytes): ");
    for (int i = 248; i < 256; i++) {
        printf("%02x ", c_bytes[i]);
    }
    printf("\n");

    /* Decrypt: m2 = c^d mod n */
    printf("\nStep 2: Decrypt m2 = c^d mod n\n");
    bn_modexp(&m2, &c, &key.d, &key.n);
    bn_to_bytes(&m2, m2_bytes, 256);
    printf("Decrypted m2 (last 8 bytes): ");
    for (int i = 248; i < 256; i++) {
        printf("%02x ", m2_bytes[i]);
    }
    printf("\n");

    /* Compare */
    printf("\nStep 3: Compare m == m2\n");
    if (memcmp(m_bytes, m2_bytes, 256) == 0) {
        printf("✓ RSA math WORKS: m == m2\n");
        return 0;
    } else {
        printf("✗ RSA math BROKEN: m != m2\n");
        printf("\nExpected: ");
        for (int i = 0; i < 32; i++) {
            printf("%02x", m_bytes[i]);
        }
        printf("\nGot:      ");
        for (int i = 0; i < 32; i++) {
            printf("%02x", m2_bytes[i]);
        }
        printf("\n");
        return 1;
    }
}
