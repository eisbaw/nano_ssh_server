# Makefile for v22-static
# v22-c25519 + musl static linking
# 100% independent crypto implementation
# Custom: AES-128-CTR, SHA-256, HMAC-SHA256, CSPRNG
# Public domain c25519 (Daniel Beer): Curve25519 X25519 ladder + Ed25519, SHA-512
# Replaces v21-static's 20 KB curve25519-donna-c64 with the small c25519 ladder.
# NO libsodium dependency!
# NO OpenSSL dependency!
# MUSL STATIC: Statically linked with musl libc (much smaller than glibc)

CC = musl-gcc
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -fwhole-program -fipa-pta -fno-common \
         -D_POSIX_C_SOURCE=200809L
LDFLAGS = -static -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr

SRCS = main_production.c f25519.c fprime.c ed25519.c edsign.c sha512.c c25519.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress verify

all: $(TARGET) verify

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) (musl static, uncompressed)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

compress: $(TARGET)
	@cp $(TARGET) $(TARGET_COMPRESSED)
	@upx --best --ultra-brute $(TARGET_COMPRESSED) 2>&1 || true
	@echo "Compressed: $(TARGET_COMPRESSED)"
	@ls -lh $(TARGET) $(TARGET_COMPRESSED) 2>/dev/null || true

verify: $(TARGET)
	@echo ""
	@echo "=== Verification ==="
	@echo -n "File type: "
	@file $(TARGET)
	@echo -n "Dynamic deps: "
	@ldd $(TARGET) 2>&1 || echo "(none - statically linked)"
	@echo ""
	@echo "=== Size Analysis ==="
	@size $(TARGET)
	@echo ""
	@ls -lh $(TARGET)
	@stat -c "Size: %s bytes" $(TARGET) 2>/dev/null || stat -f "Size: %z bytes" $(TARGET)
	@echo ""
	@echo "✅ Musl static binary built successfully!"
	@echo ""

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v22-static"
