/*
 * Nano SSH Server - v0-vanilla
 * World's smallest SSH server for microcontrollers
 *
 * Phase 1: Working implementation (correctness first, size later)
 *
 * This version prioritizes:
 * - Correctness and completeness
 * - Code readability
 * - Standard library usage
 * - Comprehensive error handling
 *
 * Size optimization comes in later versions (v2+)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sodium.h>

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
#define BLOCK_SIZE 8           /* Block size before encryption (default) */

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

/* Algorithm names for KEXINIT */
#define KEX_ALGORITHM           "curve25519-sha256"
#define HOST_KEY_ALGORITHM      "ssh-ed25519"
#define ENCRYPTION_ALGORITHM    "chacha20-poly1305@openssh.com"
#define MAC_ALGORITHM           ""    /* Not needed with AEAD */
#define COMPRESSION_ALGORITHM   "none"
#define LANGUAGE                ""

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
typedef struct {
    crypto_hash_sha256_state state;
} hash_state_t;

void hash_init(hash_state_t *h) {
    crypto_hash_sha256_init(&h->state);
}

/*
 * Encryption state for ChaCha20-Poly1305
 */
typedef struct {
    uint8_t key[32];      /* Encryption key */
    uint32_t seq_num;     /* Sequence number for nonce */
    int active;           /* 1 if encryption is active */
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
        fprintf(stderr, "Error: String too long: %u (max %zu)\n", *str_len, out_max - 1);
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
 * OpenSSH uses a two-key variant:
 * - K_1 (counter=0): Encrypts packet_length (first 4 bytes)
 * - K_2 (counter=1): Encrypts rest of packet
 * - Poly1305 key derived from counter=0
 * - Nonce: 8 zero bytes + 4-byte sequence number (big-endian)
 *
 * Input: unencrypted packet (packet_length || padding_length || payload || padding)
 * Output: encrypted packet_length || encrypted rest || MAC
 */
int chacha20_poly1305_encrypt(uint8_t *packet, size_t packet_len,
                               const uint8_t key[32], uint32_t seq_num,
                               uint8_t mac[16])
{
    uint8_t nonce[12];
    uint8_t poly_key[32];

    /* Build nonce: 8 zero bytes + 4-byte sequence number (big-endian) */
    memset(nonce, 0, 8);
    write_uint32_be(nonce + 8, seq_num);

    /* Generate Poly1305 key from ChaCha20(key, nonce, counter=0) */
    memset(poly_key, 0, 32);
    crypto_stream_chacha20_ietf_xor_ic(poly_key, poly_key, 32, nonce, 0, key);

    /* Encrypt packet_length (first 4 bytes) with counter=0 */
    crypto_stream_chacha20_ietf_xor_ic(packet, packet, 4, nonce, 0, key);

    /* Encrypt rest of packet with counter=1 */
    if (packet_len > 4) {
        crypto_stream_chacha20_ietf_xor_ic(packet + 4, packet + 4, packet_len - 4,
                                           nonce, 1, key);
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
 * Input: encrypted packet_length || encrypted rest, MAC (separate)
 * Output: decrypted packet (packet_length || padding_length || payload || padding)
 * Returns: 0 on success, -1 on MAC verification failure
 */
int chacha20_poly1305_decrypt(uint8_t *packet, size_t packet_len,
                               const uint8_t mac[16],
                               const uint8_t key[32], uint32_t seq_num)
{
    uint8_t nonce[12];
    uint8_t poly_key[32];
    int mac_valid;

    /* Build nonce: 8 zero bytes + 4-byte sequence number (big-endian) */
    memset(nonce, 0, 8);
    write_uint32_be(nonce + 8, seq_num);

    /* Generate Poly1305 key from ChaCha20(key, nonce, counter=0) */
    memset(poly_key, 0, 32);
    crypto_stream_chacha20_ietf_xor_ic(poly_key, poly_key, 32, nonce, 0, key);

    /* Verify Poly1305 MAC over encrypted packet */
    mac_valid = crypto_onetimeauth_poly1305_verify(mac, packet, packet_len, poly_key);

    /* Clear Poly1305 key */
    memset(poly_key, 0, 32);

    if (mac_valid != 0) {
        fprintf(stderr, "Error: MAC verification failed\n");
        memset(nonce, 0, 12);
        return -1;
    }

    /* Decrypt packet_length (first 4 bytes) with counter=0 */
    crypto_stream_chacha20_ietf_xor_ic(packet, packet, 4, nonce, 0, key);

    /* Decrypt rest of packet with counter=1 */
    if (packet_len > 4) {
        crypto_stream_chacha20_ietf_xor_ic(packet + 4, packet + 4, packet_len - 4,
                                           nonce, 1, key);
    }

    /* Clear nonce */
    memset(nonce, 0, 12);

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
    uint8_t mac[16];
    uint8_t padding_len;
    uint32_t packet_len;
    size_t total_len;

    if (payload_len > MAX_PACKET_SIZE - 256) {
        fprintf(stderr, "Error: Payload too large: %zu\n", payload_len);
        return -1;
    }

    /* Calculate padding */
    padding_len = calculate_padding(payload_len, BLOCK_SIZE);

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
        /* Encrypt packet in-place and generate MAC */
        if (chacha20_poly1305_encrypt(packet, total_len,
                                      encrypt_state_s2c.key,
                                      encrypt_state_s2c.seq_num,
                                      mac) < 0) {
            fprintf(stderr, "Error: Encryption failed\n");
            return -1;
        }

        /* Send encrypted packet */
        if (send_data(fd, packet, total_len) < 0) {
            fprintf(stderr, "Error: Failed to send encrypted packet\n");
            return -1;
        }

        /* Send MAC */
        if (send_data(fd, mac, 16) < 0) {
            fprintf(stderr, "Error: Failed to send MAC\n");
            return -1;
        }

        /* Increment sequence number */
        encrypt_state_s2c.seq_num++;

    } else {
        /* Send unencrypted packet */
        if (send_data(fd, packet, total_len) < 0) {
            fprintf(stderr, "Error: Failed to send packet\n");
            return -1;
        }
    }

    return 0;
}

/*
 * Receive SSH packet (supports both encrypted and unencrypted)
 *
 * Returns: payload length on success, -1 on error, 0 on connection close
 * Output: payload buffer filled with packet payload
 */
ssize_t recv_packet(int fd, uint8_t *payload, size_t payload_max) {
    uint8_t header[4];
    uint8_t mac[16];
    uint32_t packet_len;
    uint8_t padding_len;
    uint8_t *packet_buf;
    size_t payload_len;
    size_t total_packet_len;
    ssize_t n;

    if (encrypt_state_c2s.active) {
        /* ===== ENCRYPTED MODE ===== */

        /* Read encrypted packet length (4 bytes) */
        n = recv_data(fd, header, 4);
        if (n <= 0) {
            return n;  /* Error or connection closed */
        }
        if (n < 4) {
            fprintf(stderr, "Error: Incomplete encrypted packet length\n");
            return -1;
        }

        /* Read rest of encrypted packet + MAC
         * We need to read the rest based on decrypted length, but we don't know it yet.
         * So we need to decrypt the length first, then read the rest.
         *
         * For OpenSSH ChaCha20-Poly1305:
         * - First decrypt packet_length to know how much more to read
         * - Read (packet_len) more bytes + 16 byte MAC
         */

        /* Decrypt packet_length to determine how much to read */
        {
            uint8_t nonce[12];
            memset(nonce, 0, 8);
            write_uint32_be(nonce + 8, encrypt_state_c2s.seq_num);
            crypto_stream_chacha20_ietf_xor_ic(header, header, 4, nonce, 0,
                                                encrypt_state_c2s.key);
        }

        packet_len = read_uint32_be(header);

        /* Validate packet length */
        if (packet_len < 5 || packet_len > MAX_PACKET_SIZE) {
            fprintf(stderr, "Error: Invalid decrypted packet length: %u\n", packet_len);
            return -1;
        }

        /* Allocate buffer for: encrypted_length + rest_of_packet */
        total_packet_len = 4 + packet_len;
        packet_buf = malloc(total_packet_len);
        if (!packet_buf) {
            fprintf(stderr, "Error: malloc failed\n");
            return -1;
        }

        /* Put encrypted length back into buffer (for MAC verification) */
        crypto_stream_chacha20_ietf_xor_ic(header, header, 4,
                                            (uint8_t[]){0,0,0,0,0,0,0,0,
                                                       (encrypt_state_c2s.seq_num >> 24) & 0xFF,
                                                       (encrypt_state_c2s.seq_num >> 16) & 0xFF,
                                                       (encrypt_state_c2s.seq_num >> 8) & 0xFF,
                                                       encrypt_state_c2s.seq_num & 0xFF},
                                            0, encrypt_state_c2s.key);
        memcpy(packet_buf, header, 4);

        /* Read rest of encrypted packet */
        n = recv_data(fd, packet_buf + 4, packet_len);
        if (n <= 0) {
            free(packet_buf);
            return n;
        }
        if (n < (ssize_t)packet_len) {
            fprintf(stderr, "Error: Incomplete encrypted packet: got %zd, expected %u\n",
                    n, packet_len);
            free(packet_buf);
            return -1;
        }

        /* Read MAC (16 bytes) */
        n = recv_data(fd, mac, 16);
        if (n <= 0) {
            free(packet_buf);
            return n;
        }
        if (n < 16) {
            fprintf(stderr, "Error: Incomplete MAC\n");
            free(packet_buf);
            return -1;
        }

        /* Decrypt and verify MAC */
        if (chacha20_poly1305_decrypt(packet_buf, total_packet_len, mac,
                                      encrypt_state_c2s.key,
                                      encrypt_state_c2s.seq_num) < 0) {
            fprintf(stderr, "Error: Decryption failed\n");
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
            fprintf(stderr, "Error: Invalid padding length: %u\n", padding_len);
            free(packet_buf);
            return -1;
        }

        payload_len = packet_len - 1 - padding_len;

        /* Check payload buffer size */
        if (payload_len > payload_max) {
            fprintf(stderr, "Error: Payload too large: %zu (max %zu)\n",
                    payload_len, payload_max);
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
            fprintf(stderr, "Error: Incomplete packet length\n");
            return -1;
        }

        packet_len = read_uint32_be(header);

        /* Validate packet length */
        if (packet_len < 5 || packet_len > MAX_PACKET_SIZE) {
            fprintf(stderr, "Error: Invalid packet length: %u\n", packet_len);
            return -1;
        }

        /* Allocate buffer for rest of packet */
        packet_buf = malloc(packet_len);
        if (!packet_buf) {
            fprintf(stderr, "Error: malloc failed\n");
            return -1;
        }

        /* Read rest of packet */
        n = recv_data(fd, packet_buf, packet_len);
        if (n <= 0) {
            free(packet_buf);
            return n;
        }
        if (n < (ssize_t)packet_len) {
            fprintf(stderr, "Error: Incomplete packet: got %zd, expected %u\n", n, packet_len);
            free(packet_buf);
            return -1;
        }

        /* Extract padding length */
        padding_len = packet_buf[0];

        /* Calculate payload length */
        if (padding_len >= packet_len - 1) {
            fprintf(stderr, "Error: Invalid padding length: %u\n", padding_len);
            free(packet_buf);
            return -1;
        }

        payload_len = packet_len - 1 - padding_len;

        /* Check payload buffer size */
        if (payload_len > payload_max) {
            fprintf(stderr, "Error: Payload too large: %zu (max %zu)\n",
                    payload_len, payload_max);
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
        fprintf(stderr, "Error: socket() failed: %s\n", strerror(errno));
        return -1;
    }

    /* Set SO_REUSEADDR to avoid "Address already in use" errors */
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "Warning: setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
    }

    /* Bind to address and port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: bind() failed: %s\n", strerror(errno));
        close(listen_fd);
        return -1;
    }

    /* Listen for connections */
    if (listen(listen_fd, BACKLOG) < 0) {
        fprintf(stderr, "Error: listen() failed: %s\n", strerror(errno));
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
        fprintf(stderr, "Error: accept() failed: %s\n", strerror(errno));
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
            fprintf(stderr, "Error: send() failed: %s\n", strerror(errno));
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "Error: send() returned 0 (connection closed)\n");
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
        fprintf(stderr, "Error: recv() failed: %s\n", strerror(errno));
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
    char client_ip[INET_ADDRSTRLEN];
    char client_version[256];
    char server_version_line[256];
    ssize_t n;
    int i, version_len;

    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("\n[+] Client connected: %s:%d\n", client_ip, ntohs(client_addr->sin_port));

    /* ======================
     * Phase 1.2: Version Exchange
     * ====================== */

    /* Send server version string (plaintext, ends with \r\n) */
    snprintf(server_version_line, sizeof(server_version_line), "%s\r\n", SERVER_VERSION);
    if (send_data(client_fd, server_version_line, strlen(server_version_line)) < 0) {
        fprintf(stderr, "[-] Failed to send version string\n");
        close(client_fd);
        return;
    }
    printf("[+] Sent version: %s", server_version_line);

    /* Receive client version string (read until \n) */
    memset(client_version, 0, sizeof(client_version));
    version_len = 0;

    for (i = 0; i < (int)sizeof(client_version) - 1; i++) {
        n = recv_data(client_fd, &client_version[i], 1);
        if (n <= 0) {
            fprintf(stderr, "[-] Failed to receive version string\n");
            close(client_fd);
            return;
        }
        version_len++;

        /* Stop at \n */
        if (client_version[i] == '\n') {
            break;
        }
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

    printf("[+] Received version: %s\n", client_version);

    /* Validate client version starts with "SSH-2.0-" */
    if (strncmp(client_version, "SSH-2.0-", 8) != 0) {
        fprintf(stderr, "[-] Invalid SSH version: %s\n", client_version);
        fprintf(stderr, "[-] Expected: SSH-2.0-*\n");
        close(client_fd);
        return;
    }

    printf("[+] Version exchange complete\n");

    /* ======================
     * Phase 1.5: KEXINIT Exchange
     * ====================== */

    uint8_t server_kexinit[1024];
    uint8_t client_kexinit[4096];  /* Client sends comprehensive algorithm lists */
    size_t server_kexinit_len;
    ssize_t client_kexinit_len_s;

    /* Build our KEXINIT payload */
    server_kexinit_len = build_kexinit(server_kexinit);
    printf("[+] Built KEXINIT payload (%zu bytes)\n", server_kexinit_len);

    /* Send our KEXINIT */
    if (send_packet(client_fd, server_kexinit, server_kexinit_len) < 0) {
        fprintf(stderr, "[-] Failed to send KEXINIT\n");
        close(client_fd);
        return;
    }
    printf("[+] Sent KEXINIT\n");

    /* Receive client's KEXINIT */
    client_kexinit_len_s = recv_packet(client_fd, client_kexinit, sizeof(client_kexinit));
    if (client_kexinit_len_s <= 0) {
        fprintf(stderr, "[-] Failed to receive client KEXINIT\n");
        close(client_fd);
        return;
    }
    printf("[+] Received client KEXINIT (%zd bytes)\n", client_kexinit_len_s);

    /* Validate it's a KEXINIT message */
    if (client_kexinit[0] != SSH_MSG_KEXINIT) {
        fprintf(stderr, "[-] Expected KEXINIT (20), got message type %d\n", client_kexinit[0]);
        close(client_fd);
        return;
    }

    printf("[+] KEXINIT exchange complete\n");

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
    printf("[+] Generated ephemeral key pair\n");

    /* Build server host key blob (ssh-ed25519 format) */
    host_key_blob_len = 0;
    host_key_blob_len += write_string(host_key_blob + host_key_blob_len, "ssh-ed25519", 11);
    host_key_blob_len += write_string(host_key_blob + host_key_blob_len,
                                      (const char *)host_public_key, 32);
    printf("[+] Built host key blob (%zu bytes)\n", host_key_blob_len);

    /* Receive SSH_MSG_KEX_ECDH_INIT from client */
    kex_init_len = recv_packet(client_fd, kex_init_msg, sizeof(kex_init_msg));
    if (kex_init_len <= 0) {
        fprintf(stderr, "[-] Failed to receive KEX_ECDH_INIT\n");
        close(client_fd);
        return;
    }

    if (kex_init_msg[0] != SSH_MSG_KEX_ECDH_INIT) {
        fprintf(stderr, "[-] Expected KEX_ECDH_INIT (30), got %d\n", kex_init_msg[0]);
        close(client_fd);
        return;
    }

    /* Extract client ephemeral public key (Q_C) */
    if (read_string(kex_init_msg + 1, kex_init_len - 1,
                   (char *)client_ephemeral_public, 33, &client_eph_str_len) == 0) {
        fprintf(stderr, "[-] Failed to parse client ephemeral public key\n");
        close(client_fd);
        return;
    }

    if (client_eph_str_len != 32) {
        fprintf(stderr, "[-] Invalid client ephemeral key length: %u\n", client_eph_str_len);
        close(client_fd);
        return;
    }
    printf("[+] Received client ephemeral public key (32 bytes)\n");

    /* Compute shared secret K */
    if (compute_curve25519_shared(shared_secret, server_ephemeral_private,
                                  client_ephemeral_public) != 0) {
        fprintf(stderr, "[-] Failed to compute shared secret\n");
        close(client_fd);
        return;
    }
    printf("[+] Computed shared secret\n");

    /* Compute exchange hash H */
    compute_exchange_hash(exchange_hash,
                         client_version, SERVER_VERSION,
                         client_kexinit, client_kexinit_len_s,
                         server_kexinit, server_kexinit_len,
                         host_key_blob, host_key_blob_len,
                         client_ephemeral_public, 32,
                         server_ephemeral_public, 32,
                         shared_secret, 32);
    printf("[+] Computed exchange hash H\n");

    /* First exchange hash becomes session_id */
    memcpy(session_id, exchange_hash, 32);
    printf("[+] Set session_id\n");

    /* Sign exchange hash with host private key */
    crypto_sign_detached(signature, &sig_len, exchange_hash, 32, host_private_key);
    printf("[+] Signed exchange hash (signature: 64 bytes)\n");

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
        sig_blob_len += write_string(sig_blob, "ssh-ed25519", 11);
        sig_blob_len += write_string(sig_blob + sig_blob_len, (const char *)signature, 64);
        kex_reply_len += write_string(kex_reply + kex_reply_len,
                                      (const char *)sig_blob, sig_blob_len);
    }

    /* Send SSH_MSG_KEX_ECDH_REPLY */
    if (send_packet(client_fd, kex_reply, kex_reply_len) < 0) {
        fprintf(stderr, "[-] Failed to send KEX_ECDH_REPLY\n");
        close(client_fd);
        return;
    }
    printf("[+] Sent KEX_ECDH_REPLY (%zu bytes)\n", kex_reply_len);

    /* ======================
     * Phase 1.7: Key Derivation
     * ====================== */

    uint8_t iv_c2s[12];      /* IV client to server (96-bit nonce for ChaCha20) */
    uint8_t iv_s2c[12];      /* IV server to client */
    uint8_t key_c2s[32];     /* Encryption key client to server */
    uint8_t key_s2c[32];     /* Encryption key server to client */
    uint8_t int_key_c2s[32]; /* Integrity key client to server (unused for ChaCha20-Poly1305) */
    uint8_t int_key_s2c[32]; /* Integrity key server to client (unused for ChaCha20-Poly1305) */

    /* Derive all keys using session_id and exchange_hash */
    derive_key(iv_c2s, 12, shared_secret, 32, exchange_hash, 'A', session_id);
    derive_key(iv_s2c, 12, shared_secret, 32, exchange_hash, 'B', session_id);
    derive_key(key_c2s, 32, shared_secret, 32, exchange_hash, 'C', session_id);
    derive_key(key_s2c, 32, shared_secret, 32, exchange_hash, 'D', session_id);
    derive_key(int_key_c2s, 32, shared_secret, 32, exchange_hash, 'E', session_id);
    derive_key(int_key_s2c, 32, shared_secret, 32, exchange_hash, 'F', session_id);

    printf("[+] Derived encryption keys:\n");
    printf("    IV c2s: 12 bytes\n");
    printf("    IV s2c: 12 bytes\n");
    printf("    Key c2s: 32 bytes\n");
    printf("    Key s2c: 32 bytes\n");

    /* ======================
     * Phase 1.8: NEWKEYS Exchange
     * ====================== */

    uint8_t newkeys_msg[1];
    uint8_t recv_newkeys[16];
    ssize_t newkeys_len;

    /* Send SSH_MSG_NEWKEYS */
    newkeys_msg[0] = SSH_MSG_NEWKEYS;
    if (send_packet(client_fd, newkeys_msg, 1) < 0) {
        fprintf(stderr, "[-] Failed to send NEWKEYS\n");
        close(client_fd);
        return;
    }
    printf("[+] Sent SSH_MSG_NEWKEYS\n");

    /* Receive SSH_MSG_NEWKEYS from client */
    newkeys_len = recv_packet(client_fd, recv_newkeys, sizeof(recv_newkeys));
    if (newkeys_len <= 0) {
        fprintf(stderr, "[-] Failed to receive NEWKEYS\n");
        close(client_fd);
        return;
    }

    if (recv_newkeys[0] != SSH_MSG_NEWKEYS) {
        fprintf(stderr, "[-] Expected NEWKEYS (21), got %d\n", recv_newkeys[0]);
        close(client_fd);
        return;
    }
    printf("[+] Received SSH_MSG_NEWKEYS\n");

    /* ======================
     * Phase 1.9: Activate Encryption
     * ====================== */

    /* Initialize encryption state for both directions */

    /* Client to server (incoming) */
    memcpy(encrypt_state_c2s.key, key_c2s, 32);
    encrypt_state_c2s.seq_num = 0;
    encrypt_state_c2s.active = 1;

    /* Server to client (outgoing) */
    memcpy(encrypt_state_s2c.key, key_s2c, 32);
    encrypt_state_s2c.seq_num = 0;
    encrypt_state_s2c.active = 1;

    printf("[+] Encryption activated (ChaCha20-Poly1305)\n");
    printf("    Send sequence: %u\n", encrypt_state_s2c.seq_num);
    printf("    Recv sequence: %u\n", encrypt_state_c2s.seq_num);
    printf("[+] Key exchange complete!\n");

    /* TODO: Implement remaining SSH protocol
     *
     * Phase 1 tasks (see TODO.md):
     * 1.9: Encrypted Packets - ChaCha20-Poly1305
     * 1.10: Service Request - ssh-userauth
     * 1.11: Authentication - Password auth
     * 1.12: Channel Open - Session channel
     * 1.13: Channel Requests - PTY, shell
     * 1.14: Data Transfer - Send "Hello World"
     * 1.15: Channel Close - Clean disconnect
     */

    printf("[+] Closing connection\n");

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

    printf("=================================\n");
    printf("Nano SSH Server v0-vanilla\n");
    printf("=================================\n");
    printf("Port: %d\n", SERVER_PORT);
    printf("Version: %s\n", SERVER_VERSION);
    printf("Credentials: %s / %s\n", VALID_USERNAME, VALID_PASSWORD);
    printf("=================================\n\n");

    /* Initialize libsodium */
    if (sodium_init() < 0) {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        return 1;
    }
    printf("[+] Libsodium initialized\n");

    /* Generate Ed25519 host key pair */
    crypto_sign_keypair(host_public_key, host_private_key);
    printf("[+] Generated Ed25519 host key pair\n");

    /* Create TCP server socket */
    listen_fd = create_server_socket(SERVER_PORT);
    if (listen_fd < 0) {
        fprintf(stderr, "Error: Failed to create server socket\n");
        return 1;
    }
    printf("[+] Server socket created and listening on port %d\n", SERVER_PORT);

    /* Main server loop - accept connections */
    printf("[+] Waiting for connections...\n\n");

    while (1) {
        /* Accept client connection */
        client_fd = accept_client(listen_fd, &client_addr);
        if (client_fd < 0) {
            fprintf(stderr, "Warning: Failed to accept client, continuing...\n");
            continue;
        }

        /* Handle client connection */
        handle_client(client_fd, &client_addr, host_public_key, host_private_key);

        /* For now, only handle one connection then exit */
        /* TODO: In production, this should loop forever or handle multiple clients */
        break;
    }

    close(listen_fd);
    printf("\n[+] Server shutting down\n");

    return 0;
}
