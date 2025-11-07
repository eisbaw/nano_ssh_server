# Adam Dunkels Optimization Techniques - Application Plan

**Date:** 2025-11-05
**Project:** Nano SSH Server
**Baseline:** v11-opt10 (11,488 bytes with UPX, 15,552 bytes uncompressed)

---

## Research Summary: Adam Dunkels' Work

Adam Dunkels is renowned for creating the world's smallest TCP/IP stacks (uIP and lwIP) and successfully porting VNC to the Commodore 64 (1 MHz, 64KB RAM). His work demonstrates extreme optimization for 8-bit microcontrollers.

### Key Achievements
- **uVNC on C64**: VNC server on 1 MHz 6510 CPU with 64KB RAM
- **uIP**: Full TCP/IP stack in ~10KB code, few hundred bytes RAM
- **Protothreads**: Stackless threads requiring only 2 bytes per thread
- **Contiki OS**: Lightweight OS for 8-bit microcontrollers

---

## Core Optimization Techniques Identified

### 1. Single Buffer Architecture
**Concept:** Use ONE packet buffer for all operations (send/receive/processing)
- No buffer copies between layers
- Reuse same buffer across protocol phases
- Process data in-place wherever possible

**Benefits:**
- Eliminates buffer copy overhead
- Reduces RAM by 50-70%
- Smaller code (no copy functions)

**Application to SSH:**
```c
// Current approach (multiple buffers)
uint8_t send_buf[MAX_PACKET_SIZE];
uint8_t recv_buf[MAX_PACKET_SIZE];
uint8_t crypto_buf[MAX_PACKET_SIZE];

// Dunkels approach (single buffer)
uint8_t packet_buf[MAX_PACKET_SIZE];  // One buffer for everything
// Encrypt/decrypt in-place
// Build packets directly in buffer
// Process received data without copying
```

**Estimated Savings:** -800 to -1200 bytes (reduced buffer management code)

---

### 2. Event-Driven State Machine
**Concept:** Application is called by stack with events, not polling
- Callback-based architecture
- No complex state tracking
- Minimal state variables

**Benefits:**
- Simpler control flow
- Less state storage
- Faster response times

**Application to SSH:**
```c
// Current: Complex state machine with many variables
typedef struct {
    int state;
    int substates[10];
    // ... many state variables
} ssh_state_t;

// Dunkels approach: Event callbacks
typedef enum {
    EVT_CONNECTED,
    EVT_VERSION_RECEIVED,
    EVT_KEXINIT_RECEIVED,
    EVT_NEWKEYS_RECEIVED,
    EVT_AUTH_REQUEST,
    EVT_CHANNEL_OPEN,
    EVT_DATA_REQUEST
} ssh_event_t;

void ssh_handle_event(ssh_event_t event, void* data);
```

**Estimated Savings:** -400 to -600 bytes (simpler state management)

---

### 3. Application Regenerates Data (No Caching)
**Concept:** When retransmission needed, application regenerates data instead of caching
- Don't store sent packets for retransmission
- Application has callback to recreate data
- Saves enormous RAM

**Benefits:**
- No retransmission buffers needed
- Application logic becomes data source
- Dramatic RAM savings

**Application to SSH:**
```c
// Current: Store everything
uint8_t sent_packets[NUM_PACKETS][MAX_SIZE];

// Dunkels approach: Regenerate on demand
void regenerate_kexinit(uint8_t* buf) {
    // Recreate KEXINIT packet from scratch
    // Same deterministic output
}
```

**Estimated Savings:** -200 to -400 bytes (no retransmission buffer code)

---

### 4. Protothreads Pattern (Stackless Coroutines)
**Concept:** Use switch-based coroutines instead of callbacks/state machines
- Sequential code that looks synchronous
- No stack per thread (2 bytes state)
- Uses Duff's device trick

**Benefits:**
- Readable sequential code
- Minimal overhead (2 bytes)
- No complex state machines

**Application to SSH:**
```c
// Protothread-style SSH handling
#define PT_BEGIN() switch(pt->lc) { case 0:
#define PT_END() } pt->lc = 0; return 0
#define PT_WAIT_UNTIL(cond) pt->lc = __LINE__; case __LINE__: if(!(cond)) return 0

int ssh_thread(struct pt *pt) {
    PT_BEGIN();

    // Version exchange
    PT_WAIT_UNTIL(version_received);
    send_version();

    // Key exchange
    PT_WAIT_UNTIL(kexinit_received);
    send_kexinit();

    // ... sequential flow

    PT_END();
}
```

**Estimated Savings:** -300 to -500 bytes (simpler control flow)

---

### 5. Packed Structures
**Concept:** Use `__attribute__((packed))` to eliminate padding
- Every byte counts on 8-bit systems
- Remove alignment padding

**Benefits:**
- Smaller structs
- Less memory waste
- Better cache usage

**Application to SSH:**
```c
// Current: Natural alignment (padding added by compiler)
typedef struct {
    uint8_t type;      // 1 byte
    // 3 bytes padding
    uint32_t seq;      // 4 bytes
    uint8_t state;     // 1 byte
    // 7 bytes padding
} ssh_ctx_t;  // Total: 16 bytes

// Dunkels approach: Packed
typedef struct __attribute__((packed)) {
    uint32_t seq;      // 4 bytes
    uint8_t type;      // 1 byte
    uint8_t state;     // 1 byte
} ssh_ctx_t;  // Total: 6 bytes!
```

**Estimated Savings:** -100 to -200 bytes (smaller data structures)

---

### 6. Zero-Copy Buffer Operations
**Concept:** Build packets directly in output buffer
- Don't build in temp buffer then copy
- Write directly to network buffer
- Parse directly from receive buffer

**Benefits:**
- No memcpy() calls
- Faster execution
- Smaller code

**Application to SSH:**
```c
// Current: Build then copy
uint8_t temp[1024];
build_packet(temp);
memcpy(send_buf, temp, len);
send(sock, send_buf, len);

// Dunkels approach: Build directly
uint8_t *p = send_buf;
*p++ = msg_type;
write_uint32(p, value);
send(sock, send_buf, p - send_buf);
```

**Estimated Savings:** -150 to -300 bytes (eliminate copy operations)

---

### 7. Function Pointer Tables (Dispatch Tables)
**Concept:** Use function pointer arrays instead of switch statements
- More compact
- Easier to extend
- Potentially faster

**Benefits:**
- Smaller code
- Data-driven dispatch
- Better compiler optimization

**Application to SSH:**
```c
// Current: Large switch statement
void handle_message(uint8_t type) {
    switch(type) {
        case SSH_MSG_KEXINIT:      handle_kexinit();      break;
        case SSH_MSG_NEWKEYS:      handle_newkeys();      break;
        case SSH_MSG_SERVICE_REQ:  handle_service_req();  break;
        // ... 15 more cases
    }
}

// Dunkels approach: Function table
typedef void (*handler_fn)(void);
const handler_fn handlers[] = {
    [SSH_MSG_KEXINIT]     = handle_kexinit,
    [SSH_MSG_NEWKEYS]     = handle_newkeys,
    [SSH_MSG_SERVICE_REQ] = handle_service_req,
    // ... designated initializers
};

void handle_message(uint8_t type) {
    if (type < ARRAY_SIZE(handlers) && handlers[type])
        handlers[type]();
}
```

**Estimated Savings:** -200 to -400 bytes (compact dispatch)

---

### 8. String Pooling and Deduplication
**Concept:** Store strings once, reference by pointer
- Compiler does some automatically
- Manual pooling for max savings
- Use const char* arrays

**Benefits:**
- No duplicate strings in .rodata
- Smaller binary
- Easier to update

**Application to SSH:**
```c
// Current: Inline strings everywhere
send_algorithm("curve25519-sha256");
send_algorithm("aes128-ctr");

// Dunkels approach: String pool
const char* const string_pool[] = {
    "curve25519-sha256",  // [0]
    "aes128-ctr",        // [1]
    "ssh-ed25519",       // [2]
    "hmac-sha2-256",     // [3]
    // ...
};
#define STR_KEX_ALG    string_pool[0]
#define STR_CIPHER     string_pool[1]
send_algorithm(STR_KEX_ALG);
```

**Estimated Savings:** -100 to -200 bytes (deduplicated strings)

---

### 9. Minimal State Tracking
**Concept:** Track only absolutely necessary state
- Derive state from context where possible
- Use boolean flags instead of enums
- Pack flags into bitfields

**Benefits:**
- Smaller structs
- Less memory
- Simpler logic

**Application to SSH:**
```c
// Current: Lots of state variables
typedef struct {
    int kex_state;          // 4 bytes
    int auth_state;         // 4 bytes
    int channel_state;      // 4 bytes
    bool version_sent;      // 1 byte + padding
    bool version_received;  // 1 byte + padding
    bool encrypted;         // 1 byte + padding
    bool authenticated;     // 1 byte + padding
    // Total: 20+ bytes
} ssh_state_t;

// Dunkels approach: Bitfield flags
typedef struct {
    uint8_t flags;  // 1 byte for 8 flags
    uint8_t phase;  // 1 byte: 0=version, 1=kex, 2=auth, 3=session
    // Total: 2 bytes!
} ssh_state_t;

#define FLAG_VERSION_SENT     (1 << 0)
#define FLAG_VERSION_RCVD     (1 << 1)
#define FLAG_ENCRYPTED        (1 << 2)
#define FLAG_AUTHENTICATED    (1 << 3)
```

**Estimated Savings:** -50 to -150 bytes (compact state)

---

### 10. Overlapping Buffer Zones
**Concept:** Reuse same buffer regions for different protocol phases
- Crypto keys overlay packet buffer after use
- Temporary computations use packet buffer space
- Careful lifetime management

**Benefits:**
- Minimal peak RAM usage
- Same total functionality
- Smaller BSS section

**Application to SSH:**
```c
// Union of buffers that don't overlap in time
typedef union {
    struct {  // During key exchange
        uint8_t server_kex[32];
        uint8_t client_kex[32];
        uint8_t shared_secret[32];
    } kex;

    struct {  // After key exchange
        uint8_t enc_key[32];
        uint8_t mac_key[32];
        uint8_t iv[16];
        uint8_t packet[4096];
    } session;
} ssh_memory_t;
```

**Estimated Savings:** -200 to -400 bytes (reduced peak memory)

---

## Implementation Plan: New Versions

### v12-dunkels1: Single Buffer + Zero-Copy
**Goal:** Replace multiple buffers with one, eliminate memcpy

**Changes:**
- Single `packet_buf[MAX_PACKET_SIZE]` for all operations
- Build packets in-place
- Encrypt/decrypt in-place
- Parse without copying

**Estimated Size:** 14,500 bytes → **13,800 bytes** (-700 bytes, -4.8%)

**Effort:** Medium
**Risk:** Medium (must track buffer ownership carefully)

---

### v13-dunkels2: Event-Driven Architecture
**Goal:** Replace state machine with event callbacks

**Changes:**
- Event enum for all protocol events
- Single `handle_event(event, data)` dispatcher
- Minimal state tracking (current phase only)
- Event-driven control flow

**Estimated Size:** 13,800 bytes → **13,200 bytes** (-600 bytes, -4.3%)

**Effort:** High
**Risk:** Medium (different architecture pattern)

---

### v14-dunkels3: Packed Structures + Minimal State
**Goal:** Optimize data structures

**Changes:**
- Add `__attribute__((packed))` to all structs
- Use bitfields for boolean flags
- Reorder struct members for optimal packing
- Eliminate redundant state variables

**Estimated Size:** 13,200 bytes → **13,000 bytes** (-200 bytes, -1.5%)

**Effort:** Low
**Risk:** Low (mostly annotations)

---

### v15-dunkels4: Function Tables + String Pool
**Goal:** Replace switch statements and deduplicate strings

**Changes:**
- Function pointer table for message dispatch
- Const string pool array
- String deduplication
- Table-driven protocol handling

**Estimated Size:** 13,000 bytes → **12,500 bytes** (-500 bytes, -3.8%)

**Effort:** Medium
**Risk:** Low (mechanical transformation)

---

### v16-dunkels5: Protothread Pattern
**Goal:** Implement stackless coroutines for sequential flow

**Changes:**
- Protothread macros (PT_BEGIN, PT_WAIT_UNTIL, PT_END)
- Rewrite main loop as protothread
- Sequential code style
- Minimal state (line number only)

**Estimated Size:** 12,500 bytes → **12,000 bytes** (-500 bytes, -4.0%)

**Effort:** High
**Risk:** Medium (different control flow model)

---

### v17-dunkels6: Combined Optimizations
**Goal:** Combine all successful Dunkels techniques

**Changes:**
- All techniques from v12-v16
- Additional micro-optimizations discovered
- Final polish and refinement

**Estimated Size:** 12,000 bytes → **11,000 bytes** (-1,000 bytes, -8.3%)

**Effort:** Medium (integration work)
**Risk:** Medium (complex interactions)

---

## Total Potential Savings

| Technique | Savings (bytes) | Savings (%) | Difficulty |
|-----------|----------------|-------------|------------|
| Single buffer + zero-copy | -700 | -4.8% | Medium |
| Event-driven architecture | -600 | -4.3% | High |
| Packed structures | -200 | -1.5% | Low |
| Function tables + strings | -500 | -3.8% | Medium |
| Protothread pattern | -500 | -4.0% | High |
| Integration optimizations | -1,000 | -8.3% | Medium |
| **TOTAL** | **-3,500** | **-23%** | - |

**Baseline:** 15,552 bytes (v8-opt7, uncompressed)
**Target:** ~12,000 bytes (uncompressed)
**With UPX:** ~9,000 bytes (compressed)

---

## Commodore 64 Specific Techniques (Not Directly Applicable)

Some C64 optimizations don't translate to modern x86-64 but are worth noting:

### Zero Page Optimization
- C64: $00-$FF is 1-byte addressable (faster, smaller code)
- Modern: No equivalent (all addressing is uniform)
- **Not applicable**

### Self-Modifying Code
- C64: Common for speed/size (no code protection)
- Modern: Possible but requires mprotect(), overhead > savings
- **Not worth it**

### Page Boundary Alignment
- C64: Crossing page boundaries has cycle penalties
- Modern: Cache lines matter more than pages
- **Already handled by compiler**

---

## Testing Strategy

Each new version MUST pass all existing tests:

1. **Build Test:** Compiles without errors/warnings
2. **Size Test:** Measure and document size reduction
3. **Functional Test:** SSH client connects successfully
4. **Auth Test:** Password authentication works
5. **Data Test:** "Hello World" received by client
6. **Memory Test:** Valgrind shows no leaks
7. **Stress Test:** Multiple connections work

**Test Command:**
```bash
just build v12-dunkels1
just test v12-dunkels1
just size-report
```

---

## Implementation Order (Recommended)

### Phase 1: Low-Risk Optimizations (Week 1)
1. **v14-dunkels3** (Packed structures) - Lowest risk, mechanical
2. **v15-dunkels4** (Function tables) - Low risk, clear benefits

### Phase 2: Medium-Risk Refactoring (Week 2)
3. **v12-dunkels1** (Single buffer) - Moderate risk, significant savings
4. **v13-dunkels2** (Event-driven) - Moderate risk, cleaner architecture

### Phase 3: High-Risk Rewrite (Week 3)
5. **v16-dunkels5** (Protothreads) - High risk, different paradigm

### Phase 4: Integration (Week 4)
6. **v17-dunkels6** (Combined) - Integrate all successful techniques

---

## Success Criteria

**Minimum Success:**
- At least 2,000 bytes saved (-13%)
- All tests pass
- No functionality regression

**Target Success:**
- 3,000-3,500 bytes saved (-20% to -23%)
- Improved code clarity
- Better architecture for embedded

**Stretch Goal:**
- 4,000+ bytes saved (-25%+)
- Under 11,000 bytes uncompressed
- Under 8,500 bytes with UPX

---

## Documentation Requirements

For each version:
1. Document optimization applied
2. Measure size before/after
3. Explain trade-offs made
4. Note any issues encountered
5. Update COMPLETE_SIZE_OPTIMIZATION_REPORT.md

---

## References

### Adam Dunkels' Work
- **uIP Documentation:** https://www.dunkels.com/adam/download/uip-doc-0.6.pdf
- **uVNC Project:** https://dunkels.com/adam/uvnc/
- **Protothreads:** https://dunkels.com/adam/pt/
- **Full TCP/IP for 8-Bit Architectures:** USENIX MobiSys 2003

### Community Resources
- **Codebase64:** https://codebase64.org/ (C64 optimization techniques)
- **Lemon64 Forums:** Discussions on zero page, optimization
- **uIP GitHub:** https://github.com/adamdunkels/uip

### Academic Papers
- Dunkels, A. "Full TCP/IP for 8-Bit Architectures" (MobiSys 2003)
- Dunkels, A. "Protothreads: Simplifying Event-Driven Programming"
- Dunkels, A. "Contiki - A Lightweight and Flexible Operating System"

---

## Risk Mitigation

### Technical Risks
- **Buffer management errors:** Extensive testing, valgrind
- **State machine bugs:** Step-by-step verification
- **Compatibility issues:** Test with standard SSH client

### Project Risks
- **Time estimation:** Start with low-risk versions first
- **Scope creep:** Stick to plan, one version at a time
- **Breaking changes:** Never modify earlier versions

---

## Next Steps

1. **Review this plan** - Ensure alignment with project goals
2. **Start with v14-dunkels3** - Lowest risk, quick win
3. **Build incrementally** - One version at a time
4. **Test thoroughly** - Never skip testing
5. **Document everything** - Size, techniques, lessons learned

---

**Plan Version:** 1.0
**Created:** 2025-11-05
**Author:** Claude (AI Assistant)
**Status:** Ready for implementation
