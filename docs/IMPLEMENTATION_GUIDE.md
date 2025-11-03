# SSH Protocol Implementation Guide

## Complete Protocol Flow for "Hello World" SSH Server

This guide shows the COMPLETE message flow for a minimal SSH server that accepts a connection and sends "Hello World".

## Phase-by-Phase Implementation

### Phase 1: TCP Connection & Version Exchange (Plaintext)

```
Client                                Server
------                                ------
TCP Connect
|                                     |
|                                     | accept() connection
|                                     |
|<-- "SSH-2.0-NanoSSH_0.1\r\n" ------|  send version (plaintext)
|-- "SSH-2.0-OpenSSH_8.9\r\n" ------>|  recv version (plaintext)
```

**Implementation:**
```c
// Server code
int client_fd = accept(listen_fd, NULL, NULL);

// Send our version
const char *version = "SSH-2.0-NanoSSH_0.1\r\n";
send(client_fd, version, strlen(version), 0);

// Receive client version
char client_version[256];
int n = recv(client_fd, client_version, 255, 0);
client_version[n] = '\0';

// Verify starts with "SSH-2.0-"
if (strncmp(client_version, "SSH-2.0-", 8) != 0) {
    // Error: unsupported version
}

// Save for later (needed for exchange hash H)
```

### Phase 2: Algorithm Negotiation (Binary Packets, Unencrypted)

```
Client                                Server
------                                ------
|-- SSH_MSG_KEXINIT ----------------->|
|   (client's algorithm preferences)  |
|                                     |
|<-- SSH_MSG_KEXINIT -----------------|
    (server's algorithm preferences)
```

**Implementation:**

```c
// Server receives client's KEXINIT
// Structure: see RFC4253_Transport.md section 3

// Parse client KEXINIT, extract:
// - kex_algorithms
// - server_host_key_algorithms
// - encryption_algorithms_c2s/s2c
// - mac_algorithms_c2s/s2c
// - compression_algorithms

// Save client's KEXINIT payload for exchange hash

// Build server's KEXINIT
// For MINIMAL implementation, offer only:
const char *kex = "curve25519-sha256";
const char *host_key = "ssh-ed25519";
const char *cipher = "chacha20-poly1305@openssh.com";
const char *mac = "";  // not needed with AEAD cipher
const char *compress = "none";

// Send SSH_MSG_KEXINIT
// Save server's KEXINIT payload for exchange hash

// Algorithm negotiation: pick first match
// For minimal: just verify client supports our choices
```

### Phase 3: Diffie-Hellman Key Exchange

```
Client                                Server
------                                ------
|-- SSH_MSG_KEX_ECDH_INIT ----------->|
|   (client's ephemeral public key)   |
|                                     |
|<-- SSH_MSG_KEX_ECDH_REPLY -----------|
    (server's public key, ephemeral key, signature)
```

**Implementation:**

```c
// Server receives SSH_MSG_KEX_ECDH_INIT (30)
uint8_t client_ephemeral[32];  // curve25519 public key
// Extract from message

// Server generates ephemeral key pair
uint8_t server_ephemeral_private[32];
uint8_t server_ephemeral_public[32];
generate_curve25519_keypair(server_ephemeral_private,
                            server_ephemeral_public);

// Compute shared secret K
uint8_t shared_secret[32];
curve25519_scalarmult(shared_secret,
                     server_ephemeral_private,
                     client_ephemeral);

// Compute exchange hash H
// H = SHA256(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K)
// Where:
//   V_C = client version string
//   V_S = server version string
//   I_C = client KEXINIT payload
//   I_S = server KEXINIT payload
//   K_S = server host public key
//   Q_C = client ephemeral public
//   Q_S = server ephemeral public
//   K = shared secret

uint8_t exchange_hash[32];
compute_exchange_hash(exchange_hash, ...);

// First exchange hash becomes session_id
memcpy(session_id, exchange_hash, 32);

// Sign exchange hash with server host key
uint8_t signature[64];  // Ed25519 signature
ed25519_sign(signature, exchange_hash, 32, server_host_private_key);

// Send SSH_MSG_KEX_ECDH_REPLY (31)
// Contains:
//   - K_S (server host public key)
//   - Q_S (server ephemeral public key)
//   - signature of H
```

### Phase 4: Key Derivation & Activation

```
Client                                Server
------                                ------
|-- SSH_MSG_NEWKEYS ----------------->|
|                                     |
|<-- SSH_MSG_NEWKEYS ------------------|
|                                     |
[Encryption active from this point]
```

**Implementation:**

```c
// Derive 6 keys from K and H
// HASH = SHA256 for curve25519-sha256

// Initial IV client->server
uint8_t iv_c2s[12];  // 12 bytes for ChaCha20
derive_key(iv_c2s, K, H, 'A', session_id, 12);

// Initial IV server->client
uint8_t iv_s2c[12];
derive_key(iv_s2c, K, H, 'B', session_id, 12);

// Encryption key client->server
uint8_t key_c2s[32];  // 32 bytes for ChaCha20
derive_key(key_c2s, K, H, 'C', session_id, 32);

// Encryption key server->client
uint8_t key_s2c[32];
derive_key(key_s2c, K, H, 'D', session_id, 32);

// Integrity key client->server (not needed for ChaCha20-Poly1305 AEAD)
// Integrity key server->client (not needed for ChaCha20-Poly1305 AEAD)

// Receive SSH_MSG_NEWKEYS (21)
// After receiving, use keys for INCOMING packets

// Send SSH_MSG_NEWKEYS (21)
// After sending, use keys for OUTGOING packets

// Initialize cipher state
chacha20_poly1305_init(&send_cipher, key_s2c, iv_s2c);
chacha20_poly1305_init(&recv_cipher, key_c2s, iv_c2s);

// Sequence numbers start at 0
uint32_t send_seq = 0;
uint32_t recv_seq = 0;
```

### Phase 5: Service Request (Encrypted)

```
Client                                Server
------                                ------
|-- SSH_MSG_SERVICE_REQUEST --------->|
|   (service: "ssh-userauth")         |
|                                     |
|<-- SSH_MSG_SERVICE_ACCEPT -----------|
    (service: "ssh-userauth")
```

**Implementation:**

```c
// All packets now encrypted using binary packet protocol

// Receive SSH_MSG_SERVICE_REQUEST (5)
char service[32];
// Extract service name, expect "ssh-userauth"

if (strcmp(service, "ssh-userauth") == 0) {
    // Send SSH_MSG_SERVICE_ACCEPT (6)
    send_service_accept("ssh-userauth");
}
```

### Phase 6: Authentication (Encrypted)

```
Client                                Server
------                                ------
|-- SSH_MSG_USERAUTH_REQUEST -------->|
|   (password authentication)         |
|                                     |
|<-- SSH_MSG_USERAUTH_SUCCESS --------|
```

**Implementation:**

```c
// Receive SSH_MSG_USERAUTH_REQUEST (50)
char username[32];
char service[32];
char method[32];
uint8_t change_password;
char password[64];

// Extract fields (see RFC4252_Authentication.md)

// For minimal implementation: hardcoded credentials
if (strcmp(username, "user") == 0 &&
    strcmp(password, "password123") == 0 &&
    strcmp(method, "password") == 0 &&
    change_password == 0) {

    // Send SSH_MSG_USERAUTH_SUCCESS (52)
    send_packet(SSH_MSG_USERAUTH_SUCCESS);

} else {
    // Send SSH_MSG_USERAUTH_FAILURE (51)
    send_userauth_failure("password", FALSE);
}
```

### Phase 7: Channel Open (Encrypted)

```
Client                                Server
------                                ------
|-- SSH_MSG_CHANNEL_OPEN ------------>|
|   (type: "session")                 |
|                                     |
|<-- SSH_MSG_CHANNEL_OPEN_CONFIRM ----|
```

**Implementation:**

```c
// Receive SSH_MSG_CHANNEL_OPEN (90)
char channel_type[32];
uint32_t client_channel;
uint32_t client_window;
uint32_t client_max_packet;

// Extract fields

if (strcmp(channel_type, "session") == 0) {
    // Accept channel
    uint32_t server_channel = 0;  // our channel ID
    uint32_t server_window = 32768;
    uint32_t server_max_packet = 16384;

    // Send SSH_MSG_CHANNEL_OPEN_CONFIRMATION (91)
    send_channel_open_confirmation(client_channel,
                                   server_channel,
                                   server_window,
                                   server_max_packet);

    // Save channel state
    channel.client_id = client_channel;
    channel.server_id = server_channel;
    channel.client_window = client_window;
    channel.max_packet = client_max_packet;
}
```

### Phase 8: Channel Requests (Encrypted)

```
Client                                Server
------                                ------
|-- SSH_MSG_CHANNEL_REQUEST --------->| (pty-req)
|<-- SSH_MSG_CHANNEL_SUCCESS ---------|
|                                     |
|-- SSH_MSG_CHANNEL_REQUEST --------->| (shell)
|<-- SSH_MSG_CHANNEL_SUCCESS ---------|
```

**Implementation:**

```c
// Common client behavior:
// 1. Request PTY (pty-req)
// 2. Request shell or exec

// Receive SSH_MSG_CHANNEL_REQUEST (98)
uint32_t recipient_channel;
char request_type[32];
uint8_t want_reply;
// ... request-specific data

// For minimal implementation:
// - Accept "pty-req" (but ignore, we won't spawn PTY)
// - Accept "shell" or "exec"
// - Reject others

if (strcmp(request_type, "pty-req") == 0) {
    // Ignore PTY details, just accept
    if (want_reply) {
        send_channel_success(recipient_channel);
    }
}
else if (strcmp(request_type, "shell") == 0 ||
         strcmp(request_type, "exec") == 0) {
    // This is where we'd spawn a shell
    // For minimal: just accept, we'll send Hello World
    if (want_reply) {
        send_channel_success(recipient_channel);
    }
}
else {
    // Unknown request
    if (want_reply) {
        send_channel_failure(recipient_channel);
    }
}
```

### Phase 9: Data Transfer (Encrypted)

```
Client                                Server
------                                ------
|                                     |
|<-- SSH_MSG_CHANNEL_DATA -------------| "Hello World\r\n"
|                                     |
|<-- SSH_MSG_CHANNEL_EOF --------------|
|<-- SSH_MSG_CHANNEL_CLOSE -----------|
|-- SSH_MSG_CHANNEL_CLOSE ----------->|
```

**Implementation:**

```c
// Send "Hello World\r\n" to client
const char *message = "Hello World\r\n";
send_channel_data(channel.client_id, message, strlen(message));

// Send EOF (no more data)
send_channel_eof(channel.client_id);

// Send CLOSE
send_channel_close(channel.client_id);

// Wait for client's CLOSE
// Receive SSH_MSG_CHANNEL_CLOSE (97)
// Then clean up and close TCP connection

close(client_fd);
```

## Binary Packet Protocol Details

### Packet Structure (After Key Exchange)

```
[packet_length: 4 bytes] <- encrypted or not, depending on cipher
[padding_length: 1 byte] <- encrypted
[payload: N bytes]       <- encrypted
[padding: M bytes]       <- encrypted
[MAC: 16 bytes]          <- for Poly1305 in ChaCha20-Poly1305
```

### Encryption with ChaCha20-Poly1305

ChaCha20-Poly1305 is an AEAD (Authenticated Encryption with Associated Data) cipher.

**OpenSSH variant (chacha20-poly1305@openssh.com):**

```
Packet structure:
[packet_length: 4 bytes]  <- encrypted with separate cipher instance
[padding_length: 1 byte]  <- encrypted
[payload: N bytes]        <- encrypted
[padding: M bytes]        <- encrypted
[Poly1305 tag: 16 bytes]  <- MAC
```

**Encryption:**
1. Derive two ChaCha20 keys from main key:
   - K_1 (first 32 bytes) for packet_length
   - K_2 (second 32 bytes) for payload
2. Nonce = packet sequence number (8 bytes, big-endian) + 4 zero bytes
3. Encrypt packet_length with K_1
4. Encrypt [padding_length || payload || padding] with K_2
5. Compute Poly1305 MAC over entire packet
6. Append MAC

**Decryption:**
1. Decrypt first 4 bytes (packet_length) with K_1
2. Read packet_length bytes + 16 bytes MAC
3. Verify MAC
4. Decrypt payload with K_2

### Simpler Alternative: aes128-ctr + hmac-sha2-256

If ChaCha20-Poly1305 is too complex:

```
Packet:
[packet_length: 4 bytes]
[padding_length: 1 byte]
[payload: N bytes]
[padding: M bytes]
---- ALL ABOVE ENCRYPTED with AES-128-CTR ----
[HMAC: 32 bytes]  <- HMAC-SHA256 over sequence_number || encrypted_packet
```

## State Machine Overview

```
STATE_VERSION_EXCHANGE
  ↓ (sent/received versions)
STATE_KEX_INIT
  ↓ (sent/received KEXINIT)
STATE_KEX_DH
  ↓ (completed key exchange)
STATE_NEWKEYS
  ↓ (sent/received NEWKEYS)
STATE_SERVICE_REQUEST
  ↓ (accepted ssh-userauth service)
STATE_AUTH
  ↓ (user authenticated)
STATE_CHANNEL_OPEN
  ↓ (channel opened)
STATE_CHANNEL_REQUEST
  ↓ (shell requested)
STATE_CHANNEL_DATA
  ↓ (data exchanged)
STATE_CHANNEL_CLOSE
  ↓ (channel closed)
STATE_DISCONNECT
```

## Data Structure Summary

```c
// Connection state
struct ssh_connection {
    int socket_fd;

    // Version exchange
    char client_version[256];
    char server_version[256];

    // Key exchange
    uint8_t client_kexinit[1024];
    size_t client_kexinit_len;
    uint8_t server_kexinit[1024];
    size_t server_kexinit_len;

    // Cryptography
    uint8_t session_id[32];
    uint8_t send_key[32];
    uint8_t recv_key[32];
    uint8_t send_iv[12];
    uint8_t recv_iv[12];
    uint32_t send_seq;
    uint32_t recv_seq;

    // Cipher state (depends on cipher chosen)
    void *send_cipher_ctx;
    void *recv_cipher_ctx;

    // Channel
    struct {
        uint32_t client_id;
        uint32_t server_id;
        uint32_t client_window;
        uint32_t server_window;
        uint32_t max_packet;
        bool open;
    } channel;

    // State
    enum state current_state;
};
```

## Critical Implementation Notes

### String Encoding
ALL strings in SSH are length-prefixed:
```c
uint32_t len = strlen(str);  // big-endian!
write_uint32_be(buffer, len);
memcpy(buffer + 4, str, len);
```

### Sequence Numbers
- Start at 0
- Increment for EACH packet sent/received
- Used in MAC computation
- Separate for send and receive
- NEVER sent on wire (implicit)

### Padding Calculation
```c
uint32_t payload_len = /* ... */;
uint32_t block_size = 8;  // or cipher block size

uint32_t padding_len = block_size - ((5 + payload_len) % block_size);
if (padding_len < 4) {
    padding_len += block_size;
}

// Fill padding with random bytes
randombytes(padding, padding_len);
```

### Big-Endian Encoding
SSH uses network byte order (big-endian) for all integers:
```c
void write_uint32_be(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xff;
    buf[1] = (val >> 16) & 0xff;
    buf[2] = (val >> 8) & 0xff;
    buf[3] = val & 0xff;
}

uint32_t read_uint32_be(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           ((uint32_t)buf[3]);
}
```

## Testing Progression

### Level 1: Version Exchange
```bash
nc localhost 2222
# Should immediately see: SSH-2.0-NanoSSH_0.1
# Type: SSH-2.0-Test
# (Connection will probably close, that's okay)
```

### Level 2: KEXINIT
```bash
ssh -vvv -p 2222 user@localhost
# Look for "SSH2_MSG_KEXINIT sent" and "SSH2_MSG_KEXINIT received"
```

### Level 3: Key Exchange Complete
```bash
ssh -vvv -p 2222 user@localhost
# Look for "expecting SSH2_MSG_NEWKEYS"
```

### Level 4: Authentication
```bash
ssh -vvv -p 2222 user@localhost
# Should prompt for password
# Look for "Authenticated to localhost"
```

### Level 5: Full Connection
```bash
ssh -p 2222 user@localhost
# Should see "Hello World" and disconnect
```

## Common Pitfalls

1. **Forgetting string length prefix**: Every string needs uint32 length
2. **Wrong endianness**: Must use big-endian for all integers
3. **Sequence number in MAC**: Don't forget to include it
4. **Padding too small**: Minimum 4 bytes padding
5. **Wrong channel ID**: Use client's ID when sending to client
6. **Window exhaustion**: Must implement WINDOW_ADJUST for real clients
7. **Not saving KEXINIT payloads**: Needed for exchange hash H
8. **Encrypting too early**: First two messages are unencrypted
9. **Wrong key for decryption**: Use client→server keys for receiving

## Size Optimization Opportunities

After getting it working:
- Hard-code algorithm choices (no negotiation)
- Remove malloc (static buffers)
- Remove error messages (just disconnect)
- Single cipher implementation
- Inline small functions
- Use lookup tables instead of loops
- Manual crypto primitives
- Strip debugging code
