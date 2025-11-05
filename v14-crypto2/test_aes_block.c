#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "aes_minimal.h"

// AES-128 test vector from FIPS-197
// Key: 000102030405060708090a0b0c0d0e0f
// Plaintext: 00112233445566778899aabbccddeeff
// Expected Ciphertext: 69c4e0d86a7b0430d8cdb78070b4c55a

int main() {
    uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };

    uint8_t plaintext[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };

    uint8_t expected[16] = {
        0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
        0xd8, 0xcdb, 0x78, 0x07, 0x0b, 0x4c, 0x55, 0xa
    };

    uint8_t ciphertext[16];
    uint8_t round_key[176];

    printf("Testing AES-128 block encryption with FIPS-197 test vector\n\n");

    // Expand key
    aes128_key_expansion(round_key, key);

    // Encrypt
    memcpy(ciphertext, plaintext, 16);
    aes128_encrypt_block(ciphertext, round_key);

    printf("Key:        ");
    for (int i = 0; i < 16; i++) printf("%02x", key[i]);
    printf("\n");

    printf("Plaintext:  ");
    for (int i = 0; i < 16; i++) printf("%02x", plaintext[i]);
    printf("\n");

    printf("Expected:   ");
    for (int i = 0; i < 16; i++) printf("%02x", expected[i]);
    printf("\n");

    printf("Got:        ");
    for (int i = 0; i < 16; i++) printf("%02x", ciphertext[i]);
    printf("\n\n");

    if (memcmp(ciphertext, expected, 16) == 0) {
        printf("✓ SUCCESS: AES block encryption matches test vector\n");
        return 0;
    } else {
        printf("✗ FAIL: AES block encryption does NOT match test vector\n");
        return 1;
    }
}
