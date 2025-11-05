/*
 * Minimal AES-128 implementation for CTR mode
 * Optimized for size, not speed
 * Based on FIPS-197
 */

#ifndef AES_MINIMAL_H
#define AES_MINIMAL_H

#include <stdint.h>
#include <string.h>

/* AES S-box */
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

/* Round constant for key expansion */
static const uint8_t rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* AES-128 context */
typedef struct {
    uint8_t round_key[176];  /* 11 round keys * 16 bytes */
    uint8_t iv[16];          /* Current IV/counter */
} aes128_ctx;

/* Rotate word */
#define ROTL8(x) (((x) << 8) | ((x) >> 24))

/* Key expansion for AES-128 */
static inline void aes128_key_expansion(uint8_t *round_key, const uint8_t *key) {
    uint32_t *rk = (uint32_t *)round_key;
    const uint32_t *k = (const uint32_t *)key;

    /* Copy original key */
    rk[0] = k[0];
    rk[1] = k[1];
    rk[2] = k[2];
    rk[3] = k[3];

    /* Generate round keys */
    for (int i = 4; i < 44; i++) {
        uint32_t temp = rk[i - 1];

        if (i % 4 == 0) {
            /* Rotate word */
            temp = ROTL8(temp);

            /* SubWord */
            temp = (sbox[(temp >> 24) & 0xff] << 24) |
                   (sbox[(temp >> 16) & 0xff] << 16) |
                   (sbox[(temp >> 8) & 0xff] << 8) |
                   sbox[temp & 0xff];

            /* XOR with Rcon */
            temp ^= rcon[i / 4] << 24;
        }

        rk[i] = rk[i - 4] ^ temp;
    }
}

/* SubBytes transformation */
static inline void sub_bytes(uint8_t *state) {
    for (int i = 0; i < 16; i++) {
        state[i] = sbox[state[i]];
    }
}

/* ShiftRows transformation */
static inline void shift_rows(uint8_t *state) {
    uint8_t temp;

    /* Row 1: shift left by 1 */
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    /* Row 2: shift left by 2 */
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    /* Row 3: shift left by 3 */
    temp = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temp;
}

/* Galois field multiplication */
#define xtime(x) ((x << 1) ^ (((x >> 7) & 1) * 0x1b))

/* MixColumns transformation */
static inline void mix_columns(uint8_t *state) {
    uint8_t temp[4];

    for (int i = 0; i < 4; i++) {
        temp[0] = state[i * 4];
        temp[1] = state[i * 4 + 1];
        temp[2] = state[i * 4 + 2];
        temp[3] = state[i * 4 + 3];

        state[i * 4] = xtime(temp[0]) ^ xtime(temp[1]) ^ temp[1] ^ temp[2] ^ temp[3];
        state[i * 4 + 1] = temp[0] ^ xtime(temp[1]) ^ xtime(temp[2]) ^ temp[2] ^ temp[3];
        state[i * 4 + 2] = temp[0] ^ temp[1] ^ xtime(temp[2]) ^ xtime(temp[3]) ^ temp[3];
        state[i * 4 + 3] = xtime(temp[0]) ^ temp[0] ^ temp[1] ^ temp[2] ^ xtime(temp[3]);
    }
}

/* AddRoundKey transformation */
static inline void add_round_key(uint8_t *state, const uint8_t *round_key) {
    for (int i = 0; i < 16; i++) {
        state[i] ^= round_key[i];
    }
}

/* AES-128 block encryption */
static inline void aes128_encrypt_block(uint8_t *block, const uint8_t *round_key) {
    /* Initial round */
    add_round_key(block, round_key);

    /* 9 main rounds */
    for (int round = 1; round < 10; round++) {
        sub_bytes(block);
        shift_rows(block);
        mix_columns(block);
        add_round_key(block, round_key + round * 16);
    }

    /* Final round (no MixColumns) */
    sub_bytes(block);
    shift_rows(block);
    add_round_key(block, round_key + 160);
}

/* Initialize AES-128 CTR context */
static inline void aes128_ctr_init(aes128_ctx *ctx, const uint8_t *key, const uint8_t *iv) {
    aes128_key_expansion(ctx->round_key, key);
    memcpy(ctx->iv, iv, 16);
}

/* Increment counter (big-endian) */
static inline void increment_counter(uint8_t *counter) {
    for (int i = 15; i >= 0; i--) {
        if (++counter[i] != 0) break;
    }
}

/* AES-128 CTR mode encryption/decryption (same operation) */
static inline void aes128_ctr_xor(aes128_ctx *ctx, uint8_t *data, uint32_t len) {
    uint8_t keystream[16];

    for (uint32_t i = 0; i < len; i++) {
        /* Generate keystream block when needed */
        if (i % 16 == 0) {
            memcpy(keystream, ctx->iv, 16);
            aes128_encrypt_block(keystream, ctx->round_key);
            increment_counter(ctx->iv);
        }

        /* XOR with keystream */
        data[i] ^= keystream[i % 16];
    }
}

#endif /* AES_MINIMAL_H */
