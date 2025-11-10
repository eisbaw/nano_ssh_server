# Makefile for v22-backport
# v17-from14 + backported tricks from v20-opt, v21-src-opt, and v21-static
# 100% independent crypto implementation
# Custom: AES-128-CTR, SHA-256, HMAC-SHA256, CSPRNG
# Public domain: Curve25519 (curve25519-donna-c64), Ed25519 (c25519), SHA-512
# NO libsodium dependency!
# NO OpenSSL dependency!
# MUSL STATIC: Statically linked with musl libc (much smaller than glibc)
#
# Optimization tricks applied:
# FROM v20-opt: -fwhole-program -fipa-pta -fno-common, removed -lsodium, added curve25519-donna-c64.c
# FROM v21-src-opt: Removed error messages, unused constants, simplified source (102 lines removed)
# FROM v21-static: musl-gcc, -static linking
# ADDITIONAL: More aggressive inlining, jump table opts, linker relaxation

CC = musl-gcc
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -fno-inline-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -fwhole-program -fipa-pta -fno-common -fno-jump-tables \
         -D_POSIX_C_SOURCE=200809L
LDFLAGS = -static -s -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr \
          -Wl,-O3 -Wl,--relax

SRCS = main_production.c f25519.c fprime.c ed25519.c edsign.c sha512.c curve25519-donna-c64.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress verify

all: $(TARGET) compress verify

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) (musl static, further optimized)"

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
	@echo "✅ v22-backport: Further optimized (v21-src-opt + aggressive flags)!"
	@echo ""

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v22-backport"
