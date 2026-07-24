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

/* Galois Field multiplication by 2 */
#define xtime(x) (((x) << 1) ^ (((x) & 0x80) ? 0x1b : 0x00))

/* AES context for CTR mode */
typedef struct {
    uint8_t round_keys[176];  /* 11 round keys x 16 bytes */
    uint8_t counter[16];      /* CTR mode counter */
} aes128_ctr_ctx;

/*
 * Key expansion for AES-128: 128-bit key -> 11 round keys (176 bytes).
 * Byte-oriented, so no endianness concerns. The round constants are
 * generated on the fly (1, 2, 4, ... 0x80, 0x1b, 0x36 is exactly the xtime
 * chain) rather than stored as a 10-byte table.
 */
static inline void aes128_key_expansion(const uint8_t *key, uint8_t *w) {
    uint8_t rc = 1;

    memcpy(w, key, 16);

    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4];

        for (int j = 0; j < 4; j++)
            t[j] = w[i - 4 + j];

        /* Every 4th word: RotWord, SubWord, XOR round constant */
        if (i % 16 == 0) {
            uint8_t first = aes_sbox(t[0]);

            for (int j = 0; j < 3; j++)
                t[j] = aes_sbox(t[j + 1]);
            t[3] = first;
            t[0] ^= rc;
            rc = (uint8_t)xtime(rc);
        }

        for (int j = 0; j < 4; j++)
            w[i + j] = w[i - 16 + j] ^ t[j];
    }
}

/*
 * One AES round: SubBytes + ShiftRows (fused into one gather through the
 * permutation table), MixColumns (skipped in the final round), AddRoundKey.
 * The fused table costs 16 bytes and replaces the unrolled row rotations.
 */
static inline void aes_round(uint8_t *state, const uint8_t *round_key, int last) {
    static const uint8_t shift[16] = {
        0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12, 1, 6, 11
    };
    uint8_t t[16];

    for (int i = 0; i < 16; i++)
        t[i] = aes_sbox(state[shift[i]]);

    if (!last) {
        /* Per column: c'_j = c_j ^ (c0^c1^c2^c3) ^ xtime(c_j ^ c_{j+1}) */
        for (int i = 0; i < 16; i += 4) {
            uint8_t *c = t + i;
            uint8_t all = c[0] ^ c[1] ^ c[2] ^ c[3];
            uint8_t c0 = c[0];

            c[0] ^= all ^ (uint8_t)xtime(c[0] ^ c[1]);
            c[1] ^= all ^ (uint8_t)xtime(c[1] ^ c[2]);
            c[2] ^= all ^ (uint8_t)xtime(c[2] ^ c[3]);
            c[3] ^= all ^ (uint8_t)xtime(c[3] ^ c0);
        }
    }

    for (int i = 0; i < 16; i++)
        state[i] = t[i] ^ round_key[i];
}

/*
 * AES-128 encryption of one 16-byte block with expanded round keys.
 */
static inline void aes128_encrypt_block(const uint8_t *round_keys, uint8_t *block) {
    for (int i = 0; i < 16; i++)
        block[i] ^= round_keys[i];

    for (int round = 1; round <= 10; round++)
        aes_round(block, round_keys + round * 16, round == 10);
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
 * AES-128-CTR encryption/decryption (CTR is symmetric): XOR the data with
 * AES(counter), incrementing the big-endian counter every 16 bytes.
 */
static inline void aes128_ctr_crypt(aes128_ctr_ctx *ctx,
                                     uint8_t *data,
                                     size_t len) {
    uint8_t keystream[16];
    size_t i = 0;

    while (len > 0) {
        size_t block_len = (len < 16) ? len : 16;
        int j;

        memcpy(keystream, ctx->counter, 16);
        aes128_encrypt_block(ctx->round_keys, keystream);

        for (size_t k = 0; k < block_len; k++)
            data[i + k] ^= keystream[k];

        for (j = 15; j >= 0 && ++ctx->counter[j] == 0; j--)
            ;

        i += block_len;
        len -= block_len;
    }
}

#endif /* AES128_MINIMAL_H */
