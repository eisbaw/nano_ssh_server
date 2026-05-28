/*
 * chacha20poly1305_minimal.h
 *
 * Compact public-domain ChaCha20 + Poly1305, plus the OpenSSH
 * "chacha20-poly1305@openssh.com" AEAD framing.
 *
 * ChaCha20 based on D. J. Bernstein's reference / public-domain RFC 8439 layout.
 * Poly1305 based on Andrew Moon's poly1305-donna (public domain), 32-bit version.
 *
 * OpenSSH variant (see PROTOCOL.chacha20poly1305):
 *   - 64-byte key = K_2 (bytes 0..31) || K_1 (bytes 32..63).
 *   - K_1 encrypts the 4-byte packet length, ChaCha20 counter 0.
 *   - K_2 encrypts the payload, ChaCha20 counter starting at 1.
 *   - Poly1305 one-time key = ChaCha20(K_2, nonce, counter 0) first 32 bytes.
 *   - Poly1305 MAC is computed over the full ciphertext (enc_len || enc_payload).
 *   - nonce = big-endian uint64 packet sequence number (8 bytes).
 */

#ifndef CHACHA20POLY1305_MINIMAL_H
#define CHACHA20POLY1305_MINIMAL_H

#include <stdint.h>
#include <string.h>

/* ---------- ChaCha20 ---------- */

#define CC_ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

#define CC_QR(a, b, c, d)                 \
    a += b; d ^= a; d = CC_ROTL32(d, 16); \
    c += d; b ^= c; b = CC_ROTL32(b, 12); \
    a += b; d ^= a; d = CC_ROTL32(d, 8);  \
    c += d; b ^= c; b = CC_ROTL32(b, 7);

static void chacha20_block(uint32_t out[16], const uint32_t in[16]) {
    int i;
    for (i = 0; i < 16; i++) out[i] = in[i];
    for (i = 0; i < 10; i++) {
        CC_QR(out[0], out[4], out[8],  out[12]);
        CC_QR(out[1], out[5], out[9],  out[13]);
        CC_QR(out[2], out[6], out[10], out[14]);
        CC_QR(out[3], out[7], out[11], out[15]);
        CC_QR(out[0], out[5], out[10], out[15]);
        CC_QR(out[1], out[6], out[11], out[12]);
        CC_QR(out[2], out[7], out[8],  out[13]);
        CC_QR(out[3], out[4], out[9],  out[14]);
    }
    for (i = 0; i < 16; i++) out[i] += in[i];
}

static uint32_t cc_load32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*
 * ChaCha20 keystream XOR. nonce is 8 bytes (OpenSSH uses an 8-byte nonce,
 * the classic Bernstein layout: counter is 64-bit, nonce is 64-bit).
 * counter is the 64-bit initial block counter.
 */
static void chacha20_xor(uint8_t *data, size_t len,
                         const uint8_t key[32],
                         const uint8_t nonce[8],
                         uint64_t counter) {
    uint32_t state[16];
    uint32_t block[16];
    size_t i;

    /* "expand 32-byte k" */
    state[0] = 0x61707865; state[1] = 0x3320646e;
    state[2] = 0x79622d32; state[3] = 0x6b206574;
    for (i = 0; i < 8; i++) state[4 + i] = cc_load32(key + 4 * i);
    state[12] = (uint32_t)(counter & 0xffffffffu);
    state[13] = (uint32_t)(counter >> 32);
    state[14] = cc_load32(nonce);
    state[15] = cc_load32(nonce + 4);

    while (len > 0) {
        size_t n = len < 64 ? len : 64;
        uint8_t ks[64];
        chacha20_block(block, state);
        for (i = 0; i < 16; i++) {
            ks[4 * i + 0] = (uint8_t)(block[i]);
            ks[4 * i + 1] = (uint8_t)(block[i] >> 8);
            ks[4 * i + 2] = (uint8_t)(block[i] >> 16);
            ks[4 * i + 3] = (uint8_t)(block[i] >> 24);
        }
        for (i = 0; i < n; i++) data[i] ^= ks[i];
        data += n;
        len -= n;
        /* increment 64-bit counter */
        if (++state[12] == 0) state[13]++;
    }
}

/* Produce raw keystream bytes (data assumed zero-initialized externally
 * is not required; we just write keystream). */
static void chacha20_keystream(uint8_t *out, size_t len,
                               const uint8_t key[32],
                               const uint8_t nonce[8],
                               uint64_t counter) {
    memset(out, 0, len);
    chacha20_xor(out, len, key, nonce, counter);
}

/* ---------- Poly1305 (poly1305-donna 32-bit, public domain) ---------- */

typedef struct {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
    size_t leftover;
    uint8_t buffer[16];
    uint8_t final;
} poly1305_ctx;

static uint32_t pl_load32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void poly1305_init(poly1305_ctx *st, const uint8_t key[32]) {
    st->r[0] = (pl_load32(&key[0])) & 0x3ffffff;
    st->r[1] = (pl_load32(&key[3]) >> 2) & 0x3ffff03;
    st->r[2] = (pl_load32(&key[6]) >> 4) & 0x3ffc0ff;
    st->r[3] = (pl_load32(&key[9]) >> 6) & 0x3f03fff;
    st->r[4] = (pl_load32(&key[12]) >> 8) & 0x00fffff;

    st->h[0] = st->h[1] = st->h[2] = st->h[3] = st->h[4] = 0;

    st->pad[0] = pl_load32(&key[16]);
    st->pad[1] = pl_load32(&key[20]);
    st->pad[2] = pl_load32(&key[24]);
    st->pad[3] = pl_load32(&key[28]);

    st->leftover = 0;
    st->final = 0;
}

static void poly1305_blocks(poly1305_ctx *st, const uint8_t *m, size_t bytes) {
    const uint32_t hibit = (st->final) ? 0 : (1u << 24);
    uint32_t r0, r1, r2, r3, r4;
    uint32_t s1, s2, s3, s4;
    uint32_t h0, h1, h2, h3, h4;
    uint64_t d0, d1, d2, d3, d4;
    uint32_t c;

    r0 = st->r[0]; r1 = st->r[1]; r2 = st->r[2]; r3 = st->r[3]; r4 = st->r[4];
    s1 = r1 * 5; s2 = r2 * 5; s3 = r3 * 5; s4 = r4 * 5;
    h0 = st->h[0]; h1 = st->h[1]; h2 = st->h[2]; h3 = st->h[3]; h4 = st->h[4];

    while (bytes >= 16) {
        h0 += (pl_load32(m + 0)) & 0x3ffffff;
        h1 += (pl_load32(m + 3) >> 2) & 0x3ffffff;
        h2 += (pl_load32(m + 6) >> 4) & 0x3ffffff;
        h3 += (pl_load32(m + 9) >> 6) & 0x3ffffff;
        h4 += (pl_load32(m + 12) >> 8) | hibit;

        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5; c = (h0 >> 26); h0 &= 0x3ffffff;
        h1 += c;

        m += 16;
        bytes -= 16;
    }

    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2; st->h[3] = h3; st->h[4] = h4;
}

static void poly1305_update(poly1305_ctx *st, const uint8_t *m, size_t bytes) {
    size_t i;
    if (st->leftover) {
        size_t want = 16 - st->leftover;
        if (want > bytes) want = bytes;
        for (i = 0; i < want; i++) st->buffer[st->leftover + i] = m[i];
        bytes -= want;
        m += want;
        st->leftover += want;
        if (st->leftover < 16) return;
        poly1305_blocks(st, st->buffer, 16);
        st->leftover = 0;
    }
    if (bytes >= 16) {
        size_t want = bytes & ~((size_t)15);
        poly1305_blocks(st, m, want);
        m += want;
        bytes -= want;
    }
    for (i = 0; i < bytes; i++) st->buffer[st->leftover + i] = m[i];
    st->leftover += bytes;
}

static void poly1305_finish(poly1305_ctx *st, uint8_t mac[16]) {
    uint32_t h0, h1, h2, h3, h4, c;
    uint32_t g0, g1, g2, g3, g4;
    uint64_t f;
    uint32_t mask;

    if (st->leftover) {
        size_t i = st->leftover;
        st->buffer[i++] = 1;
        for (; i < 16; i++) st->buffer[i] = 0;
        st->final = 1;
        poly1305_blocks(st, st->buffer, 16);
    }

    h0 = st->h[0]; h1 = st->h[1]; h2 = st->h[2]; h3 = st->h[3]; h4 = st->h[4];

    c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    g4 = h4 + c - (1u << 26);

    mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    h0 = (h0) | (h1 << 26);
    h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 << 8);

    f = (uint64_t)h0 + st->pad[0]; h0 = (uint32_t)f;
    f = (uint64_t)h1 + st->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + st->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + st->pad[3] + (f >> 32); h3 = (uint32_t)f;

    mac[0] = (uint8_t)h0; mac[1] = (uint8_t)(h0 >> 8);
    mac[2] = (uint8_t)(h0 >> 16); mac[3] = (uint8_t)(h0 >> 24);
    mac[4] = (uint8_t)h1; mac[5] = (uint8_t)(h1 >> 8);
    mac[6] = (uint8_t)(h1 >> 16); mac[7] = (uint8_t)(h1 >> 24);
    mac[8] = (uint8_t)h2; mac[9] = (uint8_t)(h2 >> 8);
    mac[10] = (uint8_t)(h2 >> 16); mac[11] = (uint8_t)(h2 >> 24);
    mac[12] = (uint8_t)h3; mac[13] = (uint8_t)(h3 >> 8);
    mac[14] = (uint8_t)(h3 >> 16); mac[15] = (uint8_t)(h3 >> 24);
}

static void poly1305_auth(uint8_t mac[16], const uint8_t *m, size_t bytes,
                          const uint8_t key[32]) {
    poly1305_ctx st;
    poly1305_init(&st, key);
    poly1305_update(&st, m, bytes);
    poly1305_finish(&st, mac);
}

/* ---------- OpenSSH chacha20-poly1305@openssh.com framing ---------- */

/*
 * key64: 64-byte key. key64[0..31] = K_2 (payload), key64[32..63] = K_1 (length).
 * seq:   packet sequence number (nonce = big-endian uint64).
 */
static void ssh_chacha_nonce(uint8_t nonce[8], uint64_t seq) {
    nonce[0] = (uint8_t)(seq >> 56);
    nonce[1] = (uint8_t)(seq >> 48);
    nonce[2] = (uint8_t)(seq >> 40);
    nonce[3] = (uint8_t)(seq >> 32);
    nonce[4] = (uint8_t)(seq >> 24);
    nonce[5] = (uint8_t)(seq >> 16);
    nonce[6] = (uint8_t)(seq >> 8);
    nonce[7] = (uint8_t)(seq);
}

/* Encrypt/decrypt the 4-byte length field in place using K_1 (key64+32). */
static void ssh_chacha_length_crypt(uint8_t *len4, const uint8_t key64[64], uint64_t seq) {
    uint8_t nonce[8];
    ssh_chacha_nonce(nonce, seq);
    chacha20_xor(len4, 4, key64 + 32, nonce, 0);
}

/*
 * Seal a packet for sending.
 *   buf layout on entry: [plain_len(4)][plaintext payload ...]
 *   ct_len = 4 + payload length (the full ciphertext length, NOT incl MAC).
 * On return:
 *   buf holds [enc_len(4)][enc_payload ...]; mac[16] is the Poly1305 tag.
 */
static void ssh_chacha_poly_seal(uint8_t *buf, size_t ct_len,
                                 const uint8_t key64[64], uint64_t seq,
                                 uint8_t mac[16]) {
    uint8_t nonce[8];
    uint8_t poly_key[32];
    ssh_chacha_nonce(nonce, seq);
    /* one-time Poly1305 key = ChaCha20(K_2, nonce, counter 0)[0..31] */
    chacha20_keystream(poly_key, 32, key64, nonce, 0);
    /* encrypt 4-byte length with K_1, counter 0 */
    ssh_chacha_length_crypt(buf, key64, seq);
    /* encrypt payload with K_2, counter 1 */
    if (ct_len > 4) chacha20_xor(buf + 4, ct_len - 4, key64, nonce, 1);
    /* MAC over the full ciphertext (enc_len || enc_payload) */
    poly1305_auth(mac, buf, ct_len, poly_key);
}

/*
 * Decrypt the 4-byte length field only. Returns the plaintext length value.
 * Does NOT advance any state; the encrypted length is in enc_len4 (not modified).
 */
static uint32_t ssh_chacha_decrypt_length(const uint8_t enc_len4[4],
                                          const uint8_t key64[64], uint64_t seq) {
    uint8_t tmp[4];
    memcpy(tmp, enc_len4, 4);
    ssh_chacha_length_crypt(tmp, key64, seq);
    return ((uint32_t)tmp[0] << 24) | ((uint32_t)tmp[1] << 16) |
           ((uint32_t)tmp[2] << 8) | (uint32_t)tmp[3];
}

/*
 * Verify MAC and decrypt payload for a received packet.
 *   ct: [enc_len(4)][enc_payload ...], ct_len = 4 + payload length.
 *   mac: received 16-byte tag.
 * Returns 0 on success (ct payload decrypted in place), -1 on MAC failure.
 */
static int ssh_chacha_poly_open(uint8_t *ct, size_t ct_len,
                                const uint8_t key64[64], uint64_t seq,
                                const uint8_t mac[16]) {
    uint8_t nonce[8];
    uint8_t poly_key[32];
    uint8_t expected[16];
    int i, diff = 0;
    ssh_chacha_nonce(nonce, seq);
    chacha20_keystream(poly_key, 32, key64, nonce, 0);
    /* MAC is computed over the ciphertext as received */
    poly1305_auth(expected, ct, ct_len, poly_key);
    for (i = 0; i < 16; i++) diff |= expected[i] ^ mac[i];
    if (diff != 0) return -1;
    /* decrypt length in place */
    ssh_chacha_length_crypt(ct, key64, seq);
    /* decrypt payload */
    if (ct_len > 4) chacha20_xor(ct + 4, ct_len - 4, key64, nonce, 1);
    return 0;
}

#endif /* CHACHA20POLY1305_MINIMAL_H */
