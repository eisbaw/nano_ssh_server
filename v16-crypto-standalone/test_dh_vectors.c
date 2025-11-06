/*
 * Test DH Group14 implementation against OpenSSL
 * This generates test vectors and compares custom implementation with OpenSSL
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <openssl/bn.h>
#include <openssl/dh.h>

#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s:\n  ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 32 == 0 && i + 1 < len) printf("\n  ");
    }
    printf("\n");
}

int compare_arrays(const uint8_t *a, const uint8_t *b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            printf("❌ MISMATCH at byte %zu: %02x vs %02x\n", i, a[i], b[i]);
            return -1;
        }
    }
    return 0;
}

/* Test with OpenSSL to generate reference values */
int test_with_openssl(const uint8_t *private_key, uint8_t *pub_openssl_out) {
    BIGNUM *p = NULL, *g = NULL, *priv_bn = NULL, *pub_bn = NULL;
    BN_CTX *ctx = NULL;
    uint8_t pub_openssl[256];
    int ret = -1;

    ctx = BN_CTX_new();
    if (!ctx) goto cleanup;

    /* Create prime p */
    p = BN_bin2bn(dh_group14_prime, 256, NULL);
    if (!p) goto cleanup;

    /* Create generator g = 2 */
    g = BN_new();
    if (!g) goto cleanup;
    BN_set_word(g, 2);

    /* Create private key */
    priv_bn = BN_bin2bn(private_key, 256, NULL);
    if (!priv_bn) goto cleanup;

    /* Compute public = g^priv mod p */
    pub_bn = BN_new();
    if (!pub_bn) goto cleanup;

    if (!BN_mod_exp(pub_bn, g, priv_bn, p, ctx)) {
        printf("OpenSSL BN_mod_exp failed\n");
        goto cleanup;
    }

    /* Convert to bytes */
    int pub_len = BN_num_bytes(pub_bn);
    memset(pub_openssl, 0, 256);
    BN_bn2bin(pub_bn, pub_openssl + (256 - pub_len));

    print_hex("OpenSSL Public Key", pub_openssl, 256);

    /* Check if public key is valid (> 1 and < p-1) */
    BIGNUM *one = BN_new();
    BN_one(one);

    if (BN_cmp(pub_bn, one) <= 0) {
        printf("⚠️  OpenSSL: Public key <= 1 (INVALID)\n");
    } else {
        printf("✅ OpenSSL: Public key > 1 (valid)\n");
    }

    BN_free(one);

    /* Copy result out */
    if (pub_openssl_out) {
        memcpy(pub_openssl_out, pub_openssl, 256);
    }

    ret = 0;

cleanup:
    if (ctx) BN_CTX_free(ctx);
    if (p) BN_free(p);
    if (g) BN_free(g);
    if (priv_bn) BN_free(priv_bn);
    if (pub_bn) BN_free(pub_bn);

    return ret;
}

/* Test our custom implementation */
int test_custom_implementation(const uint8_t *private_key, uint8_t *pub_custom_out) {
    uint8_t public_key[256];
    bn_t priv, pub, prime, generator;

    /* Convert to bignum */
    bn_from_bytes(&priv, private_key, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);

    /* generator = 2 */
    bn_zero(&generator);
    generator.array[0] = 2;

    /* Debug: Check if private key loaded correctly */
    printf("\nCustom implementation bignum check:\n");
    printf("  priv.array[0] = %u\n", priv.array[0]);
    printf("  priv is_zero = %d\n", bn_is_zero(&priv));
    printf("  generator.array[0] = %u\n", generator.array[0]);

    /* Compute public = g^priv mod p */
    bn_modexp(&pub, &generator, &priv, &prime);

    /* Debug: Check result */
    printf("  pub.array[0] = %u\n", pub.array[0]);
    printf("  pub is_zero = %d\n", bn_is_zero(&pub));

    /* Convert to bytes */
    bn_to_bytes(&pub, public_key, 256);

    print_hex("Custom Public Key  ", public_key, 256);

    /* Check if public key is valid (> 1) */
    bn_t one;
    bn_zero(&one);
    one.array[0] = 1;

    if (bn_cmp(&pub, &one) <= 0) {
        printf("⚠️  Custom: Public key <= 1 (INVALID) ❌\n");
        return -1;
    } else {
        printf("✅ Custom: Public key > 1 (valid)\n");
    }

    /* Copy result out */
    if (pub_custom_out) {
        memcpy(pub_custom_out, public_key, 256);
    }

    return 0;
}

int main() {
    printf("==============================================\n");
    printf("DH Group14 Test Vector Generator\n");
    printf("==============================================\n\n");

    /* Test with a simple private key first */
    uint8_t simple_priv[256];
    uint8_t pub_openssl[256], pub_custom[256];

    memset(simple_priv, 0, 256);
    simple_priv[255] = 5;  /* Private key = 5 */

    printf("Test 1: Simple private key (5)\n");
    printf("-------------------------------\n");
    print_hex("Private Key        ", simple_priv, 256);

    printf("\n--- OpenSSL Reference ---\n");
    test_with_openssl(simple_priv, pub_openssl);

    printf("\n--- Custom Implementation ---\n");
    test_custom_implementation(simple_priv, pub_custom);

    printf("\n--- Comparison ---\n");
    if (compare_arrays(pub_openssl, pub_custom, 256) == 0) {
        printf("✅ PUBLIC KEYS MATCH!\n");
    } else {
        printf("❌ PUBLIC KEYS DO NOT MATCH!\n");
    }

    printf("\n\n");
    printf("==============================================\n");

    /* Test with another simple value */
    memset(simple_priv, 0, 256);
    simple_priv[255] = 10;  /* Private key = 10 */

    printf("\nTest 2: Simple private key (10)\n");
    printf("--------------------------------\n");
    print_hex("Private Key        ", simple_priv, 256);

    printf("\n--- OpenSSL Reference ---\n");
    test_with_openssl(simple_priv, pub_openssl);

    printf("\n--- Custom Implementation ---\n");
    test_custom_implementation(simple_priv, pub_custom);

    printf("\n--- Comparison ---\n");
    if (compare_arrays(pub_openssl, pub_custom, 256) == 0) {
        printf("✅ PUBLIC KEYS MATCH!\n");
    } else {
        printf("❌ PUBLIC KEYS DO NOT MATCH!\n");
    }

    printf("\n\n");
    printf("==============================================\n");

    /* Test with larger value */
    memset(simple_priv, 0, 256);
    simple_priv[254] = 1;  /* Private key = 256 */

    printf("\nTest 3: Larger private key (256)\n");
    printf("---------------------------------\n");
    print_hex("Private Key        ", simple_priv, 256);

    printf("\n--- OpenSSL Reference ---\n");
    test_with_openssl(simple_priv, pub_openssl);

    printf("\n--- Custom Implementation ---\n");
    test_custom_implementation(simple_priv, pub_custom);

    printf("\n--- Comparison ---\n");
    if (compare_arrays(pub_openssl, pub_custom, 256) == 0) {
        printf("✅ PUBLIC KEYS MATCH!\n");
    } else {
        printf("❌ PUBLIC KEYS DO NOT MATCH!\n");
    }

    /* Test actual keypair generation */
    test_actual_keypair_generation();

    return 0;
}

/* Test the actual dh_generate_keypair function */
int test_actual_keypair_generation() {
    uint8_t private_key[256], public_key[256];
    uint8_t pub_openssl[256];

    printf("\n\n==============================================\n");
    printf("Test 4: Actual dh_generate_keypair() function\n");
    printf("==============================================\n\n");

    /* Generate keypair using the actual function */
    printf("Generating keypair with dh_generate_keypair()...\n");
    if (dh_generate_keypair(private_key, public_key) < 0) {
        printf("❌ dh_generate_keypair() failed\n");
        return -1;
    }

    print_hex("Generated Private Key", private_key, 256);
    print_hex("Generated Public Key ", public_key, 256);

    /* Verify with OpenSSL using same private key */
    printf("\n--- Verifying with OpenSSL ---\n");
    test_with_openssl(private_key, pub_openssl);

    printf("\n--- Comparison ---\n");
    if (compare_arrays(pub_openssl, public_key, 256) == 0) {
        printf("✅ PUBLIC KEYS MATCH!\n");
        return 0;
    } else {
        printf("❌ PUBLIC KEYS DO NOT MATCH!\n");
        printf("\nThis indicates a bug in dh_generate_keypair()\n");
        return -1;
    }
}

/* Test with medium-sized private keys */
void test_medium_keys() {
    uint8_t priv[256], pub_openssl[256], pub_custom[256];

    printf("\n\n==============================================\n");
    printf("Test 5: Medium-sized private key\n");
    printf("==============================================\n\n");

    /* Private key with data in first 4 bytes */
    memset(priv, 0, 256);
    priv[252] = 0x12;
    priv[253] = 0x34;
    priv[254] = 0x56;
    priv[255] = 0x78;

    printf("Testing with private key ending in 12345678...\n");
    
    test_with_openssl(priv, pub_openssl);
    test_custom_implementation(priv, pub_custom);
    
    if (compare_arrays(pub_openssl, pub_custom, 256) == 0) {
        printf("✅ Medium key MATCH!\n");
    } else {
        printf("❌ Medium key MISMATCH!\n");
    }
}
