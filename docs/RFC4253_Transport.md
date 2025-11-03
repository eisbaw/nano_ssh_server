# RFC 4253: SSH Transport Layer Protocol

**Reference:** https://datatracker.ietf.org/doc/html/rfc4253

## Overview

The SSH Transport Layer Protocol provides server authentication, confidentiality, and integrity. It runs on top of TCP/IP and provides the foundation for SSH.

## Protocol Flow

```
Client                                Server
------                                ------
TCP Connection Established
|                                     |
|-- Version Exchange --------------->|  (plaintext)
|<-- Version Exchange ----------------|  (plaintext)
|                                     |
|-- SSH_MSG_KEXINIT ---------------->|  (plaintext)
|<-- SSH_MSG_KEXINIT -----------------|  (plaintext)
|                                     |
|-- Key Exchange Messages ---------->|
|<-- Key Exchange Messages -----------|
|                                     |
|-- SSH_MSG_NEWKEYS ----------------->|
|<-- SSH_MSG_NEWKEYS -----------------|
|                                     |
[All subsequent packets are encrypted]
```

## 1. Version Exchange (Plaintext)

### Format
```
SSH-protoversion-softwareversion SP comments CR LF
```

### Example
```
SSH-2.0-NanoSSH_0.1\r\n
```

### Implementation Notes
- MUST be sent immediately after TCP connection
- First line sent by both client and server
- Maximum 255 bytes
- Must start with "SSH-2.0-" (we only support SSH-2.0)
- Comments are optional
- Server waits for client version, then sends its own

### Minimal Implementation
```c
const char *server_version = "SSH-2.0-NanoSSH_0.1\r\n";
send(socket, server_version, strlen(server_version), 0);

char client_version[256];
recv(socket, client_version, 255, 0);
// Verify starts with "SSH-2.0-"
```

## 2. Binary Packet Protocol

All packets after version exchange use this format:

```
uint32    packet_length  (excluding MAC and length field itself)
byte      padding_length
byte[n1]  payload        (n1 = packet_length - padding_length - 1)
byte[n2]  padding        (n2 = padding_length, random data)
byte[m]   mac            (Message Authentication Code)
```

### Constraints
- packet_length: 4 bytes, does NOT include MAC or itself
- padding_length: 1 byte, length of padding (minimum 4 bytes)
- Total packet length (excluding MAC) must be multiple of cipher block size (or 8)
- padding: random bytes, minimum 4, maximum 255
- MAC: depends on negotiated algorithm (can be 0 for unencrypted packets)

### Before Encryption (first two packets)
```c
// Simple packet construction
uint32_t payload_len = strlen(payload);
uint32_t padding_len = 8 - ((5 + payload_len) % 8);
if (padding_len < 4) padding_len += 8;

uint32_t packet_len = 1 + payload_len + padding_len;

// Write: packet_len(4) | padding_len(1) | payload | padding
```

### After Encryption
- Encrypt: padding_length || payload || padding
- Prepend: packet_length (unencrypted or encrypted depending on mode)
- Append: MAC

## 3. Key Exchange (SSH_MSG_KEXINIT)

### Message Format
```
byte      SSH_MSG_KEXINIT (20)
byte[16]  cookie (random bytes)
name-list kex_algorithms
name-list server_host_key_algorithms
name-list encryption_algorithms_client_to_server
name-list encryption_algorithms_server_to_client
name-list mac_algorithms_client_to_server
name-list mac_algorithms_server_to_client
name-list compression_algorithms_client_to_server
name-list compression_algorithms_server_to_client
name-list languages_client_to_server
name-list languages_server_to_client
boolean   first_kex_packet_follows
uint32    0 (reserved)
```

### Name-List Format
```
uint32    length
byte[length] comma-separated-names
```

Example: `"diffie-hellman-group14-sha256,curve25519-sha256"`

### Minimal Implementation Strategy

For SMALLEST size, offer only ONE algorithm for everything:

```c
// Recommended minimal set
kex_algorithms:          "curve25519-sha256"  // Small, modern
server_host_key:         "ssh-ed25519"        // Smallest signature
encryption_c2s/s2c:      "chacha20-poly1305@openssh.com" // AEAD, no separate MAC
mac_c2s/s2c:            "" // Not needed with chacha20-poly1305
compression:            "none"
languages:              ""
```

Alternative even smaller (if client supports):
```c
// Absolute minimal (check openssh client support)
kex_algorithms:          "curve25519-sha256"
server_host_key:         "ssh-ed25519"
encryption:              "aes128-ctr"         // Widely supported
mac:                     "hmac-sha2-256"      // Must have if not AEAD
compression:            "none"
```

## 4. Diffie-Hellman Key Exchange

### For curve25519-sha256 (RECOMMENDED)

Message flow:
```
Client                          Server
|-- SSH_MSG_KEX_ECDH_INIT ----->|
|   (client ephemeral public)   |
|                                |
|<-- SSH_MSG_KEX_ECDH_REPLY -----|
    (server public, signature)
```

#### SSH_MSG_KEX_ECDH_INIT (30)
```
byte      SSH_MSG_KEX_ECDH_INIT (30)
string    Q_C (client ephemeral public key, 32 bytes for curve25519)
```

#### SSH_MSG_KEX_ECDH_REPLY (31)
```
byte      SSH_MSG_KEX_ECDH_REPLY (31)
string    K_S (server public host key)
string    Q_S (server ephemeral public key, 32 bytes)
string    signature of H
```

### Shared Secret Calculation

```
Shared secret K = DH(client_private, server_public)
```

### Exchange Hash H

```
H = SHA256(
    V_C || V_S ||           // version strings
    I_C || I_S ||           // KEXINIT payloads
    K_S ||                  // server host key
    Q_C || Q_S ||          // ephemeral public keys
    K                       // shared secret
)
```

Where `||` means concatenation and strings are encoded as:
```
uint32 length
byte[length] data
```

**IMPORTANT:** The first exchange hash H becomes the **session_id** and is used for all key derivation.

## 5. Key Derivation

From shared secret K and exchange hash H, derive all keys:

```
Initial IV client->server:  HASH(K || H || "A" || session_id)
Initial IV server->client:  HASH(K || H || "B" || session_id)
Encryption key c->s:        HASH(K || H || "C" || session_id)
Encryption key s->c:        HASH(K || H || "D" || session_id)
Integrity key c->s:         HASH(K || H || "E" || session_id)
Integrity key s->c:         HASH(K || H || "F" || session_id)
```

Where HASH = SHA256 (or whatever was negotiated).

If key length needed is longer than hash output, extend by:
```
K1 = HASH(K || H || X || session_id)
K2 = HASH(K || H || K1)
K3 = HASH(K || H || K1 || K2)
...
key = K1 || K2 || K3...
```

## 6. SSH_MSG_NEWKEYS

After key exchange complete:

```
byte    SSH_MSG_NEWKEYS (21)
```

Both sides send this. After sending, that side MUST start using new keys for OUTGOING packets. After receiving, that side MUST start using new keys for INCOMING packets.

## 7. Service Request

After key exchange, client requests service:

```
byte      SSH_MSG_SERVICE_REQUEST (5)
string    service_name ("ssh-userauth")
```

Server responds:
```
byte      SSH_MSG_SERVICE_ACCEPT (6)
string    service_name ("ssh-userauth")
```

## Message Type Numbers (Important)

```
SSH_MSG_DISCONNECT              1
SSH_MSG_IGNORE                  2
SSH_MSG_UNIMPLEMENTED          3
SSH_MSG_DEBUG                   4
SSH_MSG_SERVICE_REQUEST         5
SSH_MSG_SERVICE_ACCEPT          6
SSH_MSG_KEXINIT                20
SSH_MSG_NEWKEYS                21
SSH_MSG_KEX_ECDH_INIT          30  // for curve25519
SSH_MSG_KEX_ECDH_REPLY         31  // for curve25519
```

## Data Types

```
byte        1 byte (8 bits)
boolean     1 byte (0 = FALSE, 1 = TRUE)
uint32      4 bytes, big-endian
uint64      8 bytes, big-endian
string      uint32 length || byte[length] data
mpint       multiple precision integer
            uint32 length || byte[length] data
            (stored in big-endian, two's complement)
name-list   uint32 length || byte[length] comma-separated-names
```

## Minimal Implementation Checklist

- [ ] Version exchange (send/receive)
- [ ] Binary packet framing (before encryption)
- [ ] SSH_MSG_KEXINIT (send/receive, parse algorithm lists)
- [ ] Algorithm negotiation (pick first match)
- [ ] Curve25519 key generation
- [ ] SSH_MSG_KEX_ECDH_INIT (receive from client)
- [ ] SSH_MSG_KEX_ECDH_REPLY (send to client)
- [ ] Exchange hash H calculation
- [ ] Key derivation (6 keys total)
- [ ] SSH_MSG_NEWKEYS (send/receive)
- [ ] Enable encryption (chacha20-poly1305 or aes128-ctr)
- [ ] Binary packet framing (with encryption)
- [ ] SSH_MSG_SERVICE_REQUEST/ACCEPT

## Security Notes

- Random padding MUST be cryptographically random
- Cookie in KEXINIT MUST be random
- Sequence numbers start at 0, increment for each packet
- Implement SSH_MSG_DISCONNECT for error handling
- Session ID (first H) never changes during connection

## Code Size Optimization

- Use TweetNaCl for Curve25519 (very small)
- Use ChaCha20-Poly1305 AEAD (no separate MAC = less code)
- Hard-code algorithm choices (no negotiation flexibility)
- Static buffers (no malloc)
- Minimal error messages
