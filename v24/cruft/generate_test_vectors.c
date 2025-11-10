/*
 * AES-128-CTR Test Vector Generator
 * Uses OpenSSL to generate known-good test vectors
 * These vectors will be used to verify our custom AES implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

void generate_aes_ctr_test_vector(int test_num,
                                   const uint8_t *key,
                                   const uint8_t *iv,
                                   const uint8_t *plaintext,
                                   size_t pt_len) {
    uint8_t ciphertext[256];
    EVP_CIPHER_CTX *ctx;
    int len;

    printf("\n=== Test Vector %d ===\n", test_num);
    print_hex("Key", key, 16);
    print_hex("IV", iv, 16);
    print_hex("Plaintext", plaintext, pt_len);

    // Create and initialize context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return;
    }

    // Initialize encryption
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, key, iv)) {
        fprintf(stderr, "Failed to initialize encryption\n");
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // Encrypt
    if (!EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, pt_len)) {
        fprintf(stderr, "Failed to encrypt\n");
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    print_hex("Ciphertext", ciphertext, pt_len);

    // Cleanup
    EVP_CIPHER_CTX_free(ctx);
}

int main() {
    printf("AES-128-CTR Test Vectors\n");
    printf("========================\n");

    // Test 1: All zeros
    {
        uint8_t key[16] = {0};
        uint8_t iv[16] = {0};
        uint8_t plaintext[16] = {0};
        generate_aes_ctr_test_vector(1, key, iv, plaintext, 16);
    }

    // Test 2: Sequential bytes
    {
        uint8_t key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
        uint8_t iv[16] = {0};
        uint8_t plaintext[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        generate_aes_ctr_test_vector(2, key, iv, plaintext, 16);
    }

    // Test 3: Known test vector from NIST
    {
        uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                           0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
        uint8_t iv[16] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                          0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
        uint8_t plaintext[16] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                                  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};
        generate_aes_ctr_test_vector(3, key, iv, plaintext, 16);
    }

    // Test 4: Multiple blocks (32 bytes)
    {
        uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                           0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
        uint8_t iv[16] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                          0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
        uint8_t plaintext[32] = {
            0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
            0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
            0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
            0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51
        };
        generate_aes_ctr_test_vector(4, key, iv, plaintext, 32);
    }

    // Test 5: SSH packet-like (48 bytes)
    {
        uint8_t key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                           0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
        uint8_t iv[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
        uint8_t plaintext[48];
        // Simulate SSH packet
        for (int i = 0; i < 48; i++) plaintext[i] = i;
        generate_aes_ctr_test_vector(5, key, iv, plaintext, 48);
    }

    // Test 6: Non-aligned length (23 bytes)
    {
        uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                           0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
        uint8_t iv[16] = {0};
        uint8_t plaintext[23] = "Hello, AES-128-CTR!";
        generate_aes_ctr_test_vector(6, key, iv, plaintext, 23);
    }

    printf("\nTest vectors generated successfully!\n");
    printf("Save this output to test_vectors.txt for testing\n");

    return 0;
}
