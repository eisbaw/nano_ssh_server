/*
 * Minimal SHA-256 implementation
 * Optimized for size, not speed
 * Based on FIPS 180-4
 */

#ifndef SHA256_MINIMAL_H
#define SHA256_MINIMAL_H

#include <stdint.h>
#include <string.h>

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint8_t buffer[SHA256_BLOCK_SIZE];
    uint64_t bitlen;
    uint32_t buflen;
} sha256_ctx;

/* SHA-256 constants K */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* Rotate right */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* SHA-256 functions */
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static inline void sha256_transform(sha256_ctx *ctx) {
    uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;
    uint8_t *p = ctx->buffer;

    /* Prepare message schedule */
    for (int i = 0; i < 16; i++, p += 4) {
        m[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | p[3];
    }
    for (int i = 16; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    /* Initialize working variables */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    /* Compression function */
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Add compressed chunk to current hash value */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static inline void sha256_init(sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitlen = 0;
    ctx->buflen = 0;
}

static inline void sha256_update(sha256_ctx *ctx, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx);
            ctx->bitlen += 512;
            ctx->buflen = 0;
        }
    }
}

static inline void sha256_final(sha256_ctx *ctx, uint8_t *hash) {
    uint32_t i = ctx->buflen;

    /* Pad with 0x80 followed by zeros */
    ctx->buffer[i++] = 0x80;

    /* Pad to 56 bytes (leaving 8 bytes for length) */
    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0;
        sha256_transform(ctx);
        i = 0;
    }
    while (i < 56) ctx->buffer[i++] = 0;

    /* Append bit length */
    ctx->bitlen += ctx->buflen * 8;
    for (int j = 7; j >= 0; j--) {
        ctx->buffer[56 + j] = ctx->bitlen >> (8 * (7 - j));
    }
    sha256_transform(ctx);

    /* Produce final hash */
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (ctx->state[i] >> 24) & 0xff;
        hash[i * 4 + 1] = (ctx->state[i] >> 16) & 0xff;
        hash[i * 4 + 2] = (ctx->state[i] >> 8) & 0xff;
        hash[i * 4 + 3] = ctx->state[i] & 0xff;
    }
}

/* One-shot hash function */
static inline void sha256(uint8_t *hash, const uint8_t *data, uint32_t len) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

/* HMAC-SHA256 context */
typedef struct {
    sha256_ctx inner;
    sha256_ctx outer;
} hmac_sha256_ctx;

/* HMAC-SHA256 functions */
static inline void hmac_sha256_init(hmac_sha256_ctx *ctx, const uint8_t *key, uint32_t key_len) {
    uint8_t k_ipad[SHA256_BLOCK_SIZE];
    uint8_t k_opad[SHA256_BLOCK_SIZE];
    uint8_t key_hash[SHA256_DIGEST_SIZE];
    const uint8_t *key_ptr = key;
    uint32_t klen = key_len;

    /* If key is longer than block size, hash it */
    if (klen > SHA256_BLOCK_SIZE) {
        sha256(key_hash, key, klen);
        key_ptr = key_hash;
        klen = SHA256_DIGEST_SIZE;
    }

    /* Prepare padded key */
    memset(k_ipad, 0x36, SHA256_BLOCK_SIZE);
    memset(k_opad, 0x5c, SHA256_BLOCK_SIZE);

    for (uint32_t i = 0; i < klen; i++) {
        k_ipad[i] ^= key_ptr[i];
        k_opad[i] ^= key_ptr[i];
    }

    /* Initialize inner and outer hash contexts */
    sha256_init(&ctx->inner);
    sha256_update(&ctx->inner, k_ipad, SHA256_BLOCK_SIZE);

    sha256_init(&ctx->outer);
    sha256_update(&ctx->outer, k_opad, SHA256_BLOCK_SIZE);
}

static inline void hmac_sha256_update(hmac_sha256_ctx *ctx, const uint8_t *data, uint32_t len) {
    sha256_update(&ctx->inner, data, len);
}

static inline void hmac_sha256_final(hmac_sha256_ctx *ctx, uint8_t *mac) {
    uint8_t inner_hash[SHA256_DIGEST_SIZE];

    /* Finalize inner hash */
    sha256_final(&ctx->inner, inner_hash);

    /* Add inner hash to outer and finalize */
    sha256_update(&ctx->outer, inner_hash, SHA256_DIGEST_SIZE);
    sha256_final(&ctx->outer, mac);
}

/* Constant-time comparison (timing-attack resistant)
 * Returns 0 if equal, -1 if different (same as crypto_verify_32 from libsodium)
 */
static inline int ct_verify_32(const uint8_t *x, const uint8_t *y) {
    uint8_t d = 0;
    for (int i = 0; i < 32; i++) {
        d |= x[i] ^ y[i];
    }
    return (1 & ((d - 1) >> 8)) - 1;
}

#endif /* SHA256_MINIMAL_H */
