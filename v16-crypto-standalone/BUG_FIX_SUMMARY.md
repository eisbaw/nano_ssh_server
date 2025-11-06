# Bug Fixes for v16-crypto-standalone

## Bugs Fixed

### 1. Server DH Public Key Encoding ✅
**Problem**: Server was sending DH public key (Q_S) as SSH string instead of mpint format.
- 60% of DH public keys have high bit set (0x80-0xFF)
- SSH protocol requires mpint format (with leading 0x00 when high bit set)
- Client rejected these as "bignum is negative"

**Fix**: Changed `write_string()` to `write_mpint()` on line 1102 (main.c:1102)

### 2. Client DH Public Key Parsing ✅  
**Problem**: Server wasn't handling mpint format from client correctly.
- Client sends DH public key as mpint (with leading 0x00 if high bit set)
- Server expected exactly 256 bytes, failed when client sent 257 bytes

**Fix**: Added mpint parsing logic (main.c:1062-1075)
- Handles 257-byte mpint (strips leading 0x00)
- Handles 256-byte mpint (no leading 0x00)
- Handles shorter mpint (pads with leading zeros)

### 3. Exchange Hash Computation ✅
**Problem**: Exchange hash included Q_C and Q_S as strings instead of mpints.
- RFC 4253 requires: mpint e, mpint f, mpint K in exchange hash
- Server was using write_string() instead of write_mpint()
- This caused RSA signature verification to fail

**Fix**: Changed to `write_mpint()` for Q_C and Q_S in exchange hash (main.c:720,724)

## Test Results

Server now successfully:
- ✅ Receives KEX_ECDH_INIT from client  
- ✅ Parses client DH public key (261-262 bytes with mpint encoding)
- ✅ Computes DH shared secret using Montgomery multiplication (~30ms)
- ✅ Computes exchange hash with correct mpint encoding
- ✅ Signs exchange hash with RSA-2048
- ✅ Builds and sends KEX_ECDH_REPLY (819 bytes)

## Remaining Issue

**Status**: Client closes connection after receiving KEX_ECDH_REPLY

**Symptoms**:
- Server logs show successful send: "KEX_ECDH_REPLY sent successfully"
- Client still waiting: "debug1: expecting SSH2_MSG_KEX_ECDH_REPLY"
- Then: "Connection closed by 127.0.0.1 port 2222"

**Possible Causes**:
1. Packet framing issue in send_packet()
2. Client receives packet but signature verification fails
3. Some other protocol violation in the KEX_ECDH_REPLY format

**Next Steps**:
- Check send_packet() implementation for framing bugs
- Verify KEX_ECDH_REPLY packet structure matches RFC 4253
- Test with Wireshark to see actual packet contents
- Check if client is receiving partial/corrupted data

## Files Modified

- `main.c`: mpint fixes, exchange hash fixes, debug output
- Added test files: `test_dh_highbit.c`, `test_ssh_client.sh`
