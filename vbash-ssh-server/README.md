# BASH SSH Server - World's First BASH-Only SSH Server

A minimal SSH-2.0 server implementation written **entirely in BASH**, demonstrating that you can implement network protocols using only shell scripting.

## Overview

This is an educational implementation of an SSH server using:
- **BASH** for protocol logic and state machine
- **openssl** for cryptographic operations
- **socat/nc** for TCP socket handling
- **xxd, od, dd** for binary data manipulation
- **awk, sed** for data processing

## Features

- ✅ SSH-2.0 protocol version exchange
- ✅ Key exchange (curve25519-sha256)
- ✅ Host key authentication (ssh-ed25519)
- ✅ Password authentication
- ✅ Session channel establishment
- ✅ Sends "Hello World" message

## Limitations

⚠️ **Educational/Demonstration Purposes Only**

This implementation is a proof-of-concept and has limitations:

1. **No encryption after NEWKEYS**: Due to BASH's limitations with binary data and streaming encryption, the simplified version skips full packet encryption
2. **Simplified protocol**: Some SSH protocol messages are simplified or stubbed
3. **Performance**: BASH is slow compared to compiled languages
4. **Security**: Not suitable for production use
5. **Compatibility**: May not work with all SSH clients due to simplifications

## Requirements

- `bash` (version 4.0+)
- `openssl` (for cryptographic operations)
- `socat` or `nc` (netcat) for network listening
- `xxd` (hex dump utility)
- `dd` (data copy utility)
- Standard Unix tools: `awk`, `sed`, `od`

## Installation

```bash
# Check requirements
command -v bash openssl socat xxd dd awk sed

# Make executable
chmod +x nano_ssh_server.sh

# Optional: Install to /usr/local/bin
sudo cp nano_ssh_server.sh /usr/local/bin/bash-ssh-server
```

## Usage

### Start the server

```bash
./nano_ssh_server.sh
```

The server listens on **port 2222** by default.

### Connect with SSH client

```bash
ssh -p 2222 user@localhost
# Password: password123
```

### Testing with sshpass

```bash
echo "password123" | sshpass ssh -o StrictHostKeyChecking=no -p 2222 user@localhost
```

## How It Works

### 1. Version Exchange

```bash
# Server sends version
echo "SSH-2.0-BashSSH_0.1"

# Client sends version
read -r client_version
```

### 2. Key Exchange (KEXINIT)

The server:
1. Generates an Ed25519 host key
2. Generates an ephemeral Curve25519 key
3. Sends KEXINIT message with supported algorithms
4. Receives client KEXINIT
5. Performs Curve25519 key exchange using `openssl`

### 3. Authentication

1. Client sends `SSH_MSG_SERVICE_REQUEST` for "ssh-userauth"
2. Server responds with `SSH_MSG_SERVICE_ACCEPT`
3. Client sends `SSH_MSG_USERAUTH_REQUEST` with password
4. Server validates credentials and sends `SSH_MSG_USERAUTH_SUCCESS`

### 4. Channel Establishment

1. Client sends `SSH_MSG_CHANNEL_OPEN`
2. Server responds with `SSH_MSG_CHANNEL_OPEN_CONFIRMATION`
3. Client may send `SSH_MSG_CHANNEL_REQUEST` (e.g., for pty)
4. Server sends `SSH_MSG_CHANNEL_DATA` with "Hello World\n"
5. Server sends `SSH_MSG_CHANNEL_EOF` and `SSH_MSG_CHANNEL_CLOSE`

## File Structure

```
vbash-ssh-server/
├── nano_ssh_server.sh       # Main server implementation
├── crypto_helpers.sh         # Cryptographic helper functions
├── ssh_packets.sh            # SSH packet encoding/decoding
├── README.md                 # This file
└── test_server.sh            # Test script
```

## Implementation Details

### Binary Data Handling

BASH doesn't handle binary data natively, so we use:

```bash
# Convert hex to binary
hex_to_bin() {
    echo -n "$1" | xxd -r -p
}

# Convert binary to hex
bin_to_hex() {
    xxd -p | tr -d '\n'
}

# Read N bytes
read_bytes() {
    dd bs=1 count=$1 2>/dev/null
}
```

### SSH Packet Structure

```
uint32    packet_length
byte      padding_length
byte[n]   payload
byte[m]   random padding
byte[20]  MAC (after NEWKEYS)
```

Implemented in BASH:

```bash
send_packet() {
    local payload="$1"
    local payload_len=${#payload}

    # Calculate padding (4-255 bytes, multiple of 8)
    local padding_len=$(( (8 - (payload_len + 5) % 8) % 8 ))
    [ $padding_len -lt 4 ] && padding_len=$((padding_len + 8))

    local packet_length=$((1 + payload_len + padding_len))

    write_uint32 "$packet_length"
    write_uint8 "$padding_len"
    echo -n "$payload"
    dd if=/dev/urandom bs=1 count=$padding_len 2>/dev/null
}
```

### Cryptography with OpenSSL

```bash
# Generate Ed25519 host key
openssl genpkey -algorithm ED25519 -out host_key.pem

# Generate Curve25519 ephemeral key
openssl genpkey -algorithm X25519 -out ephemeral_key.pem

# Perform key exchange
openssl pkeyutl -derive -inkey ephemeral_key.pem -peerkey client_public.pem
```

## Size Comparison

| Implementation | Language | Size | LOC |
|----------------|----------|------|-----|
| v17-from14 (C) | C | 25 KB | 1821 |
| vbash-ssh-server | BASH | ~15 KB | ~500 |

The BASH version is smaller in **source code size** but would be larger if you count the required external tools (openssl, socat, etc.).

## Debugging

Enable verbose logging:

```bash
# Set debug mode
bash -x ./nano_ssh_server.sh

# Watch server logs
tail -f /var/log/syslog | grep "BASH SSH"
```

Capture network traffic:

```bash
# Capture packets
sudo tcpdump -i lo -w ssh_bash.pcap port 2222

# Analyze with Wireshark
wireshark ssh_bash.pcap
```

## Known Issues

1. **Encryption not fully implemented**: After NEWKEYS, packets should be encrypted with AES-128-CTR and authenticated with HMAC-SHA2-256. This is complex in BASH and currently simplified.

2. **Binary data handling**: BASH struggles with binary data, especially null bytes. We use workarounds with xxd and dd.

3. **Performance**: Starting `openssl` processes for each crypto operation is slow.

4. **Resource usage**: Each connection spawns multiple sub-processes.

5. **Edge cases**: May not handle all SSH client variations or error conditions.

## Future Improvements

- [ ] Implement full packet encryption/decryption pipeline
- [ ] Add HMAC-SHA2-256 MAC verification
- [ ] Support multiple concurrent connections
- [ ] Add configuration file support
- [ ] Implement public key authentication
- [ ] Port to other shells (zsh, dash)
- [ ] Create pure POSIX sh version

## Security Warning

⚠️ **DO NOT USE IN PRODUCTION**

This implementation:
- Has not been security audited
- May contain vulnerabilities
- Is designed for education, not security
- Should only be used in isolated test environments

## Educational Value

This project demonstrates:

1. **Protocol Implementation**: How to implement binary network protocols in scripting languages
2. **SSH Protocol**: Understanding SSH handshake and packet structure
3. **Cryptography Integration**: Using OpenSSL for key exchange and signatures
4. **Binary Data in BASH**: Techniques for handling binary data in text-oriented shells
5. **Network Programming**: TCP socket handling with socat/nc

## References

- [RFC 4253 - SSH Transport Layer Protocol](https://www.rfc-editor.org/rfc/rfc4253)
- [RFC 4252 - SSH Authentication Protocol](https://www.rfc-editor.org/rfc/rfc4252)
- [RFC 4254 - SSH Connection Protocol](https://www.rfc-editor.org/rfc/rfc4254)
- [RFC 5656 - Elliptic Curve Algorithm Integration](https://www.rfc-editor.org/rfc/rfc5656)

## License

MIT License - See LICENSE file

## Author

Created as a demonstration of protocol implementation in BASH.

## Acknowledgments

- Based on the nano_ssh_server project (C implementation)
- Inspired by the desire to see how far we can push BASH
- Thanks to the SSH protocol designers for detailed RFCs

---

**"If you can do it in C, you can probably do it in BASH... eventually."**
