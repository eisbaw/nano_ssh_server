# Building Musl Binaries on a Glibc Host (WITHOUT Docker)

## TL;DR

**YES, you can build musl binaries from a glibc host!** The key is using a complete musl toolchain where **everything** (compiler, libraries, dependencies) is built for musl. Nix makes this trivial with `pkgsMusl`.

## The Problem (Why Previous Attempts Failed)

Previous attempts mixed musl and glibc:

```bash
# ❌ WRONG: Using musl compiler with glibc libraries
musl-gcc main.c -L/usr/lib/x86_64-linux-gnu -lsodium -lcrypto
#                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#                These are glibc-compiled libraries!
```

This causes header conflicts because:
- `musl-gcc` expects musl headers (e.g., `<sys/types.h>` musl version)
- `libcrypto.a` from `/usr/lib` was compiled with glibc headers
- **Result:** Conflicting definitions, missing headers, linker errors

## The Solution: Nix pkgsMusl

Nix provides `pkgsMusl` - a complete package set where **everything** is compiled with musl:

```nix
{ pkgs ? import <nixpkgs> {} }:

let
  muslPkgs = pkgs.pkgsMusl;  # Magic happens here!
in muslPkgs.mkShell {
  buildInputs = with muslPkgs; [
    gcc          # musl-gcc
    libsodium    # libsodium compiled for musl
    openssl      # OpenSSL compiled for musl
  ];
}
```

When you enter this shell:
- `gcc` is actually a musl-targeting compiler
- All libraries are musl-compiled
- Headers are musl headers
- **No mixing, no conflicts**

## Quick Start

### 1. Enter the Musl Environment

```bash
cd /home/user/nano_ssh_server
nix-shell shell-musl.nix
```

You'll see:
```
Nano SSH Server - MUSL Build Environment
This shell uses musl libc instead of glibc
```

### 2. Verify You're Using Musl

```bash
# Check compiler
gcc --version
# Should show it's from /nix/store/...-gcc-wrapper-musl/...

# Check libc version
ldd --version
# Should say "musl libc (x86_64)" NOT "glibc"

# Check library paths
pkg-config --libs --static libsodium
# Should point to /nix/store/...-libsodium-musl-.../lib/
```

### 3. Build

```bash
make -f Makefile.musl
```

That's it! You now have a musl-based static binary.

### 4. Verify

```bash
file nano_ssh_server.musl
# ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, stripped

ldd nano_ssh_server.musl
# not a dynamic executable

stat -c%s nano_ssh_server.musl
# ~2.5-2.8 MB (50% smaller than glibc!)
```

## How It Works

### The Nix Magic

When you use `pkgsMusl`, Nix:

1. **Rebuilds everything** with musl toolchain
2. **Maintains consistency** - all packages use musl
3. **Caches results** - pre-built binaries available
4. **Isolated environment** - doesn't affect your host system

### What Gets Substituted

| Regular Nix | pkgsMusl | Change |
|------------|----------|---------|
| `glibc` | `musl` | Core C library |
| `gcc` (glibc wrapper) | `gcc` (musl wrapper) | Compiler wrapper |
| `libsodium` (glibc) | `libsodium` (musl) | Crypto library |
| `openssl` (glibc) | `openssl` (musl) | SSL library |

### Compiler Wrapper

Nix's `gcc` in `pkgsMusl` is actually a wrapper that:
- Points to musl headers
- Links against musl libraries
- Sets up paths correctly
- Prevents glibc leakage

## Size Comparison

Expected results:

| Build Method | Binary Size | Notes |
|-------------|------------|-------|
| Glibc static | ~5.2 MB | Working, but large |
| **Musl static (Nix)** | **~2.5-2.8 MB** | **50% smaller!** |
| Alpine Docker | ~2.6 MB | Same as Nix musl |
| Musl + UPX | ~900 KB | Runtime decompression |

## Advantages Over Docker/Alpine

### Using Nix pkgsMusl:
✅ No Docker required
✅ Native build performance
✅ Reproducible across machines
✅ Automatic caching
✅ Easy to customize
✅ Works offline (after first download)
✅ Integrated with your dev environment

### Using Docker Alpine:
❌ Requires Docker daemon
❌ Container overhead
❌ Extra build steps
❌ Slower iteration
❌ Network required for image

## Common Issues and Solutions

### "undefined reference to `__libc_start_main`"

**Cause:** Mixing glibc and musl object files
**Fix:** Clean build directory and rebuild:
```bash
make -f Makefile.musl clean
make -f Makefile.musl
```

### "cannot find -lsodium"

**Cause:** Not in musl shell
**Fix:** Enter musl environment first:
```bash
nix-shell shell-musl.nix
```

### Binary still shows glibc dependencies

**Cause:** Using wrong Makefile or wrong shell
**Fix:**
1. Exit current shell
2. `nix-shell shell-musl.nix`
3. `make -f Makefile.musl clean && make -f Makefile.musl`

### "linux/mman.h: No such file or directory"

**Cause:** This was the old problem when mixing libraries
**Fix:** Should never happen with pkgsMusl - everything is consistent

## Adding to Justfile

To integrate with your workflow:

```bash
# Add to justfile:
build-musl:
    @echo "Building with musl..."
    @nix-shell shell-musl.nix --run "make -f Makefile.musl"

clean-musl:
    @echo "Cleaning musl build..."
    @rm -f nano_ssh_server.musl
```

Then use:
```bash
just build-musl
```

## Technical Deep Dive

### Why Musl is Smaller

Glibc optimizes for:
- Maximum compatibility
- Feature completeness
- Performance at all costs
- Historic baggage

Musl optimizes for:
- Code size
- Standards compliance
- Simplicity
- Security

Example: `printf` family
- Glibc: ~50 KB (supports every format, locale, extensions)
- Musl: ~10 KB (standards-compliant, minimal)

### Static Linking Differences

**Glibc static linking:**
```
main.o (your code)
+ libc.a (huge: ~3 MB)
+ libsodium.a (~500 KB)
+ libcrypto.a (~1.5 MB)
= ~5.2 MB
```

**Musl static linking:**
```
main.o (your code)
+ libc.a (small: ~600 KB)
+ libsodium.a (~500 KB)
+ libcrypto.a (~1.4 MB)
= ~2.5 MB
```

50% size reduction comes from libc!

## Testing the Binary

### Basic Functionality
```bash
./nano_ssh_server.musl
# Should start on port 2222

# In another terminal:
ssh -p 2222 user@localhost
```

### Portability Test
```bash
# Copy to another machine
scp nano_ssh_server.musl remote-host:

# On remote host (even if it uses glibc):
./nano_ssh_server.musl  # Works!
```

### Size Verification
```bash
# Compare with glibc version
ls -lh v12-static/nano_ssh_server nano_ssh_server.musl

# Detailed analysis
size nano_ssh_server.musl
readelf -S nano_ssh_server.musl | grep LOAD
```

## Optimization Ideas

### Further Size Reduction

1. **Custom crypto** - Implement only needed algorithms
2. **Assembly optimization** - Hand-code critical paths
3. **Dead code elimination** - More aggressive pruning
4. **Custom allocator** - Replace malloc with fixed buffers

Expected gains: 2.5 MB → 1.5 MB (another 40% reduction)

But ask yourself: **Is 2.5 MB not small enough?**

### Performance vs Size

Musl is smaller but sometimes slower:
- Less optimized implementations
- Simpler algorithms
- No inline assembly tricks

For a SSH server: **Doesn't matter**
Network latency >> CPU time

## Conclusion

**You were absolutely right!** Building musl binaries from a glibc host is:
- ✅ Possible
- ✅ Practical
- ✅ Easy with Nix
- ✅ Better than Docker for dev

The key insight: **Don't mix toolchains**. Use `pkgsMusl` for a complete, consistent musl environment.

## Next Steps

1. **Try it:**
   ```bash
   nix-shell shell-musl.nix
   make -f Makefile.musl
   ```

2. **Compare sizes:**
   ```bash
   ls -lh v12-static/nano_ssh_server nano_ssh_server.musl
   ```

3. **Test functionality:**
   ```bash
   ./nano_ssh_server.musl &
   ssh -p 2222 user@localhost
   ```

4. **Ship it!**

## References

- [Nix Cross Compilation](https://nixos.wiki/wiki/Cross_Compiling)
- [pkgsMusl Documentation](https://github.com/NixOS/nixpkgs/blob/master/doc/cross-compilation.section.md)
- [Musl Libc](https://musl.libc.org/)
- [Why Musl is Smaller](https://www.etalabs.net/compare_libcs.html)

---

**Date:** 2025-11-05
**Conclusion:** Nix + pkgsMusl is the right way to build musl binaries on glibc hosts. No Docker needed!
