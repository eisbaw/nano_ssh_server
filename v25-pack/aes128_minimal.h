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
#include "nolibc.h"

/* AES S-box computed in GF(2^8) instead of a 256-byte table.
 * S(a) = affine(a^-1); a^-1 = a^254 (multiplicative group order 255), with
 * 0 -> 0 falling out naturally. Trades ~190 bytes of table for ~70 bytes of
 * code; slower, but the server only encrypts a few packets per session. */
static uint8_t aes_gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    while (b) {
        if (b & 1) p ^= a;
        uint8_t hi = a & 0x80;
        a = (uint8_t)(a << 1);
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}
static uint8_t aes_sbox(uint8_t a) {
    uint8_t inv = 1;
    for (int i = 0; i < 254; i++) inv = aes_gmul(inv, a);  /* inv = a^254 = a^-1 */
    uint8_t s = inv;
    for (int i = 0; i < 4; i++) { inv = (uint8_t)((inv << 1) | (inv >> 7)); s ^= inv; }
    return (uint8_t)(s ^ 0x63);
}

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
            temp[0] = aes_sbox(temp[0]);
            temp[1] = aes_sbox(temp[1]);
            temp[2] = aes_sbox(temp[2]);
            temp[3] = aes_sbox(temp[3]);

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
        state[i] = aes_sbox(state[i]);
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
