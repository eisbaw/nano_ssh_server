#include <stdio.h>
#include <string.h>
#include "bignum_simple.h"
#include "rsa.h"

#undef bn_modexp
#include "bignum_simple.h"

int main() {
    bn_t m, c, m2, n, e, d;
    
    /* Load RSA key */
    bn_from_bytes(&n, rsa_modulus, 256);
    bn_from_bytes(&d, rsa_private_exponent, 256);
    bn_zero(&e);
    e.array[0] = 65537;

    /* Test message */
    uint8_t m_bytes[256], c_bytes[256], m2_bytes[256];
    memset(m_bytes, 0, 256);
    m_bytes[255] = 0x42;  /* m = 66 */
    bn_from_bytes(&m, m_bytes, 256);

    printf("=== Testing Simple RSA Math ===\n");
    printf("Message m = 66 (0x42)\n\n");

    /* Encrypt: c = m^e mod n */
    printf("Step 1: c = m^e mod n\n");
    bn_modexp(&c, &m, &e, &n);
    bn_to_bytes(&c, c_bytes, 256);
    printf("Ciphertext (last 8 bytes): ");
    for (int i = 248; i < 256; i++) printf("%02x ", c_bytes[i]);
    printf("\n\n");

    /* Decrypt: m2 = c^d mod n */
    printf("Step 2: m2 = c^d mod n\n");
    bn_modexp(&m2, &c, &d, &n);
    bn_to_bytes(&m2, m2_bytes, 256);
    printf("Decrypted (last 8 bytes):  ");
    for (int i = 248; i < 256; i++) printf("%02x ", m2_bytes[i]);
    printf("\n\n");

    /* Compare */
    if (memcmp(m_bytes, m2_bytes, 256) == 0) {
        printf("✓ RSA math WORKS: m == m2\n");
        return 0;
    } else {
        printf("✗ RSA math BROKEN: m != m2\n");
        return 1;
    }
}
