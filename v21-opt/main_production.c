/* Nano SSH Server - v21-opt: Source-level optimizations
 * Optimizations: Removed error strings, goto error handling, removed unused vars,
 * simplified control flow, removed comments where possible */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sodium_compat_production.h"
#include "aes128_minimal.h"
#include "sha256_minimal.h"

ssize_t send_data(int fd, const void *buf, size_t len);
ssize_t recv_data(int fd, void *buf, size_t len);

#define SERVER_PORT 2222
#define SERVER_VERSION "SSH-2.0-NanoSSH_0.1"
#define BACKLOG 5
#define VALID_USERNAME "user"
#define VALID_PASSWORD "password123"
#define MAX_PACKET_SIZE 35000
#define MIN_PADDING 4
#define BLOCK_SIZE_UNENCRYPTED 8
#define BLOCK_SIZE_AES_CTR 16

#define SSH_MSG_DISCONNECT                1
#define SSH_MSG_SERVICE_REQUEST           5
#define SSH_MSG_SERVICE_ACCEPT            6
#define SSH_MSG_KEXINIT                   20
#define SSH_MSG_NEWKEYS                   21
#define SSH_MSG_KEX_ECDH_INIT             30
#define SSH_MSG_KEX_ECDH_REPLY            31
#define SSH_MSG_USERAUTH_REQUEST          50
#define SSH_MSG_USERAUTH_FAILURE          51
#define SSH_MSG_USERAUTH_SUCCESS          52
#define SSH_MSG_CHANNEL_OPEN              90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91
#define SSH_MSG_CHANNEL_OPEN_FAILURE      92
#define SSH_MSG_CHANNEL_DATA              94
#define SSH_MSG_CHANNEL_EOF               96
#define SSH_MSG_CHANNEL_CLOSE             97
#define SSH_MSG_CHANNEL_REQUEST           98
#define SSH_MSG_CHANNEL_SUCCESS           99
#define SSH_MSG_CHANNEL_FAILURE           100

#define SSH_DISCONNECT_PROTOCOL_ERROR                 2
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED            3
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE          7
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED 8

#define KEX_ALGORITHM           "curve25519-sha256"
#define HOST_KEY_ALGORITHM      "ssh-ed25519"
#define ENCRYPTION_ALGORITHM    "aes128-ctr"
#define MAC_ALGORITHM           "hmac-sha2-256"
#define COMPRESSION_ALGORITHM   "none"
#define LANGUAGE                ""

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

static crypto_state_t encrypt_state_c2s, encrypt_state_s2c;

#define write_uint32_be(b,v) do{(b)[0]=(v)>>24;(b)[1]=(v)>>16;(b)[2]=(v)>>8;(b)[3]=(v);}while(0)
#define read_uint32_be(b) (((uint32_t)(b)[0]<<24)|((uint32_t)(b)[1]<<16)|((uint32_t)(b)[2]<<8)|(b)[3])

static inline void generate_curve25519_keypair(uint8_t *priv, uint8_t *pub) {
    randombytes_buf(priv, 32);
    crypto_scalarmult_base(pub, priv);
}

static inline int compute_curve25519_shared(uint8_t *shared, const uint8_t *priv, const uint8_t *peer) {
    return crypto_scalarmult(shared, priv, peer);
}

static inline size_t write_string(uint8_t *buf, const char *str, size_t str_len) {
    write_uint32_be(buf, str_len);
    memcpy(buf + 4, str, str_len);
    return 4 + str_len;
}

static inline size_t read_string(const uint8_t *buf, size_t buf_len, char *out, size_t out_max, uint32_t *str_len) {
    if (buf_len < 4) return 0;
    *str_len = read_uint32_be(buf);
    if (*str_len > out_max - 1 || buf_len < 4 + *str_len) return 0;
    memcpy(out, buf + 4, *str_len);
    out[*str_len] = '\0';
    return 4 + *str_len;
}

static inline uint8_t calculate_padding(size_t payload_len, size_t block_size) {
    size_t total_len = 5 + payload_len;
    uint8_t padding = block_size - (total_len % block_size);
    if (padding < MIN_PADDING) padding += block_size;
    return padding;
}

static inline void compute_hmac_sha256(uint8_t *mac, const uint8_t *key,
                         uint32_t seq_num, const uint8_t *packet, size_t packet_len) {
    hmac_sha256_ctx state;
    uint8_t seq_buf[4];
    hmac_sha256_init(&state, key, 32);
    write_uint32_be(seq_buf, seq_num);
    hmac_sha256_update(&state, seq_buf, 4);
    hmac_sha256_update(&state, packet, packet_len);
    hmac_sha256_final(&state, mac);
}

static inline int aes_ctr_hmac_encrypt(uint8_t *packet, size_t packet_len,
                         crypto_state_t *state, uint8_t *mac) {
    compute_hmac_sha256(mac, state->mac_key, state->seq_num, packet, packet_len);
    aes128_ctr_crypt(&state->aes_ctx, packet, packet_len);
    return 0;
}

int send_packet(int fd, const uint8_t *payload, size_t payload_len) {
    uint8_t packet[MAX_PACKET_SIZE], mac[32];
    uint8_t padding_len;
    uint32_t packet_len;
    size_t total_len, block_size;

    if (payload_len > MAX_PACKET_SIZE - 256) return -1;

    block_size = encrypt_state_s2c.active ? BLOCK_SIZE_AES_CTR : BLOCK_SIZE_UNENCRYPTED;
    padding_len = calculate_padding(payload_len, block_size);
    packet_len = 1 + payload_len + padding_len;

    write_uint32_be(packet, packet_len);
    packet[4] = padding_len;
    memcpy(packet + 5, payload, payload_len);
    randombytes_buf(packet + 5 + payload_len, padding_len);
    total_len = 4 + packet_len;

    if (encrypt_state_s2c.active) {
        if (aes_ctr_hmac_encrypt(packet, total_len, &encrypt_state_s2c, mac) < 0) return -1;
        if (send_data(fd, packet, total_len) < 0) return -1;
        if (send_data(fd, mac, 32) < 0) return -1;
        encrypt_state_s2c.seq_num++;
    } else {
        if (send_data(fd, packet, total_len) < 0) return -1;
    }
    return 0;
}

void send_disconnect(int fd, uint32_t reason_code, const char *description) {
    uint8_t msg[512];
    size_t len = 0;
    msg[len++] = SSH_MSG_DISCONNECT;
    write_uint32_be(msg + len, reason_code);
    len += 4;
    len += write_string(msg + len, description, strlen(description));
    len += write_string(msg + len, "", 0);
    send_packet(fd, msg, len);
}

ssize_t recv_packet(int fd, uint8_t *payload, size_t payload_max) {
    uint8_t header[4], mac[32];
    uint32_t packet_len;
    uint8_t padding_len, *packet_buf;
    size_t payload_len, total_packet_len;
    ssize_t n;

    if (encrypt_state_c2s.active) {
        uint8_t first_block[16], decrypted_block[16];
        n = recv_data(fd, first_block, 16);
        if (n <= 0) return n;
        if (n < 16) return -1;

        memcpy(decrypted_block, first_block, 16);
        aes128_ctr_crypt(&encrypt_state_c2s.aes_ctx, decrypted_block, 16);
        packet_len = read_uint32_be(decrypted_block);

        if (packet_len < 5 || packet_len > MAX_PACKET_SIZE) return -1;

        total_packet_len = 4 + packet_len;
        size_t remaining = total_packet_len - 16;
        packet_buf = malloc(total_packet_len);
        if (!packet_buf) return -1;
        memcpy(packet_buf, decrypted_block, 16);

        if (remaining > 0) {
            uint8_t *encrypted_remaining = malloc(remaining);
            if (!encrypted_remaining) {
                free(packet_buf);
                return -1;
            }
            n = recv_data(fd, encrypted_remaining, remaining);
            if (n <= 0 || n < (ssize_t)remaining) {
                free(encrypted_remaining);
                free(packet_buf);
                return -1;
            }
            memcpy(packet_buf + 16, encrypted_remaining, remaining);
            aes128_ctr_crypt(&encrypt_state_c2s.aes_ctx, packet_buf + 16, remaining);
            free(encrypted_remaining);
        }

        n = recv_data(fd, mac, 32);
        if (n <= 0 || n < 32) {
            free(packet_buf);
            return -1;
        }

        uint8_t computed_mac[32];
        compute_hmac_sha256(computed_mac, encrypt_state_c2s.mac_key,
                           encrypt_state_c2s.seq_num, packet_buf, total_packet_len);
        if (ct_verify_32(computed_mac, mac) != 0) {
            free(packet_buf);
            return -1;
        }
        encrypt_state_c2s.seq_num++;

        padding_len = packet_buf[4];
        if (padding_len >= packet_len - 1) {
            free(packet_buf);
            return -1;
        }
        payload_len = packet_len - 1 - padding_len;
        if (payload_len > payload_max) {
            free(packet_buf);
            return -1;
        }
        memcpy(payload, packet_buf + 5, payload_len);
        free(packet_buf);
        return payload_len;
    } else {
        n = recv_data(fd, header, 4);
        if (n <= 0) return n;
        if (n < 4) return -1;
        packet_len = read_uint32_be(header);
        if (packet_len < 5 || packet_len > MAX_PACKET_SIZE) return -1;

        packet_buf = malloc(packet_len);
        if (!packet_buf) return -1;
        n = recv_data(fd, packet_buf, packet_len);
        if (n <= 0 || n < (ssize_t)packet_len) {
            free(packet_buf);
            return -1;
        }

        padding_len = packet_buf[0];
        if (padding_len >= packet_len - 1) {
            free(packet_buf);
            return -1;
        }
        payload_len = packet_len - 1 - padding_len;
        if (payload_len > payload_max) {
            free(packet_buf);
            return -1;
        }
        memcpy(payload, packet_buf + 1, payload_len);
        free(packet_buf);
        return payload_len;
    }
}

static inline size_t write_name_list(uint8_t *buf, const char *names) {
    size_t len = strlen(names);
    write_uint32_be(buf, len);
    memcpy(buf + 4, names, len);
    return 4 + len;
}

static inline size_t build_kexinit(uint8_t *payload) {
    size_t offset = 0;
    payload[offset++] = SSH_MSG_KEXINIT;
    randombytes_buf(payload + offset, 16);
    offset += 16;
    offset += write_name_list(payload + offset, KEX_ALGORITHM);
    offset += write_name_list(payload + offset, HOST_KEY_ALGORITHM);
    offset += write_name_list(payload + offset, ENCRYPTION_ALGORITHM);
    offset += write_name_list(payload + offset, ENCRYPTION_ALGORITHM);
    offset += write_name_list(payload + offset, MAC_ALGORITHM);
    offset += write_name_list(payload + offset, MAC_ALGORITHM);
    offset += write_name_list(payload + offset, COMPRESSION_ALGORITHM);
    offset += write_name_list(payload + offset, COMPRESSION_ALGORITHM);
    offset += write_name_list(payload + offset, LANGUAGE);
    offset += write_name_list(payload + offset, LANGUAGE);
    payload[offset++] = 0;
    write_uint32_be(payload + offset, 0);
    offset += 4;
    return offset;
}

static inline size_t write_mpint(uint8_t *buf, const uint8_t *data, size_t data_len) {
    size_t i;
    for (i = 0; i < data_len && data[i] == 0; i++);
    if (i < data_len && (data[i] & 0x80)) {
        write_uint32_be(buf, data_len - i + 1);
        buf[4] = 0;
        memcpy(buf + 5, data + i, data_len - i);
        return 4 + 1 + (data_len - i);
    }
    write_uint32_be(buf, data_len - i);
    memcpy(buf + 4, data + i, data_len - i);
    return 4 + (data_len - i);
}

void compute_exchange_hash(uint8_t *hash_out,
                           const char *client_version, const char *server_version,
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
    len = write_string(buf, client_version, strlen(client_version));
    hash_update(&h, buf, len);
    len = write_string(buf, server_version, strlen(server_version));
    hash_update(&h, buf, len);
    write_uint32_be(buf, client_kexinit_len);
    hash_update(&h, buf, 4);
    hash_update(&h, client_kexinit, client_kexinit_len);
    write_uint32_be(buf, server_kexinit_len);
    hash_update(&h, buf, 4);
    hash_update(&h, server_kexinit, server_kexinit_len);
    write_uint32_be(buf, host_key_blob_len);
    hash_update(&h, buf, 4);
    hash_update(&h, server_host_key_blob, host_key_blob_len);
    len = write_string(buf, (const char *)client_ephemeral_pub, client_eph_len);
    hash_update(&h, buf, len);
    len = write_string(buf, (const char *)server_ephemeral_pub, server_eph_len);
    hash_update(&h, buf, len);
    len = write_mpint(buf, shared_secret, shared_secret_len);
    hash_update(&h, buf, len);
    hash_final(&h, hash_out);
}

void derive_key(uint8_t *key_out, size_t key_len,
                const uint8_t *shared_secret, size_t shared_len,
                const uint8_t *exchange_hash, char key_id,
                const uint8_t *session_id) {
    hash_state_t h;
    uint8_t key_material[256];
    size_t material_len = 0, needed = key_len;

    hash_init(&h);
    {
        uint8_t mpint_buf[64];
        size_t mpint_len = write_mpint(mpint_buf, shared_secret, shared_len);
        hash_update(&h, mpint_buf, mpint_len);
    }
    hash_update(&h, exchange_hash, 32);
    hash_update(&h, (uint8_t *)&key_id, 1);
    hash_update(&h, session_id, 32);
    hash_final(&h, key_material);
    material_len = 32;

    while (material_len < needed) {
        hash_init(&h);
        {
            uint8_t mpint_buf[64];
            size_t mpint_len = write_mpint(mpint_buf, shared_secret, shared_len);
            hash_update(&h, mpint_buf, mpint_len);
        }
        hash_update(&h, exchange_hash, 32);
        hash_update(&h, key_material, material_len);
        hash_final(&h, key_material + material_len);
        material_len += 32;
    }
    memcpy(key_out, key_material, key_len);
}

int create_server_socket(int port) {
    int listen_fd;
    struct sockaddr_in server_addr;
    int reuse = 1;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_fd);
        return -1;
    }
    if (listen(listen_fd, BACKLOG) < 0) {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

int accept_client(int listen_fd, struct sockaddr_in *client_addr) {
    socklen_t addr_len = sizeof(*client_addr);
    return accept(listen_fd, (struct sockaddr *)client_addr, &addr_len);
}

ssize_t send_data(int fd, const void *buf, size_t len) {
    ssize_t sent = 0, n;
    while (sent < (ssize_t)len) {
        n = send(fd, (const uint8_t *)buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += n;
    }
    return sent;
}

ssize_t recv_data(int fd, void *buf, size_t len) {
    ssize_t n = recv(fd, buf, len, 0);
    if (n < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }
    if (n == 0) return 0;
    return n;
}

void handle_client(int client_fd, struct sockaddr_in *client_addr,
                   const uint8_t *host_public_key, const uint8_t *host_private_key) {
    char client_version[256], server_version_line[256];
    ssize_t n;
    int i, version_len;

    (void)client_addr;

    server_version_line[0] = 'S'; server_version_line[1] = 'S'; server_version_line[2] = 'H';
    server_version_line[3] = '-'; server_version_line[4] = '2'; server_version_line[5] = '.';
    server_version_line[6] = '0'; server_version_line[7] = '-';
    memcpy(server_version_line + 8, "NanoSSH_0.1\r\n", 13);
    if (send_data(client_fd, server_version_line, 21) < 0) goto err;

    memset(client_version, 0, sizeof(client_version));
    version_len = 0;
    for (i = 0; i < (int)sizeof(client_version) - 1; i++) {
        n = recv_data(client_fd, &client_version[i], 1);
        if (n <= 0) goto err;
        version_len++;
        if (client_version[i] == '\n') break;
    }
    if (i == (int)sizeof(client_version) - 1 && client_version[i] != '\n') {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR, "");
        goto err;
    }
    client_version[version_len] = '\0';
    if (version_len > 0 && client_version[version_len - 1] == '\n') {
        client_version[version_len - 1] = '\0';
        version_len--;
    }
    if (version_len > 0 && client_version[version_len - 1] == '\r') {
        client_version[version_len - 1] = '\0';
        version_len--;
    }
    if (strncmp(client_version, "SSH-2.0-", 8) != 0) {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED, "");
        goto err;
    }

    uint8_t server_kexinit[1024], client_kexinit[4096];
    size_t server_kexinit_len;
    ssize_t client_kexinit_len_s;

    server_kexinit_len = build_kexinit(server_kexinit);
    if (send_packet(client_fd, server_kexinit, server_kexinit_len) < 0) goto err;
    client_kexinit_len_s = recv_packet(client_fd, client_kexinit, sizeof(client_kexinit));
    if (client_kexinit_len_s <= 0) goto err;
    if (client_kexinit[0] != SSH_MSG_KEXINIT) {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR, "");
        goto err;
    }

    uint8_t server_ephemeral_private[32], server_ephemeral_public[32];
    uint8_t client_ephemeral_public[32], shared_secret[32];
    uint8_t exchange_hash[32], session_id[32];
    uint8_t host_key_blob[256];
    size_t host_key_blob_len;
    uint8_t signature[64];
    unsigned long long sig_len;
    uint8_t kex_reply[4096];
    size_t kex_reply_len;
    uint8_t kex_init_msg[128];
    ssize_t kex_init_len;
    uint32_t client_eph_str_len;

    generate_curve25519_keypair(server_ephemeral_private, server_ephemeral_public);
    host_key_blob_len = 0;
    host_key_blob_len += write_string(host_key_blob + host_key_blob_len, "ssh-ed25519", 11);
    host_key_blob_len += write_string(host_key_blob + host_key_blob_len, (const char *)host_public_key, 32);

    kex_init_len = recv_packet(client_fd, kex_init_msg, sizeof(kex_init_msg));
    if (kex_init_len <= 0) goto err;
    if (kex_init_msg[0] != SSH_MSG_KEX_ECDH_INIT) {
        send_disconnect(client_fd, SSH_DISCONNECT_KEY_EXCHANGE_FAILED, "");
        goto err;
    }
    if (read_string(kex_init_msg + 1, kex_init_len - 1, (char *)client_ephemeral_public, 33, &client_eph_str_len) == 0)
        goto err;
    if (client_eph_str_len != 32) goto err;
    if (compute_curve25519_shared(shared_secret, server_ephemeral_private, client_ephemeral_public) != 0)
        goto err;

    compute_exchange_hash(exchange_hash, client_version, SERVER_VERSION,
                         client_kexinit, client_kexinit_len_s,
                         server_kexinit, server_kexinit_len,
                         host_key_blob, host_key_blob_len,
                         client_ephemeral_public, 32,
                         server_ephemeral_public, 32,
                         shared_secret, 32);
    memcpy(session_id, exchange_hash, 32);
    crypto_sign_detached(signature, &sig_len, exchange_hash, 32, host_private_key);

    kex_reply_len = 0;
    kex_reply[kex_reply_len++] = SSH_MSG_KEX_ECDH_REPLY;
    kex_reply_len += write_string(kex_reply + kex_reply_len, (const char *)host_key_blob, host_key_blob_len);
    kex_reply_len += write_string(kex_reply + kex_reply_len, (const char *)server_ephemeral_public, 32);
    {
        uint8_t sig_blob[128];
        size_t sig_blob_len = 0;
        sig_blob_len += write_string(sig_blob, "ssh-ed25519", 11);
        sig_blob_len += write_string(sig_blob + sig_blob_len, (const char *)signature, 64);
        kex_reply_len += write_string(kex_reply + kex_reply_len, (const char *)sig_blob, sig_blob_len);
    }
    if (send_packet(client_fd, kex_reply, kex_reply_len) < 0) goto err;

    uint8_t iv_c2s[16], iv_s2c[16], key_c2s[16], key_s2c[16], int_key_c2s[32], int_key_s2c[32];
    derive_key(iv_c2s, 16, shared_secret, 32, exchange_hash, 'A', session_id);
    derive_key(iv_s2c, 16, shared_secret, 32, exchange_hash, 'B', session_id);
    derive_key(key_c2s, 16, shared_secret, 32, exchange_hash, 'C', session_id);
    derive_key(key_s2c, 16, shared_secret, 32, exchange_hash, 'D', session_id);
    derive_key(int_key_c2s, 32, shared_secret, 32, exchange_hash, 'E', session_id);
    derive_key(int_key_s2c, 32, shared_secret, 32, exchange_hash, 'F', session_id);

    uint8_t newkeys_msg[1] = {SSH_MSG_NEWKEYS};
    if (send_packet(client_fd, newkeys_msg, 1) < 0) goto err;

    memcpy(encrypt_state_s2c.mac_key, int_key_s2c, 32);
    aes128_ctr_init(&encrypt_state_s2c.aes_ctx, key_s2c, iv_s2c);
    encrypt_state_s2c.seq_num = 3;
    encrypt_state_s2c.active = 1;

    uint8_t recv_newkeys[16];
    ssize_t newkeys_len = recv_packet(client_fd, recv_newkeys, sizeof(recv_newkeys));
    if (newkeys_len <= 0) goto err;
    if (recv_newkeys[0] != SSH_MSG_NEWKEYS) goto err;

    memcpy(encrypt_state_c2s.mac_key, int_key_c2s, 32);
    aes128_ctr_init(&encrypt_state_c2s.aes_ctx, key_c2s, iv_c2s);
    encrypt_state_c2s.seq_num = 3;
    encrypt_state_c2s.active = 1;

    uint8_t service_request[256];
    ssize_t service_request_len = recv_packet(client_fd, service_request, sizeof(service_request));
    if (service_request_len <= 0) goto err;
    if (service_request[0] != SSH_MSG_SERVICE_REQUEST) {
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR, "");
        goto err;
    }

    char service_name[64];
    uint32_t service_name_len;
    if (read_string(service_request + 1, service_request_len - 1, service_name, sizeof(service_name), &service_name_len) == 0)
        goto err;
    service_name[service_name_len] = '\0';
    if (strcmp(service_name, "ssh-userauth") != 0) {
        send_disconnect(client_fd, SSH_DISCONNECT_SERVICE_NOT_AVAILABLE, "");
        goto err;
    }

    uint8_t service_accept[256];
    size_t service_accept_len = 0;
    service_accept[service_accept_len++] = SSH_MSG_SERVICE_ACCEPT;
    service_accept_len += write_string(service_accept + service_accept_len, "ssh-userauth", 12);
    if (send_packet(client_fd, service_accept, service_accept_len) < 0) goto err;

    uint8_t userauth_request[512];
    uint8_t *ptr;
    uint32_t username_len, service_len, method_len, password_len;
    char username[256], service[256], method[256], password[256];
    int authenticated = 0;

    while (!authenticated) {
        ssize_t userauth_request_len = recv_packet(client_fd, userauth_request, sizeof(userauth_request));
        if (userauth_request_len <= 0) goto err;
        if (userauth_request[0] != SSH_MSG_USERAUTH_REQUEST) goto err;

        ptr = userauth_request + 1;
        username_len = read_uint32_be(ptr);
        ptr += 4;
        if (username_len >= sizeof(username)) goto err;
        memcpy(username, ptr, username_len);
        username[username_len] = '\0';
        ptr += username_len;

        service_len = read_uint32_be(ptr);
        ptr += 4;
        if (service_len >= sizeof(service)) goto err;
        memcpy(service, ptr, service_len);
        service[service_len] = '\0';
        ptr += service_len;

        method_len = read_uint32_be(ptr);
        ptr += 4;
        if (method_len >= sizeof(method)) goto err;
        memcpy(method, ptr, method_len);
        method[method_len] = '\0';
        ptr += method_len;

        if (strcmp(service, "ssh-connection") != 0) goto err;

        if (strcmp(method, "password") == 0) {
            ptr += 1;
            password_len = read_uint32_be(ptr);
            ptr += 4;
            if (password_len >= sizeof(password)) goto err;
            memcpy(password, ptr, password_len);
            password[password_len] = '\0';

            if (strcmp(username, VALID_USERNAME) == 0 && strcmp(password, VALID_PASSWORD) == 0) {
                uint8_t success_msg[1] = {SSH_MSG_USERAUTH_SUCCESS};
                if (send_packet(client_fd, success_msg, 1) < 0) goto err;
                authenticated = 1;
            } else {
                uint8_t failure_msg[256];
                size_t failure_len = 0;
                failure_msg[failure_len++] = SSH_MSG_USERAUTH_FAILURE;
                failure_len += write_string(failure_msg + failure_len, "password", 8);
                failure_msg[failure_len++] = 0;
                if (send_packet(client_fd, failure_msg, failure_len) < 0) goto err;
            }
        } else {
            uint8_t failure_msg[256];
            size_t failure_len = 0;
            failure_msg[failure_len++] = SSH_MSG_USERAUTH_FAILURE;
            failure_len += write_string(failure_msg + failure_len, "password", 8);
            failure_msg[failure_len++] = 0;
            if (send_packet(client_fd, failure_msg, failure_len) < 0) goto err;
        }
    }

    uint8_t channel_open_msg[512];
    ssize_t channel_open_len = recv_packet(client_fd, channel_open_msg, sizeof(channel_open_msg));
    if (channel_open_len <= 0) goto err;
    if (channel_open_msg[0] != SSH_MSG_CHANNEL_OPEN) goto err;

    uint8_t *ch_ptr = channel_open_msg + 1;
    uint32_t channel_type_len;
    char channel_type[64];
    uint32_t client_channel_id, client_window_size;
    uint32_t server_channel_id = 0, server_window_size = 32768, server_max_packet = 16384;

    channel_type_len = read_uint32_be(ch_ptr);
    ch_ptr += 4;
    if (channel_type_len >= sizeof(channel_type)) goto err;
    memcpy(channel_type, ch_ptr, channel_type_len);
    channel_type[channel_type_len] = '\0';
    ch_ptr += channel_type_len;

    client_channel_id = read_uint32_be(ch_ptr);
    ch_ptr += 4;
    client_window_size = read_uint32_be(ch_ptr);
    ch_ptr += 8;

    if (strcmp(channel_type, "session") != 0) {
        uint8_t open_failure[256];
        size_t failure_len = 0;
        open_failure[failure_len++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
        write_uint32_be(open_failure + failure_len, client_channel_id);
        failure_len += 4;
        write_uint32_be(open_failure + failure_len, 3);
        failure_len += 4;
        failure_len += write_string(open_failure + failure_len, "", 0);
        failure_len += write_string(open_failure + failure_len, "", 0);
        send_packet(client_fd, open_failure, failure_len);
        send_disconnect(client_fd, SSH_DISCONNECT_PROTOCOL_ERROR, "");
        goto err;
    }

    uint8_t channel_confirm[256];
    size_t channel_confirm_len = 0;
    channel_confirm[channel_confirm_len++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
    write_uint32_be(channel_confirm + channel_confirm_len, client_channel_id);
    channel_confirm_len += 4;
    write_uint32_be(channel_confirm + channel_confirm_len, server_channel_id);
    channel_confirm_len += 4;
    write_uint32_be(channel_confirm + channel_confirm_len, server_window_size);
    channel_confirm_len += 4;
    write_uint32_be(channel_confirm + channel_confirm_len, server_max_packet);
    channel_confirm_len += 4;
    if (send_packet(client_fd, channel_confirm, channel_confirm_len) < 0) goto err;

    int shell_ready = 0, requests_done = 0;
    while (!requests_done) {
        uint8_t channel_request[4096];
        ssize_t channel_request_len = recv_packet(client_fd, channel_request, sizeof(channel_request));
        if (channel_request_len <= 0) goto err;
        if (channel_request[0] != SSH_MSG_CHANNEL_REQUEST) break;

        uint8_t *req_ptr = channel_request + 1;
        uint32_t req_recipient_channel = read_uint32_be(req_ptr);
        req_ptr += 4;
        uint32_t req_type_len = read_uint32_be(req_ptr);
        req_ptr += 4;
        char req_type[64];
        if (req_type_len >= sizeof(req_type)) goto err;
        memcpy(req_type, req_ptr, req_type_len);
        req_type[req_type_len] = '\0';
        req_ptr += req_type_len;
        uint8_t want_reply = *req_ptr;
        req_ptr += 1;

        if (req_recipient_channel != server_channel_id) goto err;

        int request_accepted = 0;
        if (strcmp(req_type, "pty-req") == 0) {
            request_accepted = 1;
        } else if (strcmp(req_type, "shell") == 0) {
            request_accepted = 1;
            shell_ready = 1;
            requests_done = 1;
        } else if (strcmp(req_type, "exec") == 0) {
            request_accepted = 1;
            shell_ready = 1;
            requests_done = 1;
        } else if (strcmp(req_type, "env") == 0) {
            request_accepted = 1;
        }

        if (want_reply) {
            uint8_t reply_msg[16];
            size_t reply_len = 0;
            if (request_accepted) {
                reply_msg[reply_len++] = SSH_MSG_CHANNEL_SUCCESS;
                write_uint32_be(reply_msg + reply_len, client_channel_id);
                reply_len += 4;
                if (send_packet(client_fd, reply_msg, reply_len) < 0) goto err;
            } else {
                reply_msg[reply_len++] = SSH_MSG_CHANNEL_FAILURE;
                write_uint32_be(reply_msg + reply_len, client_channel_id);
                reply_len += 4;
                if (send_packet(client_fd, reply_msg, reply_len) < 0) goto err;
            }
        }
    }

    if (shell_ready) {
        const char *hello_msg = "Hello World\r\n";
        size_t hello_len = strlen(hello_msg);
        uint8_t data_msg[1024];
        size_t data_msg_len = 0;
        data_msg[data_msg_len++] = SSH_MSG_CHANNEL_DATA;
        write_uint32_be(data_msg + data_msg_len, client_channel_id);
        data_msg_len += 4;
        data_msg_len += write_string(data_msg + data_msg_len, hello_msg, hello_len);
        if (send_packet(client_fd, data_msg, data_msg_len) < 0) goto err;
        client_window_size -= hello_len;
    }

    uint8_t eof_msg[16];
    size_t eof_len = 0;
    eof_msg[eof_len++] = SSH_MSG_CHANNEL_EOF;
    write_uint32_be(eof_msg + eof_len, client_channel_id);
    eof_len += 4;
    if (send_packet(client_fd, eof_msg, eof_len) < 0) goto err;

    uint8_t close_msg[16];
    size_t close_len = 0;
    close_msg[close_len++] = SSH_MSG_CHANNEL_CLOSE;
    write_uint32_be(close_msg + close_len, client_channel_id);
    close_len += 4;
    if (send_packet(client_fd, close_msg, close_len) < 0) goto err;

    uint8_t client_close[16];
    recv_packet(client_fd, client_close, sizeof(client_close));

err:
    close(client_fd);
}

int main(int argc, char *argv[]) {
    int listen_fd, client_fd;
    struct sockaddr_in client_addr;
    uint8_t host_public_key[32], host_private_key[64];

    (void)argc;
    (void)argv;

    crypto_sign_keypair(host_public_key, host_private_key);
    listen_fd = create_server_socket(SERVER_PORT);
    if (listen_fd < 0) return 1;

    while (1) {
        client_fd = accept_client(listen_fd, &client_addr);
        if (client_fd < 0) continue;
        handle_client(client_fd, &client_addr, host_public_key, host_private_key);
        break;
    }

    close(listen_fd);
    return 0;
}
