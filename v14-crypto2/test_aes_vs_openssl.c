#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "aes_minimal.h"

int main() {
    uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };

    uint8_t iv[16] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    uint8_t plaintext[32] = "This is a test message!!!!!";
    uint8_t openssl_cipher[32];
    uint8_t custom_cipher[32];

    printf("Testing AES-128-CTR: Custom vs OpenSSL\n\n");
    printf("Key: ");
    for (int i = 0; i < 16; i++) printf("%02x ", key[i]);
    printf("\n");
    printf("IV:  ");
    for (int i = 0; i < 16; i++) printf("%02x ", iv[i]);
    printf("\n\n");

    // OpenSSL encryption
    EVP_CIPHER_CTX *ctx_ssl = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx_ssl, EVP_aes_128_ctr(), NULL, key, iv);
    int len;
    memcpy(openssl_cipher, plaintext, 32);
    EVP_EncryptUpdate(ctx_ssl, openssl_cipher, &len, openssl_cipher, 32);
    EVP_CIPHER_CTX_free(ctx_ssl);

    printf("OpenSSL ciphertext:\n");
    for (int i = 0; i < 32; i++) {
        printf("%02x ", openssl_cipher[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }

    // Custom encryption
    aes128_ctx ctx_custom;
    aes128_ctr_init(&ctx_custom, key, iv);
    memcpy(custom_cipher, plaintext, 32);
    aes128_ctr_xor(&ctx_custom, custom_cipher, 32);

    printf("\nCustom ciphertext:\n");
    for (int i = 0; i < 32; i++) {
        printf("%02x ", custom_cipher[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }

    // Compare
    if (memcmp(openssl_cipher, custom_cipher, 32) == 0) {
        printf("\n✓ SUCCESS: Ciphertexts match!\n");
        return 0;
    } else {
        printf("\n✗ FAIL: Ciphertexts DO NOT match!\n");
        return 1;
    }
}
