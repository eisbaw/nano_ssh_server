#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "aes_minimal.h"

int main() {
    // Test AES-128-CTR with known test vector
    uint8_t key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };

    uint8_t iv[16] = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
    };

    uint8_t plaintext[16] = "Hello World!!!!";
    uint8_t ciphertext[16];
    uint8_t decrypted[16];

    printf("Testing AES-128-CTR...\n");
    printf("Plaintext:  ");
    for (int i = 0; i < 16; i++) printf("%02x ", plaintext[i]);
    printf("\n");

    // Encrypt
    aes128_ctx ctx_enc;
    aes128_ctr_init(&ctx_enc, key, iv);
    memcpy(ciphertext, plaintext, 16);
    aes128_ctr_xor(&ctx_enc, ciphertext, 16);

    printf("Ciphertext: ");
    for (int i = 0; i < 16; i++) printf("%02x ", ciphertext[i]);
    printf("\n");

    // Decrypt (reset IV)
    aes128_ctx ctx_dec;
    aes128_ctr_init(&ctx_dec, key, iv);
    memcpy(decrypted, ciphertext, 16);
    aes128_ctr_xor(&ctx_dec, decrypted, 16);

    printf("Decrypted:  ");
    for (int i = 0; i < 16; i++) printf("%02x ", decrypted[i]);
    printf("\n");

    // Check if decryption matches original
    if (memcmp(plaintext, decrypted, 16) == 0) {
        printf("✓ SUCCESS: Decryption matches plaintext\n");
        return 0;
    } else {
        printf("✗ FAIL: Decryption does not match plaintext\n");
        printf("Expected: ");
        for (int i = 0; i < 16; i++) printf("%02x ", plaintext[i]);
        printf("\n");
        return 1;
    }
}
