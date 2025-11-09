# Makefile for v22-smallest
# World's smallest SSH server - composing ALL best optimization techniques
# Base: v21-src-opt + custom linker script from v10 + macro optimizations from v12
# 100% independent crypto implementation
# Custom: AES-128-CTR, SHA-256, HMAC-SHA256, CSPRNG
# Public domain: Curve25519 (curve25519-donna-c64), Ed25519 (c25519), SHA-512
# NO libsodium dependency!
# NO OpenSSL dependency!

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -fwhole-program -fipa-pta -fno-common \
         -D_POSIX_C_SOURCE=200809L
LDFLAGS = -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr
# Linker script removed - causes segfault with complex crypto code
# TODO: Create v22-compatible linker script

SRCS = main_production.c f25519.c fprime.c ed25519.c edsign.c sha512.c curve25519-donna-c64.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress verify

all: $(TARGET) compress verify

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) (uncompressed)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

compress: $(TARGET)
	@cp $(TARGET) $(TARGET_COMPRESSED)
	@upx --best --ultra-brute $(TARGET_COMPRESSED) 2>&1 || true
	@echo "Compressed: $(TARGET_COMPRESSED)"
	@ls -lh $(TARGET) $(TARGET_COMPRESSED) 2>/dev/null || true

verify: $(TARGET)
	@echo ""
	@echo "=== Size Analysis ==="
	@size $(TARGET)
	@stat -c "Binary size: %s bytes (%.2f KB)" $(TARGET) 2>/dev/null | awk '{printf "%s %s (%s KB)\n", $$1, $$2, $$3/1024}' || stat -f "Binary size: %z bytes" $(TARGET)
	@echo ""

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v22-smallest"
