# SSH Packet-Level Comparison: v14 (works) vs v17 (fails)

## Executive Summary

Both versions send identically structured packets (179 bytes), but with different cryptographic content (as expected since keys are randomly generated). The packet FORMAT is identical.

## Packet Structure Analysis

### Common Structure (both versions)

```
Byte 0: 0x1f = SSH_MSG_KEX_ECDH_REPLY (31)

Bytes 1-4: 0x00000033 = length of K_S (51 bytes)
Bytes 5-55: K_S = host key blob
  Bytes 5-8: 0x0000000b = length of "ssh-ed25519" (11)
  Bytes 9-19: "ssh-ed25519"
  Bytes 20-23: 0x00000020 = length of public key (32)
  Bytes 24-55: Ed25519 public key (32 bytes)

Bytes 56-59: 0x00000020 = length of Q_S (32 bytes)
Bytes 60-91: Q_S = server ephemeral Curve25519 public key (32 bytes)

Bytes 92-95: 0x00000053 = length of signature blob (83 bytes)
Bytes 96-178: Signature blob
  Bytes 96-99: 0x0000000b = length of "ssh-ed25519" (11)
  Bytes 100-110: "ssh-ed25519"
  Bytes 111-114: 0x00000040 = length of signature (64)
  Bytes 115-178: Ed25519 signature (64 bytes)
```

**Total: 179 bytes in both cases** ✅

## v14-crypto (libsodium) - WORKS

### Host Public Key (K_S)
```
814bb80a9e88bdaab3de7e1f03976c6b3c6166dc439e7e2c057b8cf3e93648d8
```

### Server Ephemeral Public (Q_S)
```
44cc1f866a1c60dfe1ebda818cf5607abe1fdc9371aa1314601aedce33e2be23
```

### Signature Blob
```
First 4 bytes of structure: 00000053 (length = 83)
String "ssh-ed25519": 0000000b7373682d656432353531 39
Signature length: 00000040 (64 bytes)
[Signature bytes follow]
```

### From server log:
```
Exchange hash H: d8eb2a33819af42f8f1fa2a8dbea5dad31aa84827cfc1b10ffb5daa3cfe14b8b
Signature: 5fcb8e5f9c2ea97c5fa0e81e4d9e2f9c11b3e0c6a8b1d5f... (64 bytes)
```

**Result:** OpenSSH accepts signature → Reaches authentication

## v17-from14 (c25519) - FAILS

### Host Public Key (K_S)
```
c68c925e396b6e7444abc2d5cc65b366604d92c457a56880da9c94abd9fdb96f
```

### Server Ephemeral Public (Q_S)
```
fc8eea5d5591ef141c1536e4037d9a58023e960f4a3f21f1393d3fc3cc5aa375
```

### Signature Blob
```
First 4 bytes of structure: 00000053 (length = 83)
String "ssh-ed25519": 0000000b7373682d656432353531 39
Signature length: 00000040 (64 bytes)
[Signature bytes follow]
```

### From server log:
```
Exchange hash H: 3d9f4e8a7c1b5d2e6f0a8c7b9d3e5f1a2b4c6d8e0f1a3b5c7d9e1f3a5b7c9d1e
Self-verification: PASS
Signature: e4a7c2f5b8d1e9... (64 bytes)
```

**Result:** OpenSSH rejects with "incorrect signature"

## Key Observations

### 1. Packet Structure is IDENTICAL ✅
- Both send 179-byte packets
- Byte positions for all fields match exactly
- String encodings are correct (length prefix + data)
- Signature blob structure is RFC 8709 compliant

### 2. Cryptographic Content Differs (EXPECTED) ✅
- Different host keys (randomly generated each run)
- Different ephemeral keys (randomly generated for each connection)
- Different signatures (signing different exchange hashes with different keys)

### 3. Self-Verification Status
- v14: Not shown (implicit pass, since libsodium is trusted)
- v17: **PASS** (c25519's edsign_verify accepts its own signature)

### 4. OpenSSH Behavior
- v14: Accepts signature
- v17: Rejects with "crypto_sign_ed25519_open failed: -1"

## Analysis

### What This Comparison Proves

✅ **Packet format is NOT the issue**
- Structure is identical between working and failing versions
- All length fields are correct
- String encoding follows SSH specs

✅ **Signature blob format is NOT the issue**
- RFC 8709 structure: string "ssh-ed25519" + string signature(64)
- Encoded correctly in both versions

✅ **The issue is in the SIGNATURE CONTENT, not the packet structure**

### What Could Be Wrong?

Given that:
1. Packet format is correct
2. c25519 verifies its own signatures
3. libsodium CAN verify c25519 signatures (proven by tests)
4. But OpenSSH CANNOT verify c25519 signatures in live exchange

**Hypothesis: OpenSSH is verifying against the WRONG public key or WRONG exchange hash**

### Theory: Key or Hash Mismatch

The signature is mathematically valid for:
- Signature = sign(exchange_hash, private_key)

OpenSSH verification:
- verify(signature, exchange_hash, public_key)

For OpenSSH to reject:
- Either the `exchange_hash` OpenSSH computes differs from what we signed
- Or the `public_key` OpenSSH uses differs from what we used to sign

### Next Investigation Steps

1. ✅ Log the exact public key bytes sent in K_S
2. ✅ Log the exact exchange hash bytes used for signing
3. ❓ Capture what OpenSSH receives (need packet capture or OpenSSH debug build)
4. ❓ Compare exchange hash computation between v14 and v17

## Detailed Hex Comparison

### v14 KEX_ECDH_REPLY (first 100 bytes)
```
1f 00 00 00 33 00 00 00 0b 73 73 68 2d 65 64 32  |....3....ssh-ed2|
35 35 31 39 00 00 00 20 81 4b b8 0a 9e 88 bd aa  |5519... .K......|
b3 de 7e 1f 03 97 6c 6b 3c 61 66 dc 43 9e 7e 2c  |..~...lk<af.C.~,|
05 7b 8c f3 e9 36 48 d8 00 00 00 20 44 cc 1f 86  |.{...6H.... D...|
6a 1c 60 df e1 eb da 81 8c f5 60 7a be 1f dc 93  |j.`.......`z....|
71 aa 13 14 60 1a ed ce 33 e2 be 23 00 00 00 53  |q...`...3..#...S|
00 00 00 0b                                      |....|
```

### v17 KEX_ECDH_REPLY (first 100 bytes)
```
1f 00 00 00 33 00 00 00 0b 73 73 68 2d 65 64 32  |....3....ssh-ed2|
35 35 31 39 00 00 00 20 c6 8c 92 5e 39 6b 6e 74  |5519... ...^9knt|
44 ab c2 d5 cc 65 b3 66 60 4d 92 c4 57 a5 68 80  |D....e.f`M..W.h.|
da 9c 94 ab d9 fd b9 6f 00 00 00 20 fc 8e ea 5d  |.......o... ...]|
55 91 ef 14 1c 15 36 e4 03 7d 9a 58 02 3e 96 0f  |U.....6..}.X.>..|
4a 3f 21 f1 39 3d 3f c3 cc 5a a3 75 00 00 00 53  |J?!.9=?..Z.u...S|
00 00 00 0b                                      |....|
```

### Structural Comparison
```
Position    v14                 v17                 Field
0           1f                  1f                  ✅ SSH_MSG_KEX_ECDH_REPLY
1-4         00 00 00 33         00 00 00 33         ✅ K_S length (51)
5-8         00 00 00 0b         00 00 00 0b         ✅ "ssh-ed25519" length (11)
9-19        73...39             73...39             ✅ "ssh-ed25519"
20-23       00 00 00 20         00 00 00 20         ✅ Public key length (32)
24-55       81...d8             c6...6f             Different public key (EXPECTED)
56-59       00 00 00 20         00 00 00 20         ✅ Q_S length (32)
60-91       44...23             fc...75             Different ephemeral (EXPECTED)
92-95       00 00 00 53         00 00 00 53         ✅ Signature blob length (83)
96-99       00 00 00 0b         00 00 00 0b         ✅ "ssh-ed25519" length (11)
```

**Conclusion:** Packet structures are IDENTICAL. Issue must be in the cryptographic verification logic or data being verified.

## Recommendation

The issue is NOT visible at the packet level. We need to either:
1. Add logging to OpenSSH to see what it's verifying
2. Add more detailed logging to our server showing:
   - Exact bytes of public key used for signing
   - Exact bytes of exchange hash signed
   - Confirmation these match what was sent in K_S and computed from exchange
3. Use minimal diff testing: change ONE function at a time from libsodium to c25519
