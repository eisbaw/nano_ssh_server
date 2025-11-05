/*
 * Nano SSH Server - v15-dunkels4
 * World's smallest SSH server for microcontrollers
 *
 * Dunkels Optimization 4: String Pool + Function Tables
 *
 * This version applies Adam Dunkels' string pooling and dispatch optimization:
 * - String pool: Deduplicate all protocol string literals
 * - Const string array with #define accessors
 * - Function pointer dispatch tables (where applicable)
 * - Minimal binary size through string consolidation
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sodium.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

/* Forward declarations */
ssize_t send_data(int fd, const void *buf, size_t len);
ssize_t recv_data(int fd, void *buf, size_t len);

/* String pool - Dunkels optimization: deduplicate strings */
static const char* const string_pool[] = {
    "curve25519-sha256",      /* [0] KEX_ALGORITHM */
    "ssh-ed25519",           /* [1] HOST_KEY_ALGORITHM */
    "aes128-ctr",           /* [2] ENCRYPTION_ALGORITHM */
    "hmac-sha2-256",        /* [3] MAC_ALGORITHM */
    "none",                 /* [4] COMPRESSION_ALGORITHM */
    "",                     /* [5] LANGUAGE */
    "SSH-2.0-NanoSSH_0.1",  /* [6] SERVER_VERSION */
    "NanoSSH_0.1\r\n",      /* [7] VERSION_SUFFIX */
    "ssh-userauth",         /* [8] SERVICE_USERAUTH */
    "ssh-connection",       /* [9] SERVICE_CONNECTION */
    "session",              /* [10] CHANNEL_TYPE_SESSION */
    "password",             /* [11] METHOD_PASSWORD */
    "pty-req",              /* [12] REQ_PTY */
    "shell",                /* [13] REQ_SHELL */
    "exec",                 /* [14] REQ_EXEC */
    "env",                  /* [15] REQ_ENV */
    "Hello World\r\n",      /* [16] HELLO_MESSAGE */
};

#define STR_KEX_ALG         string_pool[0]
#define STR_HOST_KEY_ALG    string_pool[1]
#define STR_CIPHER          string_pool[2]
#define STR_MAC             string_pool[3]
#define STR_COMPRESSION     string_pool[4]
#define STR_LANGUAGE        string_pool[5]
#define STR_VERSION         string_pool[6]
#define STR_VERSION_SUFFIX  string_pool[7]
#define STR_SERVICE_USERAUTH string_pool[8]
#define STR_SERVICE_CONNECTION string_pool[9]
#define STR_CHANNEL_SESSION string_pool[10]
#define STR_METHOD_PASSWORD string_pool[11]
#define STR_REQ_PTY         string_pool[12]
#define STR_REQ_SHELL       string_pool[13]
#define STR_REQ_EXEC        string_pool[14]
#define STR_REQ_ENV         string_pool[15]
#define STR_HELLO           string_pool[16]

/* Configuration */
#define SERVER_PORT 2222
#define SERVER_VERSION STR_VERSION
#define BACKLOG 5

/* Hardcoded credentials (for minimal implementation) */
#define VALID_USERNAME "user"
#define VALID_PASSWORD "password123"

/* SSH protocol constants */
#define MAX_PACKET_SIZE 35000  /* RFC 4253: maximum packet size */
#define MIN_PADDING 4          /* Minimum padding bytes */
#define BLOCK_SIZE_UNENCRYPTED 8   /* Block size before encryption */
#define BLOCK_SIZE_AES_CTR 16      /* Block size for AES-CTR (cipher block size) */

/* SSH message type constants (RFC 4253) */
#define SSH_MSG_DISCONNECT                1
#define SSH_MSG_IGNORE                    2
#define SSH_MSG_UNIMPLEMENTED             3
#define SSH_MSG_DEBUG                     4
#define SSH_MSG_SERVICE_REQUEST           5
#define SSH_MSG_SERVICE_ACCEPT            6
#define SSH_MSG_KEXINIT                   20
#define SSH_MSG_NEWKEYS                   21
#define SSH_MSG_KEX_ECDH_INIT             30
#define SSH_MSG_KEX_ECDH_REPLY            31
#define SSH_MSG_USERAUTH_REQUEST          50
#define SSH_MSG_USERAUTH_FAILURE          51
#define SSH_MSG_USERAUTH_SUCCESS          52
#define SSH_MSG_USERAUTH_BANNER           53
#define SSH_MSG_CHANNEL_OPEN              90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91
#define SSH_MSG_CHANNEL_OPEN_FAILURE      92
#define SSH_MSG_CHANNEL_WINDOW_ADJUST     93
#define SSH_MSG_CHANNEL_DATA              94
#define SSH_MSG_CHANNEL_EXTENDED_DATA     95
#define SSH_MSG_CHANNEL_EOF               96
#define SSH_MSG_CHANNEL_CLOSE             97
#define SSH_MSG_CHANNEL_REQUEST           98
#define SSH_MSG_CHANNEL_SUCCESS           99
#define SSH_MSG_CHANNEL_FAILURE           100

/* SSH disconnect reason codes (RFC 4253 Section 11.1) */
#define SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT    1
#define SSH_DISCONNECT_PROTOCOL_ERROR                 2
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED            3
#define SSH_DISCONNECT_RESERVED                       4
#define SSH_DISCONNECT_MAC_ERROR                      5
#define SSH_DISCONNECT_COMPRESSION_ERROR              6
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE          7
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED 8
#define SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE        9
#define SSH_DISCONNECT_CONNECTION_LOST                10
#define SSH_DISCONNECT_BY_APPLICATION                 11
#define SSH_DISCONNECT_TOO_MANY_CONNECTIONS           12
#define SSH_DISCONNECT_AUTH_CANCELLED_BY_USER         13
#define SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE 14
#define SSH_DISCONNECT_ILLEGAL_USER_NAME              15

/* Algorithm names for KEXINIT - using string pool */
#define KEX_ALGORITHM           STR_KEX_ALG
#define HOST_KEY_ALGORITHM      STR_HOST_KEY_ALG
#define ENCRYPTION_ALGORITHM    STR_CIPHER
#define MAC_ALGORITHM           STR_MAC
#define COMPRESSION_ALGORITHM   STR_COMPRESSION
#define LANGUAGE                STR_LANGUAGE

/* NOTE: Using AES-128-CTR + HMAC-SHA256 instead of ChaCha20-Poly1305@openssh.com
 * because the OpenSSH Poly1305 variant proved too complex to debug. AES-128-CTR is
 * a standard, widely-supported SSH cipher with simpler implementation. */

/* ======================
 * Cryptography Helper Functions
 * ====================== */

/*
 * Compute SHA-256 hash
 *
 * libsodium provides: crypto_hash_sha256()
 * Output: 32 bytes
 */
void compute_sha256(uint8_t *out, const uint8_t *in, size_t inlen) {
    crypto_hash_sha256(out, in, inlen);
}

/*
 * Compute SHA-256 hash with multiple inputs (for exchange hash H)
 *
 * This is a convenience wrapper for computing H = SHA256(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K)
 */
typedef struct __attribute__((packed)) {
    crypto_hash_sha256_state state;
} hash_state_t;

void hash_init(hash_state_t *h) {
    crypto_hash_sha256_init(&h->state);
}

/*
 * Encryption state for AES-128-CTR + HMAC-SHA256
 *
 * Key material needed:
 * - IV: 16 bytes (initial counter value for CTR mode)
 * - Encryption key: 16 bytes (AES-128)
 * - MAC key: 32 bytes (HMAC-SHA256)
 *
 * Packed and optimized: members ordered largest to smallest
 */
typedef struct __attribute__((packed)) {
    EVP_CIPHER_CTX *ctx;  /* Persistent cipher context - reused across packets */
    uint8_t mac_key[32];  /* HMAC-SHA256 key */
    uint8_t iv[16];       /* IV for AES-128-CTR (constant for all packets) */
    uint8_t enc_key[16];  /* AES-128 encryption key */
    uint32_t seq_num;     /* Sequence number for MAC */
    uint8_t active : 1;   /* 1 if encryption is active */
} crypto_state_t;

static crypto_state_t encrypt_state_c2s = {0}; /* Client to server */
static crypto_state_t encrypt_state_s2c = {0}; /* Server to client */

void hash_update(hash_state_t *h, const uint8_t *data, size_t len) {
    crypto_hash_sha256_update(&h->state, data, len);
}

void hash_final(hash_state_t *h, uint8_t *out) {
    crypto_hash_sha256_final(&h->state, out);
}

/*
 * Generate random bytes (libsodium provides this)
 * Already using: randombytes_buf() in send_packet()
 */

/*
 * Curve25519 key generation
 *
 * libsodium provides:
 * - crypto_scalarmult_base(public, private) - generate public from private
 * - crypto_scalarmult(shared, private, peer_public) - compute shared secret
 */
void generate_curve25519_keypair(uint8_t *private_key, uint8_t *public_key) {
    /* Generate random private key */
    randombytes_buf(private_key, 32);

    /* Compute public key */
    crypto_scalarmult_base(public_key, private_key);
}

int compute_curve25519_shared(uint8_t *shared, const uint8_t *private_key, const uint8_t *peer_public) {
    /* Returns 0 on success, -1 on error (e.g., weak public key) */
    return crypto_scalarmult(shared, private_key, peer_public);
}

/*
 * Ed25519 signing (for host key)
 *
 * libsodium provides:
 * - crypto_sign_keypair(public, private) - generate key pair
 * - crypto_sign_detached(sig, &siglen, msg, msglen, private) - sign message
 * - crypto_sign_verify_detached(sig, msg, msglen, public) - verify signature
 */

/*
 * ChaCha20-Poly1305 AEAD cipher (OpenSSH variant)
 *
 * libsodium provides:
 * - crypto_aead_chacha20poly1305_ietf_encrypt()
 * - crypto_aead_chacha20poly1305_ietf_decrypt()
 *
 * Note: OpenSSH uses a custom two-key variant. We'll implement that in Phase 1.9
 */

/* ======================
 * Binary Packet Protocol Helper Functions
 * ====================== */

/*
 * Write uint32 in big-endian format
 */
void write_uint32_be(uint8_t *buf, uint32_t value) {
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
}

/*
 * Write 64-bit unsigned integer in big-endian format (for ChaCha20 nonce)
 */
void write_uint64_be(uint8_t *buf, uint64_t value) {
    buf[0] = (value >> 56) & 0xFF;
    buf[1] = (value >> 48) & 0xFF;
    buf[2] = (value >> 40) & 0xFF;
    buf[3] = (value >> 32) & 0xFF;
    buf[4] = (value >> 24) & 0xFF;
    buf[5] = (value >> 16) & 0xFF;
    buf[6] = (value >> 8) & 0xFF;
    buf[7] = value & 0xFF;
}

/*
 * Read uint32 in big-endian format
 */
uint32_t read_uint32_be(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           ((uint32_t)buf[3]);
}

/*
 * Write SSH string (length-prefixed)
 * Returns number of bytes written
 */
size_t write_string(uint8_t *buf, const char *str, size_t str_len) {
    write_uint32_be(buf, str_len);
    memcpy(buf + 4, str, str_len);
    return 4 + str_len;
}

/*
 * Read SSH string (length-prefixed)
 * Returns number of bytes read, or 0 on error
 */
size_t read_string(const uint8_t *buf, size_t buf_len, char *out, size_t out_max, uint32_t *str_len) {
    if (buf_len < 4) {
        return 0;  /* Not enough data for length */
    }

    *str_len = read_uint32_be(buf);

    if (*str_len > out_max - 1) {
        return 0;
    }

    if (buf_len < 4 + *str_len) {
        return 0;  /* Not enough data for string */
    }

    memcpy(out, buf + 4, *str_len);
    out[*str_len] = '\0';  /* Null-terminate */

    return 4 + *str_len;
}

/*
 * Calculate padding length for SSH packet
 * Total packet length (excluding MAC) must be multiple of block_size
 * Padding must be at least MIN_PADDING bytes
 */
uint8_t calculate_padding(size_t payload_len, size_t block_size) {
    size_t total_len = 5 + payload_len;  /* 4 (packet_length) + 1 (padding_length) + payload */
    uint8_t padding = block_size - (total_len % block_size);

    if (padding < MIN_PADDING) {
        padding += block_size;
    }

    return padding;
}

/* ======================
 * ChaCha20-Poly1305 Encryption (OpenSSH variant)
 * ====================== */

/*
 * Encrypt SSH packet using ChaCha20-Poly1305 (OpenSSH variant)
 *
 * OpenSSH uses a two-key variant (PROTOCOL.chacha20poly1305):
 * - K_1 (key[0..31]): Encrypts packet_length (4 bytes) only
 * - K_2 (key[32..63]): Encrypts payload, generates Poly1305 key
 * - Poly1305 key: first 32 bytes of ChaCha20(K_2, nonce, counter=0)
 * - Nonce: 8 zero bytes + 4-byte sequence number (big-endian)
 *
 * Input: unencrypted packet (packet_length || padding_length || payload || padding)
 * Output: encrypted packet_length || encrypted rest || MAC
 * Parameter key: 64 bytes (K_1 || K_2)
 */
int chacha20_poly1305_encrypt(uint8_t *packet, size_t packet_len,
                               const uint8_t key[64], uint32_t seq_num,
                               uint8_t mac[16])
{
    const uint8_t *k1 = key;          /* First 32 bytes */
    const uint8_t *k2 = key + 32;     /* Last 32 bytes */
    uint8_t nonce[12];
    uint8_t poly_key[32];

    /* Build nonce per OpenSSH spec: 4 zero bytes + 8-byte sequence number (uint64 big-endian) */
    memset(nonce, 0, 4);
    write_uint64_be(nonce + 4, (uint64_t)seq_num);

    /* Generate Poly1305 key from ChaCha20(K_2, nonce, counter=0) */
    memset(poly_key, 0, 32);
    crypto_stream_chacha20_ietf_xor_ic(poly_key, poly_key, 32, nonce, 0, k2);

    /* Encrypt packet_length (first 4 bytes) with K_1 */
    crypto_stream_chacha20_ietf_xor_ic(packet, packet, 4, nonce, 0, k1);

    /* Encrypt rest of packet with K_2, counter=1 (skip Poly1305 key block) */
    if (packet_len > 4) {
        crypto_stream_chacha20_ietf_xor_ic(packet + 4, packet + 4, packet_len - 4,
                                           nonce, 1, k2);
    }

    /* Compute Poly1305 MAC over encrypted packet */
    crypto_onetimeauth_poly1305(mac, packet, packet_len, poly_key);

    /* Clear sensitive data */
    memset(poly_key, 0, 32);
    memset(nonce, 0, 12);

    return 0;
}

/*
 * Decrypt SSH packet using ChaCha20-Poly1305 (OpenSSH variant)
 *
 * OpenSSH uses a two-key variant (PROTOCOL.chacha20poly1305):
 * - K_1 (key[0..31]): Decrypts packet_length (4 bytes) only
 * - K_2 (key[32..63]): Decrypts payload, generates Poly1305 key
 * - Poly1305 key: first 32 bytes of ChaCha20(K_2, nonce, counter=0)
 *
 * Input: encrypted packet_length || encrypted rest, MAC (separate)
 * Output: decrypted packet (packet_length || padding_length || payload || padding)
 * Parameter key: 64 bytes (K_1 || K_2)
 * Returns: 0 on success, -1 on MAC verification failure
 */
int chacha20_poly1305_decrypt(uint8_t *packet, size_t packet_len,
                               const uint8_t mac[16],
                               const uint8_t key[64], uint32_t seq_num)
{
    const uint8_t *k1 = key;          /* First 32 bytes */
    const uint8_t *k2 = key + 32;     /* Last 32 bytes */
    uint8_t nonce[12];
    uint8_t poly_key[32];
    int mac_valid;

    /* Build nonce per OpenSSH spec: 4 zero bytes + 8-byte sequence number (uint64 big-endian) */
    memset(nonce, 0, 4);
    write_uint64_be(nonce + 4, (uint64_t)seq_num);

    /* Generate Poly1305 key from ChaCha20(K_2, nonce, counter=0) */
    memset(poly_key, 0, 32);
    crypto_stream_chacha20_ietf_xor_ic(poly_key, poly_key, 32, nonce, 0, k2);

    /* Verify Poly1305 MAC over encrypted packet */
    mac_valid = crypto_onetimeauth_poly1305_verify(mac, packet, packet_len, poly_key);

    /* Clear Poly1305 key */
    memset(poly_key, 0, 32);

    if (mac_valid != 0) {
        memset(nonce, 0, 12);
        return -1;
    }

    /* Decrypt packet_length (first 4 bytes) with K_1 */
    crypto_stream_chacha20_ietf_xor_ic(packet, packet, 4, nonce, 0, k1);

    /* Decrypt rest of packet with K_2, counter=1 (skip Poly1305 key block) */
    if (packet_len > 4) {
        crypto_stream_chacha20_ietf_xor_ic(packet + 4, packet + 4, packet_len - 4,
                                           nonce, 1, k2);
    }

    /* Clear nonce */
    memset(nonce, 0, 12);

    return 0;
}

/* ======================
 * ChaCha20-IETF + HMAC-SHA256 Encryption (CURRENT IMPLEMENTATION)
 * ====================== */

/*
 * Compute HMAC-SHA256
 *
 * RFC 4253: MAC = HMAC(key, sequence_number || unencrypted_packet)
 * where sequence_number is uint32 big-endian
 */
void compute_hmac_sha256(uint8_t *mac, const uint8_t *key,
                         uint32_t seq_num, const uint8_t *packet, size_t packet_len)
{
    crypto_auth_hmacsha256_state state;
    uint8_t seq_buf[4];

    /* Initialize HMAC state with key */
    crypto_auth_hmacsha256_init(&state, key, 32);

    /* Add sequence number (big-endian) */
    write_uint32_be(seq_buf, seq_num);
    crypto_auth_hmacsha256_update(&state, seq_buf, 4);

    /* Add packet data */
    crypto_auth_hmacsha256_update(&state, packet, packet_len);

    /* Finalize - produces 32-byte MAC */
    crypto_auth_hmacsha256_final(&state, mac);
}

/*
 * Encrypt SSH packet using AES-128-CTR + HMAC-SHA256
 *
 * SSH packet encryption (RFC 4253):
 * 1. Compute MAC over unencrypted packet (seq || packet)
 * 2. Encrypt packet with AES-128-CTR
 * 3. Send encrypted_packet || MAC
 *
 * Note: MAC is computed over UNENCRYPTED packet per RFC 4253
 * IMPORTANT: Reuses persistent cipher context - counter increments automatically
 */
int aes_ctr_hmac_encrypt(uint8_t *packet, size_t packet_len,
                         crypto_state_t *state, uint8_t *mac)
{
    int len;

    /* First, compute MAC over unencrypted packet */
    compute_hmac_sha256(mac, state->mac_key, state->seq_num, packet, packet_len);

    /* Encrypt packet in-place using persistent cipher context
     * The CTR counter automatically increments across calls */
    if (!EVP_EncryptUpdate(state->ctx, packet, &len, packet, packet_len)) {
        return -1;
    }

    return 0;
}

/*
 * Send SSH packet (supports both encrypted and unencrypted)
 *
 * Packet format:
 *   uint32    packet_length  (length of packet, excluding MAC and this field)
 *   byte      padding_length
 *   byte[n1]  payload
 *   byte[n2]  random padding
 *   [byte[16] MAC]  (only if encrypted)
 */
int send_packet(int fd, const uint8_t *payload, size_t payload_len) {
    uint8_t packet[MAX_PACKET_SIZE];
    uint8_t mac[32];  /* HMAC-SHA256 produces 32-byte MAC */
    uint8_t padding_len;
    uint32_t packet_len;
    size_t total_len;
    size_t block_size;

    if (payload_len > MAX_PACKET_SIZE - 256) {
        return -1;
    }

    /* Determine block size based on encryption state
     * RFC 4253: packet must be multiple of cipher block size (or 8, whichever is larger) */
    block_size = encrypt_state_s2c.active ? BLOCK_SIZE_AES_CTR : BLOCK_SIZE_UNENCRYPTED;

    /* Calculate padding */
    padding_len = calculate_padding(payload_len, block_size);

    /* Calculate packet length (excluding packet_length field itself) */
    packet_len = 1 + payload_len + padding_len;

    /* Build packet */
    write_uint32_be(packet, packet_len);
    packet[4] = padding_len;
    memcpy(packet + 5, payload, payload_len);

    /* Add random padding */
    randombytes_buf(packet + 5 + payload_len, padding_len);

    /* Total length of packet (packet_length || padding_length || payload || padding) */
    total_len = 4 + packet_len;

    /* If encryption is active, encrypt and add MAC */
    if (encrypt_state_s2c.active) {
        /* Encrypt packet in-place and generate HMAC */
        if (aes_ctr_hmac_encrypt(packet, total_len, &encrypt_state_s2c, mac) < 0) {
            return -1;
        }

        /* Send encrypted packet */
        if (send_data(fd, packet, total_len) < 0) {
            return -1;
        }

        /* Send MAC (32 bytes for HMAC-SHA256) */
        if (send_data(fd, mac, 32) < 0) {
            return -1;
        }

        /* Increment sequence number */
        encrypt_state_s2c.seq_num++;

    } else {
        /* Send unencrypted packet */
        if (send_data(fd, packet, total_len) < 0) {
            return -1;
        }
    }

    return 0;
}

/*
 * Send SSH_MSG_DISCONNECT message
 *
 * Format (RFC 4253 Section 11.1):
 *   byte      SSH_MSG_DISCONNECT (1)
 *   uint32    reason_code
 *   string    description (ISO-10646 UTF-8)
 *   string    language_tag (empty for minimal implementation)
 *
 * This should be sent before closing the connection when an error occurs.
 */
void send_disconnect(int fd, uint32_t reason_code, const char *description) {
    uint8_t disconnect_msg[512];
    size_t msg_len = 0;

    /* Message type */
    disconnect_msg[msg_len++] = SSH_MSG_DISCONNECT;

    /* Reason code */
    write_uint32_be(disconnect_msg + msg_len, reason_code);
    msg_len += 4;

    /* Description string */
    size_t desc_len = strlen(description);
    msg_len += write_string(disconnect_msg + msg_len, description, desc_len);

    /* Language tag (empty) */
    msg_len += write_string(disconnect_msg + msg_len, "", 0);

    /* Try to send, but don't fail if it doesn't work (connection may already be broken) */
    send_packet(fd, disconnect_msg, msg_len);
}

/*
 * Receive SSH packet (supports both encrypted and unencrypted)
 *
 * Returns: payload length on success, -1 on error, 0 on connection close
 * Output: payload buffer filled with packet payload
 */
ssize_t recv_packet(int fd, uint8_t *payload, size_t payload_max) {
    uint8_t header[4];
    uint8_t mac[32];  /* HMAC-SHA256 produces 32-byte MAC */
    uint32_t packet_len;
    uint8_t padding_len;
    uint8_t *packet_buf;
    size_t payload_len;
    size_t total_packet_len;
    ssize_t n;

    if (encrypt_state_c2s.active) {
        /* ===== ENCRYPTED MODE (AES-128-CTR + HMAC-SHA256) ===== */

        /* For AES-CTR, read first block (16 bytes) which contains packet_length */
        uint8_t first_block[16];
        n = recv_data(fd, first_block, 16);
        if (n <= 0) {
            return n;  /* Error or connection closed */
        }
        if (n < 16) {
            return -1;
        }

        /* Decrypt first block to extract packet_length */
        uint8_t decrypted_block[16];
        int len;

        /* Decrypt first block using persistent cipher context
         * Counter automatically increments from previous packet */
        if (!EVP_DecryptUpdate(encrypt_state_c2s.ctx, decrypted_block, &len,
                               first_block, 16)) {
            return -1;
        }

        packet_len = read_uint32_be(decrypted_block);

        /* Validate packet length */
        if (packet_len < 5 || packet_len > MAX_PACKET_SIZE) {
            return -1;
        }

        /* Calculate total packet size and remaining bytes to read */
        total_packet_len = 4 + packet_len;
        size_t remaining = total_packet_len - 16;  /* Already read 16 bytes */

        /* Allocate buffer for full decrypted packet */
        packet_buf = malloc(total_packet_len);
        if (!packet_buf) {
            return -1;
        }

        /* Copy decrypted first block */
        memcpy(packet_buf, decrypted_block, 16);

        /* Read and decrypt remaining packet bytes */
        if (remaining > 0) {
            uint8_t *encrypted_remaining = malloc(remaining);
            if (!encrypted_remaining) {
                free(packet_buf);
                return -1;
            }

            n = recv_data(fd, encrypted_remaining, remaining);
            if (n <= 0) {
                free(encrypted_remaining);
                free(packet_buf);
                return n;
            }
            if (n < (ssize_t)remaining) {
                free(encrypted_remaining);
                free(packet_buf);
                return -1;
            }

            /* Decrypt remaining bytes using persistent context
             * Counter continues automatically from first block */
            if (!EVP_DecryptUpdate(encrypt_state_c2s.ctx, packet_buf + 16, &len,
                                   encrypted_remaining, remaining)) {
                free(encrypted_remaining);
                free(packet_buf);
                return -1;
            }

            free(encrypted_remaining);
        }

        /* Read MAC (32 bytes for HMAC-SHA256) */
        n = recv_data(fd, mac, 32);
        if (n <= 0) {
            free(packet_buf);
            return n;
        }
        if (n < 32) {
            free(packet_buf);
            return -1;
        }

        /* Verify MAC over decrypted packet (including packet_length) */
        uint8_t computed_mac[32];
        compute_hmac_sha256(computed_mac, encrypt_state_c2s.mac_key,
                           encrypt_state_c2s.seq_num, packet_buf, total_packet_len);

        if (crypto_verify_32(computed_mac, mac) != 0) {
            free(packet_buf);
            return -1;
        }

        /* Increment sequence number */
        encrypt_state_c2s.seq_num++;

        /* Now packet_buf contains decrypted: packet_length || padding_length || payload || padding */
        /* Extract padding length (first byte after packet_length) */
        padding_len = packet_buf[4];

        /* Calculate payload length */
        if (padding_len >= packet_len - 1) {
            free(packet_buf);
            return -1;
        }

        payload_len = packet_len - 1 - padding_len;

        /* Check payload buffer size */
        if (payload_len > payload_max) {
            free(packet_buf);
            return -1;
        }

        /* Copy payload */
        memcpy(payload, packet_buf + 5, payload_len);

        free(packet_buf);

        return payload_len;

    } else {
        /* ===== UNENCRYPTED MODE ===== */

        /* Read packet length (4 bytes) */
        n = recv_data(fd, header, 4);
        if (n <= 0) {
            return n;  /* Error or connection closed */
        }
        if (n < 4) {
            return -1;
        }

        packet_len = read_uint32_be(header);

        /* Validate packet length */
        if (packet_len < 5 || packet_len > MAX_PACKET_SIZE) {
            return -1;
        }

        /* Allocate buffer for rest of packet */
        packet_buf = malloc(packet_len);
        if (!packet_buf) {
            return -1;
        }

        /* Read rest of packet */
        n = recv_data(fd, packet_buf, packet_len);
        if (n <= 0) {
            free(packet_buf);
            return n;
        }
        if (n < (ssize_t)packet_len) {
            free(packet_buf);
            return -1;
        }

        /* Extract padding length */
        padding_len = packet_buf[0];

        /* Calculate payload length */
        if (padding_len >= packet_len - 1) {
            free(packet_buf);
            return -1;
        }

        payload_len = packet_len - 1 - padding_len;

        /* Check payload buffer size */
        if (payload_len > payload_max) {
            free(packet_buf);
            return -1;
        }

        /* Copy payload */
        memcpy(payload, packet_buf + 1, payload_len);

        free(packet_buf);

        return payload_len;
    }
}

/* ======================
 * SSH Protocol Helper Functions
 * ====================== */

/*
 * Helper: Write name-list (SSH string format for algorithm lists)
 * Returns bytes written
 */
size_t write_name_list(uint8_t *buf, const char *names) {
    size_t len = strlen(names);
    write_uint32_be(buf, len);
    memcpy(buf + 4, names, len);
    return 4 + len;
}

/*
 * Build KEXINIT payload
 *
 * Returns payload length
 * Buffer must be at least 1024 bytes
 */
size_t build_kexinit(uint8_t *payload) {
    size_t offset = 0;

    /* Message type */
    payload[offset++] = SSH_MSG_KEXINIT;

    /* Cookie: 16 random bytes */
    randombytes_buf(payload + offset, 16);
    offset += 16;

    /* kex_algorithms */
    offset += write_name_list(payload + offset, KEX_ALGORITHM);

    /* server_host_key_algorithms */
    offset += write_name_list(payload + offset, HOST_KEY_ALGORITHM);

    /* encryption_algorithms_client_to_server */
    offset += write_name_list(payload + offset, ENCRYPTION_ALGORITHM);

    /* encryption_algorithms_server_to_client */
    offset += write_name_list(payload + offset, ENCRYPTION_ALGORITHM);

    /* mac_algorithms_client_to_server */
    offset += write_name_list(payload + offset, MAC_ALGORITHM);

    /* mac_algorithms_server_to_client */
    offset += write_name_list(payload + offset, MAC_ALGORITHM);

    /* compression_algorithms_client_to_server */
    offset += write_name_list(payload + offset, COMPRESSION_ALGORITHM);

    /* compression_algorithms_server_to_client */
    offset += write_name_list(payload + offset, COMPRESSION_ALGORITHM);

    /* languages_client_to_server */
    offset += write_name_list(payload + offset, LANGUAGE);

    /* languages_server_to_client */
    offset += write_name_list(payload + offset, LANGUAGE);

    /* first_kex_packet_follows: FALSE */
    payload[offset++] = 0;

    /* Reserved: 0 */
    write_uint32_be(payload + offset, 0);
    offset += 4;

    return offset;
}

/*
 * Helper: Write SSH mpint (multi-precision integer) format
 * Used for shared secret K in exchange hash
 */
size_t write_mpint(uint8_t *buf, const uint8_t *data, size_t data_len) {
    size_t offset = 0;
    size_t i;

    /* Skip leading zero bytes */
    for (i = 0; i < data_len && data[i] == 0; i++);

    /* If high bit is set, need to add leading zero byte */
    if (i < data_len && (data[i] & 0x80)) {
        write_uint32_be(buf, data_len - i + 1);
        buf[4] = 0;
        memcpy(buf + 5, data + i, data_len - i);
        offset = 4 + 1 + (data_len - i);
    } else {
        write_uint32_be(buf, data_len - i);
        memcpy(buf + 4, data + i, data_len - i);
        offset = 4 + (data_len - i);
    }

    return offset;
}

/*
 * Compute exchange hash H
 *
 * H = SHA256(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K)
 *
 * All strings and mpint encoded as SSH string format (4-byte length + data)
 */
void compute_exchange_hash(uint8_t *hash_out,
                           const char *client_version,
                           const char *server_version,
                           const uint8_t *client_kexinit, size_t client_kexinit_len,
                           const uint8_t *server_kexinit, size_t server_kexinit_len,
                           const uint8_t *server_host_key_blob, size_t host_key_blob_len,
                           const uint8_t *client_ephemeral_pub, size_t client_eph_len,
                           const uint8_t *server_ephemeral_pub, size_t server_eph_len,
                           const uint8_t *shared_secret, size_t shared_secret_len) {
    hash_state_t h;
    uint8_t buf[1024];
    size_t len;

    hash_init(&h);

    /* V_C - client version string */
    len = write_string(buf, client_version, strlen(client_version));
    hash_update(&h, buf, len);

    /* V_S - server version string */
    len = write_string(buf, server_version, strlen(server_version));
    hash_update(&h, buf, len);

    /* I_C - client KEXINIT payload */
    write_uint32_be(buf, client_kexinit_len);
    hash_update(&h, buf, 4);
    hash_update(&h, client_kexinit, client_kexinit_len);

    /* I_S - server KEXINIT payload */
    write_uint32_be(buf, server_kexinit_len);
    hash_update(&h, buf, 4);
    hash_update(&h, server_kexinit, server_kexinit_len);

    /* K_S - server host public key blob */
    write_uint32_be(buf, host_key_blob_len);
    hash_update(&h, buf, 4);
    hash_update(&h, server_host_key_blob, host_key_blob_len);

    /* Q_C - client ephemeral public key */
    len = write_string(buf, (const char *)client_ephemeral_pub, client_eph_len);
    hash_update(&h, buf, len);

    /* Q_S - server ephemeral public key */
    len = write_string(buf, (const char *)server_ephemeral_pub, server_eph_len);
    hash_update(&h, buf, len);

    /* K - shared secret (as mpint) */
    len = write_mpint(buf, shared_secret, shared_secret_len);
    hash_update(&h, buf, len);

    /* Finalize hash */
    hash_final(&h, hash_out);
}

/*
 * Derive key material from shared secret
 *
 * key = HASH(K || H || X || session_id)
 * If more bytes needed: K2 = HASH(K || H || K1), K3 = HASH(K || H || K1 || K2), ...
 *
 * Per RFC 4253 Section 7.2
 */
void derive_key(uint8_t *key_out, size_t key_len,
                const uint8_t *shared_secret, size_t shared_len,
                const uint8_t *exchange_hash,
                char key_id,  /* 'A' through 'F' */
                const uint8_t *session_id) {
    hash_state_t h;
    uint8_t key_material[256];  /* Should be enough for any key */
    size_t material_len = 0;
    size_t needed = key_len;
    size_t offset = 0;

    /* First round: K1 = HASH(K || H || X || session_id) */
    hash_init(&h);

    /* K (shared secret) as mpint */
    {
        uint8_t mpint_buf[64];
        size_t mpint_len = write_mpint(mpint_buf, shared_secret, shared_len);
        hash_update(&h, mpint_buf, mpint_len);
    }

    /* H (exchange hash) */
    hash_update(&h, exchange_hash, 32);

    /* X (key identifier) */
    hash_update(&h, (uint8_t *)&key_id, 1);

    /* session_id */
    hash_update(&h, session_id, 32);

    hash_final(&h, key_material);
    material_len = 32;

    /* If we need more key material, generate additional rounds */
    while (material_len < needed) {
        hash_init(&h);

        /* K as mpint */
        {
            uint8_t mpint_buf[64];
            size_t mpint_len = write_mpint(mpint_buf, shared_secret, shared_len);
            hash_update(&h, mpint_buf, mpint_len);
        }

        /* H */
        hash_update(&h, exchange_hash, 32);

        /* K1 || K2 || ... (all previous key material) */
        hash_update(&h, key_material, material_len);

        hash_final(&h, key_material + material_len);
        material_len += 32;
    }

    /* Copy requested amount */
    memcpy(key_out, key_material + offset, key_len);
}

/* ======================
 * Network Helper Functions
 * ====================== */

/*
 * Helper function: Create and configure TCP server socket
 */
int create_server_socket(int port) {
    int listen_fd;
    struct sockaddr_in server_addr;
    int reuse = 1;

    /* Create socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        return -1;
    }

    /* Set SO_REUSEADDR to avoid "Address already in use" errors */
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    }

    /* Bind to address and port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_fd);
        return -1;
    }

    /* Listen for connections */
    if (listen(listen_fd, BACKLOG) < 0) {
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

/*
 * Helper function: Accept client connection
 */
int accept_client(int listen_fd, struct sockaddr_in *client_addr) {
    socklen_t addr_len = sizeof(*client_addr);
    int client_fd;

    client_fd = accept(listen_fd, (struct sockaddr *)client_addr, &addr_len);
    if (client_fd < 0) {
        return -1;
    }

    return client_fd;
}

/*
 * Helper function: Send data with error handling
 */
ssize_t send_data(int fd, const void *buf, size_t len) {
    ssize_t sent = 0;
    ssize_t n;

    while (sent < (ssize_t)len) {
        n = send(fd, (const uint8_t *)buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted, try again */
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        sent += n;
    }

    return sent;
}

/*
 * Helper function: Receive data with error handling
 */
ssize_t recv_data(int fd, void *buf, size_t len) {
    ssize_t n;

    n = recv(fd, buf, len, 0);
    if (n < 0) {
        if (errno == EINTR) {
            return 0;  /* Interrupted, try again */
        }
        return -1;
    }
    if (n == 0) {
        /* Connection closed by peer */
        return 0;
    }

    return n;
}

/*
 * Handle SSH connection
 */
void handle_client(int client_fd, struct sockaddr_in *client_addr,
                   const uint8_t *host_public_key, const uint8_t *host_private_key) {
    char client_version[256];
    char server_version_line[256];
    ssize_t n;
    int i, version_len;

    (void)client_addr;  /* Unused in minimal version */

    /* ======================
     * Phase 1.2: Version Exchange
     * ====================== */

    /* Send server version string (plaintext, ends with \r\n) */
    server_version_line[0] = 'S'; server_version_line[1] = 'S'; server_version_line[2] = 'H';
    server_version_line[3] = '-'; server_version_line[4] = '2'; server_version_line[5] = '.';
    server_version_line[6] = '0'; server_version_line[7] = '-';
    memcpy(server_version_line + 8, STR_VERSION_SUFFIX, 13);
    if (send_data(client_fd, server_version_line, 21) < 0) {
        close(client_fd);
        return;
    }

    /* Receive client version string (read until \n) */
    memset(client_version, 0, sizeof(client_version));
    version_len = 0;

    for (i = 0; i < (int)sizeof(client_version) - 1; i++) {
        n = recv_data(client_fd, &client_version[i], 1);
        if (n <= 0) {
            /* Network error - can't send disconnect */
            close(client_fd);
            return;
        }
        version_len++;

        /* Stop at \n */
        if (client_version[i] == '\n') {
            break;
        }
    }

    /* Check if we hit the buffer limit without finding newline */
    if (i == (int)sizeof(client_version) - 1 && client_version[i] != '\n') {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR,
                       "Version string exceeds maximum length");
        close(client_fd);
        return;
    }

    /* Null-terminate */
    client_version[version_len] = '\0';

    /* Remove \r\n if present */
    if (version_len > 0 && client_version[version_len - 1] == '\n') {
        client_version[version_len - 1] = '\0';
        version_len--;
    }
    if (version_len > 0 && client_version[version_len - 1] == '\r') {
        client_version[version_len - 1] = '\0';
        version_len--;
    }

    /* Validate client version starts with "SSH-2.0-" */
    if (strncmp(client_version, "SSH-2.0-", 8) != 0) {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED,
                       "Only SSH-2.0 is supported");
        close(client_fd);
        return;
    }

    /* ======================
     * Phase 1.5: KEXINIT Exchange
     * ====================== */

    uint8_t server_kexinit[1024];
    uint8_t client_kexinit[4096];  /* Client sends comprehensive algorithm lists */
    size_t server_kexinit_len;
    ssize_t client_kexinit_len_s;

    /* Build our KEXINIT payload */
    server_kexinit_len = build_kexinit(server_kexinit);
    
    /* Send our KEXINIT */
    if (send_packet(client_fd, server_kexinit, server_kexinit_len) < 0) {
        close(client_fd);
        return;
    }
    
    /* Receive client's KEXINIT */
    client_kexinit_len_s = recv_packet(client_fd, client_kexinit, sizeof(client_kexinit));
    if (client_kexinit_len_s <= 0) {
        /* Network error - connection likely closed, can't send disconnect */
        close(client_fd);
        return;
    }
    
    /* Validate it's a KEXINIT message */
    if (client_kexinit[0] != SSH_MSG_KEXINIT) {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR,
                       "Expected KEXINIT message");
        close(client_fd);
        return;
    }

    
    /* ======================
     * Phase 1.6: Curve25519 Key Exchange
     * ====================== */

    uint8_t server_ephemeral_private[32];
    uint8_t server_ephemeral_public[32];
    uint8_t client_ephemeral_public[32];
    uint8_t shared_secret[32];
    uint8_t exchange_hash[32];
    uint8_t session_id[32];
    uint8_t host_key_blob[256];
    size_t host_key_blob_len;
    uint8_t signature[64];
    unsigned long long sig_len;
    uint8_t kex_reply[4096];
    size_t kex_reply_len;
    uint8_t kex_init_msg[128];
    ssize_t kex_init_len;
    uint32_t client_eph_str_len;

    /* Generate server ephemeral key pair */
    generate_curve25519_keypair(server_ephemeral_private, server_ephemeral_public);
    
    /* Build server host key blob (ssh-ed25519 format) */
    host_key_blob_len = 0;
    host_key_blob_len += write_string(host_key_blob + host_key_blob_len, STR_HOST_KEY_ALG, 11);
    host_key_blob_len += write_string(host_key_blob + host_key_blob_len,
                                      (const char *)host_public_key, 32);
    
    /* Receive SSH_MSG_KEX_ECDH_INIT from client */
    kex_init_len = recv_packet(client_fd, kex_init_msg, sizeof(kex_init_msg));
    if (kex_init_len <= 0) {
        close(client_fd);
        return;
    }

    if (kex_init_msg[0] != SSH_MSG_KEX_ECDH_INIT) {
        send_disconnect(client_fd, SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
                       "Expected KEX_ECDH_INIT message");
        close(client_fd);
        return;
    }

    /* Extract client ephemeral public key (Q_C) */
    if (read_string(kex_init_msg + 1, kex_init_len - 1,
                   (char *)client_ephemeral_public, 33, &client_eph_str_len) == 0) {
        close(client_fd);
        return;
    }

    if (client_eph_str_len != 32) {
        close(client_fd);
        return;
    }
    
    /* Compute shared secret K */
    if (compute_curve25519_shared(shared_secret, server_ephemeral_private,
                                  client_ephemeral_public) != 0) {
        close(client_fd);
        return;
    }
    
    /* Compute exchange hash H */
    compute_exchange_hash(exchange_hash,
                         client_version, SERVER_VERSION,
                         client_kexinit, client_kexinit_len_s,
                         server_kexinit, server_kexinit_len,
                         host_key_blob, host_key_blob_len,
                         client_ephemeral_public, 32,
                         server_ephemeral_public, 32,
                         shared_secret, 32);
    
    /* First exchange hash becomes session_id */
    memcpy(session_id, exchange_hash, 32);
    
    /* Sign exchange hash with host private key */
    crypto_sign_detached(signature, &sig_len, exchange_hash, 32, host_private_key);
    
    /* Build SSH_MSG_KEX_ECDH_REPLY */
    kex_reply_len = 0;
    kex_reply[kex_reply_len++] = SSH_MSG_KEX_ECDH_REPLY;

    /* K_S - server host public key blob */
    kex_reply_len += write_string(kex_reply + kex_reply_len,
                                  (const char *)host_key_blob, host_key_blob_len);

    /* Q_S - server ephemeral public key */
    kex_reply_len += write_string(kex_reply + kex_reply_len,
                                  (const char *)server_ephemeral_public, 32);

    /* Signature of H (ssh-ed25519 signature format) */
    {
        uint8_t sig_blob[128];
        size_t sig_blob_len = 0;
        sig_blob_len += write_string(sig_blob, STR_HOST_KEY_ALG, 11);
        sig_blob_len += write_string(sig_blob + sig_blob_len, (const char *)signature, 64);
        kex_reply_len += write_string(kex_reply + kex_reply_len,
                                      (const char *)sig_blob, sig_blob_len);
    }

    /* Send SSH_MSG_KEX_ECDH_REPLY */
    if (send_packet(client_fd, kex_reply, kex_reply_len) < 0) {
        close(client_fd);
        return;
    }
    
    /* ======================
     * Phase 1.7: Key Derivation
     * ====================== */

    uint8_t iv_c2s[16];      /* IV client to server (AES-128-CTR) */
    uint8_t iv_s2c[16];      /* IV server to client (AES-128-CTR) */
    uint8_t key_c2s[16];     /* Encryption key client to server (AES-128) */
    uint8_t key_s2c[16];     /* Encryption key server to client (AES-128) */
    uint8_t int_key_c2s[32]; /* Integrity key client to server (HMAC-SHA256) */
    uint8_t int_key_s2c[32]; /* Integrity key server to client (HMAC-SHA256) */

    /* Derive all keys using session_id and exchange_hash
     * For AES-128-CTR + HMAC-SHA256:
     * - IV: 16 bytes (AES block size, initial counter value)
     * - Encryption key: 16 bytes (AES-128)
     * - MAC key: 32 bytes (HMAC-SHA256)
     */
    derive_key(iv_c2s, 16, shared_secret, 32, exchange_hash, 'A', session_id);
    derive_key(iv_s2c, 16, shared_secret, 32, exchange_hash, 'B', session_id);
    derive_key(key_c2s, 16, shared_secret, 32, exchange_hash, 'C', session_id);
    derive_key(key_s2c, 16, shared_secret, 32, exchange_hash, 'D', session_id);
    derive_key(int_key_c2s, 32, shared_secret, 32, exchange_hash, 'E', session_id);
    derive_key(int_key_s2c, 32, shared_secret, 32, exchange_hash, 'F', session_id);

                            
    /* ======================
     * Phase 1.8: NEWKEYS Exchange
     * ====================== */

    uint8_t newkeys_msg[1];
    uint8_t recv_newkeys[16];
    ssize_t newkeys_len;

    /* Send SSH_MSG_NEWKEYS */
    newkeys_msg[0] = SSH_MSG_NEWKEYS;
    if (send_packet(client_fd, newkeys_msg, 1) < 0) {
        close(client_fd);
        return;
    }
    
    /* ======================
     * Phase 1.9: Activate Encryption (Server→Client)
     * ====================== */

    /* RFC 4253 Section 7.3: "All messages sent after this message MUST use the new keys"
     * Activate encryption for OUTGOING packets immediately after sending NEWKEYS */
    memcpy(encrypt_state_s2c.iv, iv_s2c, 16);
    memcpy(encrypt_state_s2c.enc_key, key_s2c, 16);
    memcpy(encrypt_state_s2c.mac_key, int_key_s2c, 32);

    /* Initialize cipher context ONCE - will be reused for all packets */
    encrypt_state_s2c.ctx = EVP_CIPHER_CTX_new();
    if (!encrypt_state_s2c.ctx) {
        close(client_fd);
        return;
    }

    if (!EVP_EncryptInit_ex(encrypt_state_s2c.ctx, EVP_aes_128_ctr(), NULL,
                            encrypt_state_s2c.enc_key, encrypt_state_s2c.iv)) {
        EVP_CIPHER_CTX_free(encrypt_state_s2c.ctx);
        close(client_fd);
        return;
    }

    /* Sequence numbers start at 0 from first packet.
     * By this point we've sent: KEXINIT(0), KEX_ECDH_REPLY(1), NEWKEYS(2)
     * Next outgoing packet will be seq 3 */
    encrypt_state_s2c.seq_num = 3;
    encrypt_state_s2c.active = 1;
    
    /* Receive SSH_MSG_NEWKEYS from client (still unencrypted!) */
    newkeys_len = recv_packet(client_fd, recv_newkeys, sizeof(recv_newkeys));
    if (newkeys_len <= 0) {
        close(client_fd);
        return;
    }

    if (recv_newkeys[0] != SSH_MSG_NEWKEYS) {
        close(client_fd);
        return;
    }
    
    /* Activate encryption for INCOMING packets immediately after receiving NEWKEYS */
    memcpy(encrypt_state_c2s.iv, iv_c2s, 16);
    memcpy(encrypt_state_c2s.enc_key, key_c2s, 16);
    memcpy(encrypt_state_c2s.mac_key, int_key_c2s, 32);

    /* Initialize cipher context ONCE - will be reused for all packets */
    encrypt_state_c2s.ctx = EVP_CIPHER_CTX_new();
    if (!encrypt_state_c2s.ctx) {
        close(client_fd);
        return;
    }

    if (!EVP_DecryptInit_ex(encrypt_state_c2s.ctx, EVP_aes_128_ctr(), NULL,
                            encrypt_state_c2s.enc_key, encrypt_state_c2s.iv)) {
        EVP_CIPHER_CTX_free(encrypt_state_c2s.ctx);
        close(client_fd);
        return;
    }

    /* By this point we've received: KEXINIT(0), KEX_ECDH_INIT(1), NEWKEYS(2)
     * Next incoming packet will be seq 3 */
    encrypt_state_c2s.seq_num = 3;
    encrypt_state_c2s.active = 1;
    
                
    /* ======================
     * Phase 1.10: Service Request
     * ====================== */

    uint8_t service_request[256];
    ssize_t service_request_len;
    uint8_t service_accept[256];
    size_t service_accept_len;
    char service_name[64];
    uint32_t service_name_len;

    /* Receive SSH_MSG_SERVICE_REQUEST */
    service_request_len = recv_packet(client_fd, service_request, sizeof(service_request));
    if (service_request_len <= 0) {
        close(client_fd);
        return;
    }

    if (service_request[0] != SSH_MSG_SERVICE_REQUEST) {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR,
                       "Expected SERVICE_REQUEST message");
        close(client_fd);
        return;
    }

    /* Parse service name */
    if (read_string(service_request + 1, service_request_len - 1,
                    service_name, sizeof(service_name), &service_name_len) == 0) {
        close(client_fd);
        return;
    }

    service_name[service_name_len] = '\0';

    /* Verify service name is "ssh-userauth" */
    if (strcmp(service_name, STR_SERVICE_USERAUTH) != 0) {
        send_disconnect(client_fd, SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
                       "Only ssh-userauth service is supported");
        close(client_fd);
        return;
    }

    /* Build SSH_MSG_SERVICE_ACCEPT */
    service_accept_len = 0;
    service_accept[service_accept_len++] = SSH_MSG_SERVICE_ACCEPT;
    service_accept_len += write_string(service_accept + service_accept_len,
                                       STR_SERVICE_USERAUTH, 12);

    /* Send SSH_MSG_SERVICE_ACCEPT */
    if (send_packet(client_fd, service_accept, service_accept_len) < 0) {
        close(client_fd);
        return;
    }
    
    /* ======================
     * Phase 1.11: Authentication (Password)
     * ====================== */

    uint8_t userauth_request[512];
    ssize_t userauth_request_len;
    uint8_t *ptr;
    uint32_t username_len, service_len, method_len, password_len;
    char username[256];
    char service[256];
    char method[256];
    char password[256];
    uint8_t change_password;
    int authenticated = 0;

    /* Authentication loop - allow multiple attempts */
    while (!authenticated) {
        /* Receive SSH_MSG_USERAUTH_REQUEST */
        userauth_request_len = recv_packet(client_fd, userauth_request, sizeof(userauth_request));
        if (userauth_request_len <= 0) {
            close(client_fd);
            return;
        }

        if (userauth_request[0] != SSH_MSG_USERAUTH_REQUEST) {
            close(client_fd);
            return;
        }

    /* Parse USERAUTH_REQUEST:
     * byte      SSH_MSG_USERAUTH_REQUEST (50)
     * string    user name
     * string    service name ("ssh-connection")
     * string    method name ("password")
     * boolean   FALSE (change password flag)
     * string    password
     */
    ptr = userauth_request + 1;

    /* Parse username */
    username_len = read_uint32_be(ptr);
    ptr += 4;
    if (username_len >= sizeof(username)) {
        close(client_fd);
        return;
    }
    memcpy(username, ptr, username_len);
    username[username_len] = '\0';
    ptr += username_len;

    /* Parse service name */
    service_len = read_uint32_be(ptr);
    ptr += 4;
    if (service_len >= sizeof(service)) {
        close(client_fd);
        return;
    }
    memcpy(service, ptr, service_len);
    service[service_len] = '\0';
    ptr += service_len;

    /* Parse method name */
    method_len = read_uint32_be(ptr);
    ptr += 4;
    if (method_len >= sizeof(method)) {
        close(client_fd);
        return;
    }
    memcpy(method, ptr, method_len);
    method[method_len] = '\0';
    ptr += method_len;


    /* Verify service is "ssh-connection" */
    if (strcmp(service, STR_SERVICE_CONNECTION) != 0) {
        close(client_fd);
        return;
    }

    /* Handle password authentication */
    if (strcmp(method, STR_METHOD_PASSWORD) == 0) {
        /* Parse change password flag */
        change_password = *ptr;
        ptr += 1;

        /* Parse password */
        password_len = read_uint32_be(ptr);
        ptr += 4;
        if (password_len >= sizeof(password)) {
            close(client_fd);
            return;
        }
        memcpy(password, ptr, password_len);
        password[password_len] = '\0';

                
        /* Validate credentials */
        if (strcmp(username, VALID_USERNAME) == 0 &&
            strcmp(password, VALID_PASSWORD) == 0) {
            /* Authentication successful */
            
            /* Send SSH_MSG_USERAUTH_SUCCESS */
            uint8_t success_msg[1];
            success_msg[0] = SSH_MSG_USERAUTH_SUCCESS;

            if (send_packet(client_fd, success_msg, 1) < 0) {
                close(client_fd);
                return;
            }
                        authenticated = 1;  /* Exit authentication loop */
        } else {
            /* Authentication failed */
            
            /* Send SSH_MSG_USERAUTH_FAILURE */
            uint8_t failure_msg[256];
            size_t failure_len = 0;

            failure_msg[failure_len++] = SSH_MSG_USERAUTH_FAILURE;

            /* Authentications that can continue: "password" */
            failure_len += write_string(failure_msg + failure_len, STR_METHOD_PASSWORD, 8);

            /* Partial success: FALSE */
            failure_msg[failure_len++] = 0;

            if (send_packet(client_fd, failure_msg, failure_len) < 0) {
                close(client_fd);
                return;
            }
                    }
    } else {
        /* Unsupported authentication method (e.g., "none") */

        /* Send SSH_MSG_USERAUTH_FAILURE */
        uint8_t failure_msg[256];
        size_t failure_len = 0;

        failure_msg[failure_len++] = SSH_MSG_USERAUTH_FAILURE;

        /* Authentications that can continue: "password" */
        failure_len += write_string(failure_msg + failure_len, STR_METHOD_PASSWORD, 8);

        /* Partial success: FALSE */
        failure_msg[failure_len++] = 0;

        if (send_packet(client_fd, failure_msg, failure_len) < 0) {
            close(client_fd);
            return;
        }
                /* Continue loop - wait for another auth attempt */
    }
    }  /* End authentication loop */

    /* Authentication successful - continue with channel open, shell, etc. */
    
    /* ======================
     * Phase 1.12: Channel Open
     * ====================== */

    uint8_t channel_open_msg[512];
    ssize_t channel_open_len;
    uint8_t *ch_ptr;
    uint32_t channel_type_len;
    char channel_type[64];
    uint32_t client_channel_id;
    uint32_t client_window_size;
    uint32_t client_max_packet;
    uint32_t server_channel_id = 0;  /* We assign channel ID 0 */
    uint32_t server_window_size = 32768;
    uint32_t server_max_packet = 16384;

    /* Receive SSH_MSG_CHANNEL_OPEN */
    channel_open_len = recv_packet(client_fd, channel_open_msg, sizeof(channel_open_msg));
    if (channel_open_len <= 0) {
        close(client_fd);
        return;
    }

    if (channel_open_msg[0] != SSH_MSG_CHANNEL_OPEN) {
        close(client_fd);
        return;
    }

    /* Parse SSH_MSG_CHANNEL_OPEN:
     * byte      SSH_MSG_CHANNEL_OPEN (90)
     * string    channel_type
     * uint32    sender_channel (client's channel ID)
     * uint32    initial_window_size
     * uint32    maximum_packet_size
     */
    ch_ptr = channel_open_msg + 1;

    /* Parse channel type */
    channel_type_len = read_uint32_be(ch_ptr);
    ch_ptr += 4;
    if (channel_type_len >= sizeof(channel_type)) {
        close(client_fd);
        return;
    }
    memcpy(channel_type, ch_ptr, channel_type_len);
    channel_type[channel_type_len] = '\0';
    ch_ptr += channel_type_len;

    /* Parse sender_channel (client's channel ID) */
    client_channel_id = read_uint32_be(ch_ptr);
    ch_ptr += 4;

    /* Parse initial_window_size */
    client_window_size = read_uint32_be(ch_ptr);
    ch_ptr += 4;

    /* Parse maximum_packet_size */
    client_max_packet = read_uint32_be(ch_ptr);
    ch_ptr += 4;


    /* Verify channel type is "session" */
    if (strcmp(channel_type, STR_CHANNEL_SESSION) != 0) {
        /* Send SSH_MSG_CHANNEL_OPEN_FAILURE */
        uint8_t open_failure[256];
        size_t failure_len = 0;

        open_failure[failure_len++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
        write_uint32_be(open_failure + failure_len, client_channel_id);  /* recipient_channel */
        failure_len += 4;
        write_uint32_be(open_failure + failure_len, 3);  /* SSH_OPEN_UNKNOWN_CHANNEL_TYPE */
        failure_len += 4;
        failure_len += write_string(open_failure + failure_len, "Unknown channel type", 20);
        failure_len += write_string(open_failure + failure_len, "", 0);  /* language tag */

        send_packet(client_fd, open_failure, failure_len);
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR,
                       "Only session channel type is supported");
        close(client_fd);
        return;
    }

    /* Build SSH_MSG_CHANNEL_OPEN_CONFIRMATION */
    uint8_t channel_confirm[256];
    size_t channel_confirm_len = 0;

    channel_confirm[channel_confirm_len++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;

    /* recipient_channel (client's channel ID) */
    write_uint32_be(channel_confirm + channel_confirm_len, client_channel_id);
    channel_confirm_len += 4;

    /* sender_channel (server's channel ID) */
    write_uint32_be(channel_confirm + channel_confirm_len, server_channel_id);
    channel_confirm_len += 4;

    /* initial_window_size (server's receive window) */
    write_uint32_be(channel_confirm + channel_confirm_len, server_window_size);
    channel_confirm_len += 4;

    /* maximum_packet_size (server's max packet) */
    write_uint32_be(channel_confirm + channel_confirm_len, server_max_packet);
    channel_confirm_len += 4;

    /* Send SSH_MSG_CHANNEL_OPEN_CONFIRMATION */
    if (send_packet(client_fd, channel_confirm, channel_confirm_len) < 0) {
        close(client_fd);
        return;
    }
                    
    /* Store channel state (for future use in Phase 1.14+) */
    /* For now, we just have local variables. In a production server, we'd
     * store this in a channel state structure. */

    /* ======================
     * Phase 1.13: Channel Requests
     * ====================== */

    /* Clients typically send multiple channel requests:
     * 1. pty-req (PTY allocation)
     * 2. shell or exec (what to run)
     * We need to handle these in a loop */

    int shell_ready = 0;  /* Flag: ready to send data after shell request */
    int requests_done = 0;

    while (!requests_done) {
        uint8_t channel_request[4096];
        ssize_t channel_request_len;
        uint8_t *req_ptr;
        uint32_t req_recipient_channel;
        uint32_t req_type_len;
        char req_type[64];
        uint8_t want_reply;

        /* Receive SSH_MSG_CHANNEL_REQUEST */
        channel_request_len = recv_packet(client_fd, channel_request, sizeof(channel_request));
        if (channel_request_len <= 0) {
            close(client_fd);
            return;
        }

        if (channel_request[0] != SSH_MSG_CHANNEL_REQUEST) {
            /* This might be a different message (e.g., CHANNEL_DATA), break loop */
            break;
        }

        /* Parse SSH_MSG_CHANNEL_REQUEST:
         * byte      SSH_MSG_CHANNEL_REQUEST (98)
         * uint32    recipient_channel
         * string    request_type
         * boolean   want_reply
         * ... request-specific data (we mostly ignore)
         */
        req_ptr = channel_request + 1;

        /* Parse recipient_channel */
        req_recipient_channel = read_uint32_be(req_ptr);
        req_ptr += 4;

        /* Parse request_type */
        req_type_len = read_uint32_be(req_ptr);
        req_ptr += 4;
        if (req_type_len >= sizeof(req_type)) {
            close(client_fd);
            return;
        }
        memcpy(req_type, req_ptr, req_type_len);
        req_type[req_type_len] = '\0';
        req_ptr += req_type_len;

        /* Parse want_reply */
        want_reply = *req_ptr;
        req_ptr += 1;

                                
        /* Verify recipient channel matches our channel */
        if (req_recipient_channel != server_channel_id) {
            close(client_fd);
            return;
        }

        /* Handle different request types */
        int request_accepted = 0;

        if (strcmp(req_type, STR_REQ_PTY) == 0) {
            /* PTY allocation request
             * Format: string TERM, uint32 width_chars, uint32 height_rows,
             *         uint32 width_pixels, uint32 height_pixels, string modes
             * For minimal implementation: accept but ignore details */
                        request_accepted = 1;

        } else if (strcmp(req_type, STR_REQ_SHELL) == 0) {
            /* Shell request - no additional data */
                        request_accepted = 1;
            shell_ready = 1;  /* Mark ready to send data */
            requests_done = 1;  /* Typically shell is the last request */

        } else if (strcmp(req_type, STR_REQ_EXEC) == 0) {
            /* Exec request - format: string command
             * For minimal implementation: accept but ignore command */
            uint32_t cmd_len = read_uint32_be(req_ptr);
            char command[256];
            if (cmd_len < sizeof(command)) {
                memcpy(command, req_ptr + 4, cmd_len);
                command[cmd_len] = '\0';
                            } else {
                            }
            request_accepted = 1;
            shell_ready = 1;  /* Ready to send data after exec */
            requests_done = 1;  /* Exec is typically the last request */

        } else if (strcmp(req_type, STR_REQ_ENV) == 0) {
            /* Environment variable request - ignore */
                        request_accepted = 1;

        } else {
            /* Unknown request type */
                        request_accepted = 0;
        }

        /* Send reply if requested */
        if (want_reply) {
            uint8_t reply_msg[16];
            size_t reply_len = 0;

            if (request_accepted) {
                /* Send SSH_MSG_CHANNEL_SUCCESS */
                reply_msg[reply_len++] = SSH_MSG_CHANNEL_SUCCESS;
                write_uint32_be(reply_msg + reply_len, client_channel_id);
                reply_len += 4;

                if (send_packet(client_fd, reply_msg, reply_len) < 0) {
                    close(client_fd);
                    return;
                }
                
            } else {
                /* Send SSH_MSG_CHANNEL_FAILURE */
                reply_msg[reply_len++] = SSH_MSG_CHANNEL_FAILURE;
                write_uint32_be(reply_msg + reply_len, client_channel_id);
                reply_len += 4;

                if (send_packet(client_fd, reply_msg, reply_len) < 0) {
                    close(client_fd);
                    return;
                }
                            }
        }
    }

    
    /* ======================
     * Phase 1.14: Data Transfer
     * ====================== */

    if (shell_ready) {
        const char *hello_msg = STR_HELLO;
        size_t hello_len = strlen(hello_msg);
        uint8_t data_msg[1024];
        size_t data_msg_len = 0;

        /* Build SSH_MSG_CHANNEL_DATA:
         * byte      SSH_MSG_CHANNEL_DATA (94)
         * uint32    recipient_channel (client's channel ID)
         * string    data (length-prefixed)
         */
        data_msg[data_msg_len++] = SSH_MSG_CHANNEL_DATA;

        /* recipient_channel = client's channel ID */
        write_uint32_be(data_msg + data_msg_len, client_channel_id);
        data_msg_len += 4;

        /* data as SSH string (length + content) */
        data_msg_len += write_string(data_msg + data_msg_len, hello_msg, hello_len);

        /* Check window size before sending */
        if (hello_len > client_window_size) {
            /* In production, we'd need to split data or wait for WINDOW_ADJUST */
            /* For now, send anyway since our message is small */
        }

        /* Send the data */
        if (send_packet(client_fd, data_msg, data_msg_len) < 0) {
            close(client_fd);
            return;
        }
        
        /* Update client window size (track how much we've sent) */
        client_window_size -= hello_len;
        
    } else {
            }

    /* ======================
     * Phase 1.15: Channel Close
     * ====================== */

    
    /* Send SSH_MSG_CHANNEL_EOF (no more data) */
    uint8_t eof_msg[16];
    size_t eof_len = 0;

    eof_msg[eof_len++] = SSH_MSG_CHANNEL_EOF;
    write_uint32_be(eof_msg + eof_len, client_channel_id);
    eof_len += 4;

    if (send_packet(client_fd, eof_msg, eof_len) < 0) {
        close(client_fd);
        return;
    }
    
    /* Send SSH_MSG_CHANNEL_CLOSE */
    uint8_t close_msg[16];
    size_t close_len = 0;

    close_msg[close_len++] = SSH_MSG_CHANNEL_CLOSE;
    write_uint32_be(close_msg + close_len, client_channel_id);
    close_len += 4;

    if (send_packet(client_fd, close_msg, close_len) < 0) {
        close(client_fd);
        return;
    }
    
    /* Wait for client's SSH_MSG_CHANNEL_CLOSE */
    uint8_t client_close[16];
    ssize_t client_close_len;

    client_close_len = recv_packet(client_fd, client_close, sizeof(client_close));
    if (client_close_len <= 0) {
        /* This is acceptable - client may have already closed */
    } else if (client_close[0] != SSH_MSG_CHANNEL_CLOSE) {
        /* Non-fatal, continue with cleanup */
    } else {
            }

    /* Close TCP connection */
        close(client_fd);
    }

int main(int argc, char *argv[]) {
    int listen_fd;
    int client_fd;
    struct sockaddr_in client_addr;
    uint8_t host_public_key[32];
    uint8_t host_private_key[64];

    (void)argc;
    (void)argv;

                            
    /* Initialize libsodium */
    if (sodium_init() < 0) {
        return 1;
    }
    
    /* Generate Ed25519 host key pair */
    crypto_sign_keypair(host_public_key, host_private_key);
    
    /* Create TCP server socket */
    listen_fd = create_server_socket(SERVER_PORT);
    if (listen_fd < 0) {
        return 1;
    }
    
    /* Main server loop - accept connections */
    
    while (1) {
        /* Accept client connection */
        client_fd = accept_client(listen_fd, &client_addr);
        if (client_fd < 0) {
            continue;
        }

        /* Handle client connection */
        handle_client(client_fd, &client_addr, host_public_key, host_private_key);

        /* For now, only handle one connection then exit */
        /* TODO: In production, this should loop forever or handle multiple clients */
        break;
    }

    close(listen_fd);
    
    return 0;
}
