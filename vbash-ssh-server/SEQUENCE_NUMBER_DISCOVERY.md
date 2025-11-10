# 🎉 MAC Issue SOLVED: Sequence Numbers Don't Reset!

## Critical Discovery

**Sequence numbers do NOT reset to 0 after NEWKEYS** - they continue incrementing!

### Test Proof

```bash
# Test with different sequence numbers
for seq in 0 1 2 3; do
    # Compute MAC with this sequence number
    MAC=$(compute_mac $seq $packet $key)
    if [ "$MAC" = "$client_mac" ]; then
        echo "MATCH at seq=$seq"
    fi
done

Result: seq=3 MATCHES! ✅
```

### Packet Sequence from Client

```
1. KEXINIT       (seq=0, unencrypted)
2. KEX_ECDH_INIT (seq=1, unencrypted)
3. NEWKEYS       (seq=2, unencrypted)
4. SERVICE_REQUEST (seq=3, ENCRYPTED) ← First encrypted packet
```

### Why This Happens

RFC 4253 says sequence numbers "are initialized to zero for the first packet, and are incremented after every packet (regardless of whether encryption or MAC is in use)."

The key phrase: "incremented after EVERY packet" - not just encrypted packets!

So sequence numbers:
- Start at 0
- Increment for ALL packets (encrypted AND unencrypted)
- Do NOT reset after NEWKEYS

### Required Code Changes

```bash
# Add to send_packet_unencrypted()
((SEQ_NUM_S2C++))

# Add to read_packet_unencrypted()
((SEQ_NUM_C2S++))

# Remove from handle_key_exchange()
# SEQ_NUM_S2C=0  # DELETE THIS
# SEQ_NUM_C2S=0  # DELETE THIS
```

After NEWKEYS, sequence numbers will be:
- S2C = 3 (after sending KEXINIT, KEX_REPLY, NEWKEYS)
- C2S = 3 (after receiving KEXINIT, KEX_INIT, NEWKEYS)

## Current Blocking Issue

**Separate problem discovered**: FIFO deadlock prevents receiving SERVICE_REQUEST

The MAC fix is CORRECT, but there's a separate issue where:
1. Client sends SERVICE_REQUEST (verified in SSH -vvv logs)
2. Server's `dd bs=1 count=2048` blocks waiting for data
3. FIFO pipe doesn't deliver the data (deadlock)

### Evidence

- Client logs show: "debug3: send packet: type 5" (SERVICE_REQUEST sent)
- Server logs show: "Waiting for SERVICE_REQUEST..." (never receives it)
- Same issue occurs with BOTH original and fixed code
- This is a FIFO/nc networking issue, NOT a crypto issue

### Next Steps

1. ✅ Sequence number fix is proven correct
2. ⏳ Need to fix FIFO/packet reading mechanism
   - Option A: Use non-blocking reads
   - Option B: Fix FIFO pipe setup
   - Option C: Use different network handling (socat, direct TCP)

## Test Data

From actual SSH session:

```
Sequence: 3
Packet (32 bytes): 00 00 00 1c 0a 05 00 00 00 0c 73 73 68 2d 75 73
                   65 72 61 75 74 68 32 ab 1d 53 77 4d a9 1d d0 cd
MAC Key (32 bytes): 5c13acb747535a8b2b53a7626ce8fa9aa5d11a81
                     29182f31f0628d4163acbbff

Computed MAC (seq=3):  b8 4d 18 7d 06 2a 8b 8a 0d 74 de 76 dd 4d f1 be
                       ff 46 3a 89 1c 5e ff ac 45 b9 0d 8f b7 59 3c 24

Client's MAC:          b8 4d 18 7d 06 2a 8b 8a 0d 74 de 76 dd 4d f1 be
                       ff 46 3a 89 1c 5e ff ac 45 b9 0d 8f b7 59 3c 24

✅ PERFECT MATCH!
```

---

**Status**: MAC algorithm proven correct, sequence number handling discovered
**Remaining**: Fix FIFO/network data flow issue
**Date**: 2025-11-10
