# Cryptography Implementation Notes

## Overview

This document provides implementation guidance for the cryptographic primitives needed for a minimal SSH server, with focus on code size optimization.

## Required Cryptographic Operations

1. **Random Number Generation** - For padding, cookies, key generation
2. **Key Exchange** - Curve25519 ECDH
3. **Host Key** - Ed25519 signing
4. **Hashing** - SHA-256 for key derivation and exchange hash
5. **Symmetric Encryption** - ChaCha20-Poly1305 AEAD or AES-128-CTR
6. **MAC** - Poly1305 or HMAC-SHA256

## Library Options (Size Comparison)

### Option 1: TweetNaCl (RECOMMENDED for size)

**Size:** ~100KB source, compiles to ~15-20KB object code

**Provides:**
- Curve25519 (crypto_scalarmult)
- Ed25519 (crypto_sign)
- Salsa20 (can adapt to ChaCha20)
- Poly1305 (crypto_onetimeauth)
- SHA-512 (can use for key derivation)

**Pros:**
- Very small code size
- Audited, secure
- No dependencies
- Public domain

**Cons:**
- No ChaCha20 (has Salsa20, similar)
- No SHA-256 (has SHA-512, which is fine)
- Fixed API (less flexible)

**Code:** https://tweetnacl.cr.yp.to/

### Option 2: Monocypher (ALSO SMALL)

**Size:** ~3000 lines, compiles to ~10-15KB

**Provides:**
- Curve25519 (X25519)
- Ed25519
- ChaCha20
- Poly1305
- Blake2b (can use instead of SHA-256)

**Pros:**
- Extremely small
- Modern API
- Well-documented
- CC0 / BSD license

**Cons:**
- No SHA-256 (uses Blake2b)
- May need to adapt for SSH compatibility

**Code:** https://monocypher.org/

### Option 3: BearSSL

**Size:** Medium (~100KB compiled)

**Provides:**
- Complete crypto suite
- Multiple ciphers
- Optimized for embedded

**Pros:**
- Feature-complete
- Good documentation
- Designed for embedded

**Cons:**
- Larger than TweetNaCl/Monocypher
- More complex API

### Option 4: libsodium

**Size:** Large (~300KB+)

**Pros:**
- Complete, modern API
- Well-supported
- Fast

**Cons:**
- Too large for minimal implementation
- Many features we don't need

### Option 5: Custom/Minimal Implementation

Write only what you need:
- Curve25519 (~500 lines)
- Ed25519 (~800 lines)
- ChaCha20 (~200 lines)
- Poly1305 (~150 lines)
- SHA-256 (~300 lines)

**Total:** ~2000 lines, could be < 10KB compiled

**Pros:**
- Absolute minimum size
- Full control

**Cons:**
- Security risk (easy to get crypto wrong)
- Time-consuming
- Needs review

## Recommended Approach

**For v0-vanilla:** Use TweetNaCl or Monocypher (proven, small)

**For v3+ optimized:** Consider custom minimal implementation or stripped-down TweetNaCl

## Curve25519 Implementation

### Using TweetNaCl

```c
#include "tweetnacl.h"

// Generate key pair
uint8_t server_private[32];
uint8_t server_public[32];

randombytes(server_private, 32);
crypto_scalarmult_base(server_public, server_private);

// Compute shared secret from client's public key
uint8_t client_public[32];  // received from client
uint8_t shared_secret[32];

crypto_scalarmult(shared_secret, server_private, client_public);
```

### Manual Implementation (if needed)

Curve25519 can be implemented in ~500 lines. Key function:

```c
void curve25519(uint8_t out[32], const uint8_t scalar[32],
                const uint8_t point[32]);
```

Reference implementation: https://cr.yp.to/ecdh.html

## Ed25519 Implementation

### Using TweetNaCl

```c
#include "tweetnacl.h"

// Generate host key (do once, save to file/flash)
uint8_t public_key[32];
uint8_t secret_key[64];  // includes public key

crypto_sign_keypair(public_key, secret_key);

// Sign exchange hash H
uint8_t signature[64];
uint8_t exchange_hash[32];
unsigned long long sig_len;

crypto_sign_detached(signature, &sig_len, exchange_hash, 32, secret_key);
```

### Host Key Storage

For microcontroller, embed in firmware:

```c
// Generated once during setup
static const uint8_t HOST_PUBLIC_KEY[32] = {
    0x1a, 0x2b, 0x3c, /* ... 32 bytes ... */
};

static const uint8_t HOST_SECRET_KEY[64] = {
    0x4d, 0x5e, 0x6f, /* ... 64 bytes ... */
};
```

## SHA-256 Implementation

### Minimal Implementation

SHA-256 in ~300 lines. Needed for:
- Exchange hash H
- Key derivation

```c
void sha256(uint8_t hash[32], const uint8_t *data, size_t len);

// For multi-part hashing (exchange hash)
typedef struct {
    uint32_t state[8];
    uint8_t buffer[64];
    uint64_t bitlen;
    size_t buflen;
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx *ctx, uint8_t hash[32]);
```

Reference: https://github.com/B-Con/crypto-algorithms/blob/master/sha256.c

### Using with SSH

Exchange hash H computation:
```c
sha256_ctx ctx;
sha256_init(&ctx);

// Add each component with length prefix
write_string_to_hash(&ctx, client_version);
write_string_to_hash(&ctx, server_version);
write_string_to_hash(&ctx, client_kexinit, client_kexinit_len);
write_string_to_hash(&ctx, server_kexinit, server_kexinit_len);
write_string_to_hash(&ctx, server_host_key, 32);
write_string_to_hash(&ctx, client_ephemeral, 32);
write_string_to_hash(&ctx, server_ephemeral, 32);
write_mpint_to_hash(&ctx, shared_secret, 32);

uint8_t exchange_hash[32];
sha256_final(&ctx, exchange_hash);
```

Helper function:
```c
void write_string_to_hash(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    uint8_t len_buf[4];
    write_uint32_be(len_buf, len);
    sha256_update(ctx, len_buf, 4);
    sha256_update(ctx, data, len);
}
```

## ChaCha20-Poly1305 Implementation

### ChaCha20 Stream Cipher

Very simple, ~200 lines:

```c
typedef struct {
    uint32_t state[16];
    uint8_t keystream[64];
    size_t position;
} chacha20_ctx;

void chacha20_init(chacha20_ctx *ctx, const uint8_t key[32],
                   const uint8_t nonce[12], uint32_t counter);

void chacha20_xor(chacha20_ctx *ctx, uint8_t *data, size_t len);
```

Core operation:
```c
void chacha20_block(uint32_t out[16], const uint32_t in[16]) {
    // 20 rounds of quarter-round operations
    // See RFC 7539
}
```

### Poly1305 MAC

~150 lines:

```c
typedef struct {
    uint32_t r[4];
    uint32_t h[5];
    uint32_t pad[4];
    uint8_t buf[16];
    size_t buf_used;
} poly1305_ctx;

void poly1305_init(poly1305_ctx *ctx, const uint8_t key[32]);
void poly1305_update(poly1305_ctx *ctx, const uint8_t *data, size_t len);
void poly1305_final(poly1305_ctx *ctx, uint8_t mac[16]);
```

### ChaCha20-Poly1305 AEAD (OpenSSH variant)

SSH uses a special variant:

```c
// Two separate ChaCha20 instances with same key, different nonce/counter
void chacha20_poly1305_encrypt(uint8_t *packet, size_t packet_len,
                               const uint8_t key[32],
                               uint32_t sequence_number,
                               uint8_t mac[16])
{
    // Derive two keys from main key using ChaCha20
    uint8_t poly_key[64];
    uint8_t nonce[12] = {0};
    write_uint32_be(nonce + 8, sequence_number);

    // K_1 for packet length
    chacha20_ctx ctx1;
    chacha20_init(&ctx1, key, nonce, 0);

    // K_2 for packet payload
    chacha20_ctx ctx2;
    chacha20_init(&ctx2, key, nonce, 1);

    // Generate Poly1305 key
    chacha20_init(&ctx2, key, nonce, 0);
    chacha20_xor(&ctx2, poly_key, 64);

    // Encrypt packet length (first 4 bytes)
    chacha20_xor(&ctx1, packet, 4);

    // Encrypt rest of packet
    chacha20_xor(&ctx2, packet + 4, packet_len - 4);

    // Compute Poly1305 MAC
    poly1305_ctx poly;
    poly1305_init(&poly, poly_key);
    poly1305_update(&poly, packet, packet_len);
    poly1305_final(&poly, mac);
}
```

**Note:** OpenSSH's ChaCha20-Poly1305 is slightly different from RFC 7539. Follow OpenSSH specification.

## Alternative: AES-128-CTR + HMAC-SHA256

Simpler to implement, might be smaller depending on platform:

### AES-128

Can use tiny-AES-c: https://github.com/kokke/tiny-AES-c
- ~500 lines
- ~2KB compiled
- Public domain

```c
#include "aes.h"

// AES-CTR mode
void aes_ctr_encrypt(uint8_t *data, size_t len,
                    const uint8_t key[16],
                    uint8_t iv[16])
{
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CTR_xcrypt_buffer(&ctx, data, len);
}
```

### HMAC-SHA256

```c
void hmac_sha256(uint8_t mac[32], const uint8_t *key, size_t key_len,
                const uint8_t *data, size_t data_len)
{
    uint8_t ipad[64], opad[64];
    memset(ipad, 0x36, 64);
    memset(opad, 0x5c, 64);

    for (size_t i = 0; i < key_len && i < 64; i++) {
        ipad[i] ^= key[i];
        opad[i] ^= key[i];
    }

    sha256_ctx ctx;
    uint8_t inner_hash[32];

    // Inner hash
    sha256_init(&ctx);
    sha256_update(&ctx, ipad, 64);
    sha256_update(&ctx, data, data_len);
    sha256_final(&ctx, inner_hash);

    // Outer hash
    sha256_init(&ctx);
    sha256_update(&ctx, opad, 64);
    sha256_update(&ctx, inner_hash, 32);
    sha256_final(&ctx, mac);
}
```

### SSH Packet MAC

```c
// MAC = HMAC-SHA256(sequence_number || unencrypted_packet)
void compute_packet_mac(uint8_t mac[32], uint32_t seq,
                       const uint8_t *packet, size_t len,
                       const uint8_t key[32])
{
    uint8_t seq_buf[4];
    write_uint32_be(seq_buf, seq);

    // Combine sequence number and packet
    uint8_t *combined = malloc(4 + len);
    memcpy(combined, seq_buf, 4);
    memcpy(combined + 4, packet, len);

    hmac_sha256(mac, key, 32, combined, 4 + len);
    free(combined);
}
```

Or use static buffer:
```c
void compute_packet_mac(uint8_t mac[32], uint32_t seq,
                       const uint8_t *packet, size_t len,
                       const uint8_t key[32])
{
    sha256_ctx inner, outer;
    uint8_t ipad[64], opad[64];

    // Prepare HMAC pads
    memset(ipad, 0x36, 64);
    memset(opad, 0x5c, 64);
    for (int i = 0; i < 32; i++) {
        ipad[i] ^= key[i];
        opad[i] ^= key[i];
    }

    // Inner hash
    sha256_init(&inner);
    sha256_update(&inner, ipad, 64);

    uint8_t seq_buf[4];
    write_uint32_be(seq_buf, seq);
    sha256_update(&inner, seq_buf, 4);
    sha256_update(&inner, packet, len);

    uint8_t inner_hash[32];
    sha256_final(&inner, inner_hash);

    // Outer hash
    sha256_init(&outer);
    sha256_update(&outer, opad, 64);
    sha256_update(&outer, inner_hash, 32);
    sha256_final(&outer, mac);
}
```

## Random Number Generation

### Linux/POSIX

```c
void randombytes(uint8_t *buf, size_t len) {
    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, buf, len);
    close(fd);
}
```

### Microcontroller

Use hardware RNG if available:

```c
// ESP32 example
void randombytes(uint8_t *buf, size_t len) {
    esp_fill_random(buf, len);
}

// STM32 example
void randombytes(uint8_t *buf, size_t len) {
    HAL_RNG_GenerateRandomNumber(&hrng, (uint32_t*)buf);
}
```

## Key Derivation

```c
void derive_key(uint8_t *out, size_t out_len,
               const uint8_t *K, size_t K_len,
               const uint8_t *H,
               char letter,
               const uint8_t *session_id)
{
    sha256_ctx ctx;
    uint8_t hash[32];

    // First hash: HASH(K || H || letter || session_id)
    sha256_init(&ctx);
    write_mpint_to_hash(&ctx, K, K_len);  // shared secret as mpint
    sha256_update(&ctx, H, 32);           // exchange hash
    sha256_update(&ctx, &letter, 1);      // 'A', 'B', 'C', etc.
    sha256_update(&ctx, session_id, 32);
    sha256_final(&ctx, hash);

    // Copy first block
    size_t copied = 0;
    size_t to_copy = (out_len < 32) ? out_len : 32;
    memcpy(out, hash, to_copy);
    copied += to_copy;

    // If need more, extend: K2 = HASH(K || H || K1)
    while (copied < out_len) {
        sha256_init(&ctx);
        write_mpint_to_hash(&ctx, K, K_len);
        sha256_update(&ctx, H, 32);
        sha256_update(&ctx, out, copied);  // all previous key material
        sha256_final(&ctx, hash);

        to_copy = ((out_len - copied) < 32) ? (out_len - copied) : 32;
        memcpy(out + copied, hash, to_copy);
        copied += to_copy;
    }
}
```

## Size Comparison (Estimated)

### Full implementations:

| Component | TweetNaCl | Monocypher | Custom | tiny-AES |
|-----------|-----------|------------|--------|----------|
| Curve25519 | ✓ | ✓ | ~500 lines | N/A |
| Ed25519 | ✓ | ✓ | ~800 lines | N/A |
| ChaCha20 | ~Salsa20 | ✓ | ~200 lines | N/A |
| Poly1305 | ✓ | ✓ | ~150 lines | N/A |
| SHA-256 | SHA-512 | Blake2b | ~300 lines | N/A |
| AES-128 | ✗ | ✗ | N/A | ~500 lines |
| **Total size** | ~20KB | ~15KB | ~10KB | ~5KB (AES+SHA only) |

### Recommended combinations:

**Option A (most secure, easiest):**
- TweetNaCl for all crypto
- Add SHA-256 implementation (~2KB)
- **Total: ~22KB**

**Option B (smallest, modern):**
- Monocypher
- Adapt Blake2b for SSH (may need SHA-256 anyway)
- **Total: ~17KB**

**Option C (custom minimal):**
- Custom Curve25519
- Custom Ed25519
- Custom ChaCha20-Poly1305
- Custom SHA-256
- **Total: ~10KB**
- **Risk: Crypto is hard to get right!**

**Option D (AES alternative):**
- tiny-AES-c for AES-128-CTR
- Custom SHA-256 + HMAC
- Still need Curve25519 + Ed25519 (add ~5KB)
- **Total: ~12KB**

## Recommendation

1. **v0-vanilla:** Use TweetNaCl + SHA-256. Well-tested, reasonably small.

2. **v1-portable:** Same, ensure it compiles for microcontroller.

3. **v3-opt2:** Compare TweetNaCl vs Monocypher vs tiny-AES approach.

4. **v6-opt5:** Consider minimal custom crypto (with careful review).

## Testing Crypto Implementation

```c
// Test vectors for Curve25519
uint8_t scalar[32] = {
    0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d,
    // ... test vector from RFC 7748
};
uint8_t basepoint[32] = {9};  // standard base point
uint8_t expected[32] = { /* ... expected result ... */ };

uint8_t result[32];
curve25519(result, scalar, basepoint);
assert(memcmp(result, expected, 32) == 0);
```

Always test with known vectors before using in SSH implementation!

## Security Notes

- Never roll your own crypto primitives unless absolutely necessary
- Use constant-time implementations to prevent timing attacks
- Clear sensitive data (keys) from memory after use
- Validate all inputs (key lengths, etc.)
- Use crypto_memcmp for constant-time comparison
- Generate keys from secure random source
