/*
 * Minimal AES-128 Implementation
 * Optimized for size, not speed
 * Based on FIPS 197 (AES specification)
 *
 * Implements:
 * - AES-128 encryption (one block)
 * - Key expansion
 * - CTR mode
 */

#ifndef AES128_MINIMAL_H
#define AES128_MINIMAL_H

#include <stdint.h>
#include <string.h>

/* AES S-box (substitution box) - 256 bytes */
static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

/* Round constants for key expansion - 10 bytes (we only need first 10) */
static const uint8_t rcon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* Galois Field multiplication by 2 */
#define xtime(x) (((x) << 1) ^ (((x) & 0x80) ? 0x1b : 0x00))

/* AES context for CTR mode */
typedef struct {
    uint8_t round_keys[176];  /* 11 round keys × 16 bytes */
    uint8_t counter[16];      /* CTR mode counter */
} aes128_ctr_ctx;

/*
 * Key expansion for AES-128
 * Expands 128-bit key to 11 round keys (176 bytes)
 * Works entirely with bytes to avoid endianness issues
 */
static inline void aes128_key_expansion(const uint8_t *key, uint8_t *w) {
    int i;
    uint8_t temp[4];

    /* Copy original key (first 16 bytes) */
    for (i = 0; i < 16; i++) {
        w[i] = key[i];
    }

    /* Generate remaining bytes (16 to 175) */
    for (i = 16; i < 176; i += 4) {
        /* Copy previous word */
        temp[0] = w[i - 4];
        temp[1] = w[i - 3];
        temp[2] = w[i - 2];
        temp[3] = w[i - 1];

        /* Every 16 bytes (every 4th word), apply transformation */
        if (i % 16 == 0) {
            /* RotWord: rotate left by 1 byte */
            uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;

            /* SubWord: apply S-box to each byte */
            temp[0] = sbox[temp[0]];
            temp[1] = sbox[temp[1]];
            temp[2] = sbox[temp[2]];
            temp[3] = sbox[temp[3]];

            /* XOR with round constant */
            temp[0] ^= rcon[(i / 16) - 1];
        }

        /* XOR with word 16 bytes back */
        w[i]     = w[i - 16] ^ temp[0];
        w[i + 1] = w[i - 15] ^ temp[1];
        w[i + 2] = w[i - 14] ^ temp[2];
        w[i + 3] = w[i - 13] ^ temp[3];
    }
}

/*
 * AddRoundKey transformation
 */
static inline void add_round_key(uint8_t *state, const uint8_t *round_key) {
    for (int i = 0; i < 16; i++) {
        state[i] ^= round_key[i];
    }
}

/*
 * SubBytes transformation
 */
static inline void sub_bytes(uint8_t *state) {
    for (int i = 0; i < 16; i++) {
        state[i] = sbox[state[i]];
    }
}

/*
 * ShiftRows transformation
 * Row 0: no shift
 * Row 1: shift left by 1
 * Row 2: shift left by 2
 * Row 3: shift left by 3
 */
static inline void shift_rows(uint8_t *state) {
    uint8_t temp;

    /* Row 1 */
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    /* Row 2 */
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    /* Row 3 */
    temp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = temp;
}

/*
 * MixColumns transformation
 * Uses Galois Field arithmetic
 */
static inline void mix_columns(uint8_t *state) {
    uint8_t temp[16];

    for (int i = 0; i < 4; i++) {
        int col = i * 4;
        uint8_t s0 = state[col];
        uint8_t s1 = state[col + 1];
        uint8_t s2 = state[col + 2];
        uint8_t s3 = state[col + 3];

        temp[col]     = xtime(s0) ^ xtime(s1) ^ s1 ^ s2 ^ s3;
        temp[col + 1] = s0 ^ xtime(s1) ^ xtime(s2) ^ s2 ^ s3;
        temp[col + 2] = s0 ^ s1 ^ xtime(s2) ^ xtime(s3) ^ s3;
        temp[col + 3] = xtime(s0) ^ s0 ^ s1 ^ s2 ^ xtime(s3);
    }

    memcpy(state, temp, 16);
}

/*
 * AES-128 encryption (one block)
 * Encrypts 16-byte block using expanded round keys
 */
static inline void aes128_encrypt_block(const uint8_t *round_keys, uint8_t *block) {
    /* Initial round */
    add_round_key(block, round_keys);

    /* Main rounds (1-9) */
    for (int round = 1; round < 10; round++) {
        sub_bytes(block);
        shift_rows(block);
        mix_columns(block);
        add_round_key(block, round_keys + round * 16);
    }

    /* Final round (10) - no MixColumns */
    sub_bytes(block);
    shift_rows(block);
    add_round_key(block, round_keys + 10 * 16);
}

/*
 * Increment counter (big-endian)
 */
static inline void increment_counter(uint8_t *counter) {
    for (int i = 15; i >= 0; i--) {
        if (++counter[i] != 0) {
            break;
        }
    }
}

/*
 * Initialize AES-128-CTR context
 */
static inline void aes128_ctr_init(aes128_ctr_ctx *ctx,
                                    const uint8_t *key,
                                    const uint8_t *iv) {
    aes128_key_expansion(key, ctx->round_keys);
    memcpy(ctx->counter, iv, 16);
}

/*
 * AES-128-CTR encryption/decryption
 * CTR mode is symmetric: encrypt == decrypt
 *
 * Process:
 * 1. Encrypt counter with AES-128
 * 2. XOR result with plaintext/ciphertext
 * 3. Increment counter
 * 4. Repeat for each block
 */
static inline void aes128_ctr_crypt(aes128_ctr_ctx *ctx,
                                     uint8_t *data,
                                     size_t len) {
    uint8_t keystream[16];
    size_t i = 0;

    while (len > 0) {
        /* Generate keystream by encrypting counter */
        memcpy(keystream, ctx->counter, 16);
        aes128_encrypt_block(ctx->round_keys, keystream);

        /* XOR with data (up to 16 bytes) */
        size_t block_len = (len < 16) ? len : 16;
        for (size_t j = 0; j < block_len; j++) {
            data[i + j] ^= keystream[j];
        }

        /* Increment counter */
        increment_counter(ctx->counter);

        i += block_len;
        len -= block_len;
    }
}

#endif /* AES128_MINIMAL_H */
