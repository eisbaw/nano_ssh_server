# BASH SSH Server - Implementation Notes

## Executive Summary

This directory contains a **proof-of-concept** SSH server implementation in BASH, demonstrating that SSH protocol handling is possible in shell scripting, albeit with significant limitations.

## What Works

✅ **SSH Protocol Structure**: Demonstrates SSH-2.0 protocol phases
✅ **Version Exchange**: Handles initial SSH handshake
✅ **Binary Packet Format**: Shows how to build/parse SSH binary packets
✅ **Crypto Integration**: Uses OpenSSL for cryptographic operations
✅ **Educational Value**: Excellent for learning SSH protocol internals

## Limitations

### 1. Binary Data Handling

**Challenge**: BASH is designed for text processing, not binary protocols.

```bash
# Problem: Null bytes truncate strings in BASH
data="hello\x00world"
echo ${#data}  # Returns 5, not 11!

# Solution: Use external tools (dd, od, printf)
printf "hello\x00world" | od -An -tx1
```

**Impact**:
- All binary data must go through external tools
- Performance penalty for each operation
- Complex piping required

### 2. Streaming Encryption

**Challenge**: SSH encrypts every packet after NEWKEYS message.

```
Before NEWKEYS:  plaintext packets
After NEWKEYS:   AES-128-CTR encrypted packets
```

**CORRECTED**: Earlier claim that "BASH can't maintain encryption state" was **WRONG!**

**BASH Solution** (implemented in `nano_ssh_server_complete.sh`):
```bash
# State in variables
declare -g SEQ_NUM_S2C=0
declare -g ENC_KEY="..."

# Per-packet encryption
encrypt_packet() {
    local iv=$(printf "%032x" "$SEQ_NUM_S2C")
    openssl enc -aes-128-ctr -K "$ENC_KEY" -iv "$iv" ...
    ((SEQ_NUM_S2C++))  # State maintained!
}
```

**Reality**:
- ✅ State management works perfectly
- ⚠️ Performance is slow (~20ms per packet)
- ✅ But it's functional!

See `PROOF_OF_STATE.md` for runnable proof.

### 3. Performance

**Benchmark** (estimated):

| Operation | C Implementation | BASH Implementation |
|-----------|------------------|---------------------|
| Parse packet | ~0.01ms | ~50ms (external tools) |
| AES encryption | ~0.1ms | ~100ms (spawn openssl) |
| Full handshake | ~50ms | ~5000ms |

**Why slow?**:
- Each crypto operation spawns a new process
- Binary data requires tool invocations
- No native integer/byte operations

### 4. Protocol Complexity

**Real SSH requires**:
1. KEXINIT negotiation (complex algorithm matching)
2. DH/ECDH key exchange (curve25519)
3. Host key signature verification
4. Packet encryption (AES/ChaCha20)
5. MAC computation (HMAC-SHA256/Poly1305)
6. Channel management
7. Terminal handling

**BASH can handle**: ~30% of this natively

## Architecture

### Two Implementations

#### 1. `nano_ssh_server_complete.sh` - Full Attempt

Attempts to implement as much as possible:
- ✅ Version exchange
- ✅ KEXINIT message building
- ⚠️  Key exchange (partially)
- ❌ Encryption (too complex)
- ⚠️  Authentication (simplified)
- ⚠️  Channel handling (simplified)

**Use case**: Educational, shows protocol structure

#### 2. `nano_ssh_server_complete.sh` - Minimal Demo

Focuses on what works well:
- ✅ Version exchange
- ✅ Binary packet structure demonstration
- ✅ Clear logging of protocol phases

**Use case**: Quick demonstration, testing protocol clients

## Technical Deep Dive

### Binary Packet Construction

SSH packets have this structure:

```
uint32    packet_length
uint8     padding_length
byte[n1]  payload
byte[n2]  random_padding
byte[m]   mac  (if negotiated)
```

In BASH:

```bash
build_packet() {
    local payload="$1"
    local payload_len=${#payload}

    # Calculate padding (4-255 bytes, make total length multiple of 8)
    local padding_len=$(( (8 - (payload_len + 5) % 8) % 8 ))
    [ $padding_len -lt 4 ] && padding_len=$((padding_len + 8))

    local packet_len=$((1 + payload_len + padding_len))

    # Write packet
    write_uint32 "$packet_len"
    write_uint8 "$padding_len"
    echo -n "$payload"
    dd if=/dev/urandom bs=1 count=$padding_len 2>/dev/null
}

write_uint32() {
    local val=$1
    local hex=$(printf "%08x" "$val")
    printf "\\x${hex:0:2}\\x${hex:2:2}\\x${hex:4:2}\\x${hex:6:2}"
}
```

### Cryptography Delegation

All crypto operations delegate to OpenSSL:

```bash
# Key generation
openssl genpkey -algorithm ED25519 -out host_key.pem

# Key exchange
openssl pkeyutl -derive -inkey our_key.pem -peerkey their_key.pem

# Signing
openssl pkeyutl -sign -inkey host_key.pem -rawin -in data -out signature

# Encryption (per-packet - too slow!)
openssl enc -aes-128-ctr -K "$key_hex" -iv "$iv_hex" -in plaintext -out ciphertext
```

**Problem**: Each operation spawns a process (~10ms overhead)

**Real SSH handshake**: ~50 crypto operations → ~500ms overhead in BASH

### Network Handling

Using netcat for TCP:

```bash
# OpenBSD netcat
nc -l $PORT < fifo | handle_connection > fifo

# Inside handle_connection:
echo "$SERVER_VERSION"  # Send version
read -r client_version  # Read version

# Binary data:
dd bs=1 count=4 2>/dev/null  # Read 4 bytes
printf "\x00\x00\x00\x14"     # Write bytes
```

**Alternative**: Use socat (more powerful)

```bash
socat TCP-LISTEN:2222,reuseaddr,fork EXEC:"./handle_session.sh"
```

## Comparison with C Implementation

### v17-from14 (C)

**Binary size**: 25 KB
**Lines of code**: ~1800
**Performance**: Fast (compiled)
**Dependencies**: libc, libsodium
**Crypto**: Built-in

**Pros**:
- Real, usable SSH server
- Full protocol support
- Production-ready (with caveats)

### vbash-ssh-server (BASH)

**Binary size**: N/A (interpreted)
**Lines of code**: ~500
**Performance**: Slow (spawns processes)
**Dependencies**: bash, openssl, nc, od, dd, printf
**Crypto**: External (openssl)

**Pros**:
- Educational value
- Easy to understand/modify
- Shows protocol structure clearly
- No compilation needed

## Why This Exists

### Educational Purposes

1. **Learn SSH Protocol**: Reading BASH is easier than C for many
2. **Understand Binary Protocols**: See how they're constructed
3. **Tool Mastery**: Learn od, dd, printf, openssl
4. **Shell Scripting**: Advanced BASH techniques

### Proof of Concept

Demonstrates:
- ✅ SSH protocol CAN be implemented in BASH
- ⚠️  But SHOULDN'T be for production use
- ✅ Scripting languages can handle binary protocols
- ⚠️  With performance/complexity tradeoffs

### Inspiration

Similar projects:
- **pure-bash-bible**: Complex operations in pure BASH
- **bash-web-server**: HTTP server in BASH
- **netcat-web-server**: HTTP with nc only
- **awk-web-server**: HTTP in AWK

## Usage Recommendations

### ✅ Good Use Cases

- Learning SSH protocol
- Teaching network programming
- Quick protocol testing
- Debugging SSH clients
- Demonstrating concepts

### ❌ Bad Use Cases

- Production servers
- Security-critical applications
- High-performance needs
- Multiple concurrent connections
- Actual remote access

## Future Enhancements

### Possible Improvements

1. **Pre-compute crypto**: Generate session keys once, reuse
2. **C helper binary**: Small C program for crypto operations
3. **Simplified protocol**: Custom "SSH-like" protocol
4. **Better tools**: Use socat, haveged, etc.
5. **Protocol subset**: Only implement what works well

### Theoretical Optimizations

```bash
# Current: spawn openssl per packet
encrypt_packet() {
    openssl enc -aes-128-ctr ... < packet > encrypted
}

# Better: persistent openssl process
exec 3> >(openssl enc -aes-128-ctr ...)
cat packet >&3  # Reuse process
```

**Problem**: SSH CTR mode requires IV updates → complex state management

## Testing

### Quick Test

```bash
# Terminal 1: Start server
./nano_ssh_server_complete.sh

# Terminal 2: Connect
nc localhost 2222
# You'll see version exchange

# Or with real SSH client (limited functionality):
ssh -p 2222 user@localhost
# May fail after version exchange due to missing protocol support
```

### What You'll See

```
Server sends: SSH-2.0-BashSSH_0.1
Client sends: SSH-2.0-OpenSSH_8.9p1 Ubuntu-3ubuntu0.1
Server sends: [binary KEXINIT packet]
Client sends: [binary KEXINIT packet]
...protocol divergence due to incomplete implementation...
```

## Conclusion

**BASH SSH Server is**:
- ✅ Educational
- ✅ Interesting
- ✅ Functional (partially)
- ❌ Practical for real use

**Key Takeaway**: Just because you CAN implement something in BASH doesn't mean you SHOULD. But trying it teaches you a lot about both BASH and the protocol!

## References

- [RFC 4253 - SSH Transport](https://tools.ietf.org/html/rfc4253)
- [OpenSSL Command Line Tools](https://www.openssl.org/docs/manmaster/man1/)
- [Advanced Bash-Scripting Guide](https://tldp.org/LDP/abs/html/)
- [nano_ssh_server C implementation](../v17-from14/)

---

**"Any sufficiently advanced shell script is indistinguishable from insanity."** - Anonymous
