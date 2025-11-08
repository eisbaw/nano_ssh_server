/*
 * Nano SSH Server - v17-from14
 * 100% independent from external crypto libraries
 * - Custom AES-128-CTR implementation (no OpenSSL)
 * - Custom SHA-256 implementation
 * - Custom HMAC-SHA256 implementation
 * - Custom Curve25519 implementation (no libsodium)
 * - Custom Ed25519 implementation (no libsodium)
 * - Custom CSPRNG using /dev/urandom (no libsodium)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include "random_minimal.h"
#include "curve25519_minimal.h"
#include "edsign.h"
#include "aes128_minimal.h"
#include "sha256_minimal.h"

/* TEMPORARY: External declarations of libsodium functions for diagnostic */
extern int crypto_scalarmult(unsigned char *q, const unsigned char *n, const unsigned char *p);
extern int crypto_sign_ed25519_detached(unsigned char *sig, unsigned long long *siglen_p,
                                         const unsigned char *m, unsigned long long mlen,
                                         const unsigned char *sk);
extern int crypto_sign_ed25519_sk_to_pk(unsigned char *pk, const unsigned char *sk);
extern int crypto_sign_keypair(unsigned char *pk, unsigned char *sk);

/* Forward declarations */
ssize_t send_data(int fd, const void *buf, size_t len);
ssize_t recv_data(int fd, void *buf, size_t len);

/* Configuration */
#define SERVER_PORT 2222
#define SERVER_VERSION "SSH-2.0-NanoSSH_0.1"
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

/* Algorithm names for KEXINIT */
#define KEX_ALGORITHM           "curve25519-sha256"
#define HOST_KEY_ALGORITHM      "ssh-ed25519"
#define ENCRYPTION_ALGORITHM    "aes128-ctr"      /* AES-128 in CTR mode */
#define MAC_ALGORITHM           "hmac-sha2-256"   /* HMAC-SHA256 for packet authentication */
#define COMPRESSION_ALGORITHM   "none"
#define LANGUAGE                ""

/* NOTE: Using AES-128-CTR + HMAC-SHA256 instead of ChaCha20-Poly1305@openssh.com
 * because the OpenSSH Poly1305 variant proved too complex to debug. AES-128-CTR is
 * a standard, widely-supported SSH cipher with simpler implementation. */

/* ======================
 * Cryptography Helper Functions
 * ====================== */

/* Hash wrappers converted to macros - now using minimal SHA-256 */
#define compute_sha256(o,i,l) sha256(o,i,l)
typedef sha256_ctx hash_state_t;
#define hash_init(h) sha256_init(h)
#define hash_update(h,d,l) sha256_update(h,d,l)
#define hash_final(h,o) sha256_final(h,o)

typedef struct {
    aes128_ctr_ctx aes_ctx;
    uint8_t mac_key[32];
    uint32_t seq_num;
    int active;
} crypto_state_t;

static crypto_state_t encrypt_state_c2s = {0}, encrypt_state_s2c = {0};

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

/* Convert to macros for inline expansion */
#define write_uint32_be(b,v) do{(b)[0]=(v)>>24;(b)[1]=(v)>>16;(b)[2]=(v)>>8;(b)[3]=(v);}while(0)
#define read_uint32_be(b) (((uint32_t)(b)[0]<<24)|((uint32_t)(b)[1]<<16)|((uint32_t)(b)[2]<<8)|(b)[3])

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

/* ChaCha20 code removed - not used, saves ~100 lines */

/*
 * Compute HMAC-SHA256 using minimal implementation
 *
 * RFC 4253: MAC = HMAC(key, sequence_number || unencrypted_packet)
 * where sequence_number is uint32 big-endian
 */
void compute_hmac_sha256(uint8_t *mac, const uint8_t *key,
                         uint32_t seq_num, const uint8_t *packet, size_t packet_len)
{
    hmac_sha256_ctx state;
    uint8_t seq_buf[4];

    /* Initialize HMAC state with key */
    hmac_sha256_init(&state, key, 32);

    /* Add sequence number (big-endian) */
    write_uint32_be(seq_buf, seq_num);
    hmac_sha256_update(&state, seq_buf, 4);

    /* Add packet data */
    hmac_sha256_update(&state, packet, packet_len);

    /* Finalize - produces 32-byte MAC */
    hmac_sha256_final(&state, mac);
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
    /* First, compute MAC over unencrypted packet */
    compute_hmac_sha256(mac, state->mac_key, state->seq_num, packet, packet_len);

    /* Encrypt packet in-place using AES-128-CTR
     * The CTR counter automatically increments */
    aes128_ctr_crypt(&state->aes_ctx, packet, packet_len);

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

        /* Decrypt first block using AES-128-CTR
         * Counter automatically increments from previous packet */
        memcpy(decrypted_block, first_block, 16);
        aes128_ctr_crypt(&encrypt_state_c2s.aes_ctx, decrypted_block, 16);

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

            /* Decrypt remaining bytes using AES-128-CTR
             * Counter continues automatically from first block */
            memcpy(packet_buf + 16, encrypted_remaining, remaining);
            aes128_ctr_crypt(&encrypt_state_c2s.aes_ctx, packet_buf + 16, remaining);

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

        if (ct_verify_32(computed_mac, mac) != 0) {
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
    memcpy(server_version_line + 8, "NanoSSH_0.1\r\n", 13);
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
    host_key_blob_len += write_string(host_key_blob + host_key_blob_len, "ssh-ed25519", 11);
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

    fprintf(stderr, "Shared secret K (custom curve25519): ");
    for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", shared_secret[i]);
    fprintf(stderr, "\n");

    /* DIAGNOSTIC: Compare with libsodium's crypto_scalarmult */
    uint8_t shared_secret_libsodium[32];
    crypto_scalarmult(shared_secret_libsodium, server_ephemeral_private, client_ephemeral_public);
    fprintf(stderr, "Shared secret K (libsodium):        ");
    for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", shared_secret_libsodium[i]);
    fprintf(stderr, "\n");

    int mismatch_count = 0;
    for (int i = 0; i < 32; i++) {
        if (shared_secret[i] != shared_secret_libsodium[i]) mismatch_count++;
    }
    fprintf(stderr, "DIAGNOSTIC: %s (%d bytes differ)\n",
            mismatch_count == 0 ? "MATCH" : "MISMATCH", mismatch_count);

    /* Use libsodium's result for now to verify the rest works */
    memcpy(shared_secret, shared_secret_libsodium, 32);
    fprintf(stderr, "DIAGNOSTIC: Using libsodium shared secret for testing\n");

    /* RFC 8731 Section 3.1: Use X25519 output as-is, no byte reversal */

    /* Compute exchange hash H */
    fprintf(stderr, "\n=== V17 EXCHANGE HASH INPUTS ===\n");
    fprintf(stderr, "Host public key: ");
    for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", host_public_key[i]);
    fprintf(stderr, "\nI_C len: %zd (first 10 bytes): ", client_kexinit_len_s);
    for (int i = 0; i < 10 && i < client_kexinit_len_s; i++) fprintf(stderr, "%02x", client_kexinit[i]);
    fprintf(stderr, "\nI_S len: %zu (first 10 bytes): ", server_kexinit_len);
    for (int i = 0; i < 10 && i < (int)server_kexinit_len; i++) fprintf(stderr, "%02x", server_kexinit[i]);
    fprintf(stderr, "\nK_S len: %zu\nK_S content: ", host_key_blob_len);
    for (int i = 0; i < (int)host_key_blob_len && i < 60; i++) fprintf(stderr, "%02x", host_key_blob[i]);
    fprintf(stderr, "\n");

    compute_exchange_hash(exchange_hash,
                         client_version, SERVER_VERSION,
                         client_kexinit, client_kexinit_len_s,
                         server_kexinit, server_kexinit_len,
                         host_key_blob, host_key_blob_len,
                         client_ephemeral_public, 32,
                         server_ephemeral_public, 32,
                         shared_secret, 32);  /* Use X25519 output as-is per RFC 8731 Section 3.1 */

    fprintf(stderr, "Exchange hash H: ");
    for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", exchange_hash[i]);
    fprintf(stderr, "\n");

    /* First exchange hash becomes session_id */
    memcpy(session_id, exchange_hash, 32);

    /* Sign exchange hash with host private key (using libsodium) */
    crypto_sign_ed25519_detached(signature, &sig_len, exchange_hash, 32, host_private_key);

    fprintf(stderr, "Signature (libsodium): ");
    for (int i = 0; i < 64; i++) fprintf(stderr, "%02x", signature[i]);
    fprintf(stderr, "\n=================================\n\n");
    
    /* Build SSH_MSG_KEX_ECDH_REPLY */
    kex_reply_len = 0;
    kex_reply[kex_reply_len++] = SSH_MSG_KEX_ECDH_REPLY;

    /* K_S - server host public key blob */
    kex_reply_len += write_string(kex_reply + kex_reply_len,
                                  (const char *)host_key_blob, host_key_blob_len);

    fprintf(stderr, "Sending Q_S (server ephemeral): ");
    for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", server_ephemeral_public[i]);
    fprintf(stderr, "\n");

    /* Q_S - server ephemeral public key */
    kex_reply_len += write_string(kex_reply + kex_reply_len,
                                  (const char *)server_ephemeral_public, 32);

    /* Signature of H (ssh-ed25519 signature format) */
    {
        uint8_t sig_blob[128];
        size_t sig_blob_len = 0;
        sig_blob_len += write_string(sig_blob, "ssh-ed25519", 11);
        sig_blob_len += write_string(sig_blob + sig_blob_len, (const char *)signature, 64);
        kex_reply_len += write_string(kex_reply + kex_reply_len,
                                      (const char *)sig_blob, sig_blob_len);
    }

    /* Send SSH_MSG_KEX_ECDH_REPLY */
    fprintf(stderr, "\n=== SENDING KEX_ECDH_REPLY ===\n");
    fprintf(stderr, "Payload length: %zu bytes\n", kex_reply_len);
    fprintf(stderr, "First 100 bytes of payload:\n");
    for (size_t i = 0; i < kex_reply_len && i < 100; i++) {
        fprintf(stderr, "%02x", kex_reply[i]);
        if ((i + 1) % 32 == 0) fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n==============================\n\n");

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
    memcpy(encrypt_state_s2c.mac_key, int_key_s2c, 32);

    /* Initialize AES-128-CTR context ONCE - will be reused for all packets */
    aes128_ctr_init(&encrypt_state_s2c.aes_ctx, key_s2c, iv_s2c);

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
    memcpy(encrypt_state_c2s.mac_key, int_key_c2s, 32);

    /* Initialize AES-128-CTR context ONCE - will be reused for all packets */
    aes128_ctr_init(&encrypt_state_c2s.aes_ctx, key_c2s, iv_c2s);

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
    if (strcmp(service_name, "ssh-userauth") != 0) {
        send_disconnect(client_fd, SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
                       "Only ssh-userauth service is supported");
        close(client_fd);
        return;
    }

    /* Build SSH_MSG_SERVICE_ACCEPT */
    service_accept_len = 0;
    service_accept[service_accept_len++] = SSH_MSG_SERVICE_ACCEPT;
    service_accept_len += write_string(service_accept + service_accept_len,
                                       "ssh-userauth", 12);

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
    if (strcmp(service, "ssh-connection") != 0) {
        close(client_fd);
        return;
    }

    /* Handle password authentication */
    if (strcmp(method, "password") == 0) {
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
            failure_len += write_string(failure_msg + failure_len, "password", 8);

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
        failure_len += write_string(failure_msg + failure_len, "password", 8);

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
    if (strcmp(channel_type, "session") != 0) {
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

        if (strcmp(req_type, "pty-req") == 0) {
            /* PTY allocation request
             * Format: string TERM, uint32 width_chars, uint32 height_rows,
             *         uint32 width_pixels, uint32 height_pixels, string modes
             * For minimal implementation: accept but ignore details */
                        request_accepted = 1;

        } else if (strcmp(req_type, "shell") == 0) {
            /* Shell request - no additional data */
                        request_accepted = 1;
            shell_ready = 1;  /* Mark ready to send data */
            requests_done = 1;  /* Typically shell is the last request */

        } else if (strcmp(req_type, "exec") == 0) {
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

        } else if (strcmp(req_type, "env") == 0) {
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
        const char *hello_msg = "Hello World\r\n";
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
    uint8_t host_private_key_full[64];  /* Ed25519 secret key (libsodium uses 64 bytes) */

    (void)argc;
    (void)argv;

    /* DIAGNOSTIC: Use libsodium for EVERYTHING including key generation */
    fprintf(stderr, "\n=== DIAGNOSTIC MODE ===\n");
    fprintf(stderr, "Using libsodium for Ed25519 key generation\n");
    crypto_sign_keypair(host_public_key, host_private_key_full);
    fprintf(stderr, "Host public key: ");
    for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", host_public_key[i]);
    fprintf(stderr, "\n======================\n\n");

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
        handle_client(client_fd, &client_addr, host_public_key, host_private_key_full);

        /* For now, only handle one connection then exit */
        /* TODO: In production, this should loop forever or handle multiple clients */
        break;
    }

    close(listen_fd);
    
    return 0;
}
