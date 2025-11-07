# Makefile for v16-static
# Static musl build of v16-crypto-standalone with aggressive ELF optimizations
# 100% standalone implementation - zero external crypto dependencies
# Custom implementations: AES-128-CTR, SHA-256, HMAC-SHA256, DH Group14, RSA-2048, CSPRNG, Bignum
# Built with musl libc for smaller static binaries

CC = musl-gcc

# Aggressive size optimization flags
# -Os: Optimize for size
# -flto: Link-time optimization
# -ffunction-sections -fdata-sections: Separate sections for linker GC
# -fno-unwind-tables -fno-asynchronous-unwind-tables: Remove unwind tables
# -fno-stack-protector: Remove stack protector
# -fmerge-all-constants: Merge identical constants
# -fno-ident: Remove compiler identification
# -finline-small-functions: Inline small functions
# -fshort-enums: Use smallest enum size
# -fomit-frame-pointer: Remove frame pointer
# -ffast-math: Fast non-IEEE math (smaller code)
# -fno-math-errno: No errno for math operations
# -fvisibility=hidden: Hide symbols by default
# -fno-builtin: Don't use builtin functions
# -fno-plt: No PLT indirection
# -fwhole-program: Whole program optimization
# -fipa-pta: Interprocedural pointer analysis
# -fno-common: Place uninitialized globals in BSS
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -fwhole-program -fipa-pta -fno-common \
         -D_POSIX_C_SOURCE=200809L

# Aggressive linker flags for musl static linking with ELF optimizations
# -static: Create standalone binary (musl libc statically linked)
# -Wl,--gc-sections: Garbage collect unused sections
# -Wl,--strip-all: Strip all symbols
# -Wl,--as-needed: Only link needed libraries
# -Wl,--hash-style=gnu: Use GNU hash style (smaller)
# -Wl,--build-id=none: Remove build ID section
# -Wl,-z,norelro: Disable relocation read-only (smaller)
# -Wl,--no-eh-frame-hdr: Remove exception frame header
# -Wl,-z,noseparate-code: Don't separate code and data (smaller but less secure)
# -Wl,--nmagic: Turn off page alignment of sections (smaller)
# -Wl,--no-dynamic-linker: No dynamic linker needed (static)
LDFLAGS = -static \
          -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none \
          -Wl,-z,norelro -Wl,--no-eh-frame-hdr \
          -Wl,-z,noseparate-code -Wl,--nmagic \
          -Wl,--no-dynamic-linker

SRCS = main.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress verify help

all: $(TARGET) verify compress

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) (musl static)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

compress: $(TARGET)
	@cp $(TARGET) $(TARGET_COMPRESSED)
	@upx --best --ultra-brute $(TARGET_COMPRESSED) 2>&1 || true
	@echo "Compressed: $(TARGET_COMPRESSED)"

verify: $(TARGET)
	@echo ""
	@echo "=== Build Verification ==="
	@ls -lh $(TARGET)
	@stat -c "Size: %s bytes" $(TARGET) 2>/dev/null || stat -f "Size: %z bytes" $(TARGET)
	@echo ""
	@echo -n "File type: "
	@file $(TARGET)
	@echo -n "Dynamic deps: "
	@ldd $(TARGET) 2>&1 || echo "(none - statically linked)"
	@echo ""
	@echo "=== Section Analysis ==="
	@size $(TARGET) 2>/dev/null || true
	@echo ""
	@echo "✅ v16-static musl binary built successfully!"
	@echo ""

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v16-static"

help:
	@echo "v16-static Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build nano_ssh_server with musl static linking"
	@echo "  verify        - Show binary information"
	@echo "  compress      - Compress binary with UPX"
	@echo "  clean         - Remove build artifacts"
	@echo "  help          - Show this help"
	@echo ""
	@echo "To build: make"
	@echo "To run:   ./nano_ssh_server"
	@echo ""
	@echo "Note: This must be built in the musl environment:"
	@echo "  nix-shell shell-musl.nix"
	@echo "  cd v16-static"
	@echo "  make"
	@echo ""
