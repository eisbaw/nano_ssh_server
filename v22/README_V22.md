# v22: Web Search Optimization Techniques Applied

## Overview
v22 is based on v21-static with optimization techniques from web research on embedded C optimization.

## Size Comparison
- **v21-static (baseline)**: 54,392 bytes (musl static)
- **v22**: 41,336 bytes (gcc dynamic)
- **Note**: v22 is smaller due to dynamic vs static linking (not optimization)

## Optimizations Attempted

### ✅ Optimization #1: goto cleanup pattern
**Result**: Size maintained at 41,336 bytes (no change)
**Impact**: Code clarity improved, 44 `close(client_fd); return;` patterns replaced with `goto cleanup;`
**Verdict**: KEPT - No size penalty, cleaner error handling

### ❌ Optimization #2: Function inlining with __attribute__((always_inline))
**Result**: Size INCREASED to 45,432 bytes (+4,096 bytes, +10%)
**Impact**: Inlining small functions duplicated code at each call site
**Verdict**: REVERTED - Compiler already optimizes function calls better

### ❌ Optimization #3: const arrays for string literals
**Result**: Size INCREASED to 41,400 bytes (+64 bytes)
**Impact**: const arrays added symbol table overhead
**Verdict**: REVERTED - #define macros are more efficient for strings

### Analysis: Wrappers
**Checked**: send_data() and recv_data() wrapper functions
**Result**: KEPT - Handle EINTR and partial send/recv, essential functionality

## Web Research Findings

### Techniques that worked:
1. **goto for cleanup** - Reduces code duplication (applied in v22)
2. **Aggressive compiler flags** - Already applied in v21-static

### Techniques that failed:
1. **Always inline small functions** - Increased size significantly
2. **const for string literals** - Added overhead vs #define
3. **Native int types** - Already well-optimized

## Final Result
- **Size**: 41,336 bytes
- **Status**: ✅ FULLY FUNCTIONAL - Tested with real SSH client (sshpass)
- **SSH Test**: PASS - "Hello World" successfully received
- **Key Learning**: Modern compilers (gcc with -Os -flto) already do excellent optimization

## Lessons Learned
1. **goto is not evil** - For size-critical embedded code, goto cleanup patterns reduce duplication
2. **Inlining isn't always smaller** - Function call overhead < code duplication
3. **const isn't free** - Symbol table entries have overhead
4. **Trust the compiler** - Modern optimizers (LTO, -Os) make better decisions than manual micro-optimizations
5. **Measure everything** - Each optimization must be measured; intuition can be wrong

## Recommendation
**v22 vs v21-static choice**:
- **v21-static (54 KB)**: Portable, zero dependencies, musl static ⭐
- **v22 (41 KB)**: Smaller, requires libc at runtime, dynamic
- **Use v21-static** for embedded/portable deployments
- **Use v22** if size is critical and libc is available

## Technical Details
- **Compiler**: gcc 13.3.0
- **Flags**: -Os -flto -fwhole-program -fipa-pta + aggressive optimization
- **Dependencies**: libc.so.6 (dynamic)
- **Crypto**: Custom AES-128-CTR, SHA-256, Curve25519-donna, Ed25519
