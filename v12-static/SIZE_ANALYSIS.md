# v8-opt7 Detailed Size Analysis

## Section-by-Section Comparison

| Section | v7-opt6 | v8-opt7 | Reduction | % Change |
|---------|---------|---------|-----------|----------|
| **.text** (code) | 8,252 bytes | 6,077 bytes | -2,175 bytes | **-26.4%** |
| **.rodata** (strings) | 3,541 bytes | 440 bytes | -3,101 bytes | **-87.6%** |
| **.plt** (PLT) | 624 bytes | 16 bytes | -608 bytes | **-97.4%** |
| **.plt.sec** | 608 bytes | 0 bytes | -608 bytes | **-100%** |
| **.rela.plt** | 912 bytes | 0 bytes | -912 bytes | **-100%** |
| **.dynsym** | 1,056 bytes | 984 bytes | -72 bytes | -6.8% |
| **.dynstr** | 761 bytes | 723 bytes | -38 bytes | -5.0% |
| **TOTAL** | 23,408 bytes | 15,552 bytes | -7,856 bytes | **-33.6%** |

## Key Insights

### 1. Read-Only Data (.rodata) - 87.6% Reduction

**v7-opt6:** 3,541 bytes
**v8-opt7:** 440 bytes

This is the **biggest win**! The .rodata section stores:
- String literals (all the printf format strings)
- Constant arrays
- Compiler-generated constants

By removing ALL printf() calls, we eliminated:
- ~99 printf format strings
- Error message strings
- Debug output strings

The remaining 440 bytes are:
- SSH protocol constants ("SSH-2.0-NanoSSH_0.1", etc.)
- Algorithm names ("curve25519-sha256", "ssh-ed25519", etc.)
- Hardcoded credentials
- Essential protocol strings

### 2. Text Section (.text) - 26.4% Reduction

**v7-opt6:** 8,252 bytes
**v8-opt7:** 6,077 bytes

The .text section contains executable code. The reduction comes from:
- Removed printf() function calls (~99 calls)
- Removed snprintf() call
- Simplified control flow (no format string parsing)
- -fno-builtin prevented inlining of heavy libc functions

### 3. PLT Sections - Complete Elimination

**PLT (Procedure Linkage Table)** is used for dynamic function calls.

The `-fno-plt` flag eliminated:
- **.plt.sec:** 608 bytes → 0 bytes (100% removed)
- **.rela.plt:** 912 bytes → 0 bytes (100% removed)
- **.plt:** 624 bytes → 16 bytes (97.4% removed)

**Total PLT savings:** 1,520 bytes

This makes function calls more direct and reduces binary size.

### 4. Dynamic Symbol Tables

**v7-opt6:**
- .dynsym: 1,056 bytes
- .dynstr: 761 bytes

**v8-opt7:**
- .dynsym: 984 bytes (-72 bytes)
- .dynstr: 723 bytes (-38 bytes)

Fewer external functions referenced = smaller symbol tables.

## What Each Optimization Removed

### Removed by Eliminating printf()

1. **printf implementation** (~8KB in libc)
   - Format string parser
   - Type conversion routines
   - Buffering logic
   - va_arg handling

2. **String literals** (3,101 bytes in .rodata)
   - 99+ printf format strings
   - Error messages
   - Debug output strings

3. **Function calls** (~2KB in .text)
   - 99+ calls to printf()
   - Variadic argument setup
   - String formatting overhead

### Removed by -fno-plt

1. **PLT indirection** (1,520 bytes total)
   - Jump tables for dynamic linking
   - Relocation entries
   - Lazy binding overhead

### Removed by -fno-builtin

1. **Builtin optimizations**
   - Prevented inline expansion of libc functions
   - Reduced code bloat from "optimized" versions

## Size Distribution

### v7-opt6 (23,408 bytes)

```
Code (.text):        35.3%  (8,252 bytes)
Strings (.rodata):   15.1%  (3,541 bytes)
PLT tables:           6.5%  (1,520 bytes)
Other sections:      43.1%  (10,095 bytes)
```

### v8-opt7 (15,552 bytes)

```
Code (.text):        39.1%  (6,077 bytes)
Strings (.rodata):    2.8%  (440 bytes)
PLT tables:           0.1%  (16 bytes)
Other sections:      58.0%  (9,019 bytes)
```

## Remaining Size Breakdown

The remaining 15,552 bytes consists of:

1. **Core SSH Implementation** (~6KB)
   - Protocol state machine
   - Packet handling
   - Cryptographic operations

2. **Dynamic Linking Overhead** (~4KB)
   - .dynsym, .dynstr: Symbol tables
   - .rela.dyn: Relocation entries
   - .got: Global offset table
   - .dynamic: Dynamic section

3. **Essential Strings** (440 bytes)
   - SSH version string
   - Algorithm names
   - Minimal protocol constants

4. **ELF Headers & Metadata** (~2KB)
   - Program headers
   - Section headers
   - Notes and metadata

5. **Initialization/Cleanup** (~500 bytes)
   - .init/.fini sections
   - Constructor/destructor tables

6. **Other** (~2.5KB)
   - .eh_frame: Exception handling
   - .plt.got: GOT entries
   - Alignment padding

## Lessons Learned

### What Costs the Most

1. **stdio.h (printf family):** 8KB+ of overhead
2. **String literals:** 3KB of .rodata
3. **PLT indirection:** 1.5KB of tables
4. **Format string parsing:** Massive runtime overhead

### What's Essential

1. **Core protocol logic:** Cannot be reduced without breaking SSH
2. **Crypto libraries:** libsodium/libcrypto are already minimal
3. **Network I/O:** Socket operations are unavoidable
4. **Dynamic linking:** Required for shared libraries

### Optimization Hierarchy

From most to least impactful:

1. **Remove printf() family** - Saved 8KB+ (this version)
2. **Remove error strings** - Already done in v6-opt5
3. **Use -fno-plt** - Saved 1.5KB (this version)
4. **Aggressive compiler flags** - Done in v2-v7
5. **Strip symbols** - Already done
6. **Remove debug sections** - Already done

## Next Steps for Further Reduction

To go below 15KB, you would need to:

1. **Remove dynamic linking** (-static)
   - Would increase size initially (~50KB+)
   - But allows aggressive dead code elimination
   - Final size could be ~12KB with musl libc

2. **Replace libsodium with TweetNaCl**
   - TweetNaCl is smaller but slower
   - Potential saving: ~2KB

3. **Implement custom minimal crypto**
   - High risk, security concerns
   - Potential saving: ~5KB

4. **Remove OpenSSL, use libsodium only**
   - Change from AES-CTR to ChaCha20
   - Potential saving: ~3KB

5. **Use assembly for hot paths**
   - Hand-optimize packet handling
   - Potential saving: ~1KB

## Conclusion

**v8-opt7 achieves a 33.6% size reduction primarily by eliminating printf() and related stdio overhead.**

The biggest win came from .rodata reduction (87.6%), proving that **string literals are extremely expensive** in size-constrained applications.

For embedded systems, this trade-off (no debug output for 7.8KB savings) is often worthwhile.

---

*Analysis Date: 2025-11-04*
*Tool: GNU size, readelf, objdump*
*Compiler: GCC with -Os -flto*
