# v22-hacky: Extremely Hacky Optimizations

## Size Comparison

| Version | Size | Savings vs v21 | Notes |
|---------|------|---------------|-------|
| v21-static | 54,392 bytes | baseline | musl static build |
| v22-hacky | 54,168 bytes | -224 bytes (-0.4%) | extra sections stripped |
| v22-hacky.upx | 25,972 bytes | -28,420 bytes (-52%) | UPX compressed |

## Optimizations Applied

### Compiler Flags (Makefile)
- Changed `-Os` to `-Oz` (more aggressive size optimization)
- Added `-fmerge-constants`, `-fstrict-aliasing`, `-fstrict-overflow`
- Added `-fdelete-null-pointer-checks`, `-fno-stack-check`
- Added `-DNDEBUG` to disable assertions
- Added `-w` to suppress all warnings
- Enhanced linker flags: `-Wl,--relax`, `-Wl,-O2`, `-Wl,--sort-common`

### Binary Stripping
- Automated `objcopy` to remove unnecessary ELF sections:
  - `.comment`
  - `.note`
  - `.note.gnu.build-id`
  - `.gnu.version`
  - `.eh_frame` (exception handling - not needed in static binary)
- Saved 224 bytes from section removal

### Source-Level Optimizations
1. **Shortened version string**: `"SSH-2.0-NanoSSH_0.1"` → `"SSH-2.0-N"`
   - Reduced from 21 bytes to 11 bytes (-10 bytes)
2. **Removed error message**: `"Unknown channel type"` → `""`
   - Saved 20 bytes
3. **String deduplication**: Reused empty strings where possible

### UPX Compression
- Applied `upx --best --ultra-brute` for runtime compression
- Reduces on-disk size by 52%
- Binary self-decompresses on execution (minimal overhead)

## Section Analysis

```
   text    data     bss     dec     hex filename
  43,557      60   2,080  45,697   b281 nano_ssh_server (v22-hacky)
  44,249      60   2,080  46,389   b535 nano_ssh_server (v21-static)
```

Text section reduced by 692 bytes through compiler optimizations.

## Testing

✅ Tested with real SSH client (OpenSSH):
```bash
sshpass -p password123 ssh -p 2222 user@localhost
```

Output: `Hello World` ✅

## Build Instructions

```bash
cd v22-hacky
make clean
make
```

Produces:
- `nano_ssh_server` - 54,168 bytes (stripped)
- `nano_ssh_server.upx` - 25,972 bytes (UPX compressed)

## Warning

These optimizations are **extremely aggressive** and may:
- Break compatibility on some systems
- Make debugging harder (symbols stripped, assertions disabled)
- Rely on undefined behavior in some cases (`-fstrict-aliasing`)

**Use at your own risk!** This is for educational purposes and size competition.

For production use, stick with v21-static.

## Future Optimizations (Ideas)

If we wanted to go even further (not implemented):
- Custom `_start` instead of standard C runtime (save ~500 bytes)
- Assembly implementations of hot functions
- Manual syscalls instead of libc wrappers
- Remove more string literals
- Link-time dead code elimination with manual symbol lists
- Custom linker script to optimize memory layout
- Strip more aggressively with `sstrip` (if available)

## Conclusion

v22-hacky achieves **minimal gains** (224 bytes = 0.4%) over v21-static through:
1. Section stripping
2. Aggressive compiler flags
3. Minor string optimizations

The code was already extremely well-optimized in v21-static, so further gains are marginal without sacrificing functionality or compatibility.

**UPX compression** provides significant on-disk size reduction (52%) but is a runtime decompressor, not a true size optimization.

---

*Last updated: 2025-11-10*
