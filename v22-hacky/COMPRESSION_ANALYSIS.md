# UPX Compression Analysis: Which Sections Compress Best?

## Executive Summary

UPX achieved **52% compression** on v22-hacky (54,168 → 25,972 bytes). Analysis reveals that different ELF sections have vastly different compressibility, indicating specific optimization opportunities.

## Section-by-Section Compression Results

| Section | Original Size | Compressed (gzip -9) | Savings | Compression Ratio |
|---------|--------------|---------------------|---------|-------------------|
| **.rodata** | 6,328 B | 1,821 B | 4,507 B | **71.2%** ⭐ |
| **.text** | 45,856 B | 21,448 B | 24,408 B | **53.2%** |
| **TOTAL** | 52,184 B | 23,269 B | 28,915 B | **55.4%** |

### Key Finding
**.rodata compresses 1.34x better than .text** - string literals and lookup tables have more redundancy than code.

## Why Does .rodata Compress So Well? (71.2%)

### Contents of .rodata Section
1. **Crypto lookup tables** (AES S-boxes, curve25519 constants)
   - AES S-box: 256-byte lookup table
   - Curve25519 base point coordinates
   - SHA-256/SHA-512 round constants

2. **String literals** (protocol strings, algorithm names)
   - `"curve25519-sha256"` (17 bytes)
   - `"ssh-ed25519"` (11 bytes)
   - `"aes128-ctr"` (10 bytes)
   - `"hmac-sha2-256"` (13 bytes)
   - `"ssh-userauth"` (12 bytes)
   - `"ssh-connection"` (14 bytes)
   - `"password"` (8 bytes)
   - `"user"` (4 bytes)
   - `"password123"` (11 bytes)
   - `"session"` (7 bytes)

### Why It Compresses So Well
- **Zeros and repeated patterns** in crypto constants
- **Common substrings**: "ssh-", "sha", "25519" appear multiple times
- **Null padding** between strings (alignment)

### Optimization Opportunities for .rodata
✅ **Already done:**
- Shortened server version string `"SSH-2.0-NanoSSH_0.1"` → `"SSH-2.0-N"` (-10 bytes)
- Removed error message `"Unknown channel type"` → `""` (-20 bytes)

🔍 **Potential further optimizations:**
1. **String deduplication**: Extract common prefixes/suffixes
   - `"ssh-userauth"` and `"ssh-connection"` share `"ssh-"`
   - `"curve25519-sha256"`, `"hmac-sha2-256"`, `"ssh-ed25519"` share substrings

2. **Compress crypto lookup tables**:
   - AES S-box could be generated at runtime (trade 256 bytes for ~50 bytes of code)
   - Curve25519 constants could be computed vs. stored
   - **Trade-off**: Code size increase vs. data size decrease

3. **Use shorter algorithm names internally**:
   - Store `"c"` for `"curve25519-sha256"`
   - Expand to full string only when sending to client
   - **Risk**: More complex code, may increase .text

## Why Does .text Compress Moderately? (53.2%)

### Instruction Pattern Analysis

**Total instructions in .text:** 10,960
**Unique instructions:** 908
**Repeated instructions:** 10,052 (91.7%)

This massive redundancy (91.7%!) is what allows compression to work.

### Top Repeated Instructions
| Instruction | Count | % of Total |
|-------------|-------|------------|
| `mov` | 3,036 | 27.7% |
| `add` | 850 | 7.8% |
| `adc` | 560 | 5.1% |
| `mul` | 464 | 4.2% |
| `xchg` | 418 | 3.8% |
| `lea` | 407 | 3.7% |
| **Top 6** | **5,735** | **52.3%** |

### Top Repeated Instruction Pairs
| Pair | Count |
|------|-------|
| `mov` + `mov` | 981 |
| `add` + `mov` | 393 |
| `xchg` + `mov` | 256 |
| `add` + `adc` | 255 |
| `mov` + `adc` | 250 |

### What This Tells Us

The high redundancy comes from:
1. **Bignum arithmetic** (mul, add, adc) - Curve25519 operations
2. **Register shuffling** (mov, xchg, lea) - ABI calling conventions
3. **Crypto primitives** - SHA-256, AES, Ed25519 all have similar patterns

### Most Called Functions
| Address | Calls | Likely Function |
|---------|-------|----------------|
| 0x40abdd | 48 | Likely memory operations (memcpy/memset) |
| 0x4016b3 | 31 | Likely big integer multiply |
| 0x40147f | 20 | Likely field arithmetic |
| 0x401dbf | 17 | Likely hash function |

### Optimization Opportunities for .text

✅ **Already applied:**
- `-Oz` (optimize for size aggressively)
- `-flto` (link-time optimization)
- `-ffunction-sections` + `--gc-sections` (remove unused code)
- `-fwhole-program` (enables more aggressive inlining/optimization)

🔍 **Potential further optimizations:**

1. **Replace Curve25519-donna with smaller implementation**
   - Current: ~14KB source (curve25519-donna-c64.c)
   - Alternative: Compact C implementation or hand-optimized assembly
   - Could save 5-10 KB in .text

2. **Replace c25519 Ed25519 with minimal implementation**
   - Current: Multiple files (ed25519.c, edsign.c, f25519.c, fprime.c)
   - Alternative: Minimal signing-only implementation
   - Could save 3-5 KB in .text

3. **Unroll fewer loops** (trade speed for size)
   - Current compiler unrolls crypto loops for performance
   - Add `-fno-unroll-loops` flag
   - May save 1-2 KB but will be slower

4. **Merge similar functions** (string comparison, memory operations)
   - Many small helper functions could be merged
   - Requires manual refactoring
   - Could save 500-1000 bytes

5. **Use jump tables instead of switch statements**
   - Packet type dispatch could use smaller jump table
   - Could save 200-500 bytes

## Why UPX Works So Well

UPX uses **LZMA compression** which excels at:
- Finding repeated byte sequences (instruction patterns)
- Compressing zeros and sparse data (lookup tables)
- Dictionary-based compression (repeated strings)

### UPX Compression Strategy
1. Compresses entire binary with LZMA
2. Prepends tiny decompressor stub (~600 bytes)
3. At runtime: decompressor unpacks to memory and jumps to entry point
4. **Net result**: 52% smaller on disk, same size in RAM

## Practical Recommendations

### For Maximum Compression (beyond v22-hacky):

**High Impact, Low Risk:**
1. ✅ Use `objcopy` to strip sections (we're doing this)
2. ✅ Use `-Oz` instead of `-Os` (we're doing this)
3. ✅ Apply UPX compression (we're doing this)
4. 🔸 Add `-fno-unroll-loops` (may save 1-2 KB)

**Medium Impact, Medium Risk:**
5. 🔸 Replace crypto implementations with smaller ones
   - Requires extensive testing
   - Could save 8-15 KB
   - May break compatibility or introduce bugs

**High Impact, High Risk:**
6. 🔴 Custom `_start` (remove glibc/musl startup code)
   - Could save ~500 bytes
   - Requires assembly and deep system knowledge
7. 🔴 Manual syscalls (bypass libc wrappers)
   - Could save ~1 KB
   - Brittle, system-specific

### For v22-hacky Specifically:

**We've already hit diminishing returns.** The binary is:
- Compiled with aggressive size flags
- Using minimal crypto implementations
- Stripped of debug/metadata sections
- Statically linked with musl (13.5x smaller than glibc)

**Further size reduction requires:**
- Trading functionality (remove features)
- Trading speed (slower algorithms)
- Trading portability (platform-specific code)
- Trading safety (remove error checking)

## Conclusion

The compression analysis reveals that:

1. **.rodata (71.2% compression)** = High redundancy in strings/tables
   - Potential: ~1-2 KB savings from string deduplication
   - Effort: Medium

2. **.text (53.2% compression)** = Repeated instruction patterns
   - Potential: ~8-15 KB savings from smaller crypto libraries
   - Effort: High (requires crypto rewrite)

3. **Overall (52.1% UPX compression)** = Already near-optimal
   - v21-static → v22-hacky saved only 224 bytes (-0.4%)
   - Most low-hanging fruit has been picked

**Verdict**: v22-hacky represents the practical limit of size optimization without major architectural changes or feature removal.

---

*Analysis performed: 2025-11-10*
*Binary analyzed: v22-hacky (54,168 bytes → 25,972 bytes UPX)*
